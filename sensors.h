#pragma once
#include <Arduino.h>

// Lectura de los dos sensores y su estado de salud. Los modulos que consumen
// datos leen estas structs; no necesitan conocer las librerias de cada sensor.

struct EstadoPms {
  bool     valido;         // ha llegado algun frame valido alguna vez
  bool     rancio;         // despierto y sin frame crudo desde hace PMS_TIMEOUT_MS
  // Duty-cycle (Fase E): dormido es el reposo normal entre muestras, no una
  // averia. Sin esta bandera, web y OLED no pueden distinguir "esta
  // descansando" de "esta roto" durante el silencio deliberado del sueno.
  bool     durmiendo;
  uint16_t pm1, pm25, pm10;
  uint32_t frames;         // promedios aceptados (uno por ciclo de duty-cycle)
  uint32_t ultimoFrameMs;  // millis del ultimo promedio aceptado
};

// Un sensor puede estar en tres situaciones, y confundirlas despista mucho:
// esperando su primera muestra, midiendo bien, o fallando.
enum SaludSensor : uint8_t {
  SALUD_ESPERANDO = 0,  // arrancado sin errores, aun sin primer dato
  SALUD_OK        = 1,
  SALUD_FALLO     = 2
};

struct EstadoScd {
  bool        valido;       // hay al menos una lectura buena
  bool        presenteI2c;  // responde en 0x62
  int16_t     error;        // ultimo codigo de error (0 = sin error)
  const char *etapa;        // en que comando fallo, o "midiendo"
  uint16_t    co2;
  float       temp, hum;
  uint32_t    reads;        // lecturas validas totales
  uint32_t    errores;      // errores totales

  // Vigilancia (Fase A). Un SCD41 puede quedarse sin entregar datos sin
  // devolver un solo error: contesta a todo por I2C pero su bandera de "dato
  // listo" no vuelve a levantarse. Sin estos campos ese fallo es invisible.
  uint32_t    ultimaLecturaMs;  // millis de la ultima lectura valida
  bool        rancio;           // sin lectura valida desde hace SCD_TIMEOUT_MS
  bool        congelado;        // las tres magnitudes identicas demasiado tiempo
  uint32_t    msValorIgual;     // millis en que las magnitudes tomaron el valor actual
  uint16_t    co2Previo;
  float       tempPrevio, humPrevio;
  uint32_t    reinicios;        // recuperaciones automaticas o manuales hechas
  uint16_t    asc;              // 1 = autocalibracion activa, 0 = desactivada
  uint16_t    selftest;         // 0 = sano; SELFTEST_SIN_HACER = nunca ejecutado
  uint64_t    serie;            // si cambia entre lecturas, el chip se reinicio
};

extern EstadoPms pms_estado;
extern EstadoScd scd_estado;

void sensoresInit();       // UART del PMS + secuencia de arranque del SCD41
void sensoresLeerPms();    // no bloqueante: llamar en cada vuelta del loop
void sensoresLeerScd41();  // llamar cada INT_SCD41 ms

// Vigilante del SCD41: marca los datos rancios o congelados y lanza la
// recuperacion. Llamar justo despues de sensoresLeerScd41().
void sensoresRevisarScd();

// Reinicio por software. powerDown() + wakeUp() es lo mas parecido a quitarle
// la corriente sin desenchufar nada: el SCD41 no tiene pin de reset, y un
// reinit() a secas solo recarga la EEPROM sin resucitar una maquina de estados
// que se ha quedado colgada.
bool sensoresReiniciarScd();

// Autotest interno del chip: el propio sensor dice si esta averiado.
// Bloquea ~10 s por diseno del sensor, asi que solo bajo peticion expresa;
// nunca desde el loop. estado == 0 significa hardware sano.
int16_t sensoresAutotestScd(uint16_t &estado);

// Activa o desactiva la autocalibracion (ASC). Escribe la EEPROM del sensor,
// que tiene ciclos contados: solo cuando el usuario lo pide explicitamente.
int16_t sensoresSetAscScd(bool activo);

SaludSensor saludPms();
SaludSensor saludScd();

// Recalibracion forzada del SCD41 (FRC): le dice "el aire que estas midiendo
// ahora mismo tiene ppmRef ppm" y ajusta su referencia interna.
//
// Condiciones del datasheet, obligatorias o el resultado es basura:
//   - el sensor debe llevar >= 3 min midiendo en modo periodico
//   - en ese aire de referencia (para 420 ppm: exterior, lejos de personas)
//
// Devuelve un codigo de RESULTADO, no un codigo de error de la libreria:
enum ResCalib : uint8_t {
  CALIB_OK          = 0,
  CALIB_NO_LISTO    = 1,  // el sensor no lleva 3 min midiendo
  CALIB_RECHAZADA   = 2,  // el sensor devolvio 0xFFFF: no acepto la referencia
  CALIB_ERROR_I2C   = 3   // fallo de comunicacion
};
ResCalib sensoresCalibrar(uint16_t ppmRef, int16_t &correccion, int16_t &errorI2c);
uint32_t sensoresSegundosMidiendo();  // 0 si no esta midiendo
