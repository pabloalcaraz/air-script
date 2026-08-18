#include "config.h"
#include "sensors.h"
#include "alerts.h"
#include "stats.h"

Alertas alertas = {NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK,
                   NIVEL_OK, NIVEL_OK, false, NIVEL_OK, ""};

// Magnitudes donde solo molesta pasarse por arriba (particulas, CO2).
static Nivel nivelAlto(float v, float aviso, float malo) {
  if (v >= malo)  return NIVEL_MALO;
  if (v >= aviso) return NIVEL_AVISO;
  return NIVEL_OK;
}

// Magnitudes con franja buena en el medio (temperatura, humedad).
static Nivel nivelFranja(float v, float okMin, float okMax,
                         float maloMin, float maloMax) {
  if (v <= maloMin || v >= maloMax) return NIVEL_MALO;
  if (v < okMin || v > okMax)       return NIVEL_AVISO;
  return NIVEL_OK;
}

Nivel nivelPm25(float v) { return nivelAlto(v, PM25_AVISO, PM25_MALO); }
Nivel nivelPm10(float v) { return nivelAlto(v, PM10_AVISO, PM10_MALO); }
Nivel nivelCo2(float v)  { return nivelAlto(v, CO2_AVISO,  CO2_MALO);  }
Nivel nivelHum(float v) {
  return nivelFranja(v, HUM_OK_MIN, HUM_OK_MAX, HUM_MALO_MIN, HUM_MALO_MAX);
}
Nivel nivelTemp(float v) {
#if ALERTA_TEMP
  return nivelFranja(v, TEMP_OK_MIN, TEMP_OK_MAX, TEMP_MALO_MIN, TEMP_MALO_MAX);
#else
  (void)v;
  return NIVEL_OK;
#endif
}

static void registrarPeor(Nivel n, const char *nombre) {
  if (n > alertas.peor) {
    alertas.peor = n;
    alertas.peorQue = nombre;
  }
}

const char *nivelMarca(Nivel n) {
  if (n == NIVEL_MALO)  return " !!";
  if (n == NIVEL_AVISO) return " !";
  return "";
}

void alertasCalcular() {
  alertas = {NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK, NIVEL_OK,
             NIVEL_OK, NIVEL_OK, false, NIVEL_OK, ""};

  // Los niveles de 24 h se calculan siempre que haya historial: son
  // informacion por si solos y la API los publica aunque el sensor este caido.
  alertas.hay24h = medias24.hayPm25 || medias24.hayPm10;
  if (medias24.hayPm25) alertas.pm25_24h = nivelPm25(medias24.pm25);
  if (medias24.hayPm10) alertas.pm10_24h = nivelPm10(medias24.pm10);

  // Sin dato valido no hay alerta: un sensor caido no es "aire limpio", pero
  // tampoco puede seguir emitiendo veredictos con datos de hace tres horas.
  if (saludPms() == SALUD_OK) {
    alertas.pm25 = nivelPm25(pms_estado.pm25);
    alertas.pm10 = nivelPm10(pms_estado.pm10);
    // El resumen superior describe el aire de ahora, igual que las tarjetas.
    // La exposicion de 24 h se conserva aparte en Estadisticas: mezclarla aqui
    // dejaba "MALO: PM2.5" pegado aunque la lectura actual ya fuese correcta.
    registrarPeor(alertas.pm25, "PM2.5");
    registrarPeor(alertas.pm10, "PM10");
  }

  if (saludScd() == SALUD_OK) {
    alertas.co2 = nivelCo2(scd_estado.co2);
    alertas.hum = nivelHum(scd_estado.hum);
    registrarPeor(alertas.co2, "CO2");
    registrarPeor(alertas.hum, "HUM");
#if ALERTA_TEMP
    alertas.temp = nivelTemp(scd_estado.temp);
    registrarPeor(alertas.temp, "TEMP");
#endif
  }
}
