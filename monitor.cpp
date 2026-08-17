#include <esp_system.h>
#include "monitor.h"
#include "config.h"
#include "eventlog.h"
#include "i2cbus.h"
#include "sensors.h"
#include "alerts.h"
#include "screen.h"
#include "net.h"
#include "outdoor.h"

// Estado anterior de cada cosa vigilada. Arrancan en valores imposibles para
// que la primera revision no dispare falsas transiciones.
static SaludSensor prevPms = (SaludSensor)255;
static SaludSensor prevScd = (SaludSensor)255;
static int16_t     prevScdErr = 0;
static bool        prevWifi = false;
static bool        prevHora = false;
static bool        avisoHeap = false;
// 255 = "aun no mirado", para que la primera revision no invente transiciones.
static uint8_t     prevExt = 255;
static uint32_t    prevExtFallos = 0;

static Nivel prevNivel[5] = {NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK};
static const char *NOMBRE_MAGNITUD[5] = {"PM2.5", "PM10", "CO2", "Temp", "Hum"};
static bool nivelIniciado = false;

const char *motivoReset() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_EXT:       return "EXTERNO";
    case ESP_RST_SW:        return "SOFTWARE";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "WDT_INT";
    case ESP_RST_TASK_WDT:  return "WDT_TAREA";
    case ESP_RST_WDT:       return "WDT_OTRO";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "DESCONOCIDO";
  }
}

static const char *textoNivel(Nivel n) {
  switch (n) {
    case NIVEL_MALO:  return "malo";
    case NIVEL_AVISO: return "aviso";
    default:          return "ok";
  }
}

void monitorInit() {
  const char *motivo = motivoReset();
  // BROWNOUT delata alimentacion insuficiente: es la causa numero uno de
  // reinicios misteriosos al encender la WiFi.
  logEvento(strcmp(motivo, "BROWNOUT") == 0 ? LOG_ERROR : LOG_INFO,
            "Arranque (reset: %s)", motivo);

  logEvento(LOG_INFO, "I2C: %u dispositivo(s) en el bus", (unsigned)bus_i2c.total);
  if (!bus_i2c.scd41) logEvento(LOG_ERROR, "SCD41 no responde en 0x62");
  if (pantalla_ok) logEvento(LOG_INFO, "OLED OK en 0x%02X", pantalla_addr);
  else             logEvento(LOG_ERROR, "OLED no detectada");
}

// Vigila un sensor y registra solo el cambio de salud, no cada lectura.
static void revisarSensor(const char *nombre, SaludSensor ahora,
                          SaludSensor &prev, const char *detalle) {
  if (ahora == prev) return;
  // ESPERANDO no es noticia: es el estado normal de los primeros segundos.
  if (ahora == SALUD_FALLO)   logEvento(LOG_ERROR, "%s: fallo (%s)", nombre, detalle);
  else if (ahora == SALUD_OK && prev == SALUD_FALLO)
                              logEvento(LOG_INFO,  "%s: recuperado", nombre);
  else if (ahora == SALUD_OK) logEvento(LOG_INFO,  "%s: midiendo OK", nombre);
  prev = ahora;
}

void monitorRevisar() {
  // --- Sensores ---
  revisarSensor("PMS5003", saludPms(), prevPms,
                pms_estado.rancio ? "sin frames" : "sin datos");

  // Codigo 0 significa "ningun error": decir "cod=0" seria enganoso. Ese caso
  // es el sensor que contesta a todo pero nunca entrega una medida.
  char detalleScd[40];
  if (scd_estado.error)
    snprintf(detalleScd, sizeof detalleScd, "cod %d en %s",
             scd_estado.error, scd_estado.etapa);
  else if (scd_estado.rancio)
    snprintf(detalleScd, sizeof detalleScd, "dejo de entregar datos");
  else
    snprintf(detalleScd, sizeof detalleScd, "responde pero no mide");
  revisarSensor("SCD41", saludScd(), prevScd, detalleScd);

  // Un codigo de error distinto dentro del mismo estado de fallo si es noticia:
  // cambia el diagnostico.
  if (scd_estado.error && scd_estado.error != prevScdErr) {
    logEvento(LOG_ERROR, "SCD41 err %d en %s", scd_estado.error, scd_estado.etapa);
  }
  prevScdErr = scd_estado.error;

  // --- Red ---
  if (red_estado.conectado != prevWifi) {
    if (red_estado.conectado)
      logEvento(LOG_INFO, "WiFi OK %s (%d dBm)", red_estado.ip, (int)red_estado.rssi);
    else
      logEvento(LOG_AVISO, "WiFi caido, reintentando");
    prevWifi = red_estado.conectado;
  }

  if (red_estado.horaOk != prevHora) {
    logEvento(red_estado.horaOk ? LOG_INFO : LOG_AVISO,
              red_estado.horaOk ? "Hora sincronizada por NTP" : "NTP perdido");
    prevHora = red_estado.horaOk;
  }

  // --- Aire exterior ---
  // El sondeo corre en otra tarea y no escribe en el log a proposito: el
  // buffer de eventos no tiene cerrojo y una entrada partida entre dos nucleos
  // es peor que no tenerla. La traduccion a eventos se hace aqui, en el loop.
  Exterior ext;
  exteriorSnapshot(ext);
  uint8_t estExt = !ext.configurado ? 0 : ((ext.valido && !ext.rancio) ? 1 : 2);
  if (estExt != prevExt) {
    if (prevExt == 1 && estExt == 2)
      logEvento(LOG_AVISO, "Exterior: dato caducado (>%luh)",
                (unsigned long)(EXT_RANCIO_MS / 3600000UL));
    else if (estExt == 1)
      logEvento(LOG_INFO, "Exterior: datos de %s", ext.lugar);
    prevExt = estExt;
  }
  // El antirrebote del log convierte una racha de fallos en una sola linea con
  // su contador, asi que no hace falta filtrar aqui.
  if (ext.fallos != prevExtFallos) {
    logEvento(LOG_AVISO, "Exterior: sondeo fallido (http %d)", (int)ext.httpCode);
    prevExtFallos = ext.fallos;
  }

  // --- Niveles de alerta ---
  Nivel act[5] = {alertas.pm25, alertas.pm10, alertas.co2, alertas.temp, alertas.hum};
  if (!nivelIniciado) {
    // Primera pasada: fija la referencia sin inventar transiciones desde cero.
    for (int i = 0; i < 5; i++) prevNivel[i] = act[i];
    nivelIniciado = true;
  } else {
    for (int i = 0; i < 5; i++) {
      if (act[i] == prevNivel[i]) continue;
      logEvento(act[i] > prevNivel[i] ? LOG_AVISO : LOG_INFO, "%s: %s -> %s",
                NOMBRE_MAGNITUD[i], textoNivel(prevNivel[i]), textoNivel(act[i]));
      prevNivel[i] = act[i];
    }
  }

  // --- Memoria ---
  // Con histeresis: sin ella, un heap oscilando en el umbral genera ruido.
  uint32_t heap = ESP.getFreeHeap();
  if (!avisoHeap && heap < HEAP_MINIMO) {
    logEvento(LOG_ERROR, "Heap bajo: %u bytes libres", (unsigned)heap);
    avisoHeap = true;
  } else if (avisoHeap && heap > HEAP_MINIMO * 2) {
    logEvento(LOG_INFO, "Heap recuperado: %u bytes", (unsigned)heap);
    avisoHeap = false;
  }
}
