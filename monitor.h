#pragma once
#include <Arduino.h>

// Vigilante del sistema. Compara el estado actual con el anterior y solo
// escribe en el log cuando algo CAMBIA. Sin esto, un sensor averiado
// generaria un evento cada 5 s y borraria el historial util en minutos.

void monitorInit();     // registra arranque, motivo del reset y escaneo I2C
void monitorRevisar();  // llamar en el loop; detecta transiciones

const char *motivoReset();  // "POWERON", "BROWNOUT", "PANIC"... para /api/health
