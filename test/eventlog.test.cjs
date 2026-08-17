// Banco de pruebas del buffer circular + antirrebote de eventlog.cpp. Port a
// JS de logEvento(): valida el METODO, no el binario (en esta maquina no hay
// compilador de host, solo el cruzado de xtensa). Tocar eventlog.cpp obliga a
// tocar esto, o las dos versiones dejan de decir lo mismo.

const LOG_SIZE = 80;          // config.h

const LOG_INFO = 0, LOG_AVISO = 1, LOG_ERROR = 2;

// --- port de eventlog.cpp -----------------------------------------------

class EventLog {
  constructor() {
    this.buf = new Array(LOG_SIZE).fill(null);
    this.cabeza = 0;
    this.total = 0;
  }

  // Replica logEvento(): antirrebote si nivel y msg coinciden con el ultimo.
  logEvento(nivel, msg, ts = 0) {
    if (this.total > 0) {
      const ult = this.buf[(this.cabeza + LOG_SIZE - 1) % LOG_SIZE];
      if (ult.nivel === nivel && ult.msg === msg) {
        if (ult.repes < 65535) ult.repes++;
        ult.ts = ts;
        return;
      }
    }

    const e = { ts, nivel, repes: 1, msg };
    this.buf[this.cabeza] = e;
    this.cabeza = (this.cabeza + 1) % LOG_SIZE;
    if (this.total < LOG_SIZE) this.total++;
  }

  count() { return this.total; }

  get(i) {
    if (i >= this.total) return null;
    const inicio = (this.total === LOG_SIZE) ? this.cabeza : 0;
    return this.buf[(inicio + i) % LOG_SIZE];
  }
}

// --- pruebas ------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + JSON.stringify(visto)));
  if (!cond) fallos++;
}

console.log("--- Buffer circular + antirrebote de eventlog.cpp ---");

// 200 mensajes identicos: una sola entrada con repes=200.
{
  const log = new EventLog();
  for (let i = 0; i < 200; i++) log.logEvento(LOG_ERROR, "SCD41: NO MIDE", i);
  debe("200 mensajes identicos dejan UNA sola entrada", log.count() === 1, log.count());
  debe("el contador de repeticiones llega a 200", log.get(0).repes === 200, log.get(0));
}

// Mismo texto, nivel distinto: rompe la deduplicacion.
{
  const log = new EventLog();
  log.logEvento(LOG_AVISO, "PMS sin frames", 1);
  log.logEvento(LOG_ERROR, "PMS sin frames", 2); // mismo texto, otro nivel
  debe("nivel distinto con mismo texto crea entrada nueva", log.count() === 2, log.count());
  debe("la primera entrada conserva su nivel y repes=1",
    log.get(0).nivel === LOG_AVISO && log.get(0).repes === 1, log.get(0));
  debe("la segunda entrada tiene el nuevo nivel y repes=1",
    log.get(1).nivel === LOG_ERROR && log.get(1).repes === 1, log.get(1));
}

// El contador de repeticiones satura en 65535, no da la vuelta a 0.
{
  const log = new EventLog();
  const N = 65535 + 500;
  for (let i = 0; i < N; i++) log.logEvento(LOG_INFO, "reintento", i);
  debe("el contador de repeticiones satura en 65535", log.get(0).repes === 65535, log.get(0).repes);
}

// El buffer circular tambien da la vuelta correctamente con mensajes distintos.
{
  const log = new EventLog();
  const EXTRA = 5;
  for (let i = 0; i < LOG_SIZE + EXTRA; i++) log.logEvento(LOG_INFO, "evento " + i, i);
  debe("con wrap, count() satura en LOG_SIZE", log.count() === LOG_SIZE, log.count());
  debe("con wrap, get(0) es el mas antiguo VIVO, no uno ya sobrescrito",
    log.get(0).msg === "evento " + EXTRA, log.get(0));
  debe("con wrap, get(LOG_SIZE-1) es el ultimo insertado",
    log.get(LOG_SIZE - 1).msg === "evento " + (LOG_SIZE + EXTRA - 1), log.get(LOG_SIZE - 1));
}

console.log(fallos ? "\n" + fallos + " FALLOS" : "\nTodo OK");
process.exit(fallos ? 1 : 0);
