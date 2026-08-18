#include "history.h"
#include "sensors.h"
#include "net.h"

static Muestra  buf[HIST_SIZE];
static uint16_t cabeza = 0;
static uint16_t total  = 0;

const char *NOMBRE_CAMPO[C_TOTAL] = {"pm1", "pm25", "pm10", "co2", "temp", "hum"};

bool muestraValor(const Muestra *m, uint8_t campo, float &out) {
  if (!m) return false;
  switch (campo) {
    case C_PM1:  if (m->pm1   == NULO_U16) return false; out = m->pm1;  return true;
    case C_PM25: if (m->pm25  == NULO_U16) return false; out = m->pm25; return true;
    case C_PM10: if (m->pm10  == NULO_U16) return false; out = m->pm10; return true;
    case C_CO2:  if (m->co2   == NULO_U16) return false; out = m->co2;  return true;
    case C_TEMP: if (m->tempD == NULO_I16) return false; out = m->tempD / 10.0f; return true;
    case C_HUM:  if (m->humD  == NULO_U16) return false; out = m->humD  / 10.0f; return true;
  }
  return false;
}

void historialGuardar() {
  Muestra &m = buf[cabeza];
  m.ts = (uint32_t)redAhora();

  // Un sensor rancio o averiado no aporta el ultimo valor bueno: aporta un
  // hueco. Repetir la ultima lectura fingiria que el aire sigue igual.
  if (saludPms() == SALUD_OK) {
    m.pm1  = pms_estado.pm1;
    m.pm25 = pms_estado.pm25;
    m.pm10 = pms_estado.pm10;
  } else {
    m.pm1 = m.pm25 = m.pm10 = NULO_U16;
  }

  if (saludScd() == SALUD_OK) {
    m.co2   = scd_estado.co2;
    m.tempD = (int16_t)lroundf(scd_estado.temp * 10.0f);
    m.humD  = (uint16_t)lroundf(scd_estado.hum * 10.0f);
  } else {
    m.co2   = NULO_U16;
    m.tempD = NULO_I16;
    m.humD  = NULO_U16;
  }

  cabeza = (cabeza + 1) % HIST_SIZE;
  if (total < HIST_SIZE) total++;
}

uint16_t historialCount() { return total; }

void historialAjustarTs(int64_t desfase) {
  uint16_t inicio = (total == HIST_SIZE) ? cabeza : 0;
  for (uint16_t i = 0; i < total; i++) {
    Muestra &m = buf[(inicio + i) % HIST_SIZE];
    if (m.ts < TS_EPOCH_MIN) m.ts = (uint32_t)((int64_t)m.ts + desfase);
  }
}

const Muestra *historialGet(uint16_t i) {
  if (i >= total) return nullptr;
  uint16_t inicio = (total == HIST_SIZE) ? cabeza : 0;
  return &buf[(inicio + i) % HIST_SIZE];
}
