#pragma once
#include <Arduino.h>

// Servidor HTTP y API JSON. Usa WebServer.h del core: sincrono y suficiente
// para los 1-3 clientes de una casa. Se descarto ESPAsyncWebServer por ser
// dependencia externa con mantenimiento irregular.

void servidorInit();     // arranca en PUERTO_HTTP
void servidorAtender();  // llamar en cada vuelta del loop; no bloquea
