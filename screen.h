#pragma once
#include <Arduino.h>

// OLED SSD1306. Si no arranca, el resto del sistema sigue funcionando y
// reportando por serie: la pantalla nunca bloquea.

extern bool    pantalla_ok;
extern uint8_t pantalla_addr;  // 0x3C, 0x3D o 0 si no se encontro

bool pantallaInit();       // llamar despues de i2cEscanear()
void pantallaRefrescar();  // dibuja lecturas + alertas
void pantallaMensaje(const char *titulo, const char *l1, const char *l2);
