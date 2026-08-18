#pragma once

// Constantes de todo el proyecto. Un solo sitio que tocar para ajustar
// pines, umbrales o ritmos.

// ---- Pines ----
#define SDA_PIN 21
#define SCL_PIN 22
#define PMS_RX  16  // ESP32 RX2 <- PMS TXD
#define PMS_TX  17  // ESP32 TX2 -> PMS RXD
#define PMS_SET_PIN 25  // ESP32 -> PMS SET (duty-cycle, Fase E)

// ---- Direcciones I2C ----
#define OLED_ADDR     0x3C
#define OLED_ADDR_ALT 0x3D
#define SCD41_ADDR    0x62

// ---- OLED ----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// ---- Intervalos (ms) ----
#define INT_SCD41      5000   // intervalo minimo del sensor
// Mas rapido que cualquier consumidor de `alertas` (pantalla y serie van a
// 1 Hz) para que ninguno lea un veredicto de la vuelta anterior.
#define INT_ALERTAS 500
#define INT_PANTALLA   1000
#define INT_SERIE      1000
#define INT_MUESTRA    60000  // una muestra al historial por minuto (Fase 3)
#define INT_RED        30000  // chequeo de reconexion WiFi (Fase 2)
// Sin frame durante esto = sensor caido. Con duty-cycle (Fase E) solo se
// evalua despierto: dormido, el silencio es el comportamiento normal.
#define PMS_TIMEOUT_MS 10000
// Margen de cortesia tras arrancar. Pasado esto, un sensor que sigue sin dar
// ni un dato se considera averiado aunque no haya devuelto ningun error.
#define ARRANQUE_MS 30000

// ---- SCD41: vigilancia y recuperacion (Fase A) ----
// El sensor entrega una medida cada 5 s. Seis intervalos sin una sola lectura
// valida no es lentitud: es que ha dejado de medir. Sin este control, un SCD41
// atascado en "dato no listo" deja el ultimo valor bueno congelado en pantalla
// y en la web durante horas, con salud OK, y nadie se entera.
#define SCD_TIMEOUT_MS   30000
// Reintento de recuperacion. Mas agresivo no ayuda: la propia secuencia de
// arranque del sensor tarda ~1 s y encadenarlas solo lo deja mas colgado.
#define SCD_REINTENTO_MS 60000
// CO2, temperatura y humedad con el valor EXACTO repetido tanto tiempo es
// fisicamente imposible: el ruido propio del sensor ya mueve el ultimo digito.
// Que coincidan las tres a la vez descarta que sea un ambiente estable.
#define SCD_CONGELADO_MS 600000
// Altitud del emplazamiento. Sin esto el sensor asume 1013 hPa (nivel del mar);
// a 650 m la presion real ronda 940 hPa y el error de CO2 es del 5-7 %.
#define ALTITUD_M 650
// Marcador de "autotest no ejecutado". El datasheet usa 0 para sensor sano,
// asi que el valor de "sin dato" tiene que ser otro.
#define SELFTEST_SIN_HACER 0xFFFF

// ---- Umbrales de alerta ----
// Nivel 0 = normal, 1 = aviso, 2 = malo.
// PM2.5 y PM10: guias OMS 2021, media 24 h.
#define PM25_AVISO 15
#define PM25_MALO  35
#define PM10_AVISO 45
#define PM10_MALO  100
// CO2: 800 ppm = objetivo de ventilacion; por encima de 1200 el aire esta viciado.
#define CO2_AVISO  800
#define CO2_MALO   1200
// Temperatura: SIN alerta a proposito. En verano una casa a 28 C no es un
// problema de calidad del aire, es verano: avisar de ello solo genera ruido
// que tapa los avisos que si importan. Se mide, se pinta y se guarda igual.
// Poner a 1 para reactivarla; los umbrales de abajo siguen definidos.
#define ALERTA_TEMP 0

// Franja de confort (RITE), usada solo si ALERTA_TEMP vale 1.
// Ojo: el SCD41 lee 2-5 C alto si no se calibra su offset (fabrica: 4.0 C).
#define TEMP_OK_MIN   18.0f
#define TEMP_OK_MAX   27.0f
#define TEMP_MALO_MIN 15.0f
#define TEMP_MALO_MAX 30.0f
// Humedad: <30 reseca vias, >60 favorece moho y acaros.
#define HUM_OK_MIN   30.0f
#define HUM_OK_MAX   60.0f
#define HUM_MALO_MIN 20.0f
#define HUM_MALO_MAX 70.0f

// ---- Estadisticas (Fase B) ----
// CO2 de fondo atmosferico. No lo mide ningun servicio publico porque apenas
// varia: ~430 ppm de fondo global, 450-550 en ciudad. Se usa como asintota del
// decaimiento al calcular la renovacion de aire, no como una medida.
#define CO2_EXTERIOR_PPM 430
// Por debajo de esto el aire se considera renovado.
#define CO2_VENTILADO 600
// Ventana de la tendencia, en muestras (el historial guarda una por minuto).
#define TEND_MUESTRAS 30
// Minimo de puntos para trazar una recta. Con menos, la pendiente la decide el
// ruido y la flecha apuntaria a cualquier lado.
#define TEND_MIN_PUNTOS 5
// Muestras minimas para dar por buena una media de 24 h. Con media hora de
// datos, la "media diaria" es la lectura de ahora con otro nombre, y un pico
// de cocina se convertiria en una superacion del limite de exposicion.
#define MEDIA24_MIN_MUESTRAS 60

// ---- ACH: renovaciones de aire por hora (Fase B) ----
// Tramo minimo de bajada de CO2 para fiarse del ajuste. Por debajo de 10 min
// el ruido del sensor (+-10 ppm) domina la pendiente.
#define ACH_MIN_MUESTRAS 10
// Repunte admitido dentro de un tramo "descendente". Sin esta holgura, una
// subida de 1 ppm partiria la bajada en trozos demasiado cortos para servir.
#define ACH_TOLERANCIA 12
// Salto minimo sobre el fondo exterior al empezar la bajada. Ir de 500 a 480
// no dice nada de la ventilacion: el margen de error se come la señal.
#define ACH_SALTO_MIN 200
// Bajada real minima a lo largo del tramo. Un tramo plano cumple la condicion
// de "no sube", pero no es una ventilacion.
#define ACH_BAJADA_MIN 60
// Calidad minima del ajuste. Por debajo, la curva no es una exponencial y el
// numero seria inventado: mejor no dar ninguno.
#define ACH_R2_MIN 0.80f
// Techo de cordura. Mas de 20 renovaciones por hora en una vivienda es un
// artefacto del ajuste, no una casa con las ventanas abiertas.
#define ACH_MAX 20.0f

// ---- Aire exterior (Fase D) ----
// CAMS actualiza el modelo dos veces al dia con valores horarios: pedirlo mas
// a menudo gasta un handshake TLS para releer el mismo numero.
#define INT_EXTERIOR    1800000UL  // 30 min
// Reintento tras un fallo. Media hora de castigo porque el router estaba
// reiniciandose seria desproporcionado.
#define EXT_REINTENTO_MS 120000UL
// Sin dato nuevo en 2 h el anterior deja de describir el aire de ahora.
#define EXT_RANCIO_MS   7200000UL
#define EXT_TIMEOUT_MS  8000
// El handshake TLS reserva ~45 KB de golpe. Si no los hay, no se intenta:
// quedarse sin heap tumbaria el servidor web, que si es imprescindible.
#define EXT_HEAP_MIN    60000
#define TLS_BLOQUE_MIN  45000
#define EXT_RESPUESTA_MAX 49152
// Tiempo maximo que una tarea HTTPS espera a que la otra libere mbedTLS. Si
// vence, reintenta luego: bloquear una tarea es preferible a agotar el heap.
#define RED_TLS_ESPERA_MS 1000
// mbedtls necesita holgura de pila. Con 4096 el handshake la desborda y el
// panic sale como "Stack canary watchpoint triggered", que no dice nada.
#define EXT_TAREA_STACK 8192
#define EXT_LUGAR_LEN   40
// Misma NVS donde WiFiManager guarda las credenciales, otro namespace. Son
// pocos bytes y solo se escriben cuando el usuario cambia de sitio.
#define EXT_NVS_NS      "airscript"

// ---- AQICN (Fase F) ----
// Estacion real mas cercana a unas coordenadas. Token gratis, registro por
// email en aqicn.org/data-platform/register. Sin token configurado, se salta
// directo al fallback de Open-Meteo.
#define AQICN_TOKEN_LEN  40
#define AQICN_TIMEOUT_MS 8000
#define AQICN_ESTACION_LEN 64
// El geo: de AQICN devuelve la estacion mas cercana DE SU RED con dato fresco,
// no la fisicamente mas cercana: donde la cobertura es pobre puede caer a una
// a cientos de km. Enseñar el aire de una estacion a 250 km como si fuera el de
// tu calle es mentir. Por encima de este radio se descarta AQICN y se cae al
// modelo Open-Meteo (rejilla 11 km en TUS coordenadas), mas honesto que una
// estacion real lejana.
#define AQICN_MAX_KM 30.0f

// ---- Paginacion del OLED (Fase D) ----
// 8 s: menos cansa la vista, mas y toca esperar demasiado a la pagina que
// quieres cuando pasas por delante del aparato.
#define INT_PAGINA 8000

// Anti burn-in. Los SSD1306 queman los pixeles que llevan horas encendidos, y
// este panel pinta etiquetas fijas las 24 h. Cada 10 min todo el contenido se
// desplaza 1 px en un ciclo de 2 posiciones: suficiente para repartir el
// desgaste, poco para que se note. Con 2 px la ultima linea del cuerpo se
// saldria del panel de 64 px de alto; con 1 cabe justa.
#define INT_ANTIQUEMADO 600000UL
#define ANTIQUEMADO_PX  1

// ---- Duty-cycle del PMS5003 (Fase E) ----
// Tiempo que el ventilador tarda en estabilizar el caudal tras despertar.
// Los frames de este tramo son basura y se descartan a proposito: es el
// punto que estropea la mayoria de implementaciones caseras de duty-cycle.
#define PMS_CALENTAR_MS 30000
// Ventana de frames que se promedian justo despues del calentamiento.
#define PMS_PROMEDIO_MS 5000

// ---- Red (Fase 2) ----
#define HOSTNAME   "air"  // -> http://air.local
#define AP_SSID    "AirScript-Setup"
#define TZ_MADRID  "CET-1CEST,M3.5.0,M10.5.0/3"
#define NTP_SERVER "pool.ntp.org"
#define PORTAL_TIMEOUT_S 180  // sin esto el aparato queda atrapado en modo AP

// ---- Historial (Fase 3) ----
#define HIST_SIZE       1440  // 24 h a una muestra por minuto
#define HIST_MAX_PUNTOS 240   // maximo que devuelve la API tras downsampling
// Un hueco mayor no se interpola como si las muestras siguieran separadas un
// minuto. Deja margen para operaciones puntuales como el autotest del SCD41.
#define HIST_HUECO_MAX_S 90UL

// ---- Log de eventos (Fase 3) ----
#define LOG_SIZE    80
#define LOG_MSG_LEN 56
#define INT_MONITOR 2000   // cada cuanto se buscan cambios de estado
// Frontera para distinguir un epoch real de "segundos desde el arranque":
// 1e9 son ~33 anios, asi que cualquier ts por debajo no viene de NTP.
#define TS_EPOCH_MIN 1000000000UL
#define HEAP_MINIMO 20000  // por debajo de esto el aparato esta en apuros

// ---- Servidor web (Fase 4) ----
#define PUERTO_HTTP 80

// ---- Backup en la nube (Fase 2) ----
// Mismo ritmo que historialGuardar(): un intento de envio por minuto.
#define INT_CLOUD          60000UL
// Drenaje de la cola cuando hay red: 1/s para no ráfagar el POST ni la
// cuota gratuita del servicio al reconectar tras un corte largo.
#define INT_CLOUD_DRENAJE   1000UL
// ~2 h de margen a una muestra por minuto. 120 x 16 B = 1920 B de RAM,
// nada frente a los ~240 KB libres para locales.
#define CLOUD_BUFFER         120
#define CLOUD_URL_LEN         160
#define CLOUD_USER_LEN         24
// 170 caracteres utiles + '\0'. Los tokens actuales de Grafana Cloud pueden
// superar el limite antiguo de 99 caracteres.
#define CLOUD_TOKEN_LEN       171
#define CLOUD_TIMEOUT_MS      8000
#define CLOUD_TAREA_STACK      8192
#define CLOUD_TAREA_POLL_MS    1000UL
#define CLOUD_REINTENTO_MIN_MS 5000UL
#define CLOUD_REINTENTO_MAX_MS 300000UL
#define CLOUD_HEAP_MIN         60000
// El backup usa credenciales de escritura. Para no enviarlas a un destino
// arbitrario, solo se aceptan endpoints HTTPS oficiales de Grafana Cloud.
#define CLOUD_HOST_SUFFIX  ".grafana.net"
// Misma NVS que outdoor.cpp: son pocos bytes y solo se escriben cuando el
// usuario cambia la config desde la web, no de forma periodica.
#define CLOUD_NVS_NS       "airscript"
