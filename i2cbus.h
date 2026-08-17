#pragma once
#include <Arduino.h>

// Bus I2C compartido por el OLED y el SCD41. El escaneo se hace una sola vez
// al arrancar y su resultado lo consultan ambos modulos, para que ninguno
// dependa del otro.

struct BusI2c {
  bool    oledEn3C;
  bool    oledEn3D;
  bool    scd41;
  uint8_t total;  // dispositivos que responden en todo el bus
};

extern BusI2c bus_i2c;

void i2cInit();      // Wire.begin con los pines de config.h
void i2cEscanear();  // recorre el bus, rellena bus_i2c y reporta por serie

// ACK real de una direccion concreta. Lo usa quien no puede fiarse del begin()
// de su libreria (ver pantallaInit()) o quien necesita resondear mas tarde,
// cuando el escaneo del arranque ya ha quedado obsoleto.
bool i2cResponde(uint8_t addr);
