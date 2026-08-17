#pragma once
#include <Arduino.h>
#include "config.h"

// Historial circular de 24 h en RAM, una muestra por minuto.
// No se persiste en flash a proposito: escribir cada minuto la desgastaria
// para un dato que solo interesa mientras el aparato esta encendido.

// Centinelas para sensor caido. Se guarda esto y NO un cero: un cero se
// dibujaria como medida real y ensuciaria la grafica; el centinela permite
// pintar un hueco.
#define NULO_U16 0xFFFF
#define NULO_I16 INT16_MIN

struct Muestra {
  uint32_t ts;     // epoch si hay NTP, si no segundos desde arranque
  uint16_t pm1;
  uint16_t pm25;
  uint16_t pm10;
  uint16_t co2;
  int16_t  tempD;  // decimas de grado: 234 = 23.4 C
  uint16_t humD;   // decimas de %:     452 = 45.2 %
};                 // 16 bytes -> 1440 muestras = 23.040 bytes

// Campos serializables de una muestra. El orden define el de la API y el de
// las estadisticas: tocarlo obliga a revisar los dos sitios.
enum Campo : uint8_t { C_PM1, C_PM25, C_PM10, C_CO2, C_TEMP, C_HUM, C_TOTAL };
extern const char *NOMBRE_CAMPO[C_TOTAL];

// Devuelve false si esa muestra no tiene el campo porque el sensor estaba
// caido ese minuto. Vive aqui y no en cada consumidor para que la web y las
// estadisticas no puedan discrepar sobre que es un hueco.
bool muestraValor(const Muestra *m, uint8_t campo, float &out);

void historialGuardar();  // toma una muestra del estado actual

uint16_t       historialCount();       // muestras validas (tope HIST_SIZE)
const Muestra *historialGet(uint16_t i);  // 0 = la mas antigua

// Igual que en el log: al sincronizar la hora se corrigen hacia atras las
// muestras tomadas mientras no la habia.
void historialAjustarTs(int32_t desfase);
