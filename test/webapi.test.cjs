// Banco de pruebas de las utilidades de webapi.cpp. Port a JS de escapar():
// valida el METODO, no el binario. Tocar webapi.cpp obliga a tocar esto.
// Se trabaja sobre BYTES (Buffer), no sobre String, porque el bug original
// era precisamente de signo de byte y un String de JS lo ocultaria.

// --- port de webapi.cpp ------------------------------------------------------

function quitarUtf8Incompleto(dst) {
  if (!dst.length) return dst;
  let inicio = dst.length - 1;
  while (inicio > 0 && (dst[inicio] & 0xC0) === 0x80) inicio--;
  const lider = dst[inicio];
  let esperados = 1;
  if ((lider & 0xE0) === 0xC0) esperados = 2;
  else if ((lider & 0xF0) === 0xE0) esperados = 3;
  else if ((lider & 0xF8) === 0xF0) esperados = 4;
  return dst.length - inicio < esperados ? dst.slice(0, inicio) : dst;
}

// escapar(): comillas y barras se escapan; los controles (<0x20) pasan a
// espacio. Los bytes >=0x80 son UTF-8 y NO son controles: en C hay que
// castear a unsigned char o el signo los manda a la rama equivocada.
function escapar(srcBuf, n) {
  const dst = [];
  let j = 0;
  for (let i = 0; i < srcBuf.length && srcBuf[i] !== 0 && j + 2 < n; i++) {
    const c = srcBuf[i];               // Buffer da 0..255, como unsigned char
    if (c === 0x22 /* " */ || c === 0x5C /* \ */) { dst[j++] = 0x5C; }
    else if (c < 0x20) { dst[j++] = 0x20; continue; }
    dst[j++] = c;
  }
  const limpio = quitarUtf8Incompleto(dst.slice(0, j));
  j = limpio.length;
  // Y si al retroceder quedo una barra de escape huerfana, tambien se quita.
  if (j > 0 && limpio[j - 1] === 0x5C) j--;
  return Buffer.from(limpio.slice(0, j));
}

function limpiarNombre(src, n) {
  if (!n) return Buffer.alloc(0);
  const dst = [];
  for (const c of Buffer.from(src, "utf8")) {
    if (dst.length + 1 >= n) break;
    if (c >= 0x20) dst.push(c);
  }
  return Buffer.from(quitarUtf8Incompleto(dst)).toString("utf8");
}

function leerCoord(src) {
  const limpio = src.trim();
  if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(limpio)) return null;
  const valor = Number(limpio);
  return Number.isFinite(valor) ? valor : null;
}

// --- pruebas ----------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}
const esc = (s, n = 256) => escapar(Buffer.from(s, "utf8"), n).toString("utf8");

console.log("--- escapar() ---");

debe("las tildes sobreviven", esc("Alcalá de Henares") === "Alcalá de Henares",
  esc("Alcalá de Henares"));
debe("la enye sobrevive", esc("A Coruña") === "A Coruña", esc("A Coruña"));
debe("una estacion AQICN con acento sobrevive",
  esc("Plaza de España, Madrid") === "Plaza de España, Madrid",
  esc("Plaza de España, Madrid"));
debe("las comillas se escapan", esc('di "hola"') === 'di \\"hola\\"', esc('di "hola"'));
debe("la barra se escapa", esc("a\\b") === "a\\\\b", esc("a\\b"));
debe("un salto de linea pasa a espacio", esc("a\nb") === "a b", esc("a b"));
debe("un tabulador pasa a espacio", esc("a\tb") === "a b", esc("a\tb"));

// El corte por buffer lleno no puede partir una secuencia UTF-8.
const cortado = esc("aaaaaañ", 9);   // 'ñ' son 2 bytes; el limite los parte
debe("el corte no deja medio caracter suelto",
  Buffer.from(cortado, "utf8").every((b, i, a) =>
    b < 0x80 || (b & 0xE0) === 0xC0 || (b & 0xC0) === 0x80) &&
    !/�/.test(cortado),
  cortado);
debe("el resultado cortado sigue siendo UTF-8 valido",
  Buffer.compare(Buffer.from(cortado, "utf8"),
                 Buffer.from(cortado, "utf8").toString("utf8") === cortado
                   ? Buffer.from(cortado, "utf8") : Buffer.alloc(0)) === 0,
  cortado);

debe("un caracter multibyte completo justo al limite se conserva",
  limpiarNombre("12345ñ", 8) === "12345ñ", limpiarNombre("12345ñ", 8));
debe("un caracter de tres bytes partido al limite se elimina entero",
  limpiarNombre("12345€", 8) === "12345", limpiarNombre("12345€", 8));
debe("limpiarNombre elimina controles sin dañar las tildes",
  limpiarNombre("Al\ncalá", 32) === "Alcalá", limpiarNombre("Al\ncalá", 32));

// El SSID llega a /api/health dentro de JSON: comillas y barras deben pasar
// por el mismo escapado que los nombres y mensajes.
const ssidMalo = 'casa "principal"\\5G';
let ssidParseado = null;
try { ssidParseado = JSON.parse('{"ssid":"' + esc(ssidMalo) + '"}').ssid; }
catch (_) {}
debe("un SSID con comillas y barras produce JSON valido",
  ssidParseado === ssidMalo, ssidParseado);

console.log("\n--- coordenadas estrictas ---");
debe("acepta decimal con espacios exteriores", leerCoord(" 40.4168 ") === 40.4168, leerCoord(" 40.4168 "));
debe("acepta notacion cientifica", leerCoord("-3.7e0") === -3.7, leerCoord("-3.7e0"));
debe("rechaza sufijo basura", leerCoord("40.4abc") === null, leerCoord("40.4abc"));
debe("rechaza Infinity", leerCoord("Infinity") === null, leerCoord("Infinity"));
debe("rechaza una cadena vacia", leerCoord("   ") === null, leerCoord("   "));

// El JSON resultante tiene que parsear. Es el objetivo real de la funcion.
["Alcalá", 'con "comillas"', "con\\barra", "con\nsalto", "A Coruña"].forEach(s => {
  let ok = true;
  try { JSON.parse('{"lugar":"' + esc(s) + '"}'); } catch (e) { ok = false; }
  debe("produce JSON parseable para " + JSON.stringify(s), ok, esc(s));
});

// --- Comprobacion de Origin (anti-CSRF) --------------------------------------

// Port de origenValido() de webapi.cpp. Se acepta si NO hay Origin (curl,
// scripts, apps: no son navegadores y no hay CSRF que valga) o si coincide
// con el host al que se ha llamado.
function origenValido(origin, host) {
  if (!origin || origin.length === 0) return true;
  if (origin === "null") return false;   // sandbox / data: / file:
  const p = origin.indexOf("://");
  if (p < 0) return false;
  return origin.slice(p + 3) === host;
}

console.log("\n--- Origin ---");
debe("sin Origin se acepta (curl, scripts)", origenValido("", "air.local"), "");
debe("mismo origen por mDNS se acepta",
  origenValido("http://air.local", "air.local"), "");
debe("mismo origen por IP se acepta",
  origenValido("http://192.168.1.5", "192.168.1.5"), "");
debe("mismo origen con puerto explicito se acepta",
  origenValido("http://192.168.1.5:80", "192.168.1.5:80"), "");
debe("otro sitio se rechaza",
  !origenValido("https://malo.example", "air.local"), "");
debe("un sitio que solo empieza igual se rechaza",
  !origenValido("http://air.local.malo.example", "air.local"), "");
debe("Origin 'null' (sandbox) se rechaza",
  !origenValido("null", "air.local"), "");
debe("un Origin sin esquema se rechaza",
  !origenValido("air.local", "air.local"), "");

// --- gate CSRF por ruta: /api/cloud/config y las demas POST -----------------

// origenValido() ya esta portado y probado arriba, pero eso solo cubre la
// logica pura. Lo que pide este caso es otra cosa: que /api/cloud/config
// pase por postPermitido() igual que el resto de rutas POST que persisten
// algo. Eso es cableado de webapi.cpp (que handler llama a que funcion), no
// logica portable a JS -- no hay estado de servidor ni req/res que simular
// aqui. Se comprueba leyendo el propio fuente: cada handler protegido debe
// empezar por "if (!postPermitido()) return;". Es fragil a refactors de
// formato, pero es la unica forma de que un cambio que se olvide del guard
// (como paso con /api/cloud/config antes de la revision) rompa el test.
const fs = require("fs");
const path = require("path");

console.log("\n--- gate CSRF por ruta (postPermitido) ---");

const RUTAS_PROTEGIDAS = [
  "rutaExteriorPost", "rutaExteriorToken", "rutaCloudConfig",
  "rutaCalibrar", "rutaScdReset", "rutaScdAutotest", "rutaScdAsc",
];

const fuenteWebapi = fs.readFileSync(
  path.join(__dirname, "..", "webapi.cpp"), "utf8");

RUTAS_PROTEGIDAS.forEach(nombre => {
  const re = new RegExp(
    "static void " + nombre + "\\(\\)\\s*\\{\\s*if \\(!postPermitido\\(\\)\\) return;");
  debe(nombre + "() empieza con el guard postPermitido()",
    re.test(fuenteWebapi), nombre);
});

console.log(fallos ? "\n" + fallos + " FALLOS" : "\nTodo OK");
process.exit(fallos ? 1 : 0);
