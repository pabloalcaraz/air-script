#include <Wire.h>
#include <PMS.h>
#include <SensirionI2cScd4x.h>
#include <math.h>
#include "config.h"
#include "i2cbus.h"
#include "eventlog.h"
#include "sensors.h"
#include "flashstats.h"

EstadoPms pms_estado = {false, false, false, 0, 0, 0, 0, 0};
EstadoScd scd_estado = {
  false, false, 0, "arranque", 0, 0, 0, 0, 0,          // lectura y errores
  0, false, false, 0, 0, 0.0f, 0.0f, 0, 0, SELFTEST_SIN_HACER, 0  // vigilancia
};

static PMS pms(Serial2);
static PMS::DATA pmsData;
static SensirionI2cScd4x scd4x;
static char errMsg[64];
static uint32_t msArranque = 0;  // para no dar por averiado un sensor que aun calienta
static uint32_t msPrimeraLectura = 0;  // cuando entrego su primer dato bueno
static uint32_t msUltimoIntento = 0;   // ultimo reinicio automatico lanzado

// Duty-cycle del PMS5003 (Fase E). El sensor pasa dormido la mayor parte del
// minuto (SET=LOW): el laser esta especificado para ~8000 h y en continuo se
// gasta en menos de un anio; con esto llega a las ~20000 h de vida, unos 20
// anios.
static uint32_t msCicloIni    = 0;      // ancla del periodo de INT_MUESTRA
static uint32_t msDespertar   = 0;      // millis() del despertar en curso
static bool     pmsCalentando = false;  // tramo cuyos frames se descartan
static uint32_t msUltimoCrudo = 0;      // ultimo frame recibido, se acepte o no
static uint32_t sumPm1 = 0, sumPm25 = 0, sumPm10 = 0;
static uint16_t nProm  = 0;

// Registra un fallo del SCD41: guarda etapa + codigo y lo reporta por serie.
static void scdFallo(const char *etapa, int16_t error) {
  scd_estado.etapa = etapa;
  scd_estado.error = error;
  scd_estado.errores++;
  errorToString(error, errMsg, sizeof errMsg);
  Serial.printf("[SCD41] ERROR en %s -> cod=%d (%s)\n", etapa, error, errMsg);
}

// El CRC de la libreria detecta tramas rotas, pero aun asi no se debe publicar
// un NaN/Inf ni una magnitud fuera del dominio fisico del SCD41. Los limites de
// CO2 cubren todo su rango de salida; temperatura y humedad dejan margen fuera
// del uso interior sin aceptar valores imposibles.
static bool scdMuestraValida(uint16_t co2, float temp, float hum) {
  return co2 >= 200 && co2 <= 40000 &&
         isfinite(temp) && temp >= -40.0f && temp <= 85.0f &&
         isfinite(hum)  && hum  >=   0.0f && hum  <= 100.0f;
}

static void scdDatoInvalido(uint16_t co2, float temp, float hum) {
  scd_estado.error = 0;  // la comunicacion I2C fue correcta; el dato no lo fue
  scd_estado.etapa = "muestra fuera de rango";
  scd_estado.errores++;
  Serial.printf("[SCD41] Muestra descartada: CO2=%u T=%.2f H=%.2f\n",
                (unsigned)co2, temp, hum);
}

// Secuencia de arranque del SCD41 respetando los tiempos del datasheet.
static void iniciarScd41() {
  scd_estado.presenteI2c = bus_i2c.scd41;

  int16_t error;
  scd4x.begin(Wire, SCD41_I2C_ADDR_62);
  delay(30);

  // wakeUp no devuelve ACK por diseno: su "error" es normal, se ignora.
  scd4x.wakeUp();
  delay(30);

  // stop_periodic_measurement tarda 500 ms en ejecutarse. Sin esta espera
  // el siguiente comando llega al sensor a medias y lo deja colgado.
  error = scd4x.stopPeriodicMeasurement();
  if (error) scdFallo("stopPeriodicMeasurement", error);
  delay(500);

  error = scd4x.reinit();
  if (error) scdFallo("reinit", error);
  delay(30);

  // La altitud no se persiste en la EEPROM a proposito: se aplica en cada
  // arranque y asi no se gastan ciclos de escritura por un dato que ya esta
  // en config.h. Sin ella el sensor asume el nivel del mar.
  scd4x.setSensorAltitude(ALTITUD_M);
  delay(30);

  // Lecturas de diagnostico: sin ellas no hay forma de saber si el ASC esta
  // desviando la referencia ni de detectar que el chip se ha reiniciado solo.
  scd4x.getAutomaticSelfCalibrationEnabled(scd_estado.asc);
  scd4x.getSerialNumber(scd_estado.serie);

  error = scd4x.startPeriodicMeasurement();
  if (error) {
    scdFallo("startPeriodicMeasurement", error);
  } else {
    scd_estado.error = 0;
    scd_estado.etapa = "midiendo";
    // Se parte de "recien leido" para que el vigilante no cuente como rancios
    // los segundos que el sensor tarda legitimamente en su primera medida.
    scd_estado.ultimaLecturaMs = millis();
    scd_estado.msValorIgual    = millis();
    Serial.printf("[SCD41] Arrancado OK. ASC=%u, altitud=%u m. Primer dato en ~5s.\n",
                  (unsigned)scd_estado.asc, (unsigned)ALTITUD_M);
  }
}

// Arranca un ciclo despierto: enciende el sensor y limpia el acumulador del
// promedio. msUltimoCrudo se reancla aqui a proposito, para que el silencio
// del sueno que acaba de terminar no cuente como los primeros segundos de
// silencio despierto.
static void pmsDespertar() {
  digitalWrite(PMS_SET_PIN, HIGH);
  pms_estado.durmiendo = false;
  pmsCalentando = true;
  msDespertar   = millis();
  msUltimoCrudo = msDespertar;
  sumPm1 = sumPm25 = sumPm10 = 0;
  nProm = 0;
}

static void pmsDormir() {
  digitalWrite(PMS_SET_PIN, LOW);
  pms_estado.durmiendo = true;
}

void sensoresInit() {
  msArranque = millis();
  Serial2.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  pinMode(PMS_SET_PIN, OUTPUT);
  msCicloIni = millis();
  pmsDespertar();  // el primer ciclo arranca despierto: no tiene sentido dormir sin haber medido nunca
  Serial.println("[PMS] Calentando, ~30s hasta lecturas fiables.");
  iniciarScd41();
}

// Un sensor que responde pero no entrega datos NO esta bien: pasado el margen
// de arranque se marca como averiado aunque no haya devuelto ningun error.
static bool pasoElArranque() {
  return millis() - msArranque > ARRANQUE_MS;
}

SaludSensor saludPms() {
  if (pms_estado.rancio) return SALUD_FALLO;
  if (pms_estado.valido) return SALUD_OK;
  return pasoElArranque() ? SALUD_FALLO : SALUD_ESPERANDO;
}

SaludSensor saludScd() {
  if (scd_estado.error) return SALUD_FALLO;
  // Un sensor que dejo de entregar datos NO esta sano aunque no de errores.
  // Sin esta linea, el ultimo valor bueno se sirve como actual para siempre.
  if (scd_estado.rancio) return SALUD_FALLO;
  if (scd_estado.valido) return SALUD_OK;
  return pasoElArranque() ? SALUD_FALLO : SALUD_ESPERANDO;
}

void sensoresLeerPms() {
  uint32_t ahora = millis();

  // Un despertar por INT_MUESTRA, anclado al despertar anterior y no al
  // dormir: asi un despertar que se retrasa un instante no le roba tiempo de
  // sueno al siguiente ciclo.
  if (pms_estado.durmiendo && (ahora - msCicloIni >= INT_MUESTRA)) {
    msCicloIni = ahora;
    pmsDespertar();
  }

  bool huboFrame = pms.read(pmsData);
  if (huboFrame) msUltimoCrudo = ahora;  // el sensor sigue vivo, se acepte o no el dato

  if (!pms_estado.durmiendo) {
    if (pmsCalentando) {
      // Frames del calentamiento: se descartan a proposito. El ventilador aun
      // no ha estabilizado el caudal y el valor no significa nada.
      if (ahora - msDespertar >= PMS_CALENTAR_MS) pmsCalentando = false;
    } else {
      if (huboFrame) {
        sumPm1  += pmsData.PM_AE_UG_1_0;
        sumPm25 += pmsData.PM_AE_UG_2_5;
        sumPm10 += pmsData.PM_AE_UG_10_0;
        nProm++;
      }
      if (ahora - msDespertar >= PMS_CALENTAR_MS + PMS_PROMEDIO_MS) {
        // nProm en 0 significa que no llego ni un frame en toda la ventana:
        // no se inventa una lectura, se deja que el temporizador de rancio de
        // abajo lo delate por su cuenta.
        if (nProm > 0) {
          pms_estado.valido = true;
          pms_estado.pm1  = sumPm1  / nProm;
          pms_estado.pm25 = sumPm25 / nProm;
          pms_estado.pm10 = sumPm10 / nProm;
          pms_estado.ultimoFrameMs = ahora;
          pms_estado.frames++;
        }
        pmsDormir();
      }
    }
  }

  // Rancio solo tiene sentido despierto: dormido, el silencio es el
  // comportamiento normal del duty-cycle, no una averia. Sin este guarda la
  // Fase E generaria una alarma falsa en cada sueno.
  pms_estado.rancio = !pms_estado.durmiendo && pms_estado.valido &&
                      (ahora - msUltimoCrudo > PMS_TIMEOUT_MS);
}

void sensoresLeerScd41() {
  bool dataReady = false;
  int16_t error = scd4x.getDataReadyStatus(dataReady);
  if (error) {
    scdFallo("getDataReadyStatus", error);
    return;
  }
  if (!dataReady) return;

  uint16_t co2;
  float temp, hum;
  error = scd4x.readMeasurement(co2, temp, hum);
  if (error) {
    scdFallo("readMeasurement", error);
    return;
  }
  if (!scdMuestraValida(co2, temp, hum)) {
    scdDatoInvalido(co2, temp, hum);
    return;
  }

  if (!msPrimeraLectura) msPrimeraLectura = millis();

  // Detector de valor congelado: se exige que cambie ALGUNA de las tres
  // magnitudes. El CO2 solo daria falsos positivos en un ambiente muy quieto,
  // pero que ademas coincidan temperatura y humedad al decimal descarta el
  // ambiente estable y solo deja la explicacion mala: dato repetido.
  if (co2 != scd_estado.co2Previo ||
      temp != scd_estado.tempPrevio ||
      hum  != scd_estado.humPrevio) {
    scd_estado.co2Previo   = co2;
    scd_estado.tempPrevio  = temp;
    scd_estado.humPrevio   = hum;
    scd_estado.msValorIgual = millis();
    scd_estado.congelado    = false;
  }

  scd_estado.co2  = co2;
  scd_estado.temp = temp;
  scd_estado.hum  = hum;
  scd_estado.valido = true;
  scd_estado.error  = 0;
  scd_estado.etapa  = "midiendo";
  scd_estado.reads++;
  scd_estado.ultimaLecturaMs = millis();
  scd_estado.rancio = false;
}

void sensoresRevisarScd() {
  uint32_t ahora = millis();
  scd_estado.rancio    = (ahora - scd_estado.ultimaLecturaMs) > SCD_TIMEOUT_MS;
  scd_estado.congelado = scd_estado.valido &&
                         (ahora - scd_estado.msValorIgual) > SCD_CONGELADO_MS;

  if (!scd_estado.rancio && !scd_estado.congelado) return;

  // Los dos sintomas se curan igual, pero no se reintenta sin parar: entre
  // intento e intento tiene que haber tiempo real de medida para saber si
  // el reinicio ha servido de algo.
  if (msUltimoIntento && (ahora - msUltimoIntento) < SCD_REINTENTO_MS) return;
  msUltimoIntento = ahora;

  if (scd_estado.rancio) {
    if (scd_estado.valido)
      logEvento(LOG_ERROR, "SCD41 mudo %lus: reiniciando",
                (unsigned long)((ahora - scd_estado.ultimaLecturaMs) / 1000));
    else
      logEvento(LOG_ERROR, "SCD41 sin primera medida %lus: reiniciando",
                (unsigned long)((ahora - scd_estado.ultimaLecturaMs) / 1000));
  } else {
    logEvento(LOG_ERROR, "SCD41 valor congelado %lumin: reiniciando",
              (unsigned long)((ahora - scd_estado.msValorIgual) / 60000));
  }
  sensoresReiniciarScd();
}

bool sensoresReiniciarScd() {
  int16_t error = scd4x.stopPeriodicMeasurement();
  if (error) scdFallo("reset/stop", error);
  delay(500);  // stop tarda 500 ms en ejecutarse

  // Apagado real del sensor. reinit() solo recarga la configuracion desde la
  // EEPROM: si lo que esta colgado es la maquina de estados interna, no la
  // toca. powerDown corta su alimentacion interna, que es justo lo que hace
  // desenchufar el aparato, y sin pin de reset es la unica via.
  scd4x.powerDown();
  delay(50);
  scd4x.wakeUp();  // no devuelve ACK por diseno: su "error" es normal
  delay(30);

  error = scd4x.reinit();
  if (error) scdFallo("reset/reinit", error);
  delay(30);

  scd4x.setSensorAltitude(ALTITUD_M);
  delay(30);
  scd4x.getAutomaticSelfCalibrationEnabled(scd_estado.asc);
  scd4x.getSerialNumber(scd_estado.serie);

  error = scd4x.startPeriodicMeasurement();
  scd_estado.reinicios++;
  msPrimeraLectura = 0;  // los 3 min previos a una calibracion FRC vuelven a contar

  if (error) {
    scdFallo("reset/start", error);
    return false;
  }

  scd_estado.error = 0;
  scd_estado.etapa = "midiendo";
  // Se le da margen limpio: sin esto el propio reinicio seguiria contando como
  // rancio y dispararia otro reinicio en la siguiente revision, en bucle.
  scd_estado.ultimaLecturaMs = millis();
  scd_estado.msValorIgual    = millis();
  scd_estado.rancio    = false;
  scd_estado.congelado = false;
  Serial.printf("[SCD41] Reiniciado (%lu en total)\n",
                (unsigned long)scd_estado.reinicios);
  return true;
}

int16_t sensoresAutotestScd(uint16_t &estado) {
  estado = SELFTEST_SIN_HACER;

  int16_t error = scd4x.stopPeriodicMeasurement();
  if (error) scdFallo("autotest/stop", error);
  delay(500);

  // El sensor tarda 10 s en responder; la libreria espera dentro con delay(),
  // que cede a FreeRTOS, asi que no dispara el watchdog de tareas.
  error = scd4x.performSelfTest(estado);

  // Se relanza la medicion pase lo que pase: dejar el sensor parado por un
  // autotest fallido seria peor que no haberlo hecho.
  int16_t errStart = scd4x.startPeriodicMeasurement();
  msPrimeraLectura = 0;
  if (errStart) {
    scdFallo("autotest/start", errStart);
    return errStart;
  }
  scd_estado.error = 0;
  scd_estado.etapa = "midiendo";
  scd_estado.ultimaLecturaMs = millis();
  scd_estado.msValorIgual    = millis();
  scd_estado.rancio    = false;
  scd_estado.congelado = false;

  if (!error) scd_estado.selftest = estado;
  return error;
}

int16_t sensoresSetAscScd(bool activo) {
  int16_t error = scd4x.stopPeriodicMeasurement();
  if (error) scdFallo("asc/stop", error);
  delay(500);

  error = scd4x.setAutomaticSelfCalibrationEnabled(activo ? 1 : 0);
  if (!error) {
    // persistSettings escribe la EEPROM del sensor, con ciclos contados. Sin
    // ella el cambio se pierde al quitar la corriente; con ella hay que
    // llamarla lo menos posible. Por eso solo se hace aqui, a peticion.
    error = scd4x.persistSettings();
    if (!error) contarEscrituraFlash();
    delay(800);
  }
  scd4x.getAutomaticSelfCalibrationEnabled(scd_estado.asc);

  int16_t errStart = scd4x.startPeriodicMeasurement();
  msPrimeraLectura = 0;
  if (errStart) {
    scdFallo("asc/start", errStart);
    return errStart;
  }
  scd_estado.error = 0;
  scd_estado.etapa = "midiendo";
  scd_estado.ultimaLecturaMs = millis();
  scd_estado.msValorIgual    = millis();
  scd_estado.rancio    = false;
  scd_estado.congelado = false;
  return error;
}

uint32_t sensoresSegundosMidiendo() {
  if (!msPrimeraLectura) return 0;
  return (millis() - msPrimeraLectura) / 1000;
}

ResCalib sensoresCalibrar(uint16_t ppmRef, int16_t &correccion, int16_t &errorI2c) {
  correccion = 0;
  errorI2c   = 0;

  // El datasheet exige 3 min de medicion periodica previa en el aire de
  // referencia. Sin eso el sensor calibra contra un valor que aun se mueve.
  if (saludScd() != SALUD_OK || sensoresSegundosMidiendo() < 180) {
    return CALIB_NO_LISTO;
  }

  int16_t error = scd4x.stopPeriodicMeasurement();
  if (error) { errorI2c = error; scdFallo("calib/stop", error); }
  delay(500);  // stop tarda 500 ms en ejecutarse

  uint16_t bruto = 0;
  error = scd4x.performForcedRecalibration(ppmRef, bruto);
  delay(400);

  // Se relanza la medicion pase lo que pase: dejar el sensor parado por un
  // fallo de calibracion seria peor que no haber calibrado.
  int16_t errorStart = scd4x.startPeriodicMeasurement();
  if (errorStart) {
    scdFallo("calib/start", errorStart);
    errorI2c = errorStart;
    return CALIB_ERROR_I2C;
  }
  scd_estado.error = 0;
  scd_estado.etapa = "midiendo";
  msPrimeraLectura = 0;  // vuelve a contar: los 3 min se cumplen de nuevo

  if (error) { errorI2c = error; return CALIB_ERROR_I2C; }

  // 0xFFFF significa que el sensor rechazo la referencia. El valor util viene
  // desplazado 0x8000; restarlo da la correccion real en ppm.
  if (bruto == 0xFFFF) return CALIB_RECHAZADA;
  correccion = (int16_t)(bruto - 0x8000);
  Serial.printf("[SCD41] Calibrado a %u ppm. Correccion: %d ppm\n",
                (unsigned)ppmRef, (int)correccion);
  return CALIB_OK;
}
