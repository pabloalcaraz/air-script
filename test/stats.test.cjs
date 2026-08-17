// Banco de pruebas temporal: port 1:1 de los algoritmos de stats.cpp para
// comprobar formulas y unidades sin tener que subir el firmware.
// Valida el ALGORITMO, no el binario compilado.

const CO2_EXTERIOR_PPM = 430, ACH_MIN_MUESTRAS = 10, ACH_TOLERANCIA = 12;
const ACH_SALTO_MIN = 200, ACH_BAJADA_MIN = 60, ACH_R2_MIN = 0.80, ACH_MAX = 20.0;
const NULO = null;

// ---------- p95, tal cual esta en unaMagnitud() ----------
function p95(vals) {
  const s = vals.slice().sort((a, b) => a - b);
  const c = s.length;
  if (!c) return NaN;
  const pos = 0.95 * (c - 1), k = Math.floor(pos);
  return (k + 1 < c) ? s[k] + (pos - k) * (s[k + 1] - s[k]) : s[k];
}
// PERCENTILE.INC de una hoja de calculo, definicion oficial.
function percentilInc(vals, p) {
  const s = vals.slice().sort((a, b) => a - b), n = s.length;
  const r = p * (n - 1), lo = Math.floor(r);
  return lo + 1 < n ? s[lo] + (r - lo) * (s[lo + 1] - s[lo]) : s[lo];
}

// ---------- ACH, port literal de estadisticasAch() ----------
function ach(co2) {           // array con null donde el sensor estaba caido
  const total = co2.length;
  if (total < ACH_MIN_MUESTRAS) return NaN;

  let iniRun = -1, prevIdx = -1, mejorIni = -1, mejorFin = -1, prev = 0;
  for (let i = 0; i < total; i++) {
    const v = co2[i];
    if (v === NULO) {
      if (iniRun >= 0 && prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) {
        mejorIni = iniRun; mejorFin = prevIdx;
      }
      iniRun = -1; prevIdx = -1;
      continue;
    }
    if (prevIdx < 0) iniRun = i;
    else if (v > prev + ACH_TOLERANCIA) {
      if (prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) { mejorIni = iniRun; mejorFin = prevIdx; }
      iniRun = i;
    }
    prev = v; prevIdx = i;
  }
  if (iniRun >= 0 && prevIdx - iniRun + 1 >= ACH_MIN_MUESTRAS) {
    mejorIni = iniRun; mejorFin = prevIdx;
  }
  if (mejorIni < 0) return NaN;

  let sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0, n = 0, c0 = 0, cFin = 0;
  for (let i = mejorIni; i <= mejorFin; i++) {
    const v = co2[i];
    if (v === NULO) continue;
    const exceso = v - CO2_EXTERIOR_PPM;
    if (exceso < 1.0) break;
    if (!n) c0 = v;
    cFin = v;
    const x = (i - mejorIni) / 60.0, y = Math.log(exceso);
    sx += x; sy += y; sxx += x * x; sxy += x * y; syy += y * y; n++;
  }
  if (n < ACH_MIN_MUESTRAS) return NaN;
  if (c0 - CO2_EXTERIOR_PPM < ACH_SALTO_MIN) return NaN;
  if (c0 - cFin < ACH_BAJADA_MIN) return NaN;

  const den = n * sxx - sx * sx, denY = n * syy - sy * sy;
  if (den < 1e-12 || denY < 1e-12) return NaN;
  const num = n * sxy - sx * sy, r2 = (num * num) / (den * denY);
  if (r2 < ACH_R2_MIN) return NaN;
  const a = -num / den;
  if (a <= 0 || a > ACH_MAX) return NaN;
  return a;
}

// ---------- utilidades de prueba ----------
let fallos = 0;
function comp(nombre, real, esperado) {
  const ok = esperado === "NaN" ? Number.isNaN(real) : Math.abs(real - esperado) < 1e-9;
  console.log((ok ? "  ok   " : "  FALLO") + " " + nombre + " = " +
    (Number.isNaN(real) ? "NaN" : real));
  if (!ok) fallos++;
}
function cerca(nombre, real, esperado, tol) {
  const ok = Math.abs(real - esperado) <= tol;
  console.log((ok ? "  ok   " : "  FALLO") + " " + nombre + " = " +
    (Number.isNaN(real) ? "NaN" : real.toFixed(3)) +
    " (esperado " + esperado + " ±" + tol + ")");
  if (!ok) fallos++;
}
function esNaN(nombre, real) {
  const ok = Number.isNaN(real);
  console.log((ok ? "  ok   " : "  FALLO") + " " + nombre + " = " +
    (Number.isNaN(real) ? "NaN (rechazado, correcto)" : real));
  if (!ok) fallos++;
}
// Generador reproducible: sin semilla fija, un fallo no se podria repetir.
let sem = 12345;
const rnd = () => ((sem = (sem * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);

// ================= p95 =================
console.log("\n--- P95 contra PERCENTILE.INC de hoja de calculo ---");
const muestras = Array.from({ length: 1440 }, () => Math.round(rnd() * 200) / 10);
comp("p95 (1440 valores) igual que PERCENTILE.INC",
  p95(muestras) - percentilInc(muestras, 0.95), 0);
comp("p95 de [5] (un solo valor)", p95([5]), 5);
comp("p95 de [1,2] interpolado", p95([1, 2]), 1.95);
comp("p95 de 20 enteros 1..20", p95(Array.from({ length: 20 }, (_, i) => i + 1)), 19.05);

// ================= ACH =================
console.log("\n--- ACH contra decaimiento sintetico de tasa conocida ---");
// C(t) = C_ext + (C0 - C_ext) * e^(-ACH*t), t en horas, 1 muestra por minuto.
// Se redondea a entero porque el historial guarda uint16, igual que el sensor.
function decaimiento(achReal, c0, minutos, ruido) {
  return Array.from({ length: minutos }, (_, i) => {
    const t = i / 60;
    const v = CO2_EXTERIOR_PPM + (c0 - CO2_EXTERIOR_PPM) * Math.exp(-achReal * t);
    return Math.round(v + (ruido ? (rnd() - 0.5) * 2 * ruido : 0));
  });
}
cerca("ACH 0.80 limpio, 40 min", ach(decaimiento(0.8, 1600, 40, 0)), 0.80, 0.02);
cerca("ACH 0.80 con ruido ±8 ppm", ach(decaimiento(0.8, 1600, 40, 8)), 0.80, 0.10);
cerca("ACH 2.50 (ventanas abiertas), 20 min", ach(decaimiento(2.5, 1800, 20, 5)), 2.50, 0.25);
cerca("ACH 0.30 (casa sellada), 90 min", ach(decaimiento(0.3, 1500, 90, 5)), 0.30, 0.05);

console.log("\n--- ACH: casos que DEBE rechazar ---");
esNaN("aire plano a 1000 ppm 60 min", ach(Array(60).fill(1000)));
esNaN("CO2 subiendo (habitacion ocupandose)",
  ach(Array.from({ length: 60 }, (_, i) => 500 + i * 10)));
esNaN("bajada de solo 6 min", ach(decaimiento(0.8, 1600, 6, 0)));
esNaN("salto insuficiente sobre el fondo (500->470)",
  ach(Array.from({ length: 40 }, (_, i) => Math.round(500 - i * 0.75))));
esNaN("historial demasiado corto", ach([900, 880, 860]));
esNaN("todo por debajo del fondo exterior", ach(Array(40).fill(420)));

console.log("\n--- ACH: robustez ---");
// Un hueco por sensor caido parte el tramo: el algoritmo debe usar solo el
// trozo posterior, no coser los dos lados como si fueran continuos.
const partido = decaimiento(0.8, 1600, 15, 0)
  .concat([null, null, null])
  .concat(decaimiento(1.5, 1500, 30, 0));
cerca("con hueco en medio usa el tramo posterior (1.50)", ach(partido), 1.50, 0.10);
// Dos ventilaciones: interesa la mas reciente, no la mas larga.
const dos = decaimiento(0.4, 1700, 80, 0)
  .concat(Array.from({ length: 30 }, (_, i) => 700 + i * 20))   // sube: corta
  .concat(decaimiento(2.0, 1300, 25, 0));
cerca("con dos ventilaciones coge la mas reciente (2.00)", ach(dos), 2.00, 0.15);

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
