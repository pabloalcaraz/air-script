#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "outdoor.h"
#include "outdoor_ca.h"
#include "flashstats.h"
#include "net.h"

// Estado compartido entre la tarea de red (nucleo 0) y el loop (nucleo 1).
// Solo se toca con el mutex cogido, y el mutex nunca se retiene durante una
// peticion: la tarea trabaja sobre copias locales y al final vuelca.
static Exterior         exterior;
static SemaphoreHandle_t candado = nullptr;
static TaskHandle_t      tarea   = nullptr;
static char              aqicnToken[AQICN_TOKEN_LEN] = "";
// Cada cambio de lugar/token invalida las peticiones que ya estaban en vuelo.
// Sin esta generacion, una respuesta lenta del sitio anterior podia publicarse
// bajo el nombre y las coordenadas nuevos.
static uint32_t          revisionCfg = 0;

// ------------------------------------------------------------------ parseo

// Devuelve el bloque "current":{...} del JSON de Open-Meteo. Anclar ahi es
// obligatorio: el mismo documento trae un "current_units" con LAS MISMAS
// claves, y buscar "pm2_5" a pelo puede devolver la unidad en vez del numero.
static const char *bloqueActual(const char *json) {
  return strstr(json, "\"current\":{");
}

// Un campo ausente o a null devuelve false, nunca 0. Un cero aqui se leeria
// como "hoy no hay contaminacion fuera", que es la mentira mas cara posible
// en un panel cuyo unico proposito es comparar dentro contra fuera.
static bool campo(const char *obj, const char *clave, float &out) {
  char patron[40];
  snprintf(patron, sizeof patron, "\"%s\":", clave);
  const char *p = strstr(obj, patron);
  if (!p) return false;
  p += strlen(patron);
  char *fin = nullptr;
  float v = strtof(p, &fin);
  // fin==p: "null", una cadena, o basura. !isfinite: strtof acepta "inf"/
  // "nan" como texto valido, y eso no es un numero real que mostrar.
  if (fin == p || !isfinite(v)) return false;
  out = v;
  return true;
}

// AQICN anida cada magnitud como {"v":NUM}: el patron incluye la llave y
// "v" para que "pm25":{"v":72} (iaqi) no se confunda con el array del
// pronostico diario forecast.daily.pm25 ("pm25":[{"avg":...}]), que usa la
// misma clave con otra forma.
static bool campoAqicn(const char *json, const char *clave, float &out) {
  char patron[48];
  snprintf(patron, sizeof patron, "\"%s\":{\"v\":", clave);
  const char *p = strstr(json, patron);
  if (!p) return false;
  p += strlen(patron);
  char *fin = nullptr;
  float v = strtof(p, &fin);
  if (fin == p || !isfinite(v)) return false;
  out = v;
  return true;
}

// Extrae una cadena "clave":"valor" (AQICN no escapa comillas en el nombre
// de estacion). Trunca a n-1 bytes si hace falta.
static bool campoTexto(const char *obj, const char *clave, char *out, size_t n) {
  char patron[40];
  snprintf(patron, sizeof patron, "\"%s\":\"", clave);
  const char *p = strstr(obj, patron);
  if (!p) return false;
  p += strlen(patron);
  const char *fin = strchr(p, '"');
  if (!fin) return false;
  size_t len = (size_t)(fin - p);
  if (len >= n) len = n - 1;
  memcpy(out, p, len);
  out[len] = '\0';
  return true;
}

static bool statusOk(const char *json) {
  return strstr(json, "\"status\":\"ok\"") != nullptr;
}

// Aproximacion plana: valida con error <0.5% a la escala de Espana (<1000 km
// entre dos puntos cualesquiera). Evita traer una libreria de trigonometria
// esferica solo para saber si una estacion esta "cerca" o "lejos".
static float distanciaKm(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371.0f;
  float dLat = (lat2 - lat1) * (float)M_PI / 180.0f;
  float dLon = (lon2 - lon1) * (float)M_PI / 180.0f;
  float mlat = (lat1 + lat2) / 2.0f * (float)M_PI / 180.0f;
  float x = dLon * cosf(mlat);
  float y = dLat;
  return R * sqrtf(x * x + y * y);
}

// ------------------------------------------------------------------- red

// HTTPClient::getString() puede crecer sin limite si un servidor responde con
// transferencia chunked y sin Content-Length. Este Stream corta la escritura
// antes de consumir el heap del dispositivo.
class StringLimitada : public Stream {
 public:
  explicit StringLimitada(String &destino) : excedida(false), dst(destino) {}

  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *buf, size_t n) override {
    size_t libre = EXT_RESPUESTA_MAX - dst.length();
    size_t tomar = n < libre ? n : libre;
    if (tomar && !dst.concat((const char *)buf, (unsigned)tomar)) {
      setWriteError();
      return 0;
    }
    if (tomar != n) {
      excedida = true;
      setWriteError();
    }
    return tomar;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  bool excedida;

 private:
  String &dst;
};

// AQICN y Open-Meteo usan actualmente una cadena de Let's Encrypt anclada en
// ISRG Root X1. Validarla protege tanto el token AQICN como la integridad del
// consejo de ventilacion frente a respuestas manipuladas.
static bool descargar(const char *url, String &cuerpo, int16_t &code,
                       uint32_t timeoutMs) {
  if (!redTlsTomar(RED_TLS_ESPERA_MS)) {
    code = -1001;  // otra tarea HTTPS esta usando el presupuesto TLS
    return false;
  }

  WiFiClientSecure cli;
  cli.setCACert(OUTDOOR_ROOT_CA);

  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  http.setReuse(false);
  if (!http.begin(cli, url)) {
    code = -1000;  // ni siquiera se pudo formar la peticion
    redTlsSoltar();
    return false;
  }
  int r = http.GET();
  code = (int16_t)r;
  bool ok = (r == HTTP_CODE_OK);
  int tam = http.getSize();
  if (ok && tam > (int)EXT_RESPUESTA_MAX) {
    code = -1002;  // respuesta inesperadamente grande; no arriesgar el heap
    ok = false;
  }
  if (ok) {
    if (tam > 0) cuerpo.reserve((unsigned)tam + 1);
    StringLimitada salida(cuerpo);
    int escritos = http.writeToStream(&salida);
    if (salida.excedida) {
      code = -1002;
      ok = false;
    } else if (escritos < 0) {
      code = (int16_t)escritos;
      ok = false;
    }
  }
  http.end();
  redTlsSoltar();
  return ok;
}

// Estacion real mas cercana a las coordenadas. No toca `exterior` ni los
// contadores intentos/fallos si falla: el fallback a Open-Meteo que viene
// despues es el que lleva la cuenta de "sondeo fallido", para no contar dos
// veces el mismo ciclo de 30 min como dos fallos.
static bool sondearAqicn(float lat, float lon, const char *token,
                         uint32_t revision) {
  char url[200];
  snprintf(url, sizeof url,
           "https://api.waqi.info/feed/geo:%.4f;%.4f/?token=%s",
           lat, lon, token);

  String cuerpo;
  int16_t code = 0;
  if (!descargar(url, cuerpo, code, AQICN_TIMEOUT_MS)) return false;

  const char *json = cuerpo.c_str();
  if (!statusOk(json)) return false;

  // PM2.5 es el minimo imprescindible, igual que en el sondeo de Open-Meteo:
  // sin el no hay comparacion que hacer.
  float pm25 = NAN;
  if (!campoAqicn(json, "pm25", pm25)) return false;

  float pm10 = NAN, o3 = NAN, no2 = NAN;
  campoAqicn(json, "pm10", pm10);
  campoAqicn(json, "o3", o3);
  campoAqicn(json, "no2", no2);

  float temp = NAN, hum = NAN;
  bool okTemp = campoAqicn(json, "t", temp);
  bool okHum  = campoAqicn(json, "h", hum);
  bool hayMeteo = okTemp && okHum;

  char estacion[AQICN_ESTACION_LEN] = "";
  float distKm = NAN;
  const char *city = strstr(json, "\"city\":{");
  if (city) {
    campoTexto(city, "name", estacion, sizeof estacion);
    const char *geo = strstr(city, "\"geo\":[");
    if (geo) {
      const char *p = geo + 7;
      char *fin = nullptr;
      float glat = strtof(p, &fin);
      if (fin != p && isfinite(glat) && *fin == ',') {
        const char *inicioLon = fin + 1;
        char *finLon = nullptr;
        float glon = strtof(inicioLon, &finLon);
        if (finLon != inicioLon && isfinite(glon))
          distKm = distanciaKm(lat, lon, glat, glon);
      }
    }
  }

  // Estacion demasiado lejos: se descarta AQICN y se deja caer a Open-Meteo,
  // que modela TUS coordenadas. Solo se rechaza si SABEMOS la distancia; si la
  // estacion no trajo geo (distKm NAN) no hay con que juzgar y se acepta. No se
  // tocan los contadores: el fallback que viene detras lleva la cuenta, para no
  // contar dos veces el mismo ciclo (mismo criterio que el fallo de arriba).
  if (isfinite(distKm) && distKm > AQICN_MAX_KM) return false;

  if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    if (revision != revisionCfg) {
      xSemaphoreGive(candado);
      return false;
    }
    exterior.intentos++;
    exterior.httpCode = code;
    exterior.pm25 = pm25;
    exterior.pm10 = pm10;
    exterior.o3   = o3;
    exterior.no2  = no2;
    // AQICN usa la escala EPA (0-500, contaminante dominante), incompatible
    // con el european_aqi (0-100) de Open-Meteo: no se rellena para no
    // mezclar dos escalas distintas bajo el mismo campo.
    exterior.aqi  = -1;
    exterior.hayMeteo = hayMeteo;
    exterior.temp = hayMeteo ? temp : NAN;
    exterior.hum  = hayMeteo ? hum  : NAN;
    exterior.fuente = FUENTE_AQICN;
    strncpy(exterior.estacion, estacion, sizeof exterior.estacion - 1);
    exterior.estacion[sizeof exterior.estacion - 1] = '\0';
    exterior.distanciaKm = distKm;
    exterior.valido   = true;
    exterior.ultimaMs = millis();
    xSemaphoreGive(candado);
  }
  return true;
}

// Modelo CAMS de Open-Meteo: particulas primero, meteorologia despues. Si la
// segunda falla se conservan las particulas, que son el dato que justifica
// la funcion. Es el sondeo original del proyecto, sin cambios de logica.
static bool sondearOpenMeteo(float lat, float lon, uint32_t revision) {
  char url[220];
  String cuerpo;
  int16_t code = 0;

  snprintf(url, sizeof url,
           "https://air-quality-api.open-meteo.com/v1/air-quality"
           "?latitude=%.4f&longitude=%.4f"
           "&current=pm10,pm2_5,ozone,nitrogen_dioxide,european_aqi",
           lat, lon);

  bool ok = descargar(url, cuerpo, code, EXT_TIMEOUT_MS);
  const char *cur = ok ? bloqueActual(cuerpo.c_str()) : nullptr;

  float pm25 = NAN, pm10 = NAN, o3 = NAN, no2 = NAN, aqi = NAN;
  bool hayPart = false;
  if (cur) {
    hayPart = campo(cur, "pm2_5", pm25);
    campo(cur, "pm10", pm10);
    campo(cur, "ozone", o3);
    campo(cur, "nitrogen_dioxide", no2);
    campo(cur, "european_aqi", aqi);
  }

  if (!hayPart) {
    if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
      if (revision != revisionCfg) {
        xSemaphoreGive(candado);
        return false;
      }
      exterior.intentos++;
      exterior.fallos++;
      exterior.httpCode = code;
      // El dato anterior NO se borra ni se pone a cero: se queda como estaba
      // y envejece hasta marcarse rancio. Un hueco temporal de red no
      // convierte el aire de fuera en aire limpio.
      xSemaphoreGive(candado);
    }
    return false;
  }

  float temp = NAN, hum = NAN;
  bool hayMeteo = false;
  snprintf(url, sizeof url,
           "https://api.open-meteo.com/v1/forecast"
           "?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m",
           lat, lon);
  int16_t code2 = 0;
  String cuerpo2;
  if (descargar(url, cuerpo2, code2, EXT_TIMEOUT_MS)) {
    const char *c2 = bloqueActual(cuerpo2.c_str());
    if (c2) hayMeteo = campo(c2, "temperature_2m", temp) &&
                       campo(c2, "relative_humidity_2m", hum);
  }

  if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    if (revision != revisionCfg) {
      xSemaphoreGive(candado);
      return false;
    }
    exterior.intentos++;
    exterior.httpCode = code;
    exterior.pm25 = pm25;
    exterior.pm10 = pm10;
    exterior.o3   = o3;
    exterior.no2  = no2;
    exterior.aqi  = isnan(aqi) ? -1 : (int16_t)lroundf(aqi);
    exterior.hayMeteo = hayMeteo;
    exterior.temp = hayMeteo ? temp : NAN;
    exterior.hum  = hayMeteo ? hum  : NAN;
    exterior.fuente = FUENTE_MODELO;
    // Se limpian por si el sondeo anterior fue AQICN: ensenar el nombre de
    // una estacion junto a datos que en realidad vienen del modelo mentiria
    // sobre el origen del dato.
    exterior.estacion[0] = '\0';
    exterior.distanciaKm = NAN;
    exterior.valido   = true;
    exterior.ultimaMs = millis();
    xSemaphoreGive(candado);
  }
  return true;
}

// Intenta primero la estacion real; si no hay token, si AQICN falla, o si la
// estacion mas cercana no tiene PM2.5, cae al modelo sin tratarlo como un
// error: es el comportamiento normal sin token o con AQICN caido.
static bool sondear(float lat, float lon, uint32_t revision) {
  char token[AQICN_TOKEN_LEN];
  token[0] = '\0';
  if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    strncpy(token, aqicnToken, sizeof token - 1);
    token[sizeof token - 1] = '\0';
    xSemaphoreGive(candado);
  }
  if (token[0] && sondearAqicn(lat, lon, token, revision)) return true;

  // Si la configuracion cambio durante AQICN, no se desperdicia otro TLS con
  // las coordenadas antiguas. La notificacion pendiente despierta la tarea y
  // la siguiente vuelta toma la configuracion nueva.
  if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    bool vigente = revision == revisionCfg;
    xSemaphoreGive(candado);
    if (!vigente) return false;
  }
  return sondearOpenMeteo(lat, lon, revision);
}

// La peticion es sincrona y puede tardar segundos: por eso vive en su propia
// tarea y en el otro nucleo. El loop no puede pararse ni un instante, que es
// donde se atiende el servidor web y se drena el UART del PMS.
static void tareaExterior(void *) {
  for (;;) {
    bool  hacer = false;
    float lat = 0, lon = 0;
    uint32_t revision = 0;
    if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
      hacer = exterior.configurado;
      lat = exterior.lat;
      lon = exterior.lon;
      revision = revisionCfg;
      xSemaphoreGive(candado);
    }

    bool ok = false;
    if (hacer && WiFi.status() == WL_CONNECTED) {
      // El handshake TLS pide ~45 KB de golpe. Con el heap justo, intentarlo
      // no consigue el dato y ademas se lleva por delante el servidor web.
      if (redTlsHayMemoria(EXT_HEAP_MIN)) ok = sondear(lat, lon, revision);
    }

    // Espera despertable: cambiar de localizacion no puede obligar a esperar
    // media hora para ver el dato del sitio nuevo.
    uint32_t espera = ok ? INT_EXTERIOR : EXT_REINTENTO_MS;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(espera));
  }
}

static bool arrancarTareaExterior() {
  if (tarea) return true;
  tarea = nullptr;
  BaseType_t creada = xTaskCreatePinnedToCore(
    tareaExterior, "exterior", EXT_TAREA_STACK, nullptr, 1, &tarea, 0);
  if (creada == pdPASS && tarea) return true;

  tarea = nullptr;
  Serial.println("[EXT] No se pudo crear la tarea de sondeo.");
  return false;
}

// ------------------------------------------------------------------ publico

void exteriorInit() {
  memset(&exterior, 0, sizeof exterior);
  exterior.pm25 = exterior.pm10 = exterior.o3 = exterior.no2 = NAN;
  exterior.temp = exterior.hum = NAN;
  exterior.aqi  = -1;
  exterior.fuente = FUENTE_NINGUNA;
  exterior.distanciaKm = NAN;
  strncpy(exterior.lugar, "sin elegir", sizeof exterior.lugar - 1);

  candado = xSemaphoreCreateMutex();
  if (!candado) {
    Serial.println("[EXT] No se pudo crear el mutex. Exterior desactivado.");
    return;
  }

  Preferences p;
  if (p.begin(EXT_NVS_NS, true)) {
    float lat = p.getFloat("lat", NAN);
    float lon = p.getFloat("lon", NAN);
    if (!isnan(lat) && !isnan(lon)) {
      exterior.lat = lat;
      exterior.lon = lon;
      exterior.configurado = true;
      if (p.getString("lugar", exterior.lugar, sizeof exterior.lugar) == 0)
        snprintf(exterior.lugar, sizeof exterior.lugar, "%.2f,%.2f", lat, lon);
    }
    p.getString("aqicn", aqicnToken, sizeof aqicnToken);
    p.end();
  }

  // Sin localizacion elegida no se arranca la tarea: no hay a donde preguntar
  // y una tarea despierta cada dos minutos para no hacer nada es solo RAM.
  if (!exterior.configurado) {
    Serial.println("[EXT] Sin localizacion. Elige una en la web (Sistema).");
    return;
  }
  Serial.printf("[EXT] Localizacion: %s (%.4f, %.4f)%s\n",
                exterior.lugar, exterior.lat, exterior.lon,
                aqicnToken[0] ? " - AQICN configurado" : "");
  arrancarTareaExterior();
}

void exteriorSnapshot(Exterior &out) {
  if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    out = exterior;
    xSemaphoreGive(candado);
  } else {
    out = exterior;
  }
  // Se deriva al leer en vez de mantener una bandera: asi no hay forma de que
  // "rancio" se quede desfasado porque nadie ha pasado a actualizarlo.
  out.rancio = out.valido && (millis() - out.ultimaMs) > EXT_RANCIO_MS;
}

bool exteriorFijarLugar(float lat, float lon, const char *nombre) {
  if (!candado || !nombre || !nombre[0] || isnan(lat) || isnan(lon)) return false;
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) return false;

  Preferences p;
  if (!p.begin(EXT_NVS_NS, false)) return false;
  size_t nLat   = p.putFloat("lat", lat);
  size_t nLon   = p.putFloat("lon", lon);
  size_t nLugar = p.putString("lugar", nombre);
  p.end();
  contarEscrituraFlash((nLat ? 1 : 0) + (nLon ? 1 : 0) + (nLugar ? 1 : 0));
  if (nLat != sizeof lat || nLon != sizeof lon || nLugar == 0) return false;

  bool actualizado = false;
  if (xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    exterior.lat = lat;
    exterior.lon = lon;
    strncpy(exterior.lugar, nombre, sizeof exterior.lugar - 1);
    exterior.lugar[sizeof exterior.lugar - 1] = '\0';
    exterior.configurado = true;
    // Los valores del sitio anterior se tiran: seguir ensenandolos con el
    // nombre nuevo seria atribuir a esta ciudad el aire de otra.
    exterior.valido = false;
    exterior.hayMeteo = false;
    exterior.pm25 = exterior.pm10 = exterior.o3 = exterior.no2 = NAN;
    exterior.temp = exterior.hum = NAN;
    exterior.aqi = -1;
    exterior.fuente = FUENTE_NINGUNA;
    exterior.estacion[0] = '\0';
    exterior.distanciaKm = NAN;
    exterior.ultimaMs = 0;
    revisionCfg++;
    actualizado = true;
    xSemaphoreGive(candado);
  }
  if (!actualizado) return false;

  if (tarea) xTaskNotifyGive(tarea);
  else if (!arrancarTareaExterior()) return false;
  return true;
}

bool exteriorFijarToken(const char *token) {
  if (!candado || !token) return false;
  if (strlen(token) >= AQICN_TOKEN_LEN) return false;

  Preferences p;
  if (!p.begin(EXT_NVS_NS, false)) return false;
  bool guardado, huboEscritura = false;
  if (token[0]) {
    size_t escritos = p.putString("aqicn", token);
    guardado = escritos > 0 && p.getString("aqicn", "\x01") == token;
    huboEscritura = guardado;
  } else {
    bool existia = p.isKey("aqicn");
    guardado = !existia || p.remove("aqicn");
    huboEscritura = existia && guardado;
  }
  p.end();
  if (huboEscritura) contarEscrituraFlash();
  if (!guardado) return false;

  bool actualizado = false;
  if (xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
    strncpy(aqicnToken, token, sizeof aqicnToken - 1);
    aqicnToken[sizeof aqicnToken - 1] = '\0';
    revisionCfg++;
    actualizado = true;
    xSemaphoreGive(candado);
  }
  if (actualizado) {
    if (tarea) xTaskNotifyGive(tarea);
    else if (exterior.configurado) actualizado = arrancarTareaExterior();
  }
  return actualizado;
}

bool exteriorUtil() {
  Exterior e;
  exteriorSnapshot(e);
  return e.valido && !e.rancio;
}
