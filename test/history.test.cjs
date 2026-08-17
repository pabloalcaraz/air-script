// Banco de pruebas del buffer circular de history.cpp. Port a JS de
// historialGet() y historialAjustarTs(): valida el METODO, no el binario
// (en esta maquina no hay compilador de host, solo el cruzado de xtensa).
// Tocar history.cpp obliga a tocar esto, o las dos versiones dejan de decir
// lo mismo.

const HIST_SIZE = 1440;       // 24 h a una muestra por minuto (config.h)
const TS_EPOCH_MIN = 1000000000; // config.h: por debajo de esto el ts es "segundos desde arranque"

// --- port de history.cpp -----------------------------------------------------

class Historial {
  constructor() {
    this.buf = new Array(HIST_SIZE).fill(null);
    this.cabeza = 0;
    this.total = 0;
  }

  guardar(ts) {
    this.buf[this.cabeza] = { ts };
    this.cabeza = (this.cabeza + 1) % HIST_SIZE;
    if (this.total < HIST_SIZE) this.total++;
  }

  count() { return this.total; }

  // Replica historialGet(): 0 = la mas antigua.
  get(i) {
    if (i >= this.total) return null;
    const inicio = (this.total === HIST_SIZE) ? this.cabeza : 0;
    return this.buf[(inicio + i) % HIST_SIZE];
  }

  // Replica historialAjustarTs() TRAS el fix: inicio se calcula UNA vez,
  // fuera del bucle (era invariante y antes se recalculaba cada iteracion).
  ajustarTs(desfase) {
    const inicio = (this.total === HIST_SIZE) ? this.cabeza : 0;
    for (let i = 0; i < this.total; i++) {
      const m = this.buf[(inicio + i) % HIST_SIZE];
      if (m.ts < TS_EPOCH_MIN) m.ts = m.ts + desfase;
    }
  }
}

// --- pruebas ------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}

console.log("--- Buffer circular de history.cpp ---");

// Sin wrap-around: la muestra 0 es la primera guardada.
{
  const h = new Historial();
  for (let i = 0; i < 5; i++) h.guardar(1000 + i);
  debe("sin wrap, count() es 5", h.count() === 5, h.count());
  debe("sin wrap, get(0) es la primera guardada (ts=1000)", h.get(0).ts === 1000, h.get(0));
  debe("sin wrap, get(4) es la ultima guardada (ts=1004)", h.get(4).ts === 1004, h.get(4));
  debe("get(5) fuera de rango devuelve null", h.get(5) === null, h.get(5));
}

// Con wrap-around: se llena el buffer con MAS muestras que su capacidad.
{
  const h = new Historial();
  const EXTRA = 10;
  for (let i = 0; i < HIST_SIZE + EXTRA; i++) h.guardar(i); // ts = indice de insercion
  debe("con wrap, count() satura en HIST_SIZE", h.count() === HIST_SIZE, h.count());
  // Las EXTRA primeras (ts 0..9) fueron sobrescritas; la mas antigua viva es ts=EXTRA.
  debe("con wrap, get(0) es la mas antigua VIVA, no una ya sobrescrita",
    h.get(0).ts === EXTRA, h.get(0));
  debe("con wrap, get(HIST_SIZE-1) es la ultima guardada",
    h.get(HIST_SIZE - 1).ts === HIST_SIZE + EXTRA - 1, h.get(HIST_SIZE - 1));
  debe("con wrap, get(HIST_SIZE) fuera de rango devuelve null",
    h.get(HIST_SIZE) === null, h.get(HIST_SIZE));
}

// ajustarTs(): solo toca las muestras con ts < TS_EPOCH_MIN.
{
  const h = new Historial();
  h.guardar(500);                    // segundos desde arranque: se debe tocar
  h.guardar(TS_EPOCH_MIN + 200);     // ya es epoch real: NO se debe tocar
  h.guardar(999999);                 // segundos desde arranque: se debe tocar
  const desfase = TS_EPOCH_MIN + 100000;
  h.ajustarTs(desfase);
  debe("ajustarTs toca la muestra con ts < TS_EPOCH_MIN",
    h.get(0).ts === 500 + desfase, h.get(0));
  debe("ajustarTs deja intacta la muestra con ts >= TS_EPOCH_MIN",
    h.get(1).ts === TS_EPOCH_MIN + 200, h.get(1));
  debe("ajustarTs toca la segunda muestra con ts < TS_EPOCH_MIN",
    h.get(2).ts === 999999 + desfase, h.get(2));
}

// ajustarTs() tambien funciona correctamente tras un wrap-around (el
// motivo original del bug del calculo dentro del bucle).
{
  const h = new Historial();
  const EXTRA = 3;
  for (let i = 0; i < HIST_SIZE + EXTRA; i++) h.guardar(100 + i); // todas < TS_EPOCH_MIN
  const desfase = TS_EPOCH_MIN;
  const antes = [];
  for (let i = 0; i < h.count(); i++) antes.push(h.get(i).ts);
  h.ajustarTs(desfase);
  let ok = true;
  for (let i = 0; i < h.count(); i++) {
    if (h.get(i).ts !== antes[i] + desfase) ok = false;
  }
  debe("ajustarTs con wrap-around desplaza TODAS las muestras vivas por igual", ok,
    { antes, despues: Array.from({length: h.count()}, (_, i) => h.get(i).ts) });
}

console.log(fallos ? "\n" + fallos + " FALLOS" : "\nTodo OK");
process.exit(fallos ? 1 : 0);
