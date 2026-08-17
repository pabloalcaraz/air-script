#include <math.h>
#include <stdlib.h>
#include "stats.h"
#include "alerts.h"

Medias24 medias24 = {false, false, NAN, NAN};

// Buffer de trabajo para ordenar y sacar el percentil. Se reutiliza magnitud a
// magnitud: se calculan de una en una, asi que seis copias simultaneas no
// harian falta y costarian 35 KB de RAM permanentes.
static float scratch[HIST_SIZE];

static int cmpFloat(const void *a, const void *b) {
  float x = *(const float *)a, y = *(const float *)b;
  return (x > y) - (x < y);
}

// Que magnitudes tienen umbral. PM1.0 no lo tiene en ninguna guia: pintarle un
// "OK" seria emitir un veredicto que nadie ha definido.
static bool tieneNivel(uint8_t campo) {
  if (campo == C_PM1) return false;
  if (campo == C_TEMP) return ALERTA_TEMP != 0;
  return true;
}

static Nivel nivelDe(uint8_t campo, float v) {
  switch (campo) {
    case C_PM25: return nivelPm25(v);
    case C_PM10: return nivelPm10(v);
    case C_CO2:  return nivelCo2(v);
    case C_TEMP: return nivelTemp(v);
    case C_HUM:  return nivelHum(v);
    default:     return NIVEL_OK;
  }
}

// Pendiente de los ultimos TEND_MUESTRAS minutos por minimos cuadrados, en
// unidades por minuto. Se usa el indice como eje X porque el historial guarda
// exactamente una muestra por minuto: el ts real no cambiaria el resultado y
// obligaria a manejar el caso de "aun no hay hora NTP".
static float tendencia(uint8_t campo, uint16_t total) {
  uint16_t desde = total > TEND_MUESTRAS ? total - TEND_MUESTRAS : 0;
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  uint16_t n = 0;
  for (uint16_t i = desde; i < total; i++) {
    float v;
    if (!muestraValor(historialGet(i), campo, v)) continue;
    double x = (double)i;
    sx += x; sy += v; sxx += x * x; sxy += x * v; n++;
  }
  if (n < TEND_MIN_PUNTOS) return NAN;
  double den = (double)n * sxx - sx * sx;
  if (fabs(den) < 1e-9) return NAN;
  return (float)(((double)n * sxy - sx * sy) / den);
}

static void unaMagnitud(uint8_t campo, uint16_t desde, uint16_t total,
                        EstadUna &e) {
  e.hay = false;
  e.n = 0;
  e.min = e.max = e.media = e.p95 = e.tendencia = NAN;
  e.hayNivel = tieneNivel(campo);
  e.pctNivel[0] = e.pctNivel[1] = e.pctNivel[2] = 0;

  uint16_t cuenta = 0, porNivel[3] = {0, 0, 0};
  double suma = 0;
  for (uint16_t i = desde; i < total; i++) {
    float v;
    // Las muestras con centinela no cuentan en ningun sitio: sumarlas como
    // cero inventaria aire limpio donde solo hubo un sensor caido.
    if (!muestraValor(historialGet(i), campo, v)) continue;
    scratch[cuenta++] = v;
    suma += v;
    if (e.hayNivel) porNivel[nivelDe(campo, v)]++;
  }
  if (!cuenta) return;

  e.hay = true;
  e.n = cuenta;
  e.media = (float)(suma / cuenta);

  qsort(scratch, cuenta, sizeof(float), cmpFloat);
  e.min = scratch[0];
  e.max = scratch[cuenta - 1];
  // Percentil interpolado, el mismo metodo que usan las hojas de calculo. Con
  // pocas muestras, coger el elemento del indice redondeado se desvia lo
  // bastante como para que los dos numeros no cuadren al compararlos.
  float pos = 0.95f * (cuenta - 1);
  uint16_t k = (uint16_t)pos;
  e.p95 = (k + 1 < cuenta) ? scratch[k] + (pos - k) * (scratch[k + 1] - scratch[k])
                           : scratch[k];

  if (e.hayNivel)
    for (uint8_t n = 0; n < 3; n++)
      e.pctNivel[n] = (uint16_t)lroundf(1000.0f * porNivel[n] / cuenta);

  e.tendencia = tendencia(campo, total);
}

void estadisticasCalcular(uint16_t ventanaMin, Estadisticas &out) {
  uint16_t total = historialCount();
  uint16_t n     = total < ventanaMin ? total : ventanaMin;
  uint16_t desde = total - n;

  out.ventanaMin = ventanaMin;
  out.muestras   = n;

  EstadUna *dest[C_TOTAL] = {&out.pm1, &out.pm25, &out.pm10,
                             &out.co2, &out.temp, &out.hum};
  for (uint8_t c = 0; c < C_TOTAL; c++) unaMagnitud(c, desde, total, *dest[c]);

  // ACH y "sin ventilar" miran siempre las 24 h enteras: la ultima ventilacion
  // puede ser de hace ocho horas y seguir siendo el dato relevante.
  out.ach = estadisticasAch();
  out.sinVentilar_min = estadisticasSinVentilar();
}

void estadisticasRefrescarMedias() {
  uint16_t total = historialCount();
  double sPm25 = 0, sPm10 = 0;
  uint16_t nPm25 = 0, nPm10 = 0;
  for (uint16_t i = 0; i < total; i++) {
    const Muestra *m = historialGet(i);
    float v;
    if (muestraValor(m, C_PM25, v)) { sPm25 += v; nPm25++; }
    if (muestraValor(m, C_PM10, v)) { sPm10 += v; nPm10++; }
  }
  medias24.hayPm25 = nPm25 >= MEDIA24_MIN_MUESTRAS;
  medias24.hayPm10 = nPm10 >= MEDIA24_MIN_MUESTRAS;
  medias24.pm25 = nPm25 ? (float)(sPm25 / nPm25) : NAN;
  medias24.pm10 = nPm10 ? (float)(sPm10 / nPm10) : NAN;
}

uint16_t estadisticasSinVentilar() {
  uint16_t total = historialCount();
  for (uint16_t i = total; i > 0; i--) {
    float v;
    if (!muestraValor(historialGet(i - 1), C_CO2, v)) continue;
    if (v < CO2_VENTILADO) return total - i;  // minutos desde esa muestra
  }
  return SIN_VENTILAR_NUNCA;
}

// Renovaciones de aire por hora a partir del decaimiento del CO2:
//   C(t) = C_ext + (C_0 - C_ext) * e^(-ACH * t)
// Tomando logaritmos, ln(C - C_ext) es una recta de pendiente -ACH. Se busca
// la bajada sostenida mas reciente y se ajusta por minimos cuadrados.
float estadisticasAch() {
  uint16_t total = historialCount();
  if (total < ACH_MIN_MUESTRAS) return NAN;

  // Se recorre hacia delante y se conserva el ultimo tramo valido: interesa la
  // ventilacion de hace diez minutos, no la de hace veinte horas.
  int32_t iniRun = -1, prevIdx = -1, mejorIni = -1, mejorFin = -1;
  float prev = 0;
  for (uint16_t i = 0; i < total; i++) {
    float v;
    if (!muestraValor(historialGet(i), C_CO2, v)) {
      // Un hueco corta el tramo: no se puede afirmar que el CO2 siguiera
      // bajando mientras el sensor estaba caido.
      if (iniRun >= 0 && prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) {
        mejorIni = iniRun; mejorFin = prevIdx;
      }
      iniRun = -1; prevIdx = -1;
      continue;
    }
    if (prevIdx < 0) {
      iniRun = i;
    } else if (v > prev + ACH_TOLERANCIA) {
      // Repunte por encima del ruido: el tramo termina aqui.
      if (prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) {
        mejorIni = iniRun; mejorFin = prevIdx;
      }
      iniRun = i;
    }
    prev = v; prevIdx = i;
  }
  if (iniRun >= 0 && prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) {
    mejorIni = iniRun; mejorFin = prevIdx;
  }
  if (mejorIni < 0) return NAN;

  double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
  uint16_t n = 0;
  float c0 = 0, cFin = 0;
  for (int32_t i = mejorIni; i <= mejorFin; i++) {
    float v;
    if (!muestraValor(historialGet((uint16_t)i), C_CO2, v)) continue;
    float exceso = v - CO2_EXTERIOR_PPM;
    // Por debajo del fondo atmosferico el logaritmo no existe. Pasa con el
    // ruido del sensor cuando el aire ya esta renovado: a partir de ahi el
    // tramo no aporta informacion sobre la velocidad de renovacion.
    if (exceso < 1.0f) break;
    if (!n) c0 = v;
    cFin = v;
    double x = (double)(i - mejorIni) / 60.0;  // horas
    double y = log(exceso);
    sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y; n++;
  }
  if (n < ACH_MIN_MUESTRAS) return NAN;
  if (c0 - CO2_EXTERIOR_PPM < ACH_SALTO_MIN) return NAN;
  // Un tramo plano cumple "no sube" pero no es una ventilacion: sin esta
  // comprobacion, una noche de aire estancado devolveria un ACH de 0,02 con
  // pinta de dato bueno.
  if (c0 - cFin < ACH_BAJADA_MIN) return NAN;

  double den  = (double)n * sxx - sx * sx;
  double denY = (double)n * syy - sy * sy;
  if (den < 1e-12 || denY < 1e-12) return NAN;
  double num = (double)n * sxy - sx * sy;
  double r2  = (num * num) / (den * denY);
  if (r2 < ACH_R2_MIN) return NAN;

  float ach = (float)(-num / den);
  if (ach <= 0.0f || ach > ACH_MAX) return NAN;
  return ach;
}
