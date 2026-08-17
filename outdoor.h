#pragma once
#include <Arduino.h>
#include "config.h"

// Calidad del aire exterior via AQICN (estacion real) con Open-Meteo (modelo
// CAMS, rejilla 11 km) como reserva si AQICN no responde o no hay token.

// De donde vino el ultimo dato bueno: ninguna fuente aun, estacion real (AQICN), o modelo (Open-Meteo).
enum FuenteExterior : uint8_t { FUENTE_NINGUNA, FUENTE_AQICN, FUENTE_MODELO };

struct Exterior {
  bool     configurado;  // hay lat/lon guardadas en la NVS
  bool     valido;       // hay al menos un sondeo de particulas correcto
  bool     rancio;       // el ultimo dato bueno tiene mas de EXT_RANCIO_MS
  bool     hayMeteo;     // temp y humedad exteriores disponibles
  uint32_t ultimaMs;     // millis() del ultimo sondeo correcto
  uint32_t intentos;
  uint32_t fallos;
  int16_t  httpCode;     // ultimo codigo HTTP o error de la libreria
  char     lugar[EXT_LUGAR_LEN];
  float    lat, lon;
  float    pm25, pm10, o3, no2;
  float    temp, hum;
  int16_t  aqi;          // european_aqi (solo si fuente==FUENTE_MODELO), -1 si no vino
  FuenteExterior fuente; // de donde vino el ultimo dato bueno
  char     estacion[AQICN_ESTACION_LEN]; // nombre de estacion AQICN (solo si fuente==FUENTE_AQICN)
  float    distanciaKm;  // distancia estacion-coordenadas elegidas (solo AQICN)
};

// Carga la localizacion y el token de la NVS y arranca la tarea de red. Se
// llama una vez en setup(), despues de redInit().
void exteriorInit();

// Copia coherente del estado. El sondeo corre en otro nucleo: leer la struct
// global a pelo puede pillarla a medio escribir.
void exteriorSnapshot(Exterior &out);

// Guarda la localizacion en la NVS y fuerza un sondeo inmediato. Devuelve
// false si las coordenadas estan fuera de rango.
bool exteriorFijarLugar(float lat, float lon, const char *nombre);

// Guarda el token de AQICN en la NVS. Cadena vacia = "sin token", el sondeo
// vuelve a saltar directo al fallback de Open-Meteo. No dispara un sondeo
// inmediato por si sola (usa exteriorFijarLugar para eso). Devuelve false si
// el token no cabe en AQICN_TOKEN_LEN-1 bytes o si falla el acceso a la NVS.
bool exteriorFijarToken(const char *token);

// Atajo para la pantalla: hay dato que merezca ocupar una pagina del OLED.
bool exteriorUtil();
