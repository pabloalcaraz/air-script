// Air Script — estacion de calidad del aire sobre ESP32.
// PMS5003 (particulas) + SCD41 (CO2/temp/humedad) + OLED SSD1306.
// Cableado y montaje en README.md. Constantes en config.h.

#include "config.h"
#include "i2cbus.h"
#include "sensors.h"
#include "alerts.h"
#include "screen.h"
#include "net.h"
#include "outdoor.h"
#include "eventlog.h"
#include "history.h"
#include "stats.h"
#include "monitor.h"
#include "webapi.h"
#include "cloud.h"

// Temporizadores del loop. Se comparan siempre por resta para que el
// desbordamiento de millis() a los ~49 dias no rompa la logica.
static uint32_t tScd = 0, tPantalla = 0, tSerie = 0, tRed = 0;
static uint32_t tMuestra = 0, tMonitor = 0, tAlertas = 0;
static uint32_t tCloud = 0;

// Una linea de estado por segundo: lecturas, marcas de alerta y salud.
static void serieEstado() {
  if (!pms_estado.valido) {
    Serial.print(saludPms() == SALUD_ESPERANDO ? "PMS=calentando" : "PMS=SIN-FRAMES");
  } else {
    Serial.printf("PM1=%u PM2.5=%u%s PM10=%u%s frames=%u%s",
                  (unsigned)pms_estado.pm1,
                  (unsigned)pms_estado.pm25, nivelMarca(alertas.pm25),
                  (unsigned)pms_estado.pm10, nivelMarca(alertas.pm10),
                  (unsigned)pms_estado.frames,
                  pms_estado.rancio ? " [RANCIO]" : "");
  }

  if (scd_estado.rancio) {
    // Sin esta rama se imprimiria la ultima lectura buena como si fuera de
    // ahora, que es exactamente el fallo que este aviso existe para delatar.
    Serial.printf(" | SCD41=RANCIO (sin dato hace %lus, %lu reinicios)\n",
                  (unsigned long)((millis() - scd_estado.ultimaLecturaMs) / 1000),
                  (unsigned long)scd_estado.reinicios);
  } else if (scd_estado.valido && !scd_estado.error) {
    Serial.printf(" | CO2=%u%s T=%.1f%s H=%.1f%s reads=%u%s\n",
                  (unsigned)scd_estado.co2, nivelMarca(alertas.co2),
                  scd_estado.temp, nivelMarca(alertas.temp),
                  scd_estado.hum,  nivelMarca(alertas.hum),
                  (unsigned)scd_estado.reads,
                  scd_estado.congelado ? " [CONGELADO]" : "");
  } else if (saludScd() == SALUD_ESPERANDO) {
    Serial.println(" | SCD41=esperando primer dato");
  } else if (scd_estado.error) {
    Serial.printf(" | SCD41=FALLO cod=%d en '%s' (bus:%s)\n",
                  scd_estado.error, scd_estado.etapa,
                  scd_estado.presenteI2c ? "0x62 ok" : "0x62 ausente");
  } else {
    Serial.printf(" | SCD41=NO-MIDE (responde en 0x62 sin errores, 0 medidas"
                  " en %us) -> revisa alimentacion\n",
                  (unsigned)(millis() / 1000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Air Script ===");

  i2cInit();
  i2cEscanear();   // diagnostico del bus antes de tocar nada
  pantallaInit();  // no bloquea si falla
  monitorInit();   // deja constancia en el log de con que hardware arrancamos
  redInit();       // si no hay WiFi, seguimos midiendo igual
  // WiFiManager puede retener setup() hasta 180 s. Arrancar despues evita
  // dejar el laser del PMS encendido y su UART sin drenar durante el portal.
  sensoresInit();
  // Despues de redInit(): la tarea de sondeo comprueba el WiFi por su cuenta,
  // pero sin localizacion guardada ni siquiera se crea.
  exteriorInit();
  // Se arranca aunque no haya red: si el WiFi vuelve luego, ya esta sirviendo.
  servidorInit();
  cloudInit();

  // Sin esto, la primera vuelta de loop() puede pintar pantallaRefrescar()
  // en t=0 antes de que el temporizador de abajo dispare a los 500 ms, y
  // leeria el inicializador estatico de `alertas` (todo NIVEL_OK).
  alertasCalcular();
}

void loop() {
  uint32_t ahora = millis();

  sensoresLeerPms();  // no bloqueante, drena el buffer del UART

  if (ahora - tScd >= INT_SCD41) {
    tScd = ahora;
    sensoresLeerScd41();
    sensoresRevisarScd();  // detecta datos rancios o congelados y recupera
  }

  // Aqui y no en cada vuelta del loop: las entradas (lectura del SCD41, del
  // PMS y las medias de 24 h) no cambian mas rapido que esto, y el loop gira
  // a ~1000 Hz. Mismo criterio que estadisticasRefrescarMedias(). Va antes que
  // pantalla, serie, monitor e historial, que son todos consumidores.
  if (ahora - tAlertas >= INT_ALERTAS) {
    tAlertas = ahora;
    alertasCalcular();
  }

  if (ahora - tPantalla >= INT_PANTALLA) {
    tPantalla = ahora;
    pantallaRefrescar();
  }

  if (ahora - tSerie >= INT_SERIE) {
    tSerie = ahora;
    serieEstado();
  }

  if (ahora - tRed >= INT_RED) {
    tRed = ahora;
    redComprobar();
  }

  if (ahora - tMonitor >= INT_MONITOR) {
    tMonitor = ahora;
    monitorRevisar();  // solo escribe en el log cuando algo cambia
  }

  if (ahora - tMuestra >= INT_MUESTRA) {
    tMuestra = ahora;
    historialGuardar();
    // Aqui y no dentro de alertasCalcular(): esa se ejecuta en cada vuelta del
    // loop y recorrer 1440 muestras miles de veces por segundo no tendria
    // sentido para un valor que solo cambia una vez por minuto.
    estadisticasRefrescarMedias();
  }

  if (ahora - tCloud >= INT_CLOUD) {
    tCloud = ahora;
    cloudTick();
  }

  servidorAtender();  // cada vuelta: no puede haber esperas antes de esto

  // Cede CPU a las tareas de sistema (WiFi, TCP). No bloquea nada:
  // el ritmo real lo marcan los temporizadores de arriba.
  delay(1);
}
