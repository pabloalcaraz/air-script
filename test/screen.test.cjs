// Banco de pruebas del recorte de la cabecera del OLED. Port a JS de
// plegarAscii() y recortar() de screen.cpp: valida el METODO, no el binario
// (en esta maquina no hay compilador de host, solo el cruzado de xtensa).
// Tocar screen.cpp obliga a tocar esto, o las dos versiones dejan de decir
// lo mismo.

const SCREEN_WIDTH = 128;
const CHAR_W = 6;                                  // fuente clasica GFX: 5 px + 1 de separacion
const COLS = Math.floor(SCREEN_WIDTH / CHAR_W);    // 21

// --- port de screen.cpp ------------------------------------------------------

// La fuente clasica de GFX es CP437, no UTF-8: una tilde son 2 bytes y se
// pintan como 2 glifos basura. Se pliega a ASCII SOLO al pintar; en la NVS y
// en la web el nombre sigue completo.
const TILDES = {
  "á":"a","à":"a","ä":"a","â":"a","Á":"A","À":"A","Ä":"A","Â":"A",
  "é":"e","è":"e","ë":"e","ê":"e","É":"E","È":"E","Ë":"E","Ê":"E",
  "í":"i","ì":"i","ï":"i","î":"i","Í":"I","Ì":"I","Ï":"I","Î":"I",
  "ó":"o","ò":"o","ö":"o","ô":"o","Ó":"O","Ò":"O","Ö":"O","Ô":"O",
  "ú":"u","ù":"u","ü":"u","û":"u","Ú":"U","Ù":"U","Ü":"U","Û":"U",
  "ñ":"n","Ñ":"N","ç":"c","Ç":"C"
};
function plegarAscii(s) {
  let out = "";
  for (const ch of s) {
    if (TILDES[ch]) { out += TILDES[ch]; continue; }
    const c = ch.codePointAt(0);
    // Cualquier otro no-ASCII (cirilico, arabe, CJK...) se sustituye por '?':
    // pintar el byte crudo daria un glifo aleatorio de CP437.
    out += (c >= 0x20 && c < 0x7F) ? ch : "?";
  }
  return out;
}

// Recorta a `max` glifos. Si no cabe, deja max-2 y anade "..". Antes de pegar
// los puntos se quitan espacios y comas del final: "Delicias, .." se lee peor
// que "Delicias..".
function recortar(s, max) {
  if (max <= 0) return "";
  if (s.length <= max) return s;
  if (max <= 2) return ".".repeat(max);
  let corte = s.slice(0, max - 2);
  corte = corte.replace(/[ ,;.\-]+$/, "");
  return corte + "..";
}

// Presupuesto de la cabecera: 21 columnas - len(titulo) - 1 del espacio.
const presupuesto = titulo => COLS - titulo.length - 1;

// --- pruebas ----------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}

console.log("--- Recorte de la cabecera del OLED ---");

debe("el presupuesto de EXTERIOR son 12 glifos", presupuesto("EXTERIOR") === 12,
  presupuesto("EXTERIOR"));

// Caso real observado en el OLED.
const caso = plegarAscii("Delicias, Arganzuela, Madrid");
const pintado = "EXTERIOR " + recortar(caso, presupuesto("EXTERIOR"));
debe("EXTERIOR + Delicias... cabe en una linea de 21", pintado.length <= COLS, pintado);
debe("el recorte no deja coma ni espacio antes de los puntos",
  !/[ ,]\.\.$/.test(pintado), pintado);
debe("se ve el barrio, que es lo que identifica el sitio",
  pintado.startsWith("EXTERIOR Delicias"), pintado);

// Nombre corto: no se toca.
debe("un nombre que cabe no se recorta",
  recortar("Madrid", 12) === "Madrid", recortar("Madrid", 12));
debe("un nombre justo en el limite no se recorta",
  recortar("123456789012", 12) === "123456789012", recortar("123456789012", 12));
debe("un nombre de max+1 si se recorta",
  recortar("1234567890123", 12) === "1234567890..",
  recortar("1234567890123", 12));

// Plegado a ASCII.
debe("las tildes se pliegan", plegarAscii("Alcalá de Henares") === "Alcala de Henares",
  plegarAscii("Alcalá de Henares"));
debe("la enye se pliega", plegarAscii("A Coruña") === "A Coruna", plegarAscii("A Coruña"));
debe("un no-ASCII sin equivalente sale como '?'",
  plegarAscii("Москва") === "??????", plegarAscii("Москва"));
debe("plegar no cambia la longitud en glifos de un nombre acentuado",
  plegarAscii("Alcalá").length === 6, plegarAscii("Alcalá").length);

// Coordenadas de reserva (lo que guarda webapi cuando no hay nombre).
debe("unas coordenadas caben enteras",
  recortar(plegarAscii("40.40,-3.69"), 12) === "40.40,-3.69",
  recortar(plegarAscii("40.40,-3.69"), 12));

// Ninguna otra linea del OLED puede pasar de 21 glifos, porque tras este
// cambio textWrap queda desactivado y lo que sobre se corta en seco.
const LINEAS_FIJAS = [
  "PMS: calentando", "~30s...", "PMS: sin frames", "rev. GPIO16 y VIN",
  "SCD41: SIN DATOS", "SCD41: esperando", "primer dato...", "SCD41: NO MIDE",
  "I2C ok, 0 datos", "rev. alimentacion", "sin meteo exterior",
  "PM1.0: 999 ug/m3 !!", "PM2.5: 999 !!", "PM10 : 999 !!",
  "CO2  : 9999 ppm !!", "Temp : -10.0 C !!", "Hum  : 100.0 % !!",
  "O3   : 999 ug/m3", "NO2  : 999 ug/m3", "hace 999999s",
  "reinicios: 999", "SCD41 ERR -32768", "I2C 0x62: si"
];
LINEAS_FIJAS.forEach(l =>
  debe("cabe sin wrap: '" + l + "'", l.length <= COLS, l.length));

// --- port de buscarPanel()/pantallaInit() de screen.cpp ---------------------
// Adafruit_SSD1306::begin() no comprueba el ACK del panel: solo falla si no
// consigue el malloc del framebuffer. Por eso pantallaInit() sondea el bus
// ANTES de llamarlo. Estos casos fijan ese contrato: sin ACK no hay pantalla,
// pase lo que pase con begin().

const OLED_ADDR = 0x3C, OLED_ADDR_ALT = 0x3D;

// `enBus` = direcciones que responden ACK. 0x3C tiene prioridad sobre 0x3D.
function buscarPanel(enBus) {
  if (enBus.includes(OLED_ADDR))     return OLED_ADDR;
  if (enBus.includes(OLED_ADDR_ALT)) return OLED_ADDR_ALT;
  return 0;
}

// `beginOk` modela el retorno de Adafruit: true salvo fallo de memoria.
function pantallaInit(enBus, beginOk = true) {
  const addr = buscarPanel(enBus);
  if (!addr)     return { ok: false, addr: 0 };
  if (!beginOk)  return { ok: false, addr: 0 };
  return { ok: true, addr };
}

// El caso que motivo el arreglo: bus vacio y begin() diciendo que si.
const vacio = pantallaInit([], true);
debe("bus vacio: no hay pantalla aunque begin() devuelva true",
  vacio.ok === false && vacio.addr === 0, JSON.stringify(vacio));

// Solo el SCD41 en el bus tampoco es una pantalla.
debe("con solo 0x62 en el bus no se declara OLED",
  pantallaInit([0x62]).ok === false, JSON.stringify(pantallaInit([0x62])));

const p3C = pantallaInit([OLED_ADDR]);
debe("panel en 0x3C se detecta en 0x3C",
  p3C.ok === true && p3C.addr === OLED_ADDR, JSON.stringify(p3C));

// El fallback a 0x3D del codigo viejo era inalcanzable porque begin() nunca
// devolvia false. Ahora se elige por ACK, asi que si funciona.
const p3D = pantallaInit([OLED_ADDR_ALT]);
debe("panel solo en 0x3D usa la direccion alternativa",
  p3D.ok === true && p3D.addr === OLED_ADDR_ALT, JSON.stringify(p3D));

debe("con ambas direcciones ocupadas gana 0x3C",
  pantallaInit([OLED_ADDR, OLED_ADDR_ALT]).addr === OLED_ADDR,
  pantallaInit([OLED_ADDR, OLED_ADDR_ALT]).addr);

// Responde en el bus pero no hay RAM para el framebuffer: no hay pantalla, y
// pantalla_addr vuelve a 0 para que nadie la de por buena mas adelante.
const sinRam = pantallaInit([OLED_ADDR], false);
debe("ACK pero begin() falla: sin pantalla y addr a 0",
  sinRam.ok === false && sinRam.addr === 0, JSON.stringify(sinRam));

console.log(fallos ? "\n" + fallos + " FALLOS" : "\nTodo OK");
process.exit(fallos ? 1 : 0);
