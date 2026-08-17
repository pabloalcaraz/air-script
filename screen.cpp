#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include "config.h"
#include "i2cbus.h"
#include "sensors.h"
#include "alerts.h"
#include "outdoor.h"
#include "screen.h"

bool    pantalla_ok   = false;
uint8_t pantalla_addr = 0;

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pinta una linea con su marca de nivel: " !" para aviso, " !!" para malo.
// Sin video inverso: la marca ya distingue el nivel y el fondo blanco robaba
// legibilidad a un panel de 128x64 donde cada pixel cuenta.
static void pintarLinea(Nivel n, const char *txt) {
  display.print(txt);
  display.print(nivelMarca(n));
  display.println();
}

// Direccion del panel, o 0 si no hay ninguno. Se resondea aqui en vez de leer
// bus_i2c porque el escaneo del arranque pudo pillar al modulo aun despertando,
// y porque asi este modulo no depende del orden de llamadas de setup().
static uint8_t buscarPanel() {
  if (i2cResponde(OLED_ADDR))     return OLED_ADDR;
  if (i2cResponde(OLED_ADDR_ALT)) return OLED_ADDR_ALT;
  return 0;
}

bool pantallaInit() {
  // Adafruit_SSD1306::begin() NO comprueba el ACK: solo falla si no consigue
  // el malloc del framebuffer, asi que devuelve true con el panel desconectado
  // y su propio reintento en 0x3D nunca llega a ejecutarse. Sin este sondeo
  // previo el log anunciaba "OLED OK en 0x3C" sin nada en el bus, que es
  // justo la mentira mas cara cuando lo que estas depurando es el hardware.
  pantalla_addr = buscarPanel();
  if (!pantalla_addr) {
    pantalla_ok = false;
    Serial.println("[OLED] NO responde en 0x3C ni 0x3D. Sigo sin pantalla.");
    return false;
  }

  pantalla_ok = display.begin(SSD1306_SWITCHCAPVCC, pantalla_addr);
  if (!pantalla_ok) {
    pantalla_addr = 0;
    Serial.println("[OLED] responde en I2C pero begin() fallo (sin memoria).");
    return false;
  }

  Serial.printf("[OLED] OK en 0x%02X\n", pantalla_addr);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  // Sin wrap: lo que no cabe se corta en seco en vez de saltar de linea y
  // pisar lo de abajo. Es la red de seguridad de recortarAscii(); ninguna
  // linea fija del OLED pasa de 21 columnas (verificado en test/screen.test.cjs).
  display.setTextWrap(false);
  display.setCursor(0, 0);
  display.println("Iniciando...");
  display.display();
  return true;
}

void pantallaMensaje(const char *titulo, const char *l1, const char *l2) {
  if (!pantalla_ok) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(titulo);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
  display.setCursor(0, 18);
  if (l1) display.println(l1);
  if (l2) display.println(l2);
  display.display();
}

// Ancho de la fuente clasica de GFX: 5 px de glifo + 1 de separacion. A
// tamano 1 caben 128/6 = 21 columnas por linea.
#define OLED_CHAR_W 6
#define OLED_COLS   (SCREEN_WIDTH / OLED_CHAR_W)

// La fuente clasica es CP437, no UTF-8: una tilde son 2 bytes y saldrian como
// dos glifos aleatorios. Se pliega a ASCII SOLO aqui, al pintar; en la NVS y
// en la web el nombre sigue completo y con tildes.
static char plegarByte(const unsigned char *s, uint8_t &consumidos) {
  unsigned char c = s[0];
  if (c < 0x80) { consumidos = 1; return (c >= 0x20) ? (char)c : ' '; }
  // Solo se traduce Latin-1 suplementario (2 bytes, 0xC3 0x8x-0xBx), que es
  // donde viven todas las vocales acentuadas y la enye del castellano.
  if (c == 0xC3 && s[1]) {
    consumidos = 2;
    switch (s[1]) {
      case 0xA1: case 0xA0: case 0xA4: case 0xA2: return 'a';
      case 0x81: case 0x80: case 0x84: case 0x82: return 'A';
      case 0xA9: case 0xA8: case 0xAB: case 0xAA: return 'e';
      case 0x89: case 0x88: case 0x8B: case 0x8A: return 'E';
      case 0xAD: case 0xAC: case 0xAF: case 0xAE: return 'i';
      case 0x8D: case 0x8C: case 0x8F: case 0x8E: return 'I';
      case 0xB3: case 0xB2: case 0xB6: case 0xB4: return 'o';
      case 0x93: case 0x92: case 0x96: case 0x94: return 'O';
      case 0xBA: case 0xB9: case 0xBC: case 0xBB: return 'u';
      case 0x9A: case 0x99: case 0x9C: case 0x9B: return 'U';
      case 0xB1: return 'n';
      case 0x91: return 'N';
      case 0xA7: return 'c';
      case 0x87: return 'C';
      default:   return '?';
    }
  }
  // Cualquier otro multibyte (cirilico, CJK, emoji): se salta la secuencia
  // entera y se pinta un solo '?'. Pintar los bytes sueltos daria glifos
  // aleatorios de CP437 y ademas descuadraria la cuenta de columnas.
  consumidos = 1;
  while (s[consumidos] && ((s[consumidos] & 0xC0) == 0x80)) consumidos++;
  return '?';
}

// Copia `src` a `dst` plegado a ASCII y recortado a `max` glifos. Si no cabe,
// deja max-2 y anade ".." (la fuente clasica no tiene glifo de elipsis). Se
// quitan espacios y comas del final antes de pegar los puntos: "Delicias, .."
// se lee peor que "Delicias..".
static void recortarAscii(const char *src, char *dst, size_t max) {
  if (max == 0) { dst[0] = '\0'; return; }
  const unsigned char *s = (const unsigned char *)src;
  size_t j = 0;
  // Se llena hasta max+1 para saber si sobraba algo (y por tanto hay que
  // poner puntos) sin recorrer la cadena dos veces.
  while (*s && j <= max) {
    uint8_t n = 1;
    char c = plegarByte(s, n);
    dst[j++] = c;
    s += n;
  }
  if (j <= max) { dst[j] = '\0'; return; }

  size_t corte = (max > 2) ? max - 2 : 0;
  while (corte > 0) {
    char c = dst[corte - 1];
    if (c == ' ' || c == ',' || c == ';' || c == '.' || c == '-') corte--;
    else break;
  }
  dst[corte]     = '.';
  dst[corte + 1] = '.';
  dst[corte + 2] = '\0';
}

// Cabecera de pagina. Vuelve a existir desde que hay dos paginas: sin ella no
// hay forma de saber si el 12 de PM2.5 es el del salon o el de la ciudad.
// No es el resumen de alerta de antes: eso lo dice la marca " !" de cada linea.
static void cabecera(const char *nombre, const char *extra, uint8_t dy) {
  display.setCursor(0, dy);
  display.print(nombre);
  if (extra && *extra) {
    // Presupuesto real: las 21 columnas menos el titulo y el espacio. Sin
    // este recorte, GFX envuelve el texto, la raya de y=10 lo cruza y el
    // cuerpo de y=14 se le superpone.
    size_t titulo = strlen(nombre);
    size_t max = (titulo + 1 < OLED_COLS) ? OLED_COLS - titulo - 1 : 0;
    char corto[OLED_COLS + 1];
    recortarAscii(extra, corto, max);
    display.print(' ');
    display.print(corto);
  }
  display.drawLine(0, 10 + dy, SCREEN_WIDTH, 10 + dy, SSD1306_WHITE);
  display.setCursor(0, 14 + dy);  // 6 lineas de 8 px caben justas hasta y=62
}

static void paginaInterior(uint8_t dy) {
  char buf[26];
  // El "zzz" reutiliza el hueco de cabecera que ya usa EXTERIOR para el
  // nombre del sitio: distingue "esta descansando" (Fase E) de "esta roto",
  // que si sigue mostrando el aviso de mas abajo.
  cabecera("INTERIOR", pms_estado.durmiendo ? "zzz" : nullptr, dy);

  if (!pms_estado.valido) {
    if (saludPms() == SALUD_ESPERANDO) {
      display.println("PMS: calentando");
      display.println("~30s...");
    } else {
      display.println("PMS: sin frames");
      display.println("rev. GPIO16 y VIN");
    }
  } else {
    snprintf(buf, sizeof buf, "PM1.0: %u ug/m3", (unsigned)pms_estado.pm1);
    pintarLinea(NIVEL_OK, buf);  // PM1.0 no tiene umbral normativo
    snprintf(buf, sizeof buf, "PM2.5: %u", (unsigned)pms_estado.pm25);
    pintarLinea(alertas.pm25, buf);
    snprintf(buf, sizeof buf, "PM10 : %u", (unsigned)pms_estado.pm10);
    pintarLinea(alertas.pm10, buf);
  }

  if (scd_estado.rancio) {
    // Un valor viejo pintado como si fuera de ahora es peor que no pintar
    // nada: te hace creer que el aire esta como estaba hace horas.
    display.println("SCD41: SIN DATOS");
    display.printf("hace %lus\n",
                   (unsigned long)((millis() - scd_estado.ultimaLecturaMs) / 1000));
    display.printf("reinicios: %lu\n", (unsigned long)scd_estado.reinicios);
  } else if (scd_estado.valido && !scd_estado.error) {
    snprintf(buf, sizeof buf, "CO2  : %u ppm", (unsigned)scd_estado.co2);
    pintarLinea(alertas.co2, buf);
    snprintf(buf, sizeof buf, "Temp : %.1f C", scd_estado.temp);
    pintarLinea(alertas.temp, buf);
    snprintf(buf, sizeof buf, "Hum  : %.1f %%", scd_estado.hum);
    pintarLinea(alertas.hum, buf);
  } else if (saludScd() == SALUD_ESPERANDO) {
    display.println("SCD41: esperando");
    display.println("primer dato...");
  } else if (scd_estado.error) {
    display.printf("SCD41 ERR %d\n", scd_estado.error);
    display.printf("en:%s\n", scd_estado.etapa);
    display.printf("I2C 0x62: %s\n", scd_estado.presenteI2c ? "si" : "NO");
  } else {
    // Responde a todo sin errores pero no entrega ni una medicion: el caso
    // tipico de un sensor que no logra alimentar su emisor de infrarrojos.
    display.println("SCD41: NO MIDE");
    display.println("I2C ok, 0 datos");
    display.println("rev. alimentacion");
  }
}

// Solo se pinta si hay dato bueno: quien llama ya lo ha comprobado.
static void paginaExterior(const Exterior &e, uint8_t dy) {
  char buf[26];
  cabecera("EXTERIOR", e.lugar, dy);

  snprintf(buf, sizeof buf, "PM2.5: %.1f", e.pm25);
  pintarLinea(nivelPm25(e.pm25), buf);
  if (isnan(e.pm10)) display.println("PM10 : --");
  else {
    snprintf(buf, sizeof buf, "PM10 : %.1f", e.pm10);
    pintarLinea(nivelPm10(e.pm10), buf);
  }
  // Ozono y NO2 sin marca de nivel: sus limites son medias de 8 h y de 1 h
  // sobre datos de estacion, no sobre un modelo de rejilla de 11 km.
  if (isnan(e.o3)) display.println("O3   : --");
  else             display.printf("O3   : %.0f ug/m3\n", e.o3);
  if (isnan(e.no2)) display.println("NO2  : --");
  else              display.printf("NO2  : %.0f ug/m3\n", e.no2);

  if (e.hayMeteo) {
    display.printf("Temp : %.1f C\n", e.temp);
    display.printf("Hum  : %.0f %%\n", e.hum);
  } else {
    display.println("sin meteo exterior");
  }
}

// Rotacion de paginas. El indice se guarda entre llamadas: pantallaRefrescar()
// entra una vez por segundo y la pagina tiene que durar INT_PAGINA.
static uint8_t  pagina  = 0;
static uint32_t tPagina = 0;

// Desplazamiento anti burn-in: ciclo de 2 posiciones (0, 1 px).
static uint32_t tDesplazar = 0;
static uint8_t  desplazar  = 0;

void pantallaRefrescar() {
  if (!pantalla_ok) return;

  uint32_t ahora = millis();
  if (ahora - tPagina >= INT_PAGINA) {
    tPagina = ahora;
    pagina ^= 1;
  }

  if (ahora - tDesplazar >= INT_ANTIQUEMADO) {
    tDesplazar = ahora;
    desplazar = (desplazar + 1) & 0x01;
  }
  // Ciclo de 2, no de 4: con 2 px de sangrado la ultima linea del cuerpo
  // (hasta y=62 en un panel de 64) se saldria del panel. Con este ciclo
  // "desplazar" ya vale 0 o 1, sin ajuste adicional.
  uint8_t dy = desplazar;

  Exterior ext;
  exteriorSnapshot(ext);
  // La pagina exterior se salta si no hay dato bueno: media pantalla diciendo
  // "sin datos" la mitad del tiempo es peor que no tener la pagina.
  bool hayExt = ext.valido && !ext.rancio;
  if (pagina && !hayExt) pagina = 0;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (pagina) paginaExterior(ext, dy);
  else        paginaInterior(dy);
  display.display();
}
