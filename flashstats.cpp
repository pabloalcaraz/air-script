#include "flashstats.h"

static uint32_t contador = 0;

void contarEscrituraFlash() { contador++; }
uint32_t escriturasFlash() { return contador; }
