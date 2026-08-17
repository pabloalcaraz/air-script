// Banco de pruebas de alerts.cpp. Port a JS de registrarPeor() y de los
// clasificadores de nivel (nivelAlto/nivelFranja): valida el METODO, no el
// binario (en esta maquina no hay compilador de host, solo el cruzado de
// xtensa). Tocar alerts.cpp obliga a tocar esto, o las dos versiones dejan
// de decir lo mismo.

const NIVEL_OK = 0, NIVEL_AVISO = 1, NIVEL_MALO = 2;

// Umbrales reales de config.h.
const PM25_AVISO = 15, PM25_MALO = 35;
const PM10_AVISO = 15, PM10_MALO = 35; // no usado en estos casos, solo PM25/CO2/HUM
const CO2_AVISO = 800, CO2_MALO = 1200;
const HUM_OK_MIN = 30.0, HUM_OK_MAX = 60.0;
const HUM_MALO_MIN = 20.0, HUM_MALO_MAX = 70.0;

// --- port de alerts.cpp ---------------------------------------------------

// Magnitudes donde solo molesta pasarse por arriba (particulas, CO2).
function nivelAlto(v, aviso, malo) {
  if (v >= malo) return NIVEL_MALO;
  if (v >= aviso) return NIVEL_AVISO;
  return NIVEL_OK;
}

// Magnitudes con franja buena en el medio (temperatura, humedad).
function nivelFranja(v, okMin, okMax, maloMin, maloMax) {
  if (v <= maloMin || v >= maloMax) return NIVEL_MALO;
  if (v < okMin || v > okMax) return NIVEL_AVISO;
  return NIVEL_OK;
}

const nivelPm25 = v => nivelAlto(v, PM25_AVISO, PM25_MALO);
const nivelCo2 = v => nivelAlto(v, CO2_AVISO, CO2_MALO);
const nivelHum = v => nivelFranja(v, HUM_OK_MIN, HUM_OK_MAX, HUM_MALO_MIN, HUM_MALO_MAX);

// Replica registrarPeor() operando sobre un objeto "alertas" local, tal
// como hace alertasCalcular() sobre la global.
function registrarPeor(alertas, n, nombre) {
  if (n > alertas.peor) {
    alertas.peor = n;
    alertas.peorQue = nombre;
  }
}

// --- pruebas ------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}

console.log("--- registrarPeor() y umbrales de alerts.cpp ---");

// PM2.5 en "malo" y CO2 en "aviso" a la vez: gana el mas grave (PM2.5).
{
  const alertas = { peor: NIVEL_OK, peorQue: "" };
  const pm25 = nivelPm25(40);  // >= 35 -> MALO
  const co2 = nivelCo2(900);   // >= 800 y < 1200 -> AVISO
  debe("PM2.5=40 clasifica como MALO", pm25 === NIVEL_MALO, pm25);
  debe("CO2=900 clasifica como AVISO", co2 === NIVEL_AVISO, co2);
  registrarPeor(alertas, pm25, "PM2.5");
  registrarPeor(alertas, co2, "CO2");
  debe("el peor resultante es MALO", alertas.peor === NIVEL_MALO, alertas.peor);
  debe("peorQue queda en PM2.5 (el mas grave gana)", alertas.peorQue === "PM2.5", alertas.peorQue);
}

// Orden inverso de registro: el resultado no debe depender del orden.
{
  const alertas = { peor: NIVEL_OK, peorQue: "" };
  registrarPeor(alertas, nivelCo2(900), "CO2");     // AVISO primero
  registrarPeor(alertas, nivelPm25(40), "PM2.5");   // MALO despues
  debe("registrando CO2 antes que PM2.5, PM2.5 sigue ganando", alertas.peorQue === "PM2.5", alertas.peorQue);
}

// Un nivel igual al peor actual NO sustituye (solo un nivel MAYOR gana).
{
  const alertas = { peor: NIVEL_OK, peorQue: "" };
  registrarPeor(alertas, NIVEL_MALO, "PM2.5");
  registrarPeor(alertas, NIVEL_MALO, "CO2"); // mismo nivel, no debe reemplazar
  debe("un nivel igual (no mayor) no reemplaza al peor ya registrado",
    alertas.peorQue === "PM2.5", alertas.peorQue);
}

console.log("\n--- Franja de humedad (bordes 20 / 30 / 60 / 70) ---");

// Zona MALO por debajo: <= 20.
debe("hum=20 (borde) es MALO", nivelHum(20) === NIVEL_MALO, nivelHum(20));
debe("hum=19.9 (bajo el borde) es MALO", nivelHum(19.9) === NIVEL_MALO, nivelHum(19.9));

// Zona AVISO baja: entre 20 (exclusivo) y 30 (exclusivo).
debe("hum=25 (intermedio 20-30) es AVISO", nivelHum(25) === NIVEL_AVISO, nivelHum(25));
debe("hum=20.1 (justo tras el borde de 20) es AVISO", nivelHum(20.1) === NIVEL_AVISO, nivelHum(20.1));

// Borde 30: OK empieza en 30 inclusive.
debe("hum=30 (borde) es OK", nivelHum(30) === NIVEL_OK, nivelHum(30));
debe("hum=29.9 (justo antes del borde de 30) es AVISO", nivelHum(29.9) === NIVEL_AVISO, nivelHum(29.9));

// Zona OK intermedia: 30-60.
debe("hum=45 (intermedio 30-60) es OK", nivelHum(45) === NIVEL_OK, nivelHum(45));

// Borde 60: OK termina en 60 inclusive.
debe("hum=60 (borde) es OK", nivelHum(60) === NIVEL_OK, nivelHum(60));
debe("hum=60.1 (justo tras el borde de 60) es AVISO", nivelHum(60.1) === NIVEL_AVISO, nivelHum(60.1));

// Zona AVISO alta: entre 60 (exclusivo) y 70 (exclusivo).
debe("hum=65 (intermedio 60-70) es AVISO", nivelHum(65) === NIVEL_AVISO, nivelHum(65));
debe("hum=69.9 (justo antes del borde de 70) es AVISO", nivelHum(69.9) === NIVEL_AVISO, nivelHum(69.9));

// Borde 70: MALO empieza en 70 inclusive.
debe("hum=70 (borde) es MALO", nivelHum(70) === NIVEL_MALO, nivelHum(70));
debe("hum=70.1 (sobre el borde) es MALO", nivelHum(70.1) === NIVEL_MALO, nivelHum(70.1));

console.log(fallos ? "\n" + fallos + " FALLOS" : "\nTodo OK");
process.exit(fallos ? 1 : 0);
