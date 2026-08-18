#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "screen.h"
#include "net.h"
#include "eventlog.h"
#include "history.h"

EstadoRed red_estado = {false, false, false, "", "", 0, 0};

static bool mdnsArrancado = false;
static SemaphoreHandle_t tlsCandado = nullptr;

bool redTlsTomar(uint32_t esperaMs) {
  return tlsCandado &&
         xSemaphoreTake(tlsCandado, pdMS_TO_TICKS(esperaMs)) == pdTRUE;
}

void redTlsSoltar() {
  if (tlsCandado) xSemaphoreGive(tlsCandado);
}

bool redTlsHayMemoria(uint32_t minimoTotal) {
  return ESP.getFreeHeap() >= minimoTotal &&
         heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= TLS_BLOQUE_MIN;
}

// Copia el SSID, la IP y el RSSI actuales al estado publico.
static void refrescarDatos() {
  strncpy(red_estado.ssid, WiFi.SSID().c_str(), sizeof(red_estado.ssid) - 1);
  red_estado.ssid[sizeof(red_estado.ssid) - 1] = '\0';
  strncpy(red_estado.ip, WiFi.localIP().toString().c_str(), sizeof(red_estado.ip) - 1);
  red_estado.ip[sizeof(red_estado.ip) - 1] = '\0';
  red_estado.rssi = WiFi.RSSI();
}

static void arrancarMdns() {
  if (mdnsArrancado) MDNS.end();
  mdnsArrancado = MDNS.begin(HOSTNAME);
  if (mdnsArrancado) {
    MDNS.addService("http", "tcp", PUERTO_HTTP);
    Serial.printf("[RED] mDNS activo: http://%s.local\n", HOSTNAME);
  } else {
    Serial.println("[RED] mDNS no arranco. Usa la IP directamente.");
  }
}

// Pide la hora por NTP. Sin ella los timestamps del historial no tienen
// sentido tras un reinicio, asi que se marca para reintentarlo luego.
static void sincronizarHora(uint32_t esperaMs) {
  bool antes = red_estado.horaOk;
  configTzTime(TZ_MADRID, NTP_SERVER);
  struct tm t;
  red_estado.horaOk = getLocalTime(&t, esperaMs);
  if (red_estado.horaOk) {
    if (!antes) {
      // Lo guardado hasta ahora lleva segundos desde el arranque. Sin este
      // ajuste esos eventos aparecerian fechados el 1 de enero de 1970.
      int64_t desfase = (int64_t)time(nullptr) - (int64_t)(millis() / 1000);
      logAjustarTs(desfase);
      historialAjustarTs(desfase);
    }
    Serial.printf("[RED] Hora sincronizada: %04d-%02d-%02d %02d:%02d:%02d\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    Serial.println("[RED] NTP no respondio. Se reintentara.");
  }
}

// WiFiManager avisa cuando no ha podido conectar y abre su punto de acceso.
static void alAbrirPortal(WiFiManager *wm) {
  red_estado.portalAbierto = true;
  Serial.printf("[RED] Sin red conocida. Portal abierto en '%s'\n", AP_SSID);
  Serial.println("[RED] Conecta desde el movil y elige tu WiFi.");
  pantallaMensaje("Config WiFi", AP_SSID, "desde el movil");
}

void redInit() {
  tlsCandado = xSemaphoreCreateMutex();
  if (!tlsCandado)
    Serial.println("[RED] No se pudo crear el mutex TLS. Red HTTPS desactivada.");

  WiFiManager wm;
  wm.setAPCallback(alAbrirPortal);
  wm.setDebugOutput(false);
  // Sin este timeout, un corte de luz con el router aun arrancando dejaria el
  // aparato atrapado en modo punto de acceso para siempre, sin medir nada.
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);

  Serial.println("[RED] Conectando...");
  red_estado.conectado = wm.autoConnect(AP_SSID);
  red_estado.portalAbierto = false;

  if (!red_estado.conectado) {
    Serial.println("[RED] Sin conexion. Sigo midiendo sin red.");
    pantallaMensaje("Sin WiFi", "Sigo midiendo", "sin red");
    delay(1500);
    return;
  }

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  refrescarDatos();
  Serial.printf("[RED] Conectado a '%s'  IP %s  RSSI %d dBm\n",
                red_estado.ssid, red_estado.ip, (int)red_estado.rssi);

  arrancarMdns();
  sincronizarHora(5000);

  char l1[24], l2[24];
  snprintf(l1, sizeof l1, "%s", red_estado.ip);
  snprintf(l2, sizeof l2, "http://%s.local", HOSTNAME);
  pantallaMensaje("WiFi conectado", l1, l2);
  delay(5000);  // solo en el arranque: da tiempo a apuntar la IP
}

void redComprobar() {
  bool ahoraConectado = (WiFi.status() == WL_CONNECTED);

  if (!ahoraConectado) {
    if (red_estado.conectado) {
      Serial.println("[RED] Conexion perdida. Reintentando...");
      red_estado.conectado = false;
    }
    WiFi.reconnect();  // no bloqueante
    return;
  }

  if (!red_estado.conectado) {  // acabamos de recuperar la conexion
    red_estado.conectado = true;
    red_estado.reconexiones++;
    refrescarDatos();
    Serial.printf("[RED] Reconectado. IP %s (reconexiones: %u)\n",
                  red_estado.ip, (unsigned)red_estado.reconexiones);
    arrancarMdns();  // mDNS no sobrevive a un cambio de IP
  } else {
    red_estado.rssi = WiFi.RSSI();
  }

  // Espera corta: esto corre dentro del loop y no puede bloquearlo.
  if (!red_estado.horaOk) sincronizarHora(500);
}

time_t redAhora() {
  if (red_estado.horaOk) return time(nullptr);
  return (time_t)(millis() / 1000);  // sin NTP: segundos desde arranque
}
