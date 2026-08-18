#include "flashstats.h"

static uint32_t contador = 0;

void contarEscrituraFlash(uint32_t cuantas) { contador += cuantas; }
uint32_t escriturasFlash() { return contador; }
