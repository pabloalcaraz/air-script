#include <stdarg.h>
#include <string.h>
#include "eventlog.h"
#include "net.h"

static Evento  buf[LOG_SIZE];
static uint16_t cabeza = 0;  // siguiente hueco a escribir
static uint16_t total  = 0;  // cuantos hay guardados

static const char *etiqueta(NivelLog n) {
  switch (n) {
    case LOG_ERROR: return "ERROR";
    case LOG_AVISO: return "AVISO";
    default:        return "INFO ";
  }
}

void logEvento(NivelLog nivel, const char *fmt, ...) {
  char msg[LOG_MSG_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);

  // Antirrebote: un fallo en bucle llenaria las 80 entradas en minutos y
  // borraria todo lo util. Si el mensaje repite el anterior, solo cuenta.
  if (total > 0) {
    Evento &ult = buf[(cabeza + LOG_SIZE - 1) % LOG_SIZE];
    if (ult.nivel == nivel && strcmp(ult.msg, msg) == 0) {
      if (ult.repes < 65535) ult.repes++;
      ult.ts = (uint32_t)redAhora();
      return;
    }
  }

  Evento &e = buf[cabeza];
  e.ts    = (uint32_t)redAhora();
  e.nivel = nivel;
  e.repes = 1;
  strncpy(e.msg, msg, LOG_MSG_LEN - 1);
  e.msg[LOG_MSG_LEN - 1] = '\0';

  cabeza = (cabeza + 1) % LOG_SIZE;
  if (total < LOG_SIZE) total++;

  Serial.printf("[%s] %s\n", etiqueta(nivel), msg);
}

uint16_t logCount() { return total; }

void logAjustarTs(int64_t desfase) {
  for (uint16_t i = 0; i < total; i++) {
    uint16_t inicio = (total == LOG_SIZE) ? cabeza : 0;
    Evento &e = buf[(inicio + i) % LOG_SIZE];
    if (e.ts < TS_EPOCH_MIN) e.ts = (uint32_t)((int64_t)e.ts + desfase);
  }
}

const Evento *logGet(uint16_t i) {
  if (i >= total) return nullptr;
  // Cuando el buffer ya dio la vuelta, el mas antiguo esta justo en cabeza.
  uint16_t inicio = (total == LOG_SIZE) ? cabeza : 0;
  return &buf[(inicio + i) % LOG_SIZE];
}
