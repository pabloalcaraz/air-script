// Banco de pruebas del ciclo de duty-cycle del PMS5003 (Fase E). Port a JS de
// la maquina de estados de sensoresLeerPms() en sensors.cpp: valida el
// METODO, no el binario (aqui no hay compilador de host, solo el cruzado de
// xtensa). Tocar el .cpp obliga a tocar esto.
//
// Constantes iguales a las de config.h en el momento de escribir esto. Si se
// tocan alli, hay que tocarlas aqui tambien.
const INT_MUESTRA    = 60000;
const PMS_CALENTAR_MS = 30000;
const PMS_PROMEDIO_MS = 5000;
const PMS_TIMEOUT_MS  = 10000;

// --- port de sensoresLeerPms() / pmsDespertar() / pmsDormir() ---------------

function crearSimulador() {
  const st = {
    valido: false, rancio: false, durmiendo: false,
    pm1: 0, pm25: 0, pm10: 0, frames: 0, ultimoFrameMs: 0,
  };
  let msCicloIni = 0, msDespertar = 0, pmsCalentando = false, msUltimoCrudo = 0;
  let sumPm1 = 0, sumPm25 = 0, sumPm10 = 0, nProm = 0;

  function despertar(ahora) {
    st.durmiendo = false;
    pmsCalentando = true;
    msDespertar = ahora;
    msUltimoCrudo = ahora;
    sumPm1 = sumPm25 = sumPm10 = 0;
    nProm = 0;
  }
  function dormir() { st.durmiendo = true; }

  function init(ahora) {
    msCicloIni = ahora;
    despertar(ahora);
  }

  // frame: null si no llega nada este tick, o {pm1,pm25,pm10}
  function tick(ahora, frame) {
    if (st.durmiendo && (ahora - msCicloIni >= INT_MUESTRA)) {
      msCicloIni = ahora;
      despertar(ahora);
    }

    const huboFrame = frame !== null;
    if (huboFrame) msUltimoCrudo = ahora;

    if (!st.durmiendo) {
      if (pmsCalentando) {
        if (ahora - msDespertar >= PMS_CALENTAR_MS) pmsCalentando = false;
      } else {
        if (huboFrame) {
          sumPm1 += frame.pm1; sumPm25 += frame.pm25; sumPm10 += frame.pm10;
          nProm++;
        }
        if (ahora - msDespertar >= PMS_CALENTAR_MS + PMS_PROMEDIO_MS) {
          if (nProm > 0) {
            st.valido = true;
            st.pm1  = sumPm1  / nProm;
            st.pm25 = sumPm25 / nProm;
            st.pm10 = sumPm10 / nProm;
            st.ultimoFrameMs = ahora;
            st.frames++;
          }
          dormir();
        }
      }
    }

    st.rancio = !st.durmiendo && st.valido && (ahora - msUltimoCrudo > PMS_TIMEOUT_MS);
  }

  return { st, init, tick };
}

// --- arnes --------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre + (cond ? "" : "  <-- " + visto));
  if (!cond) fallos++;
}
function igual(nombre, a, b) {
  debe(nombre + " = " + b, a === b, String(a));
}

// --- Ciclo normal, sensor sano, frames a 1 Hz -------------------------------

console.log("--- Ciclo normal ---");
{
  const s = crearSimulador();
  s.init(0);
  igual("arranca despierto", s.st.durmiendo, false);

  // Frames continuos a 1 Hz durante toda la fase despierta (calentamiento +
  // promedio). El valor real no importa para el calentamiento: se descarta.
  for (let t = 1000; t <= 34000; t += 1000) {
    s.tick(t, { pm1: 3, pm25: 10, pm10: 15 });
  }
  debe("durante el calentamiento no hay dato valido todavia",
    s.st.valido === false, String(s.st.valido));

  // Fin del calentamiento a los 30000: el frame de esa marca se descarta (cae
  // en el tick que apaga pmsCalentando, no en el que acumula). Los cinco
  // siguientes (31000..35000) entran en el promedio.
  for (let t = 35000; t <= 35000; t += 1000) s.tick(t, { pm1: 3, pm25: 10, pm10: 15 });

  igual("promedio aceptado tras calentar+promediar", s.st.valido, true);
  igual("PM2.5 promediado", s.st.pm25, 10);
  igual("frames aceptados = 1 ciclo", s.st.frames, 1);
  igual("se duerme justo al terminar el promedio", s.st.durmiendo, true);

  // Silencio total durante el sueno: no debe dispararse rancio en ningun
  // instante. Esta es LA prueba critica de la Fase E.
  let huboRancioDormido = false;
  for (let t = 36000; t < 60000; t += 1000) {
    s.tick(t, null);
    if (s.st.rancio) huboRancioDormido = true;
  }
  debe("nunca se marca rancio mientras duerme", !huboRancioDormido, "si se marco");

  // A los 60000 (60s desde el ULTIMO despertar) toca despertar otra vez.
  s.tick(60000, { pm1: 3, pm25: 12, pm10: 18 });
  igual("despierta de nuevo al cumplirse INT_MUESTRA", s.st.durmiendo, false);
  igual("el dato del ciclo anterior no se pierde al despertar", s.st.pm25, 10);

  for (let t = 61000; t <= 95000; t += 1000) {
    s.tick(t, { pm1: 3, pm25: 20, pm10: 25 });
  }
  igual("segundo ciclo tambien promedia bien", s.st.pm25, 20);
  igual("segundo promedio contado", s.st.frames, 2);
}

// --- Sensor que nunca llega a entregar nada ---------------------------------

console.log("\n--- Ventana de promedio sin ni un frame ---");
{
  const s = crearSimulador();
  s.init(0);
  for (let t = 1000; t <= 35000; t += 1000) s.tick(t, null);
  igual("no inventa una lectura de la nada", s.st.valido, false);
  igual("frames sigue en 0", s.st.frames, 0);
  igual("igualmente se va a dormir", s.st.durmiendo, true);
}

// --- Averia real tras un ciclo bueno: debe delatarse en <2 min -------------

console.log("\n--- Averia real (sensor desconectado a mitad de servicio) ---");
{
  const s = crearSimulador();
  s.init(0);
  // Un ciclo bueno primero, para que valido quede en true (como un aparato
  // que lleva dias funcionando y de repente pierde el sensor).
  for (let t = 1000; t <= 35000; t += 1000) s.tick(t, { pm1: 1, pm25: 8, pm10: 9 });
  igual("ciclo bueno de referencia", s.st.valido, true);

  // Se desconecta justo al dormirse: el peor caso, duerme la ventana entera
  // antes de volver a intentarlo.
  for (let t = 36000; t < 60000; t += 1000) s.tick(t, null);
  debe("sigue sin rancio mientras duerme (esperado)", !s.st.rancio, "se marco antes de tiempo");

  // Despierta a los 60000 y sigue sin dar ni un frame.
  let msRancio = null;
  for (let t = 60000; t <= 60000 + PMS_TIMEOUT_MS + 1000; t += 1000) {
    s.tick(t, null);
    if (s.st.rancio && msRancio === null) msRancio = t;
  }
  debe("se marca rancio tras el timeout despierto", msRancio !== null, "nunca se marco");
  const peorCasoMs = INT_MUESTRA + PMS_TIMEOUT_MS;  // desconexion justo al dormirse
  debe("el peor caso cae bajo el limite de 2 min del criterio de aceptacion",
    peorCasoMs < 120000, String(peorCasoMs));
}

// --- Duerme y averiado nunca coinciden --------------------------------------

console.log("\n--- durmiendo y rancio son mutuamente excluyentes ---");
{
  const s = crearSimulador();
  s.init(0);
  let violacion = false;
  for (let t = 1000; t <= 200000; t += 500) {
    // Frames aleatorios pero deterministas: unos ciclos con dato, otros sin,
    // para pasar por todas las combinaciones de estado.
    const frame = (t % 7000 < 3000) ? { pm1: 2, pm25: 9, pm10: 11 } : null;
    s.tick(t, frame);
    if (s.st.durmiendo && s.st.rancio) violacion = true;
  }
  debe("nunca durmiendo=true y rancio=true a la vez", !violacion, "coincidieron");
}

// --- Cordura de las constantes ---------------------------------------------

console.log("\n--- Constantes ---");
debe("la fase despierta cabe dentro de INT_MUESTRA (si no, nunca duerme)",
  PMS_CALENTAR_MS + PMS_PROMEDIO_MS < INT_MUESTRA,
  (PMS_CALENTAR_MS + PMS_PROMEDIO_MS) + " >= " + INT_MUESTRA);

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
