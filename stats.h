#pragma once
#include <Arduino.h>
#include "config.h"
#include "history.h"

// Estadisticas derivadas del historial de RAM. Se calculan bajo demanda y no
// se cachean (salvo las medias de 24 h): recorrer 1440 muestras en un ESP32 a
// 240 MHz cuesta menos de un milisegundo, y una cache seria una copia mas que
// mantener sincronizada para nada.

struct EstadUna {
  bool     hay;         // false si no hubo ni un dato valido en la ventana
  uint16_t n;           // muestras validas usadas
  float    min, max, media, p95;   // NAN si !hay
  float    tendencia;   // unidades por minuto; NAN si no calculable
  bool     hayNivel;    // false para magnitudes sin umbrales (PM1.0)
  uint16_t pctNivel[3]; // % de tiempo en OK / AVISO / MALO, en decimas de %
};

// El total de sinVentilar cuenta en minutos porque el historial guarda una
// muestra por minuto: dar segundos fingiria una precision que no existe.
#define SIN_VENTILAR_NUNCA 0xFFFF

struct Estadisticas {
  uint16_t ventanaMin;      // ventana pedida, en minutos
  uint16_t muestras;        // muestras del historial dentro de la ventana
  EstadUna pm1, pm25, pm10, co2, temp, hum;
  float    ach;             // renovaciones de aire por hora; NAN si no calculable
  uint16_t sinVentilar_min; // SIN_VENTILAR_NUNCA si el CO2 nunca bajo del umbral
};

void estadisticasCalcular(uint16_t ventanaMin, Estadisticas &out);

// Medias moviles de 24 h de particulas. Son las que hay que comparar con los
// limites de la OMS, y las unicas que se cachean: alertasCalcular() se ejecuta
// en cada vuelta del loop y no puede permitirse recorrer el historial entero.
struct Medias24 {
  bool  hayPm25, hayPm10;
  float pm25, pm10;
};
extern Medias24 medias24;

// Llamar justo despues de historialGuardar(): una vez por minuto, no mas.
void estadisticasRefrescarMedias();

float    estadisticasAch();          // NAN si no hay una bajada de CO2 utilizable
uint16_t estadisticasSinVentilar();  // minutos desde el ultimo aire renovado
