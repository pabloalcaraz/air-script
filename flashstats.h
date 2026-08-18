#pragma once
#include <Arduino.h>

// Contador de escrituras en NVS/EEPROM desde el arranque. Vive en RAM a
// proposito: es una foto de "cuanto se ha escrito desde que se encendio",
// no un historico. Persistirlo en flash para contar escrituras en flash
// seria el mismo problema que se quiere vigilar.

void contarEscrituraFlash(uint32_t cuantas = 1);
uint32_t escriturasFlash();
