#pragma once
#include <Arduino.h>
#include "config.h"

// Log circular de eventos en RAM. Sustituye al monitor serie: lo que pasa
// queda aqui y se consulta desde la web sin cable.

enum NivelLog : uint8_t {
  LOG_INFO  = 0,
  LOG_AVISO = 1,
  LOG_ERROR = 2
};

struct Evento {
  uint32_t ts;     // epoch si hay NTP, si no segundos desde arranque
  NivelLog nivel;
  uint16_t repes;  // veces seguidas que se repitio el mismo mensaje
  char     msg[LOG_MSG_LEN];
};

void logEvento(NivelLog nivel, const char *fmt, ...);

uint16_t      logCount();       // eventos guardados (tope LOG_SIZE)
const Evento *logGet(uint16_t i);  // 0 = el mas antiguo; nullptr si i >= count

// Al llegar el NTP, reescribe las marcas anteriores (que eran segundos desde
// el arranque) para que no aparezcan fechadas en 1970.
void logAjustarTs(int32_t desfase);
