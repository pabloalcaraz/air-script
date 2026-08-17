#pragma once
#include <Arduino.h>
#include "config.h"
#include "history.h"  // reutiliza el struct Muestra y sus centinelas NULO_*

// Backup opcional en Grafana Cloud (Influx Line Protocol). Cola circular en
// RAM que amortigua cortes de WiFi -- NO cortes de alimentacion: no se
// persiste en flash a proposito, mismo criterio que el historial de 24 h.

void cloudInit();     // carga config de NVS, arranca "apagado" si no hay nada

// Llamadas periodicas desde loop(), con sus propios temporizadores en
// air-script.ino (igual que historialGuardar()/alertasCalcular()).
void cloudTick();     // cada INT_CLOUD: toma una muestra, intenta enviarla
void cloudDrenar();   // cada INT_CLOUD_DRENAJE: vacia la cola si hay red

// Config desde la web. Solo admite HTTPS bajo *.grafana.net. Si cambian URL o
// usuario hay que proporcionar tambien un token nuevo: nunca se reutiliza una
// credencial de escritura contra un destino distinto.
bool cloudConfigurar(const char *url, const char *usuario, const char *token,
                     bool activo);
const char *cloudErrorConfig();

// Snapshot para /api/cloud/estado. NO incluye el token: no tiene sentido
// devolverselo al navegador que ya lo mando, y evita que quede en un log
// de red o en el historial del navegador.
struct CloudEstado {
  bool activo;
  bool configurado;   // hay URL+usuario+token guardados
  bool hayToken;       // hay token guardado (para pintar "ya tienes uno" en la web)
  uint16_t enCola;     // muestras pendientes de enviar
  uint32_t enviosOk;
  uint32_t enviosFallo;
  uint32_t ultimoEnvioMs;  // 0 = nunca
  char url[CLOUD_URL_LEN];
  char usuario[CLOUD_USER_LEN];
};
void cloudSnapshot(CloudEstado &out);
