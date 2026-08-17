// Genera webpage_gz.h a partir de webpage.h. El dashboard son ~41 KB de texto
// servidos en cada carga desde un aparato con una sola conexion; comprimido
// baja a ~10 KB.
//
// El .h generado SE COMMITEA a proposito: arduino-cli no ejecuta scripts antes
// de compilar, asi que el artefacto tiene que estar en el arbol. La huella
// evita que se quede desincronizado sin que nadie se entere.
//
//   node tools/gzip-web.cjs          -> regenera
//   node tools/gzip-web.cjs --check  -> falla si esta desincronizado (para CI/tests)

const fs = require("fs"), zlib = require("zlib"), crypto = require("crypto"), path = require("path");

const RAIZ = path.join(__dirname, "..");
const ORIGEN = path.join(RAIZ, "webpage.h");
const DESTINO = path.join(RAIZ, "webpage_gz.h");
const soloComprobar = process.argv.includes("--check");

// Normalizar antes de extraer evita que el contenido servido dependa de los
// finales de linea usados por el checkout o el editor.
const fuente = fs.readFileSync(ORIGEN, "utf8").replace(/\r\n?/g, "\n");
// Se extrae exactamente el mismo bloque que sirve el firmware: el interior del
// raw string R"HTMLPAGE(...)HTMLPAGE".
const m = fuente.match(/R"HTMLPAGE\(([\s\S]*?)\)HTMLPAGE"/);
if (!m) { console.error("No encuentro el bloque R\"HTMLPAGE(...) en webpage.h"); process.exit(1); }
const html = m[1];

const huella = crypto.createHash("sha256").update(html).digest("hex");
// level 9 y no el por defecto: se comprime una vez en el escritorio y se
// descomprime miles de veces en navegadores. El tiempo de compresion da igual.
const gz = zlib.gzipSync(Buffer.from(html, "utf8"), { level: 9 });

let out = "#pragma once\n#include <Arduino.h>\n\n";
out += "// GENERADO POR tools/gzip-web.cjs — NO EDITAR A MANO.\n";
out += "// Regenerar con:  node tools/gzip-web.cjs\n";
out += "// Origen: webpage.h (" + html.length + " B) -> " + gz.length + " B gzip ("
     + (100 - Math.round(100 * gz.length / html.length)) + "% menos).\n";
out += "// Huella del origen (sha256): " + huella + "\n\n";
out += "#define PAGINA_GZ_SHA256 \"" + huella + "\"\n";
out += "#define PAGINA_GZ_LEN " + gz.length + "\n\n";
out += "static const uint8_t PAGINA_GZ[] PROGMEM = {\n";
for (let i = 0; i < gz.length; i += 16) {
  out += "  " + [...gz.slice(i, i + 16)].map(b => "0x" + b.toString(16).padStart(2, "0")).join(",") + ",\n";
}
out += "};\n";

if (soloComprobar) {
  const previo = fs.existsSync(DESTINO)
    ? fs.readFileSync(DESTINO, "utf8").replace(/\r\n?/g, "\n")
    : "";
  const shaGuardada = previo.match(/#define PAGINA_GZ_SHA256 "([0-9a-f]{64})"/i)?.[1];
  const lenGuardada = Number(previo.match(/#define PAGINA_GZ_LEN (\d+)/)?.[1]);
  const bloque = previo.match(/PAGINA_GZ\[\][\s\S]*?=\s*\{([\s\S]*?)\};/);
  const bytes = bloque
    ? Buffer.from([...bloque[1].matchAll(/0x([0-9a-f]{2})/gi)].map(x => parseInt(x[1], 16)))
    : Buffer.alloc(0);

  let contenidoValido = false;
  try {
    contenidoValido = zlib.gunzipSync(bytes).equals(Buffer.from(html, "utf8"));
  } catch (_) {
    contenidoValido = false;
  }

  // No se comparan los bytes producidos de nuevo: distintas versiones de
  // zlib pueden generar streams gzip diferentes pero equivalentes. Verificar
  // la huella, longitud y descompresion detecta tanto desfases como corrupcion.
  if (shaGuardada !== huella || lenGuardada !== bytes.length || !contenidoValido) {
    console.error("FALLO: webpage_gz.h no corresponde a webpage.h.");
    console.error("       Ejecuta: node tools/gzip-web.cjs");
    process.exit(1);
  }
  console.log("  ok    webpage_gz.h esta al dia (" + gz.length + " B)");
  process.exit(0);
}

fs.writeFileSync(DESTINO, out);
console.log("webpage_gz.h escrito: " + html.length + " B -> " + gz.length + " B");
