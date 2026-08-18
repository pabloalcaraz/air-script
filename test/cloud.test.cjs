const fs = require("node:fs");
const path = require("node:path");

// El tamaño incluye el byte NUL del buffer C: 171 permite 170 caracteres.
const configSrc = fs.readFileSync(path.join(__dirname, "..", "config.h"), "utf8");
const cloudSrc = fs.readFileSync(path.join(__dirname, "..", "cloud.cpp"), "utf8");
const tokenLen = Number(/#define\s+CLOUD_TOKEN_LEN\s+(\d+)/.exec(configSrc)?.[1]);
if (tokenLen !== 171) {
  console.error("FALLO: CLOUD_TOKEN_LEN debe permitir 170 caracteres utiles");
  process.exitCode = 1;
} else {
  console.log("OK: token de Grafana admite 170 caracteres utiles");
}

function destinoSeguro(url) {
  try {
    const u = new URL(url);
    return u.protocol === "https:" && u.port === "" &&
      u.username === "" && u.password === "" &&
      u.search === "" && u.hash === "" &&
      u.hostname.toLowerCase().endsWith(".grafana.net") &&
      u.hostname.length > ".grafana.net".length;
  } catch (_) { return false; }
}

// Port de la cola circular de cloud.cpp: FIFO con descarte de la mas vieja
// cuando esta llena. Mismo criterio de test que history.test.cjs (Task 11
// del plan anterior): se valida la aritmetica del indice, no el hardware.

const CLOUD_BUFFER = 120;

function crearCola() {
  return { buf: new Array(CLOUD_BUFFER).fill(null), cabeza: 0, cuenta: 0 };
}

function encolar(cola, valor) {
  const cola_idx = (cola.cabeza + cola.cuenta) % CLOUD_BUFFER;
  if (cola.cuenta === CLOUD_BUFFER) {
    cola.cabeza = (cola.cabeza + 1) % CLOUD_BUFFER;
  } else {
    cola.cuenta++;
  }
  cola.buf[cola_idx] = valor;
}

function desencolar(cola) {
  if (cola.cuenta === 0) return null;
  const v = cola.buf[cola.cabeza];
  cola.cabeza = (cola.cabeza + 1) % CLOUD_BUFFER;
  cola.cuenta--;
  return v;
}

function assert(cond, msg) {
  if (!cond) { console.error("FALLO:", msg); process.exitCode = 1; }
  else console.log("OK:", msg);
}

function reinsertarFrente(cola, valor) {
  if (cola.cuenta === CLOUD_BUFFER) cola.cuenta--;
  cola.cabeza = (cola.cabeza + CLOUD_BUFFER - 1) % CLOUD_BUFFER;
  cola.buf[cola.cabeza] = valor;
  cola.cuenta++;
}

console.log("\n--- Seguridad del destino cloud ---");
assert(destinoSeguro("https://influx-prod-01.grafana.net/api/v1/push/influx/write"),
  "acepta un endpoint HTTPS oficial de Grafana");
assert(!destinoSeguro("http://influx-prod-01.grafana.net/write"),
  "rechaza HTTP aunque el host sea Grafana");
assert(!destinoSeguro("https://grafana.net.evil.example/write"),
  "rechaza dominios que solo contienen el nombre de Grafana");
assert(!destinoSeguro("https://usuario@influx-prod-01.grafana.net/write"),
  "rechaza credenciales embebidas en la URL");
assert(!destinoSeguro("https://influx-prod-01.grafana.net:8443/write"),
  "rechaza puertos TLS alternativos");
assert(!destinoSeguro("https://influx-prod-01.grafana.net/write?token=secreto"),
  "rechaza secretos en la query string");
assert(/setCACert\s*\(\s*CLOUD_ROOT_CA\s*\)/.test(cloudSrc),
  "cloud valida TLS con una CA fijada");
assert(!/setInsecure\s*\(\s*\)/.test(cloudSrc),
  "cloud no desactiva la validacion TLS");
assert(/cambiaDestino\s*&&\s*!tocaToken/.test(cloudSrc),
  "cambiar URL o usuario exige un token nuevo");
assert(/xTaskCreatePinnedToCore[\s\S]*tareaCloud/.test(cloudSrc),
  "el transporte cloud vive en una tarea FreeRTOS");
const bloqueTick = /void cloudTick\(\)\s*\{([\s\S]*?)\n\}/.exec(cloudSrc)?.[1] || "";
assert(!/enviarLinea|HTTPClient|\.POST\(/.test(bloqueTick),
  "cloudTick solo encola y no hace red bloqueante");
assert(/redTlsTomar\s*\(\s*RED_TLS_ESPERA_MS\s*\)/.test(cloudSrc),
  "cloud serializa su handshake TLS con el resto de tareas");
assert(/CLOUD_REINTENTO_MIN_MS[\s\S]*CLOUD_REINTENTO_MAX_MS/.test(cloudSrc),
  "los fallos aplican backoff acotado");
assert(!/void cloudDrenar\s*\(/.test(cloudSrc),
  "ya no existe un drenaje HTTP síncrono desde el loop");

// Caso 1: FIFO simple sin llenar
let c = crearCola();
encolar(c, "a"); encolar(c, "b"); encolar(c, "c");
assert(desencolar(c) === "a", "FIFO: sale 'a' primero");
assert(desencolar(c) === "b", "FIFO: sale 'b' segundo");
assert(c.cuenta === 1, "queda 1 en cola");

// Caso 2: cola vacia
c = crearCola();
assert(desencolar(c) === null, "desencolar vacia da null");

// Caso 3: llenar y desbordar -> descarta la mas vieja
c = crearCola();
for (let i = 0; i < CLOUD_BUFFER; i++) encolar(c, i);
assert(c.cuenta === CLOUD_BUFFER, "cola llena a tope");
encolar(c, "nueva");  // deberia tirar el 0 y meter "nueva" al final
assert(desencolar(c) === 1, "el 0 se descarto, ahora sale el 1");
let ultimo;
for (let i = 0; i < CLOUD_BUFFER - 1; i++) ultimo = desencolar(c);
assert(ultimo === "nueva", "la nueva se conservo, quedo la ultima");
assert(c.cuenta === 0, "cola vacia tras sacar todo");

// Caso 3b: si falla el elemento más antiguo, vuelve delante y no al final.
c = crearCola();
encolar(c, "antigua"); encolar(c, "segunda");
const fallida = desencolar(c);
encolar(c, "nueva-durante-post");
reinsertarFrente(c, fallida);
assert(desencolar(c) === "antigua", "un POST fallido conserva el orden FIFO");
assert(desencolar(c) === "segunda", "la segunda muestra no adelanta a la fallida");

// Caso 4: encolar-desencolar alternado no corrompe el orden (wrap-around)
c = crearCola();
for (let ronda = 0; ronda < 5; ronda++) {
  for (let i = 0; i < CLOUD_BUFFER; i++) encolar(c, `r${ronda}-${i}`);
  for (let i = 0; i < CLOUD_BUFFER; i++) {
    assert(desencolar(c) === `r${ronda}-${i}`, `ronda ${ronda}: orden ${i} preservado`);
  }
}

// Port de cloudLinea() de cloud.cpp: formateador a Influx Line Protocol.
// Campos con centinela NULO_* se omiten; si no queda ninguno, devuelve null
// (equivalente al "return 0" en C, que cloudTick()/tareaCloud() interpretan
// como "nada que mandar").

const NULO_U16 = 0xFFFF;
const NULO_I16 = -32768; // INT16_MIN
const TS_EPOCH_MIN = 1000000000; // config.h: por debajo de esto el ts es "segundos desde arranque"

function cloudLinea(m) {
  const campos = [];
  if (m.pm1  !== NULO_U16) campos.push(`pm1=${m.pm1}`);
  if (m.pm25 !== NULO_U16) campos.push(`pm25=${m.pm25}`);
  if (m.pm10 !== NULO_U16) campos.push(`pm10=${m.pm10}`);
  if (m.co2  !== NULO_U16) campos.push(`co2=${m.co2}`);
  if (m.tempD !== NULO_I16) campos.push(`temp=${(m.tempD / 10.0).toFixed(1)}`);
  if (m.humD  !== NULO_U16) campos.push(`hum=${(m.humD / 10.0).toFixed(1)}`);

  if (campos.length === 0) return null; // los dos sensores caidos: nada que mandar

  const camposStr = campos.join(",");
  if (m.ts >= TS_EPOCH_MIN) {
    return `aire,dispositivo=airscript ${camposStr} ${m.ts}000000000`;
  }
  return `aire,dispositivo=airscript ${camposStr}`;
}

// Caso 1: todos los campos presentes
let m = { ts: 100, pm1: 3, pm25: 8, pm10: 12, co2: 620, tempD: 215, humD: 452 };
let linea = cloudLinea(m);
assert(linea.includes("pm1=3"), "linea completa: incluye pm1");
assert(linea.includes("pm25=8"), "linea completa: incluye pm25");
assert(linea.includes("pm10=12"), "linea completa: incluye pm10");
assert(linea.includes("co2=620"), "linea completa: incluye co2");
assert(linea.includes("temp=21.5"), "linea completa: temp reconstruida a 1 decimal (215 -> 21.5)");
assert(linea.includes("hum=45.2"), "linea completa: hum reconstruida a 1 decimal (452 -> 45.2)");

// Caso 2: PMS caido (pm1/pm25/pm10 a NULO) -> solo co2/temp/hum
m = { ts: 100, pm1: NULO_U16, pm25: NULO_U16, pm10: NULO_U16, co2: 620, tempD: 215, humD: 452 };
linea = cloudLinea(m);
assert(!linea.includes("pm1="), "PMS caido: pm1 ausente");
assert(!linea.includes("pm25="), "PMS caido: pm25 ausente");
assert(!linea.includes("pm10="), "PMS caido: pm10 ausente");
assert(linea.includes("co2=620"), "PMS caido: co2 presente");
assert(linea.includes("temp=21.5"), "PMS caido: temp presente");
assert(linea.includes("hum=45.2"), "PMS caido: hum presente");

// Caso 2b: SCD caido (co2/tempD/humD a NULO) -> solo pm1/pm25/pm10
m = { ts: 100, pm1: 3, pm25: 8, pm10: 12, co2: NULO_U16, tempD: NULO_I16, humD: NULO_U16 };
linea = cloudLinea(m);
assert(linea.includes("pm1=3") && linea.includes("pm25=8") && linea.includes("pm10=12"),
  "SCD caido: pm1/pm25/pm10 presentes");
assert(!linea.includes("co2=") && !linea.includes("temp=") && !linea.includes("hum="),
  "SCD caido: co2/temp/hum ausentes");

// Caso 3: los dos sensores caidos a la vez -> nada que mandar
m = { ts: 100, pm1: NULO_U16, pm25: NULO_U16, pm10: NULO_U16, co2: NULO_U16, tempD: NULO_I16, humD: NULO_U16 };
assert(cloudLinea(m) === null, "los dos sensores caidos: no hay nada que mandar (equivale a return 0)");

// Caso 4: timestamp NTP valido (>= TS_EPOCH_MIN) -> sufijo nanosegundos al final
m = { ts: TS_EPOCH_MIN + 500, pm1: 1, pm25: 2, pm10: 3, co2: 400, tempD: 200, humD: 400 };
linea = cloudLinea(m);
assert(linea.endsWith(`${TS_EPOCH_MIN + 500}000000000`), "ts con NTP: sufijo de nanosegundos al final");
assert(linea.startsWith("aire,dispositivo=airscript "), "ts con NTP: measurement y tag al inicio");

// Caso 5: timestamp sin NTP (< TS_EPOCH_MIN, segundos desde arranque) -> sin timestamp
m = { ts: 12345, pm1: 1, pm25: 2, pm10: 3, co2: 400, tempD: 200, humD: 400 };
linea = cloudLinea(m);
assert(!linea.includes("12345"), "ts sin NTP: no aparece el ts de segundos-desde-arranque");
assert(!/ \d+000000000$/.test(linea), "ts sin NTP: no hay sufijo de nanosegundos");

console.log("cloud.test.cjs: todo OK");
