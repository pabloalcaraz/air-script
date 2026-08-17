// Pruebas de la politica de aceptacion y recuperacion del SCD41. Es un port
// deliberadamente pequeno de sensors.cpp: valida los limites y, sobre todo,
// que el watchdog tambien cubra el tramo anterior a la primera medida.

const SCD_TIMEOUT_MS = 30000;
const SCD_REINTENTO_MS = 60000;
const SCD_CONGELADO_MS = 600000;

function muestraValida(co2, temp, hum) {
  return co2 >= 200 && co2 <= 40000 &&
    Number.isFinite(temp) && temp >= -40 && temp <= 85 &&
    Number.isFinite(hum) && hum >= 0 && hum <= 100;
}

function crearWatchdog() {
  const estado = {
    valido: false, ultimaLecturaMs: 0, msValorIgual: 0,
    rancio: false, congelado: false, reinicios: 0,
  };
  let ultimoIntento = 0;

  function arrancar(ahora) {
    estado.ultimaLecturaMs = ahora;
    estado.msValorIgual = ahora;
    estado.rancio = false;
    estado.congelado = false;
  }

  function revisar(ahora) {
    estado.rancio = ahora - estado.ultimaLecturaMs > SCD_TIMEOUT_MS;
    estado.congelado = estado.valido &&
      ahora - estado.msValorIgual > SCD_CONGELADO_MS;
    if (!estado.rancio && !estado.congelado) return false;
    if (ultimoIntento && ahora - ultimoIntento < SCD_REINTENTO_MS) return false;
    ultimoIntento = ahora;
    estado.reinicios++;
    arrancar(ahora);
    return true;
  }

  return { estado, arrancar, revisar };
}

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}

console.log("--- Watchdog antes de la primera medida ---");
{
  const w = crearWatchdog();
  w.arrancar(1000);
  debe("no reinicia durante el margen inicial", !w.revisar(31000), w.estado);
  debe("reinicia al vencer el timeout sin primera medida",
    w.revisar(31001) && w.estado.reinicios === 1, w.estado);
  debe("el reinicio vuelve a dar un margen limpio", !w.revisar(60000), w.estado);
}

console.log("\n--- Watchdog con medidas ---");
{
  const w = crearWatchdog();
  w.arrancar(1000);
  w.estado.valido = true;
  w.estado.ultimaLecturaMs = 25000;
  debe("una medida reciente no se reinicia", !w.revisar(50000), w.estado);
  debe("una medida antigua se recupera", w.revisar(55001), w.estado);
}

console.log("\n--- Validacion de muestras ---");
debe("acepta una muestra interior normal", muestraValida(800, 23.5, 45), "normal");
debe("rechaza CO2 cero", !muestraValida(0, 23.5, 45), "co2=0");
debe("rechaza CO2 absurdo", !muestraValida(65535, 23.5, 45), "co2=65535");
debe("rechaza temperatura NaN", !muestraValida(800, NaN, 45), "temp=NaN");
debe("rechaza humedad infinita", !muestraValida(800, 23.5, Infinity), "hum=Infinity");
debe("rechaza humedad fuera del dominio", !muestraValida(800, 23.5, 100.1), "hum=100.1");

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
