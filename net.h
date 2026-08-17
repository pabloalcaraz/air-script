#pragma once
#include <Arduino.h>
#include <time.h>

// WiFi, mDNS y hora. La red es un extra: si no hay, el aparato sigue midiendo
// y pintando en el OLED. Nada del camino de medicion espera por la conexion.

struct EstadoRed {
  bool     conectado;
  bool     horaOk;       // NTP sincronizado
  bool     portalAbierto;
  char     ip[16];
  char     ssid[33];
  int32_t  rssi;
  uint32_t reconexiones;
};

extern EstadoRed red_estado;

void   redInit();       // WiFiManager + mDNS + NTP
void   redComprobar();  // cada INT_RED: reconexion, RSSI y reintento de NTP
time_t redAhora();      // epoch si hay NTP; si no, segundos desde arranque
