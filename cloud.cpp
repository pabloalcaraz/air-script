#include "cloud.h"
#include "config.h"
#include "sensors.h"
#include "net.h"
#include "flashstats.h"
#include "eventlog.h"
#include "cloud_ca.h"
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <strings.h>

static char urlCfg[CLOUD_URL_LEN]     = "";
static char userCfg[CLOUD_USER_LEN]   = "";
static char tokenCfg[CLOUD_TOKEN_LEN] = "";
static bool activoCfg = false;

static Muestra cola[CLOUD_BUFFER];
static uint16_t colaCabeza = 0;
static uint16_t colaCuenta = 0;

static uint32_t enviosOk = 0, enviosFallo = 0, ultimoEnvioMs = 0;
static const char *ultimoErrorCfg = "";
static SemaphoreHandle_t candado = nullptr;
static TaskHandle_t tarea = nullptr;
static uint32_t revisionCfg = 0;
static uint32_t proximoIntentoMs = 0;
static uint32_t reintentoMs = CLOUD_REINTENTO_MIN_MS;

static void tareaCloud(void *);

static bool arrancarTareaCloud() {
  if (tarea) return true;
  tarea = nullptr;
  BaseType_t creada = xTaskCreatePinnedToCore(
    tareaCloud, "cloud", CLOUD_TAREA_STACK, nullptr, 1, &tarea, 0);
  if (creada == pdPASS && tarea) return true;
  tarea = nullptr;
  Serial.println("[CLOUD] No se pudo crear la tarea de envio.");
  return false;
}

// La URL contiene credenciales indirectamente: determina a quien se manda la
// cabecera Basic Auth. Se restringe a HTTPS y a un subdominio real de Grafana,
// sin usuario embebido, puerto alternativo, query ni fragmento.
static bool destinoSeguro(const char *url) {
  static const char PREFIJO[] = "https://";
  if (!url || strncmp(url, PREFIJO, sizeof PREFIJO - 1) != 0) return false;
  const char *host = url + sizeof PREFIJO - 1;
  const char *fin = strchr(host, '/');
  if (!fin) fin = host + strlen(host);
  if (fin == host || strchr(host, '@') || strchr(host, '?') || strchr(host, '#'))
    return false;
  for (const unsigned char *p = (const unsigned char *)url; *p; p++)
    if (*p <= 0x20 || *p >= 0x7F || *p == '\\') return false;
  for (const char *p = host; p < fin; p++) if (*p == ':') return false;

  const size_t nHost = (size_t)(fin - host);
  const size_t nSufijo = strlen(CLOUD_HOST_SUFFIX);
  return nHost > nSufijo &&
         strncasecmp(host + nHost - nSufijo, CLOUD_HOST_SUFFIX, nSufijo) == 0;
}

// Solo se llama con `candado` tomado, o durante cloudInit() antes de crear la
// tarea. Evita copiar credenciales a medias mientras la web las actualiza.
static bool configValidaSinLock() {
  return urlCfg[0] && userCfg[0] && tokenCfg[0] && destinoSeguro(urlCfg);
}

static bool guardarString(Preferences &p, const char *clave, const char *valor) {
  if (!valor[0]) {
    if (!p.isKey(clave)) return true;
    bool borrado = p.remove(clave);
    if (borrado) contarEscrituraFlash();
    return borrado;
  }
  size_t escritos = p.putString(clave, valor);
  bool guardado = escritos > 0 && p.getString(clave, "\x01") == valor;
  if (guardado) contarEscrituraFlash();
  return guardado;
}

static void colaEncolar(const Muestra &m) {
  uint16_t idx = (colaCabeza + colaCuenta) % CLOUD_BUFFER;
  if (colaCuenta == CLOUD_BUFFER) colaCabeza = (colaCabeza + 1) % CLOUD_BUFFER;
  else colaCuenta++;
  cola[idx] = m;
}

static bool colaDesencolar(Muestra &out) {
  if (colaCuenta == 0) return false;
  out = cola[colaCabeza];
  colaCabeza = (colaCabeza + 1) % CLOUD_BUFFER;
  colaCuenta--;
  return true;
}

void cloudInit() {
  candado = xSemaphoreCreateMutex();
  if (!candado) {
    Serial.println("[CLOUD] No se pudo crear el mutex. Backup desactivado.");
    return;
  }

  Preferences p;
  if (p.begin(CLOUD_NVS_NS, true)) {
    p.getString("cloud_url",  urlCfg,   sizeof urlCfg);
    p.getString("cloud_user", userCfg,  sizeof userCfg);
    p.getString("cloud_tok",  tokenCfg, sizeof tokenCfg);
    activoCfg = p.getBool("cloud_on", false);
    p.end();
  }
  if (activoCfg && !configValidaSinLock()) {
    activoCfg = false;
    Serial.println("[CLOUD] Config insegura o incompleta: backup desactivado.");
  }
  if (activoCfg && urlCfg[0])
    Serial.printf("[CLOUD] Backup activo -> %s\n", urlCfg);
  if (activoCfg && !arrancarTareaCloud()) activoCfg = false;
}

bool cloudConfigurar(const char *url, const char *usuario, const char *token,
                     bool activo) {
  ultimoErrorCfg = "";
  if (!url || !usuario || !token) {
    ultimoErrorCfg = "Configuracion incompleta";
    return false;
  }
  if (strlen(url) >= CLOUD_URL_LEN || strlen(usuario) >= CLOUD_USER_LEN ||
      strlen(token) >= CLOUD_TOKEN_LEN) {
    ultimoErrorCfg = "URL, usuario o token demasiado largos";
    return false;
  }
  if (url[0] && !destinoSeguro(url)) {
    ultimoErrorCfg = "La URL debe ser HTTPS y pertenecer a *.grafana.net";
    return false;
  }

  // Token vacio = "no lo toques". El formulario web nunca rellena este campo
  // con el valor guardado (no se lo devuelve la API, ver cloudSnapshot()), asi
  // que un envio en blanco significa "no lo he cambiado", no "bordalo".
  bool tocaToken = token[0] != '\0';
  bool cambiaDestino = strcmp(url, urlCfg) != 0 || strcmp(usuario, userCfg) != 0;
  if (cambiaDestino && !tocaToken && tokenCfg[0]) {
    ultimoErrorCfg = "Al cambiar URL o usuario debes introducir de nuevo el token";
    return false;
  }
  const char *tokenEfectivo = tocaToken ? token : tokenCfg;
  if (activo && (!url[0] || !usuario[0] || !tokenEfectivo[0])) {
    ultimoErrorCfg = "Para activar el backup hacen falta URL, usuario y token";
    return false;
  }
  if (!candado) {
    ultimoErrorCfg = "Subsistema cloud no disponible";
    return false;
  }
  if (activo && !arrancarTareaCloud()) {
    ultimoErrorCfg = "No se pudo crear la tarea de envio";
    return false;
  }

  Preferences p;
  if (!p.begin(CLOUD_NVS_NS, false)) {
    ultimoErrorCfg = "No se pudo abrir la NVS";
    return false;
  }
  bool ok = guardarString(p, "cloud_url", url) &&
            guardarString(p, "cloud_user", usuario);
  if (ok && tocaToken) ok = guardarString(p, "cloud_tok", token);
  if (ok) {
    size_t escritos = p.putBool("cloud_on", activo);
    if (escritos) contarEscrituraFlash();
    ok = escritos == 1;
  }
  p.end();
  if (!ok) {
    ultimoErrorCfg = "No se pudo verificar la escritura en la NVS";
    return false;
  }
  if (xSemaphoreTake(candado, portMAX_DELAY) != pdTRUE) {
    ultimoErrorCfg = "No se pudo bloquear la configuracion cloud";
    return false;
  }
  strncpy(urlCfg, url, sizeof urlCfg - 1);       urlCfg[sizeof urlCfg - 1] = '\0';
  strncpy(userCfg, usuario, sizeof userCfg - 1); userCfg[sizeof userCfg - 1] = '\0';
  if (tocaToken) {
    strncpy(tokenCfg, token, sizeof tokenCfg - 1);
    tokenCfg[sizeof tokenCfg - 1] = '\0';
  }
  activoCfg = activo;
  if (cambiaDestino) {
    colaCabeza = 0;
    colaCuenta = 0;
  }
  revisionCfg++;
  proximoIntentoMs = 0;
  reintentoMs = CLOUD_REINTENTO_MIN_MS;
  xSemaphoreGive(candado);
  if (tarea) xTaskNotifyGive(tarea);
  return true;
}

// Un POST fallido debe volver a ser el mas antiguo. Si la cola se lleno
// mientras la red estaba ocupada, se descarta el dato mas nuevo.
static void colaReinsertarFrente(const Muestra &m) {
  if (colaCuenta == CLOUD_BUFFER) colaCuenta--;  // descarta el ultimo logico
  colaCabeza = (colaCabeza + CLOUD_BUFFER - 1) % CLOUD_BUFFER;
  cola[colaCabeza] = m;
  colaCuenta++;
}

const char *cloudErrorConfig() { return ultimoErrorCfg; }

void cloudSnapshot(CloudEstado &out) {
  memset(&out, 0, sizeof out);
  if (!candado || xSemaphoreTake(candado, portMAX_DELAY) != pdTRUE) return;
  out.activo = activoCfg;
  out.hayToken = tokenCfg[0] != '\0';
  out.configurado = urlCfg[0] && userCfg[0] && out.hayToken;
  out.enCola = colaCuenta;
  out.enviosOk = enviosOk;
  out.enviosFallo = enviosFallo;
  out.ultimoEnvioMs = ultimoEnvioMs;
  strncpy(out.url, urlCfg, sizeof out.url - 1);
  out.url[sizeof out.url - 1] = '\0';
  strncpy(out.usuario, userCfg, sizeof out.usuario - 1);
  out.usuario[sizeof out.usuario - 1] = '\0';
  xSemaphoreGive(candado);
}

// Formato Influx Line Protocol. Campos ausentes (sensor caido, centinela
// NULO_*) se omiten -- Influx acepta una lista de campos parcial, lo que no
// acepta es una linea sin ningun campo, por eso puede devolver 0.
static size_t cloudLinea(char *dst, size_t n, const Muestra &m) {
  char campos[160] = "";
  size_t j = 0;
  bool primero = true;

#define CAMPO(nombre, valor, fmt) do { \
    int w = snprintf(campos + j, sizeof(campos) - j, "%s" nombre "=" fmt, \
                     primero ? "" : ",", valor); \
    if (w > 0 && (size_t)w < sizeof(campos) - j) { j += w; primero = false; } \
  } while (0)

  if (m.pm1  != NULO_U16) CAMPO("pm1",  (unsigned)m.pm1,  "%u");
  if (m.pm25 != NULO_U16) CAMPO("pm25", (unsigned)m.pm25, "%u");
  if (m.pm10 != NULO_U16) CAMPO("pm10", (unsigned)m.pm10, "%u");
  if (m.co2  != NULO_U16) CAMPO("co2",  (unsigned)m.co2,  "%u");
  if (m.tempD != NULO_I16) CAMPO("temp", m.tempD / 10.0f, "%.1f");
  if (m.humD  != NULO_U16) CAMPO("hum",  m.humD  / 10.0f, "%.1f");
#undef CAMPO

  if (j == 0) return 0;  // los dos sensores caidos a la vez: no hay nada que mandar

  if (m.ts >= TS_EPOCH_MIN) {
    return snprintf(dst, n, "aire,dispositivo=airscript %s %lu000000000",
                    campos, (unsigned long)m.ts);
  }
  // Sin hora NTP: se omite el timestamp, el servidor pone la de ingesta.
  return snprintf(dst, n, "aire,dispositivo=airscript %s", campos);
}

// POST bloqueante, pero exclusivamente dentro de tareaCloud(). El loop
// principal solo encola y nunca espera a DNS, TLS ni al servidor de Grafana.
static bool enviarLinea(const char *linea, const char *url,
                        const char *usuario, const char *token) {
  WiFiClientSecure cli;
  cli.setCACert(CLOUD_ROOT_CA);
  HTTPClient http;
  http.setTimeout(CLOUD_TIMEOUT_MS);
  if (!http.begin(cli, url)) return false;
  http.setAuthorization(usuario, token);
  http.addHeader("Content-Type", "text/plain");
  int code = http.POST((uint8_t *)linea, strlen(linea));
  http.end();
  return code == 204 || code == 200;
}

static void tomarMuestra(Muestra &m) {
  m.ts = (uint32_t)redAhora();
  if (saludPms() == SALUD_OK) {
    m.pm1 = pms_estado.pm1; m.pm25 = pms_estado.pm25; m.pm10 = pms_estado.pm10;
  } else {
    m.pm1 = m.pm25 = m.pm10 = NULO_U16;
  }
  if (saludScd() == SALUD_OK) {
    m.co2 = scd_estado.co2;
    m.tempD = (int16_t)lroundf(scd_estado.temp * 10.0f);
    m.humD  = (uint16_t)lroundf(scd_estado.hum * 10.0f);
  } else {
    m.co2 = NULO_U16; m.tempD = NULO_I16; m.humD = NULO_U16;
  }
}

void cloudTick() {
  if (!candado) return;

  Muestra m;
  tomarMuestra(m);
  char linea[220];
  size_t n = cloudLinea(linea, sizeof linea, m);
  if (n == 0) return;  // nada que mandar este minuto

  if (xSemaphoreTake(candado, portMAX_DELAY) != pdTRUE) return;
  bool activo = activoCfg && configValidaSinLock();
  if (activo) colaEncolar(m);
  xSemaphoreGive(candado);
  if (activo && tarea) xTaskNotifyGive(tarea);
}

static bool antesDe(uint32_t ahora, uint32_t objetivo) {
  return objetivo && (int32_t)(ahora - objetivo) < 0;
}

static void tareaCloud(void *) {
  for (;;) {
    Muestra m;
    char url[CLOUD_URL_LEN] = "";
    char usuario[CLOUD_USER_LEN] = "";
    char token[CLOUD_TOKEN_LEN] = "";
    uint32_t revision = 0;
    bool listo = false;
    uint32_t ahora = millis();

    if (candado && xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
      listo = activoCfg && configValidaSinLock() && colaCuenta > 0 &&
              WiFi.status() == WL_CONNECTED &&
              !antesDe(ahora, proximoIntentoMs);
      if (listo) {
        colaDesencolar(m);
        strncpy(url, urlCfg, sizeof url - 1);
        strncpy(usuario, userCfg, sizeof usuario - 1);
        strncpy(token, tokenCfg, sizeof token - 1);
        revision = revisionCfg;
      }
      xSemaphoreGive(candado);
    }

    if (!listo) {
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CLOUD_TAREA_POLL_MS));
      continue;
    }

    char linea[220];
    size_t n = cloudLinea(linea, sizeof linea, m);
    if (n == 0) continue;

    bool recursos = redTlsHayMemoria(CLOUD_HEAP_MIN) &&
                    redTlsTomar(RED_TLS_ESPERA_MS);
    bool enviado = false;
    if (recursos) {
      enviado = enviarLinea(linea, url, usuario, token);
      redTlsSoltar();
    }

    if (xSemaphoreTake(candado, portMAX_DELAY) == pdTRUE) {
      bool mismaConfig = revision == revisionCfg;
      if (enviado) {
        enviosOk++;
        ultimoEnvioMs = millis();
        if (mismaConfig) {
          reintentoMs = CLOUD_REINTENTO_MIN_MS;
          proximoIntentoMs = millis() + INT_CLOUD_DRENAJE;
        }
      } else if (mismaConfig) {
        colaReinsertarFrente(m);
        if (recursos) {
          enviosFallo++;
          proximoIntentoMs = millis() + reintentoMs;
          uint32_t doble = reintentoMs * 2;
          reintentoMs = doble < CLOUD_REINTENTO_MAX_MS
                      ? doble : CLOUD_REINTENTO_MAX_MS;
        } else {
          proximoIntentoMs = millis() + CLOUD_TAREA_POLL_MS;
        }
      }
      xSemaphoreGive(candado);
    }
  }
}
