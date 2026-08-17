#pragma once
#include <Arduino.h>

// Traduce lecturas a niveles de alerta segun los umbrales de config.h.

enum Nivel : uint8_t {
  NIVEL_OK    = 0,
  NIVEL_AVISO = 1,
  NIVEL_MALO  = 2
};

struct Alertas {
  Nivel       pm25, pm10;   // instantaneos: "esta pasando algo ahora"
  Nivel       co2, temp, hum;
  // Los limites de la OMS 2021 para particulas son medias de 24 h, no valores
  // instantaneos. Freir dos minutos dispara el instantaneo y eso es ruido, no
  // exposicion. Se calculan los dos y cada uno se usa donde corresponde.
  Nivel       pm25_24h, pm10_24h;
  bool        hay24h;       // false mientras no haya historial suficiente
  Nivel       peor;         // el peor nivel de todos, para el resumen
  const char *peorQue;      // que magnitud lo provoca
};

extern Alertas alertas;

void alertasCalcular();          // recalcula a partir del estado de los sensores
const char *nivelMarca(Nivel n);  // "", " !", " !!"

// Clasificadores publicos: las estadisticas los reutilizan sobre el historico.
// Repetir alli los umbrales de config.h seria garantizar que algun dia dejen
// de coincidir sin que nadie se entere.
Nivel nivelPm25(float v);
Nivel nivelPm10(float v);
Nivel nivelCo2(float v);
Nivel nivelTemp(float v);  // siempre NIVEL_OK si ALERTA_TEMP vale 0
Nivel nivelHum(float v);
