#include <WebServer.h>
#include <ctype.h>
#include "webapi.h"
#include "flashstats.h"
#include "config.h"
#include "sensors.h"
#include "alerts.h"
#include "screen.h"
#include "net.h"
#include "outdoor.h"
#include "history.h"
#include "stats.h"
#include "eventlog.h"
#include "monitor.h"
#include "cloud.h"
#include "webpage.h"
#include "webpage_gz.h"

static WebServer server(PUERTO_HTTP);

// ---------------------------------------------------------------- utilidades

// Si un buffer se llena a mitad del ultimo caracter UTF-8, elimina solamente
// esa secuencia incompleta. Un caracter multibyte completo al final se conserva:
// quitar a ciegas sus bytes de continuacion dejaria precisamente el lider
// huerfano que se intenta evitar.
static void quitarUtf8Incompleto(char *dst, size_t &len) {
  if (!len) return;

  size_t inicio = len - 1;
  while (inicio > 0 && ((unsigned char)dst[inicio] & 0xC0) == 0x80) inicio--;

  unsigned char lider = (unsigned char)dst[inicio];
  size_t esperados = 1;
  if ((lider & 0xE0) == 0xC0)      esperados = 2;
  else if ((lider & 0xF0) == 0xE0) esperados = 3;
  else if ((lider & 0xF8) == 0xF0) esperados = 4;

  if (len - inicio < esperados) len = inicio;
}

// Escapa comillas y barras para no romper el JSON con un mensaje de log raro.
// El cast a unsigned char NO es cosmetico: `char` es signed en xtensa, y sin
// el todo byte UTF-8 (>=0x80) sale negativo, cae en la rama de "control" y se
// convierte en un espacio. Con eso "Alcala" perdia la tilde en toda la API.
static void escapar(const char *src, char *dst, size_t n) {
  size_t j = 0;
  for (size_t i = 0; src[i] && j + 2 < n; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c == '"' || c == '\\') dst[j++] = '\\';
    else if (c < 0x20) { dst[j++] = ' '; continue; }
    dst[j++] = (char)c;
  }
  quitarUtf8Incompleto(dst, j);
  // Y si al retroceder quedo una barra de escape huerfana, tambien se quita:
  // si no, el JSON acaba en \" y el parseo del navegador se come la comilla
  // de cierre.
  if (j > 0 && dst[j - 1] == '\\') j--;
  dst[j] = '\0';
}

// snprintf trunca en silencio si el buffer se queda corto, y un JSON cortado a
// medias llega al navegador como error de parseo sin pista de por que. Mejor
// devolver un 500 explicito y dejar constancia en el log.
static bool enviarJson(int escrito, size_t capacidad, const char *ruta,
                       const char *buf) {
  if (escrito < 0 || (size_t)escrito >= capacidad) {
    logEvento(LOG_ERROR, "JSON truncado en %s (%d/%u)", ruta, escrito,
              (unsigned)capacidad);
    server.send(500, "application/json",
                "{\"error\":\"respuesta truncada en el aparato\"}");
    return false;
  }
  server.send(200, "application/json", buf);
  return true;
}

// ------------------------------------------------------------------ anti-CSRF

// De los siete POST de este servidor, cinco escriben la EEPROM del sensor o
// la NVS (exterior, token AQICN, config de nube, calibrar y ASC; reset y
// autotest del SCD41 no persisten nada). Sin esta comprobacion, cualquier
// pagina abierta en el navegador de casa puede dispararlos con un
// fetch(...,{mode:"no-cors"}): POST no basta, no hay preflight que lo pare.
//
// Criterio: si NO viene Origin se acepta (curl, scripts y apps no la mandan y
// no hay CSRF posible sin navegador). Si viene, tiene que ser exactamente el
// host al que se ha llamado - mDNS o IP, con o sin puerto, da igual: se compara
// contra el Host de la propia peticion.
static bool origenValido() {
  String origin = server.header("Origin");
  if (origin.length() == 0) return true;
  if (origin == "null") return false;   // sandbox, data:, file:
  int p = origin.indexOf("://");
  if (p < 0) return false;
  return origin.substring(p + 3) == server.hostHeader();
}

// Envuelve un manejador POST con la comprobacion de origen. Devuelve false si
// ya ha respondido 403 y el manejador no debe seguir.
static bool postPermitido() {
  if (origenValido()) return true;
  logEvento(LOG_AVISO, "POST rechazado: origen %s",
            server.header("Origin").c_str());
  server.send(403, "application/json",
              "{\"ok\":false,\"msg\":\"Origen no permitido. Abre el panel desde "
              "la direccion del aparato.\"}");
  return false;
}

// ------------------------------------------------------------------ /api/now

static void rutaNow() {
  // 1050 y no 900: el bloque "umbrales" (los 4 #define de config.h) añade
  // ~70 B en el peor caso; se sube el margen para no rozar el truncado.
  char buf[1050];
  uint32_t scdHace = scd_estado.ultimaLecturaMs
                   ? (millis() - scd_estado.ultimaLecturaMs) / 1000 : 0;

  // Las medias de 24 h viajan aqui y no solo en /api/stats porque el veredicto
  // de la cabecera depende de ellas: si la web tuviera que pedir dos endpoints
  // para explicar un titular, acabaria enseñando el titular sin explicacion.
  char m25[12] = "null", m10[12] = "null", n25[8] = "null", n10[8] = "null";
  if (medias24.hayPm25) {
    snprintf(m25, sizeof m25, "%.1f", medias24.pm25);
    snprintf(n25, sizeof n25, "%u", (unsigned)alertas.pm25_24h);
  }
  if (medias24.hayPm10) {
    snprintf(m10, sizeof m10, "%.1f", medias24.pm10);
    snprintf(n10, sizeof n10, "%u", (unsigned)alertas.pm10_24h);
  }

  int n = snprintf(buf, sizeof buf,
    "{\"ts\":%lu,\"hora_ok\":%s,\"uptime_s\":%lu,"
    "\"pms\":{\"ok\":%s,\"estado\":%u,\"pm1\":%u,\"pm25\":%u,\"pm10\":%u,"
    "\"n_pm25\":%u,\"n_pm10\":%u,\"frames\":%lu,\"rancio\":%s},"
    "\"scd\":{\"ok\":%s,\"estado\":%u,\"co2\":%u,\"temp\":%.1f,\"hum\":%.1f,"
    "\"n_co2\":%u,\"n_temp\":%u,\"n_hum\":%u,\"reads\":%lu,"
    "\"error\":%d,\"etapa\":\"%s\",\"i2c\":%s,"
    "\"rancio\":%s,\"congelado\":%s,\"hace_s\":%lu,\"reinicios\":%lu},"
    "\"m24\":{\"pm25\":%s,\"pm10\":%s,\"n_pm25\":%s,\"n_pm10\":%s},"
    "\"umbrales\":{\"pm25_aviso\":%u,\"pm25_malo\":%u,"
    "\"co2_aviso\":%u,\"co2_malo\":%u},"
    "\"peor\":{\"nivel\":%u,\"que\":\"%s\"}}",
    (unsigned long)redAhora(),
    red_estado.horaOk ? "true" : "false",
    (unsigned long)(millis() / 1000),

    saludPms() == SALUD_OK ? "true" : "false", (unsigned)saludPms(),
    (unsigned)pms_estado.pm1, (unsigned)pms_estado.pm25, (unsigned)pms_estado.pm10,
    (unsigned)alertas.pm25, (unsigned)alertas.pm10,
    (unsigned long)pms_estado.frames,
    pms_estado.rancio ? "true" : "false",

    saludScd() == SALUD_OK ? "true" : "false", (unsigned)saludScd(),
    (unsigned)scd_estado.co2, scd_estado.temp, scd_estado.hum,
    (unsigned)alertas.co2, (unsigned)alertas.temp, (unsigned)alertas.hum,
    (unsigned long)scd_estado.reads,
    (int)scd_estado.error, scd_estado.etapa,
    scd_estado.presenteI2c ? "true" : "false",
    scd_estado.rancio ? "true" : "false",
    scd_estado.congelado ? "true" : "false",
    (unsigned long)scdHace, (unsigned long)scd_estado.reinicios,

    m25, m10, n25, n10,

    (unsigned)PM25_AVISO, (unsigned)PM25_MALO,
    (unsigned)CO2_AVISO,  (unsigned)CO2_MALO,

    (unsigned)alertas.peor, alertas.peorQue);

  enviarJson(n, sizeof buf, "/api/now", buf);
}

// --------------------------------------------------------------- /api/health

static void rutaHealth() {
  // 1100 y no 900: con SSID de 32 caracteres, etapa larga y numero de serie de
  // 16 hex el peor caso ronda los 800 B y el margen se quedaba corto.
  char buf[1100];
  char ssidEsc[sizeof(red_estado.ssid) * 2];
  escapar(red_estado.ssid, ssidEsc, sizeof ssidEsc);
  uint32_t sinFrame = pms_estado.ultimoFrameMs
                    ? (millis() - pms_estado.ultimoFrameMs) / 1000 : 0;
  uint32_t scdHace = scd_estado.ultimaLecturaMs
                   ? (millis() - scd_estado.ultimaLecturaMs) / 1000 : 0;
  int n = snprintf(buf, sizeof buf,
    "{\"uptime_s\":%lu,\"reset\":\"%s\","
    "\"heap_libre\":%lu,\"heap_min\":%lu,\"heap_total\":%lu,"
    "\"wifi\":{\"ok\":%s,\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"recon\":%lu},"
    "\"hora_ok\":%s,"
    "\"pms\":{\"ok\":%s,\"estado\":%u,\"frames\":%lu,\"hace_s\":%lu,\"durmiendo\":%s},"
    "\"scd41\":{\"ok\":%s,\"estado\":%u,\"i2c\":%s,\"reads\":%lu,"
    "\"errores\":%lu,\"ultimo_error\":%d,\"etapa\":\"%s\","
    "\"rancio\":%s,\"congelado\":%s,\"hace_s\":%lu,\"reinicios\":%lu,"
    "\"asc\":%u,\"selftest\":%u,\"serie\":\"%llX\",\"altitud\":%u},"
    "\"flash\":{\"usado\":%lu,\"libre\":%lu,\"escrituras\":%lu},"
    "\"oled\":{\"ok\":%s,\"addr\":%u},"
    "\"historial\":{\"muestras\":%u,\"capacidad\":%u},"
    "\"log\":{\"eventos\":%u,\"capacidad\":%u}}",
    (unsigned long)(millis() / 1000), motivoReset(),
    (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
    (unsigned long)ESP.getHeapSize(),
    red_estado.conectado ? "true" : "false",
    ssidEsc, red_estado.ip, (int)red_estado.rssi,
    (unsigned long)red_estado.reconexiones,
    red_estado.horaOk ? "true" : "false",
    saludPms() == SALUD_OK ? "true" : "false", (unsigned)saludPms(),
    (unsigned long)pms_estado.frames, (unsigned long)sinFrame,
    pms_estado.durmiendo ? "true" : "false",
    saludScd() == SALUD_OK ? "true" : "false", (unsigned)saludScd(),
    scd_estado.presenteI2c ? "true" : "false",
    (unsigned long)scd_estado.reads, (unsigned long)scd_estado.errores,
    (int)scd_estado.error, scd_estado.etapa,
    scd_estado.rancio ? "true" : "false",
    scd_estado.congelado ? "true" : "false",
    (unsigned long)scdHace, (unsigned long)scd_estado.reinicios,
    (unsigned)scd_estado.asc, (unsigned)scd_estado.selftest,
    (unsigned long long)scd_estado.serie, (unsigned)ALTITUD_M,
    (unsigned long)ESP.getSketchSize(),
    (unsigned long)ESP.getFreeSketchSpace(),
    (unsigned long)escriturasFlash(),
    pantalla_ok ? "true" : "false", (unsigned)pantalla_addr,
    (unsigned)historialCount(), (unsigned)HIST_SIZE,
    (unsigned)logCount(), (unsigned)LOG_SIZE);

  enviarJson(n, sizeof buf, "/api/health", buf);
}

// ------------------------------------------------------------------ /api/log

static void rutaLog() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  String out;
  out.reserve(1200);
  out = "{\"eventos\":[";

  uint16_t n = logCount();
  char esc[LOG_MSG_LEN * 2];
  char linea[LOG_MSG_LEN * 2 + 64];

  // Del mas reciente al mas antiguo: es como se lee un log.
  for (uint16_t k = 0; k < n; k++) {
    const Evento *e = logGet(n - 1 - k);
    if (!e) continue;
    escapar(e->msg, esc, sizeof esc);
    snprintf(linea, sizeof linea, "%s{\"ts\":%lu,\"n\":%u,\"r\":%u,\"m\":\"%s\"}",
             k ? "," : "", (unsigned long)e->ts, (unsigned)e->nivel,
             (unsigned)e->repes, esc);
    out += linea;
    // Se envia a trozos: un String de 10 KB fragmentaria el heap.
    if (out.length() > 1024) { server.sendContent(out); out = ""; }
  }

  out += "]}";
  server.sendContent(out);
  server.sendContent("");  // fin del chunked
}

// El parametro `range` solo admite tres valores. Devolver el texto del cliente
// tal cual tiene dos problemas: una comilla parte el JSON, y un valor invalido
// se sirve con datos de 24 h mientras la respuesta dice que es otra cosa. Se
// normaliza a la constante que de verdad se ha usado.
static const char *normalizarRango(const String &r, uint16_t &ventana) {
  if (r == "1h") { ventana = 60;  return "1h";  }
  if (r == "6h") { ventana = 360; return "6h";  }
  ventana = HIST_SIZE;             return "24h";
}

// -------------------------------------------------------------- /api/history

static void rutaHistory() {
  uint16_t ventana = 0;
  const char *r = normalizarRango(server.hasArg("range") ? server.arg("range") : "24h",
                                  ventana);

  uint16_t total = historialCount();
  uint16_t n     = total < ventana ? total : ventana;
  uint16_t desde = total - n;

  // Downsampling: 1440 muestras x 7 series serian ~86 KB de JSON y no caben
  // pixeles en pantalla para tanto punto. Se promedian bloques.
  uint16_t bloque = (n + HIST_MAX_PUNTOS - 1) / HIST_MAX_PUNTOS;
  if (bloque < 1) bloque = 1;
  uint16_t puntos = n ? (n + bloque - 1) / bloque : 0;

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  String out;
  out.reserve(1200);
  char tmp[24];

  out  = "{\"range\":\"";
  out += r;
  out += "\",\"n\":";
  snprintf(tmp, sizeof tmp, "%u", (unsigned)puntos);
  out += tmp;
  out += ",\"hora_ok\":";
  out += red_estado.horaOk ? "true" : "false";

  // Eje de tiempo: se usa el ts de la ultima muestra del bloque.
  out += ",\"t\":[";
  for (uint16_t p = 0; p < puntos; p++) {
    uint16_t fin = desde + (p + 1) * bloque - 1;
    if (fin >= total) fin = total - 1;
    const Muestra *m = historialGet(fin);
    snprintf(tmp, sizeof tmp, "%s%lu", p ? "," : "",
             (unsigned long)(m ? m->ts : 0));
    out += tmp;
    if (out.length() > 1024) { server.sendContent(out); out = ""; }
  }
  out += "]";

  for (uint8_t c = 0; c < C_TOTAL; c++) {
    out += ",\"";
    out += NOMBRE_CAMPO[c];
    out += "\":[";
    for (uint16_t p = 0; p < puntos; p++) {
      float suma = 0; uint16_t cuenta = 0;
      for (uint16_t k = 0; k < bloque; k++) {
        uint16_t idx = desde + p * bloque + k;
        if (idx >= total) break;
        float v;
        if (muestraValor(historialGet(idx), c, v)) { suma += v; cuenta++; }
      }
      // Bloque entero sin datos -> null, para que la grafica pinte un hueco
      // en vez de interpolar una linea que nunca existio.
      if (cuenta == 0) snprintf(tmp, sizeof tmp, "%snull", p ? "," : "");
      else             snprintf(tmp, sizeof tmp, "%s%.1f", p ? "," : "", suma / cuenta);
      out += tmp;
      if (out.length() > 1024) { server.sendContent(out); out = ""; }
    }
    out += "]";
  }

  out += "}";
  server.sendContent(out);
  server.sendContent("");
}

// ---------------------------------------------------------------- /api/stats

// Un valor no calculable se serializa como null, nunca como 0. Un cero se
// leeria en la web como "medido y salio cero", que es justo lo contrario.
static void jsonFloat(String &out, const char *clave, float v, uint8_t dec) {
  char tmp[40];
  if (isnan(v)) snprintf(tmp, sizeof tmp, "\"%s\":null", clave);
  else          snprintf(tmp, sizeof tmp, "\"%s\":%.*f", clave, (int)dec, v);
  out += tmp;
}

static void jsonMagnitud(String &out, const char *nombre, const EstadUna &e,
                         uint8_t dec) {
  char tmp[64];
  snprintf(tmp, sizeof tmp, "\"%s\":{\"n\":%u,", nombre, (unsigned)e.n);
  out += tmp;
  jsonFloat(out, "min",   e.min,   dec); out += ",";
  jsonFloat(out, "media", e.media, dec); out += ",";
  jsonFloat(out, "p95",   e.p95,   dec); out += ",";
  jsonFloat(out, "max",   e.max,   dec); out += ",";
  // La tendencia se publica por hora, no por minuto: "+38 ppm/h" se entiende
  // de un vistazo y "+0.63 ppm/min" no.
  jsonFloat(out, "tend_h", e.tendencia * 60.0f, 1);
  if (e.hayNivel) {
    snprintf(tmp, sizeof tmp, ",\"pct\":[%.1f,%.1f,%.1f]}",
             e.pctNivel[0] / 10.0f, e.pctNivel[1] / 10.0f, e.pctNivel[2] / 10.0f);
    out += tmp;
  } else {
    out += ",\"pct\":null}";
  }
}

static void rutaStats() {
  uint16_t ventana = 0;
  const char *r = normalizarRango(server.hasArg("range") ? server.arg("range") : "24h",
                                  ventana);

  // La struct son ~200 bytes: en la pila del servidor entra sin problema y
  // evita otro bloque estatico que solo se usa mientras dura la peticion.
  Estadisticas s;
  estadisticasCalcular(ventana, s);

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  String out;
  out.reserve(1200);
  char tmp[96];

  snprintf(tmp, sizeof tmp, "{\"range\":\"%s\",\"muestras\":%u,\"series\":{",
           r, (unsigned)s.muestras);
  out = tmp;
  jsonMagnitud(out, "pm1",  s.pm1,  1); out += ",";
  jsonMagnitud(out, "pm25", s.pm25, 1); out += ",";
  jsonMagnitud(out, "pm10", s.pm10, 1);
  server.sendContent(out);
  out = ",";
  jsonMagnitud(out, "co2",  s.co2,  0); out += ",";
  jsonMagnitud(out, "temp", s.temp, 1); out += ",";
  jsonMagnitud(out, "hum",  s.hum,  1);
  out += "},";

  // Las medias de 24 h van fuera de "series" a proposito: no dependen de la
  // ventana elegida y mezclarlas invitaria a compararlas con el resto.
  jsonFloat(out, "pm25_24h", medias24.hayPm25 ? medias24.pm25 : NAN, 1);
  out += ",";
  jsonFloat(out, "pm10_24h", medias24.hayPm10 ? medias24.pm10 : NAN, 1);
  // Sin media no hay nivel: mandar NIVEL_OK cuando no se ha calculado nada
  // pintaria un "correcto" que nadie ha comprobado.
  char n25[8] = "null", n10[8] = "null";
  if (medias24.hayPm25) snprintf(n25, sizeof n25, "%u", (unsigned)alertas.pm25_24h);
  if (medias24.hayPm10) snprintf(n10, sizeof n10, "%u", (unsigned)alertas.pm10_24h);
  snprintf(tmp, sizeof tmp, ",\"n_pm25_24h\":%s,\"n_pm10_24h\":%s,", n25, n10);
  out += tmp;
  jsonFloat(out, "ach", s.ach, 2);

  // "Nunca ha bajado del umbral" y "bajo hace 0 minutos" son cosas distintas:
  // la primera es null, no un numero grande.
  char nunca[12] = "null";
  if (s.sinVentilar_min != SIN_VENTILAR_NUNCA)
    snprintf(nunca, sizeof nunca, "%u", (unsigned)s.sinVentilar_min);
  snprintf(tmp, sizeof tmp,
           ",\"sin_ventilar_min\":%s,\"co2_ext\":%u,\"co2_ventilado\":%u,"
           "\"umbral_media24\":%u}",
           nunca, (unsigned)CO2_EXTERIOR_PPM, (unsigned)CO2_VENTILADO,
           (unsigned)MEDIA24_MIN_MUESTRAS);
  out += tmp;

  server.sendContent(out);
  server.sendContent("");
}

// ------------------------------------------------------------- /api/exterior

static const char *nombreFuente(FuenteExterior f) {
  switch (f) {
    case FUENTE_AQICN:  return "aqicn";
    case FUENTE_MODELO: return "modelo";
    default:             return "ninguna";
  }
}

static void rutaExteriorGet() {
  Exterior e;
  exteriorSnapshot(e);

  char esc[EXT_LUGAR_LEN * 2];
  escapar(e.lugar, esc, sizeof esc);
  char escEst[AQICN_ESTACION_LEN * 2];
  escapar(e.estacion, escEst, sizeof escEst);

  // "Nunca se ha traido un dato" no es "se trajo hace 0 s", y el AQI europeo
  // empieza en 0: el centinela tiene que ser null, no un numero.
  char hace[12] = "null", aqi[8] = "null";
  if (e.valido) snprintf(hace, sizeof hace, "%lu",
                         (unsigned long)((millis() - e.ultimaMs) / 1000));
  if (e.aqi >= 0) snprintf(aqi, sizeof aqi, "%d", (int)e.aqi);

  String out;
  // 760: cabecera (~262 en el peor caso) + 6 jsonFloat + bloque
  // fuente/estacion/distancia_km (~159 en el peor caso) + cierre, con margen.
  out.reserve(760);
  // 360 y no 240: un nombre de 39 caracteres con todo comillas se escapa a 78,
  // y con eso la plantilla se pasaba de 240. Truncar aqui devolveria un JSON
  // partido que el navegador solo sabe reportar como "error de parseo".
  char tmp[360];
  snprintf(tmp, sizeof tmp,
    "{\"configurado\":%s,\"lugar\":\"%s\",\"lat\":%.4f,\"lon\":%.4f,"
    "\"valido\":%s,\"rancio\":%s,\"hace_s\":%s,\"http\":%d,"
    "\"intentos\":%lu,\"fallos\":%lu,\"meteo\":%s,",
    e.configurado ? "true" : "false", esc, e.lat, e.lon,
    e.valido ? "true" : "false", e.rancio ? "true" : "false",
    hace, (int)e.httpCode,
    (unsigned long)e.intentos, (unsigned long)e.fallos,
    e.hayMeteo ? "true" : "false");
  out = tmp;

  jsonFloat(out, "pm25", e.pm25, 1); out += ",";
  jsonFloat(out, "pm10", e.pm10, 1); out += ",";
  jsonFloat(out, "o3",   e.o3,   0); out += ",";
  jsonFloat(out, "no2",  e.no2,  1); out += ",";
  jsonFloat(out, "temp", e.temp, 1); out += ",";
  jsonFloat(out, "hum",  e.hum,  0); out += ",";

  // "estacion"/"distancia_km" solo tienen sentido con fuente==aqicn; en
  // cualquier otro caso van vacio/null en vez de omitirse, para que el JS no
  // tenga que comprobar si la clave existe.
  snprintf(tmp, sizeof tmp, "\"fuente\":\"%s\",\"estacion\":\"%s\",",
           nombreFuente(e.fuente), e.fuente == FUENTE_AQICN ? escEst : "");
  out += tmp;
  jsonFloat(out, "distancia_km", e.fuente == FUENTE_AQICN ? e.distanciaKm : NAN, 1);
  out += ",";

  snprintf(tmp, sizeof tmp,
           "\"aqi\":%s,\"co2_ext\":%u,\"cada_min\":%lu}",
           aqi, (unsigned)CO2_EXTERIOR_PPM,
           (unsigned long)(INT_EXTERIOR / 60000UL));
  out += tmp;
  server.send(200, "application/json", out);
}

// toFloat() devuelve 0 ante cualquier basura, y 0,0 son coordenadas validas
// (golfo de Guinea): sin comprobar que el texto era un numero, un parametro
// corrupto apuntaria el aparato al Atlantico sin decir nada.
static bool leerCoord(const String &s, float &out) {
  const char *p = s.c_str();
  char *fin = nullptr;
  float v = strtof(p, &fin);
  if (fin == p || !isfinite(v)) return false;
  while (*fin && isspace((unsigned char)*fin)) fin++;
  if (*fin) return false;
  out = v;
  return true;
}

// Copia el nombre recortando lo que rompe el JSON o la pantalla. El corte
// respeta UTF-8: partir "Alcala de Henares" a mitad de una tilde deja un byte
// suelto que el navegador pinta como rombo negro.
static void limpiarNombre(const String &src, char *dst, size_t n) {
  if (!n) return;
  size_t j = 0;
  for (size_t i = 0; i < src.length() && j + 1 < n; i++) {
    char c = src[i];
    if ((unsigned char)c < 0x20) continue;
    dst[j++] = c;
  }
  quitarUtf8Incompleto(dst, j);
  dst[j] = '\0';
}

static void rutaExteriorPost() {
  if (!postPermitido()) return;
  float lat = 0, lon = 0;
  if (!server.hasArg("lat") || !server.hasArg("lon") ||
      !leerCoord(server.arg("lat"), lat) || !leerCoord(server.arg("lon"), lon)) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"Faltan lat y lon o no son numeros\"}");
    return;
  }
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"Coordenadas fuera de rango\"}");
    return;
  }

  char nombre[EXT_LUGAR_LEN];
  if (server.hasArg("nombre")) limpiarNombre(server.arg("nombre"), nombre, sizeof nombre);
  else nombre[0] = '\0';
  // Sin nombre se usan las coordenadas: dejarlo vacio daria una cabecera de
  // OLED que solo dice "EXTERIOR" y no se sabria de donde es el dato.
  if (!nombre[0]) snprintf(nombre, sizeof nombre, "%.2f,%.2f", lat, lon);

  if (!exteriorFijarLugar(lat, lon, nombre)) {
    server.send(500, "application/json",
                "{\"ok\":false,\"msg\":\"No se pudo guardar o arrancar el sondeo\"}");
    return;
  }
  logEvento(LOG_INFO, "Exterior: %s (%.2f,%.2f)", nombre, lat, lon);

  char esc[EXT_LUGAR_LEN * 2], buf[220];
  escapar(nombre, esc, sizeof esc);
  snprintf(buf, sizeof buf,
           "{\"ok\":true,\"lugar\":\"%s\",\"lat\":%.4f,\"lon\":%.4f,"
           "\"msg\":\"Guardado. El primer dato tarda unos segundos.\"}",
           esc, lat, lon);
  server.send(200, "application/json", buf);
}

static void rutaExteriorToken() {
  if (!postPermitido()) return;
  String token = server.hasArg("token") ? server.arg("token") : "";
  if (token.length() >= AQICN_TOKEN_LEN) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"Token demasiado largo\"}");
    return;
  }
  if (!exteriorFijarToken(token.c_str())) {
    server.send(500, "application/json",
                "{\"ok\":false,\"msg\":\"No se pudo guardar en la NVS\"}");
    return;
  }
  logEvento(LOG_INFO, token.length() ? "Token AQICN guardado"
                                      : "Token AQICN borrado (vuelve a Open-Meteo)");
  server.send(200, "application/json",
              token.length()
                ? "{\"ok\":true,\"msg\":\"Token guardado. Se usa en el proximo sondeo.\"}"
                : "{\"ok\":true,\"msg\":\"Token borrado. Vuelve a usarse el modelo de Open-Meteo.\"}");
}

// ------------------------------------------------------------- /api/cloud

static void rutaCloudConfig() {
  if (!postPermitido()) return;

  String url    = server.hasArg("url")    ? server.arg("url")    : "";
  String user   = server.hasArg("user")   ? server.arg("user")   : "";
  String token  = server.hasArg("token")  ? server.arg("token")  : "";
  bool   activo = server.hasArg("on") && server.arg("on").toInt() != 0;

  if (!cloudConfigurar(url.c_str(), user.c_str(), token.c_str(), activo)) {
    server.send(400, "application/json",
                String("{\"ok\":false,\"msg\":\"") + cloudErrorConfig() + "\"}");
    return;
  }
  logEvento(LOG_INFO, "Backup nube: %s (%s)",
            activo ? "activado" : "desactivado", url.c_str());
  server.send(200, "application/json",
              "{\"ok\":true,\"msg\":\"Configuracion guardada.\"}");
}

static void rutaCloudEstado() {
  CloudEstado e;
  cloudSnapshot(e);
  char urlEsc[CLOUD_URL_LEN * 2], userEsc[CLOUD_USER_LEN * 2];
  escapar(e.url, urlEsc, sizeof urlEsc);
  escapar(e.usuario, userEsc, sizeof userEsc);
  char buf[600];
  int n = snprintf(buf, sizeof buf,
    "{\"activo\":%s,\"configurado\":%s,\"hay_token\":%s,\"en_cola\":%u,"
    "\"envios_ok\":%lu,\"envios_fallo\":%lu,\"hace_s\":%lu,"
    "\"url\":\"%s\",\"usuario\":\"%s\"}",
    e.activo ? "true" : "false", e.configurado ? "true" : "false",
    e.hayToken ? "true" : "false",
    (unsigned)e.enCola, (unsigned long)e.enviosOk, (unsigned long)e.enviosFallo,
    e.ultimoEnvioMs ? (unsigned long)((millis() - e.ultimoEnvioMs) / 1000) : 0,
    urlEsc, userEsc);
  enviarJson(n, sizeof buf, "/api/cloud/estado", buf);
}

// -------------------------------------------------------------- /api/calibrar

// Accion con efecto persistente (escribe la EEPROM del sensor), por eso solo
// por POST: un GET lo dispararia cualquier rastreador o precarga del navegador.
static void rutaCalibrar() {
  if (!postPermitido()) return;
  uint16_t ppm = server.hasArg("ppm") ? server.arg("ppm").toInt() : 420;
  // Fuera de este rango la referencia no es aire: seria estropear el sensor.
  if (ppm < 350 || ppm > 2000) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"Referencia fuera de 350-2000 ppm\"}");
    return;
  }

  int16_t correccion = 0, errorI2c = 0;
  ResCalib r = sensoresCalibrar(ppm, correccion, errorI2c);

  char buf[220];
  switch (r) {
    case CALIB_OK:
      logEvento(LOG_INFO, "SCD41 calibrado a %u ppm (%+d)", (unsigned)ppm,
                (int)correccion);
      snprintf(buf, sizeof buf,
               "{\"ok\":true,\"ppm\":%u,\"correccion\":%d,"
               "\"msg\":\"Calibrado. Correccion aplicada: %+d ppm.\"}",
               (unsigned)ppm, (int)correccion, (int)correccion);
      break;
    case CALIB_NO_LISTO:
      snprintf(buf, sizeof buf,
               "{\"ok\":false,\"segundos\":%lu,\"msg\":\"El sensor debe llevar "
               "3 min midiendo seguidos en este aire. Lleva %lu s.\"}",
               (unsigned long)sensoresSegundosMidiendo(),
               (unsigned long)sensoresSegundosMidiendo());
      break;
    case CALIB_RECHAZADA:
      logEvento(LOG_AVISO, "SCD41 rechazo la calibracion a %u ppm", (unsigned)ppm);
      snprintf(buf, sizeof buf,
               "{\"ok\":false,\"msg\":\"El sensor rechazo la referencia. "
               "Suele pasar si acaba de reiniciarse.\"}");
      break;
    default:
      snprintf(buf, sizeof buf,
               "{\"ok\":false,\"error\":%d,\"msg\":\"Fallo de comunicacion "
               "(codigo %d).\"}", (int)errorI2c, (int)errorI2c);
      break;
  }
  server.send(r == CALIB_OK ? 200 : 409, "application/json", buf);
}

// ------------------------------------------------------- diagnostico SCD41

// Reinicio del sensor. Es POST porque tiene efecto real sobre el hardware,
// aunque no sea destructivo: interrumpe la medicion durante ~1 s.
static void rutaScdReset() {
  if (!postPermitido()) return;
  bool ok = sensoresReiniciarScd();
  logEvento(ok ? LOG_AVISO : LOG_ERROR,
            ok ? "SCD41 reiniciado a mano" : "SCD41: fallo al reiniciar a mano");

  char buf[200];
  snprintf(buf, sizeof buf,
           "{\"ok\":%s,\"reinicios\":%lu,\"msg\":\"%s\"}",
           ok ? "true" : "false", (unsigned long)scd_estado.reinicios,
           ok ? "Sensor reiniciado. Primer dato en unos 5 s."
              : "El sensor no respondio al reinicio. Revisa alimentacion y cableado.");
  server.send(ok ? 200 : 409, "application/json", buf);
}

// Autotest interno. Bloquea el aparato ~10 s: el sensor no responde antes y no
// hay forma de trocearlo. Es aceptable porque solo se lanza a mano.
static void rutaScdAutotest() {
  if (!postPermitido()) return;
  uint16_t estado = SELFTEST_SIN_HACER;
  int16_t  error  = sensoresAutotestScd(estado);

  char buf[260];
  if (error) {
    snprintf(buf, sizeof buf,
             "{\"ok\":false,\"error\":%d,\"msg\":\"No se pudo ejecutar el "
             "autotest (codigo %d).\"}", (int)error, (int)error);
    server.send(409, "application/json", buf);
    return;
  }

  bool sano = (estado == 0);
  logEvento(sano ? LOG_INFO : LOG_ERROR,
            sano ? "SCD41 autotest OK" : "SCD41 autotest FALLO (0x%04X)",
            (unsigned)estado);
  snprintf(buf, sizeof buf,
           "{\"ok\":true,\"estado\":%u,\"sano\":%s,\"msg\":\"%s\"}",
           (unsigned)estado, sano ? "true" : "false",
           sano ? "El sensor se declara sano. Si los valores siguen mal, el "
                  "problema es de calibracion o de alimentacion, no del chip."
                : "El sensor declara una averia interna. Esto no se arregla "
                  "calibrando.");
  server.send(200, "application/json", buf);
}

// Activa o desactiva el ASC. Escribe la EEPROM del sensor, por eso POST y por
// eso la web pide confirmacion antes.
static void rutaScdAsc() {
  if (!postPermitido()) return;
  if (!server.hasArg("on")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"msg\":\"Falta el parametro on=0|1\"}");
    return;
  }
  bool activo = server.arg("on").toInt() != 0;
  int16_t error = sensoresSetAscScd(activo);

  char buf[240];
  if (error) {
    snprintf(buf, sizeof buf,
             "{\"ok\":false,\"error\":%d,\"asc\":%u,\"msg\":\"Fallo al cambiar "
             "el ASC (codigo %d).\"}", (int)error, (unsigned)scd_estado.asc,
             (int)error);
    server.send(409, "application/json", buf);
    return;
  }
  logEvento(LOG_AVISO, "SCD41 ASC %s", scd_estado.asc ? "activado" : "desactivado");
  snprintf(buf, sizeof buf,
           "{\"ok\":true,\"asc\":%u,\"msg\":\"Autocalibracion %s y guardada en "
           "el sensor.\"}", (unsigned)scd_estado.asc,
           scd_estado.asc ? "activada" : "desactivada");
  server.send(200, "application/json", buf);
}

// ------------------------------------------------------------------- paginas

static void rutaRaiz() {
  // Precomprimido en tools/gzip-web.cjs: ~41 KB de texto pasan a ~10 KB. El
  // aparato atiende una sola conexion y el servidor corre dentro del loop, asi
  // que cada KB que no se manda es tiempo que el loop no pasa bloqueado.
  // send_P sirve directo desde flash: no copia nada al heap.
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char *)PAGINA_GZ, PAGINA_GZ_LEN);
}

static void rutaNoEncontrada() {
  // La URI la elige quien llama: una comilla en la ruta partiria este JSON.
  char esc[128];
  escapar(server.uri().c_str(), esc, sizeof esc);
  char buf[200];
  snprintf(buf, sizeof buf, "{\"error\":\"no existe\",\"ruta\":\"%s\"}", esc);
  server.send(404, "application/json", buf);
}

// --------------------------------------------------------------------- ciclo

void servidorInit() {
  // WebServer tira todas las cabeceras que no se declaren aqui. Sin esto,
  // server.header("Origin") devuelve siempre "" y origenValido() no filtra nada.
  static const char *CABECERAS[] = {"Origin"};
  server.collectHeaders(CABECERAS, 1);

  server.on("/", HTTP_GET, rutaRaiz);
  server.on("/api/now", HTTP_GET, rutaNow);
  server.on("/api/health", HTTP_GET, rutaHealth);
  server.on("/api/log", HTTP_GET, rutaLog);
  server.on("/api/history", HTTP_GET, rutaHistory);
  server.on("/api/stats", HTTP_GET, rutaStats);
  server.on("/api/exterior", HTTP_GET, rutaExteriorGet);
  // POST y no GET: escribe la NVS. Un GET lo dispararia cualquier precarga.
  server.on("/api/exterior", HTTP_POST, rutaExteriorPost);
  server.on("/api/exterior/token", HTTP_POST, rutaExteriorToken);
  server.on("/api/cloud/config", HTTP_POST, rutaCloudConfig);
  server.on("/api/cloud/estado", HTTP_GET,  rutaCloudEstado);
  server.on("/api/calibrar", HTTP_POST, rutaCalibrar);
  server.on("/api/scd/reset", HTTP_POST, rutaScdReset);
  server.on("/api/scd/autotest", HTTP_POST, rutaScdAutotest);
  server.on("/api/scd/asc", HTTP_POST, rutaScdAsc);
  server.onNotFound(rutaNoEncontrada);
  server.begin();
  Serial.printf("[WEB] Servidor en http://%s/\n", red_estado.ip);
}

void servidorAtender() { server.handleClient(); }
