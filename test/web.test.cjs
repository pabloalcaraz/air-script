// Banco de pruebas temporal del JS del dashboard. No forma parte del sketch.
const fs = require("fs"), vm = require("vm");

// Guarda: webpage_gz.h es lo que sirve el firmware. Si se ha tocado webpage.h
// sin regenerarlo, el aparato seguiria sirviendo la version vieja y el cambio
// pareceria "no aplicarse". Es el fallo mas caro de depurar de este proyecto.
require("child_process").execFileSync(process.execPath,
  [__dirname + "/../tools/gzip-web.cjs", "--check"], { stdio: "inherit" });

const t = fs.readFileSync(__dirname+"/../webpage.h", "utf8");
const src = t.match(/<script>\n([\s\S]*?)\n<\/script>/)[1];

const els = {};
const mkEl = id => els[id] || (els[id] = {
  id, innerHTML: "", textContent: "", className: "", hidden: false,
  disabled: false, dataset: {}, value: "420",
  classList: { toggle() {}, add() {}, remove() {} },
  getContext() { return {}; }, clientWidth: 600, clientHeight: 320
});
const ctx = {
  console, Math, JSON, Date, String, Number, Array, Object,
  parseInt, parseFloat, isNaN,
  setTimeout: () => {}, setInterval: () => {}, confirm: () => false,
  document: { querySelector: s => mkEl(s), querySelectorAll: () => [] },
  window: { addEventListener() {}, devicePixelRatio: 1 },
  fetch: async () => ({ ok: true, status: 200, json: async () => ({ series: {} }) })
};
vm.createContext(ctx);
new vm.Script(src).runInContext(ctx);

const limpio = s => s.replace(/<[^>]+>/g, " ").replace(/&nbsp;/g, " ")
                      .replace(/\s+/g, " ").trim();
let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre +
    (cond ? "" : "  <-- " + visto));
  if (!cond) fallos++;
}
// Un cero o un "undefined" colados en la interfaz son el fallo tipico de este
// tipo de panel: se leen como medida real. Se comprueban explicitamente.
function sinBasura(nombre, html) {
  debe(nombre + ": sin undefined/NaN/null visibles",
    !/undefined|NaN|>null</.test(html), limpio(html).slice(0, 80));
}
const serie = (n, min, med, p95, max, tend, pct) =>
  ({ n, min, media: med, p95, max, tend_h: tend, pct });

// --- /api/stats con datos representativos ---
const S = {
  range: "24h", muestras: 812, series: {
    pm1:  serie(800, 0, 4.2, 11, 19, 1.2, null),
    pm25: serie(800, 0, 6.1, 18.4, 63, 2.5, [88.4, 9.1, 2.5]),
    pm10: serie(800, 1, 9.8, 24.0, 71, 3.1, [95.0, 4.0, 1.0]),
    co2:  serie(790, 412, 812, 1490, 1902, -38.4, [62.0, 30.5, 7.5]),
    temp: serie(790, 19.2, 23.4, 26.1, 27.0, 0.2, null),
    hum:  serie(790, 31.0, 45.2, 58.0, 61.4, -0.4, [97.0, 3.0, 0.0])
  },
  pm25_24h: 6.1, pm10_24h: 9.8, n_pm25_24h: 0, n_pm10_24h: 0,
  ach: 0.62, sin_ventilar_min: 187, co2_ext: 430, co2_ventilado: 600,
  umbral_media24: 60
};
console.log("\n--- Estadisticas con datos ---");
ctx.pintarEst(S);
debe("media 24 h de PM2.5 con su límite OMS",
  /6\.1 µg\/m³/.test(els["#m24"].innerHTML) && /OMS 15/.test(els["#m24"].innerHTML),
  limpio(els["#m24"].innerHTML).slice(0, 90));
debe("ACH 0.62 calificado como Aceptable",
  /0\.62 ren\/h/.test(els["#vent"].innerHTML) && /Aceptable/.test(els["#vent"].innerHTML),
  limpio(els["#vent"].innerHTML).slice(0, 90));
debe("187 min se muestran como 3 h 7 min",
  /3 h 7 min/.test(els["#vent"].innerHTML), limpio(els["#vent"].innerHTML).slice(0, 120));
debe("tabla con cabecera + 6 magnitudes",
  (els["#tstats"].innerHTML.match(/<tr>/g) || []).length === 7,
  (els["#tstats"].innerHTML.match(/<tr>/g) || []).length + " filas");
// PM1.0 y temperatura no tienen umbrales: no pueden salir en el reparto de
// tiempo por nivel o estarian afirmando un veredicto inexistente.
debe("reparto por nivel solo para las 4 magnitudes con umbral",
  (els["#pcts"].innerHTML.match(/class='stack'/g) || []).length === 4,
  (els["#pcts"].innerHTML.match(/class='stack'/g) || []).length + " barras");
sinBasura("estadisticas", els["#tstats"].innerHTML + els["#vent"].innerHTML);

// --- historial vacio: recien arrancado ---
const vacia = () => ({ n: 0, min: null, media: null, p95: null, max: null, tend_h: null, pct: null });
const V = {
  range: "24h", muestras: 0, series: {
    pm1: vacia(), pm25: vacia(), pm10: vacia(),
    co2: vacia(), temp: vacia(), hum: vacia()
  },
  pm25_24h: null, pm10_24h: null, n_pm25_24h: null, n_pm10_24h: null,
  ach: null, sin_ventilar_min: null, co2_ext: 430, co2_ventilado: 600,
  umbral_media24: 60
};
console.log("\n--- Estadisticas con el historial vacio ---");
ctx.pintarEst(V);
debe("sin media de 24 h se explica por qué, no se pinta un 0",
  /sin datos suficientes/.test(els["#m24"].innerHTML) &&
  !/0\.0 µg/.test(els["#m24"].innerHTML), limpio(els["#m24"].innerHTML).slice(0, 90));
debe("sin ACH se dice que no se puede medir",
  /no hay ninguna bajada/.test(els["#vent"].innerHTML),
  limpio(els["#vent"].innerHTML).slice(0, 90));
debe("sin ventilación nunca registrada no se inventa un número",
  /no ha bajado de 600/.test(els["#vent"].innerHTML),
  limpio(els["#vent"].innerHTML).slice(0, 120));
debe("reparto de tiempo vacío", /Sin datos todavía/.test(els["#pcts"].innerHTML),
  limpio(els["#pcts"].innerHTML).slice(0, 60));
debe("tabla con todo a guiones, sin ceros",
  (els["#tstats"].innerHTML.match(/>—</g) || []).length === 30,
  (els["#tstats"].innerHTML.match(/>—</g) || []).length + " guiones");
sinBasura("vacio", els["#tstats"].innerHTML + els["#vent"].innerHTML + els["#m24"].innerHTML);

// --- /api/now ---
const now = scdOk => ({
  ts: 1000, hora_ok: false, uptime_s: 3600,
  pms: { ok: true, estado: 1, pm1: 4, pm25: 7, pm10: 11, n_pm25: 0, n_pm10: 0, frames: 900, rancio: false },
  scd: {
    ok: scdOk, estado: scdOk ? 1 : 2, co2: 812, temp: 23.4, hum: 45.2,
    n_co2: 1, n_temp: 0, n_hum: 0, reads: 5100, error: 0, etapa: "midiendo",
    i2c: true, rancio: !scdOk, congelado: false, hace_s: 340, reinicios: 2
  },
  m24: { pm25: 6.1, pm10: 9.8, n_pm25: 0, n_pm10: 0 },
  peor: { nivel: 1, que: "CO2" }
});
// tend es un `let` del script: no se puede reescribir desde fuera del vm, hay
// que rellenarlo por el camino real (refrescarTend -> fetch -> /api/stats).
ctx.fetch = async () => ({ ok: true, status: 200, json: async () => S });

(async () => {
  console.log("\n--- Tarjetas de Ahora y derivados ---");
  await ctx.refrescarTend();
  ctx.pintarAhora(now(true));
  const f = (els["#cards"].innerHTML.match(/class="tend"[^>]*>(.)</g) || [])
    .map(x => x.slice(-2, -1));
  // Umbrales de RUIDO: pm1 1.2<2 →, pm25 2.5>=2 ↑, pm10 3.1>=3 ↑,
  // co2 -38.4 ↓, temp 0.2<0.3 →, hum -0.4<1 →.
  debe("flechas de tendencia respetan el suelo de ruido de cada magnitud",
    f.join("") === "→↑↑↓→→", f.join(" ") || "ninguna");

  // Referencia: 23.4 °C y 45.2 % HR -> rocio 10.9 °C, absoluta 9.5 g/m³.
  debe("punto de rocío calculado", /10\.9 °C/.test(els["#deriv"].innerHTML),
    limpio(els["#deriv"].innerHTML).slice(0, 70));
  debe("humedad absoluta calculada", /9\.5 g\/m³/.test(els["#deriv"].innerHTML),
    limpio(els["#deriv"].innerHTML).slice(0, 70));
  sinBasura("derivados", els["#deriv"].innerHTML);

  // Con el SCD41 caído no hay T ni HR: el punto de rocío no puede salir de la
  // última lectura buena, tiene que desaparecer.
  ctx.pintarAhora(now(false));
  debe("SCD41 caído: sin punto de rocío inventado",
    /sin datos del SCD41/.test(els["#deriv"].innerHTML) &&
    !/°C/.test(els["#deriv"].innerHTML), limpio(els["#deriv"].innerHTML).slice(0, 80));

  const n0 = now(true);
  n0.m24 = { pm25: null, pm10: null, n_pm25: null, n_pm10: null };
  ctx.pintarAhora(n0);
  debe("sin media de 24 h todavía se explica, no se pinta 0",
    /sin datos suficientes/.test(els["#deriv"].innerHTML),
    limpio(els["#deriv"].innerHTML).slice(0, 90));
  sinBasura("sin m24", els["#deriv"].innerHTML);

  // --- Aire exterior ---
  console.log("\n--- Aire exterior ---");
  const EXT = {
    configurado: true, lugar: "Madrid", lat: 40.4168, lon: -3.7038,
    valido: true, rancio: false, hace_s: 640, http: 200,
    intentos: 3, fallos: 0, meteo: true,
    pm25: 13.7, pm10: 25.3, o3: 116, no2: 5.7, temp: 37.5, hum: 19,
    aqi: 51, co2_ext: 430, cada_min: 30,
    fuente: "modelo", estacion: "", distancia_km: null
  };
  // `ext` es un `let` del script: igual que `tend`, hay que llenarlo por el
  // camino real (cargarExt -> fetch -> /api/exterior).
  const conExt = async o => {
    ctx.fetch = async () => ({ ok: true, status: 200, json: async () => o });
    await ctx.cargarExt();
  };
  await conExt(EXT);
  ctx.pintarAhora(now(true));   // interior: pm25=7, pm10=11, temp=23.4, hum=45.2
  const H = () => els["#ext"].innerHTML + els["#extnota"].innerHTML;
  debe("delta de PM2.5 dentro-fuera (7 - 13.7)",
    /-6\.7/.test(els["#ext"].innerHTML), limpio(els["#ext"].innerHTML).slice(0, 90));
  // 7 / 13.7 = 0.51: la casa filtra. Es la única cifra accionable del panel.
  debe("ratio I/O con veredicto de fuente exterior",
    /0\.51/.test(els["#extnota"].innerHTML) &&
    /dentro está mejor/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(0, 100));
  debe("el CO₂ de fuera se etiqueta como fondo atmosférico, no como medida",
    /fondo atmosférico \(430 ppm\)/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(-110));
  debe("se avisa de que es un modelo de rejilla, no una estación",
    /11 km/.test(els["#extnota"].innerHTML) &&
    /no una estación/.test(els["#extnota"].innerHTML), "sin la advertencia");
  sinBasura("exterior", H());

  // --- Fuente del dato exterior (AQICN vs modelo) ---
  await conExt(Object.assign({}, EXT, {
    fuente: "aqicn", estacion: "Méndez Álvaro", distancia_km: 1.2
  }));
  ctx.pintarAhora(now(true));
  ctx.pintarExtInfo();
  debe("con fuente AQICN, la nota habla de estación real, no de modelo",
    /Estación real/.test(els["#extnota"].innerHTML) &&
    !/rejilla de 11 km/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(-140));
  debe("fila Fuente muestra el nombre de estación y la distancia",
    /Méndez Álvaro/.test(els["#extinfo"].innerHTML) &&
    /1\.2 km/.test(els["#extinfo"].innerHTML),
    limpio(els["#extinfo"].innerHTML));
  sinBasura("fuente aqicn", els["#extnota"].innerHTML + els["#extinfo"].innerHTML);

  await conExt(Object.assign({}, EXT, { fuente: "modelo", estacion: "", distancia_km: null }));
  ctx.pintarAhora(now(true));
  ctx.pintarExtInfo();
  debe("con fuente modelo, se mantiene el aviso de rejilla de 11 km",
    /rejilla de 11 km/.test(els["#extnota"].innerHTML) &&
    /no una estación/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(-140));
  debe("fila Fuente dice modelo CAMS",
    /Modelo CAMS/.test(els["#extinfo"].innerHTML),
    limpio(els["#extinfo"].innerHTML));
  sinBasura("fuente modelo", els["#extnota"].innerHTML + els["#extinfo"].innerHTML);

  // Con el PMS caído no hay columna "dentro": el delta y el ratio tienen que
  // desaparecer, no calcularse contra la última lectura buena.
  const nPms = now(true); nPms.pms.ok = false; nPms.pms.estado = 2;
  ctx.pintarAhora(nPms);
  debe("PMS caído: sin ratio I/O inventado",
    !/Ratio/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(0, 80));
  sinBasura("exterior sin PMS", H());

  // La fuente está dentro: 40 dentro contra 13.7 fuera.
  const nAlto = now(true); nAlto.pms.pm25 = 40;
  ctx.pintarAhora(nAlto);
  debe("ratio > 1.25 culpa a una fuente interior",
    /la fuente está dentro/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(0, 100));

  // Sin fuente real: antes del primer sondeo bueno o tras un fallo sostenido.
  // No hay ni estación ni modelo, así que no se puede afirmar CAMS/Copernicus.
  await conExt(Object.assign({}, EXT, {
    configurado: true, valido: false, fuente: "ninguna",
    estacion: "", distancia_km: null, hace_s: 0
  }));
  ctx.pintarAhora(now(true));
  ctx.pintarExtInfo();
  debe("sin fuente real, fila Fuente muestra el guion por defecto",
    /<td class='k'>Fuente<\/td><td>—<\/td>/.test(els["#extinfo"].innerHTML),
    limpio(els["#extinfo"].innerHTML));
  debe("sin fuente real, la nota no atribuye el dato a CAMS/Copernicus",
    !/Copernicus/.test(els["#extdisc"].innerHTML) && !/CAMS/.test(els["#extdisc"].innerHTML),
    limpio(els["#extdisc"].innerHTML));
  sinBasura("fuente ninguna", els["#extinfo"].innerHTML + els["#extdisc"].innerHTML);

  await conExt({ configurado: false, lugar: "sin elegir", valido: false });
  ctx.pintarAhora(now(true));
  debe("sin localización elegida se explica dónde configurarla",
    /sin localización elegida/.test(els["#ext"].innerHTML) &&
    /Sistema/.test(els["#extnota"].innerHTML), limpio(H()).slice(0, 90));
  sinBasura("exterior sin configurar", H());

  await conExt({ configurado: true, lugar: "Madrid", valido: false, intentos: 4,
                 fallos: 4, http: -11, lat: 40.4, lon: -3.7, cada_min: 30 });
  ctx.pintarAhora(now(true));
  debe("fallo de red: se enseña el código HTTP, no ceros",
    /HTTP -11/.test(els["#ext"].innerHTML) && !/0\.0/.test(els["#ext"].innerHTML),
    limpio(els["#ext"].innerHTML).slice(0, 90));
  ctx.pintarExtInfo();
  debe("Sistema muestra los sondeos fallidos",
    /4 fallidos/.test(els["#extinfo"].innerHTML),
    limpio(els["#extinfo"].innerHTML).slice(0, 90));
  sinBasura("exterior con fallo", H() + els["#extinfo"].innerHTML);

  // Un nombre de ciudad viene de un servicio externo: no puede inyectar HTML.
  await conExt(Object.assign({}, EXT, { lugar: '<img src=x onerror=alert(1)>' }));
  ctx.pintarAhora(now(true));
  debe("el nombre del sitio se escapa antes de tocar innerHTML",
    !/<img/.test(els["#extnota"].innerHTML) && /&lt;img/.test(els["#extnota"].innerHTML),
    limpio(els["#extnota"].innerHTML).slice(0, 80));

  // Historial demasiado corto para trazar una recta: ninguna flecha.
  const Sn = JSON.parse(JSON.stringify(S));
  Object.keys(Sn.series).forEach(k => Sn.series[k].tend_h = null);
  ctx.fetch = async () => ({ ok: true, status: 200, json: async () => Sn });
  await ctx.refrescarTend();
  ctx.pintarAhora(now(true));
  debe("sin tendencia calculable no se pinta ninguna flecha",
    (els["#cards"].innerHTML.match(/class="tend"/g) || []).length === 0,
    (els["#cards"].innerHTML.match(/class="tend"/g) || []).length + " flechas");

  // --- Token AQICN (#btoken) ---
  console.log("\n--- Token AQICN ---");
  ctx.document.querySelector("#qtoken").value = "abc123token";
  ctx.fetch = async () =>
    ({ ok: true, status: 200, json: async () => ({ ok: true, msg: "Token guardado." }) });
  await els["#btoken"].onclick();
  debe("token guardado con éxito: mensaje ok en #tokenmsg",
    els["#tokenmsg"].className === "okmsg" && /Token guardado/.test(els["#tokenmsg"].textContent),
    els["#tokenmsg"].className + " / " + els["#tokenmsg"].textContent);

  ctx.fetch = async () =>
    ({ ok: true, status: 200, json: async () => ({ ok: false, msg: "Token no válido." }) });
  await els["#btoken"].onclick();
  debe("aparato rechaza el token: mensaje de error en #tokenmsg",
    els["#tokenmsg"].className === "err" && /Token no válido/.test(els["#tokenmsg"].textContent),
    els["#tokenmsg"].className + " / " + els["#tokenmsg"].textContent);

  ctx.fetch = async () => { throw new Error("network down"); };
  await els["#btoken"].onclick();
  debe("fallo de red al guardar token: mensaje de error genérico",
    els["#tokenmsg"].className === "err" && /No se pudo contactar/.test(els["#tokenmsg"].textContent),
    els["#tokenmsg"].className + " / " + els["#tokenmsg"].textContent);

  // --- Buscador: geocoder Nominatim (barrio) ---
  console.log("\n--- Buscador Nominatim ---");
  // Respuesta real capturada 2026-07-30 (nominatim.openstreetmap.org, "Delicias, Madrid").
  const NOMINATIM_DELICIAS = [
    {"place_id":293075422,"lat":"40.3967843","lon":"-3.6900384","addresstype":"quarter",
     "name":"Delicias","display_name":"Delicias, Arganzuela, Madrid, Comunidad de Madrid, 28045, España"},
    {"place_id":293221687,"lat":"40.3993312","lon":"-3.6941225","addresstype":"railway",
     "name":"Delicias","display_name":"Delicias, Paseo de las Delicias, Delicias, Arganzuela, Madrid, Comunidad de Madrid, 28045, España"}
  ];
  ctx.document.querySelector("#qlugar").value = "Delicias, Madrid";
  let urlPedida = "";
  ctx.fetch = async (url) => {
    urlPedida = url;
    return { ok: true, status: 200, json: async () => NOMINATIM_DELICIAS };
  };
  await els["#bbuscar"].onclick();
  debe("pide a Nominatim, no al geocoder de Open-Meteo",
    /nominatim\.openstreetmap\.org/.test(urlPedida) && !/geocoding-api/.test(urlPedida),
    urlPedida);
  debe("extrae barrio + distrito/ciudad + comunidad del display_name",
    /Delicias, Arganzuela, Madrid/.test(els["#extres"].innerHTML),
    limpio(els["#extres"].innerHTML));

  let latPedida = null, lonPedida = null;
  ctx.fetch = async (url) => {
    const m = /lat=([\d.-]+)&lon=([\d.-]+)/.exec(url);
    if (m) { latPedida = parseFloat(m[1]); lonPedida = parseFloat(m[2]); }
    return { ok: true, status: 200, json: async () => ({ ok: true, lugar: "x", msg: "Guardado." }) };
  };
  els["#extres"].onclick({ target: { dataset: { g: "0" } } });
  await new Promise(r => setTimeout(r, 0));
  debe("elegir el primer resultado guarda sus coordenadas exactas (no las del segundo)",
    latPedida === 40.3967843 && lonPedida === -3.6900384,
    latPedida + "," + lonPedida);

  els["#qlugar"].value = "Sitio Que No Existe Nunca Jamas Xyz";
  ctx.fetch = async () => ({ ok: true, status: 200, json: async () => [] });
  await els["#bbuscar"].onclick();
  debe("sin resultados de Nominatim, muestra mensaje de 'sin resultados'",
    /Sin resultados/.test(els["#extres"].innerHTML),
    limpio(els["#extres"].innerHTML));

  els["#qlugar"].value = "Delicias";
  ctx.fetch = async () => ({ ok: false, status: 429, json: async () => [] });
  await els["#bbuscar"].onclick();
  debe("HTTP de error de Nominatim (ej. 429) da mensaje específico, no 'necesita internet'",
    /429/.test(els["#extres"].innerHTML) && !/necesita internet/.test(els["#extres"].innerHTML),
    limpio(els["#extres"].innerHTML));

  // --- Banner "¿Abrir la ventana?" ---
  console.log("\n--- Banner de ventana ---");
  // pi = interior PM2.5, pe = exterior PM2.5, co2 = interior CO2.
  // Umbrales: PM25_AVISO=15, PM25_MALO=35, CO2_AVISO=800, CO2_MALO=1200.
  const conVentana = async (pi, pe, co2, extValido) => {
    const n = now(true);
    n.pms.pm25 = pi;
    n.scd.co2 = co2;
    await conExt(Object.assign({}, EXT, { valido: extValido !== false, pm25: pe }));
    ctx.pintarAhora(n);
    return els["#ventana"];
  };

  let v = await conVentana(10, 40, 500, true);   // fuera malo, CO2 normal
  debe("fuera malo + CO2 normal: no abras (nivel 2)",
    /No abras/.test(v.innerHTML) && v.className === "panel v2", v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(10, 40, 900, true);        // fuera malo, CO2 viciado pero no muyViciado (el hueco cerrado)
  debe("fuera malo + CO2 medio-viciado: sigue siendo no abras, no 'da igual'",
    /No abras/.test(v.innerHTML) && v.className === "panel v2", v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(10, 40, 1300, true);       // fuera malo, CO2 muy viciado
  debe("fuera malo + CO2 muy viciado: ventila poco y rápido (nivel 1)",
    /Ventila poco y rápido/.test(v.innerHTML) && v.className === "panel v1", v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(20, 10, 500, true);        // ratio 2.0 > 1.25, fuera limpio
  debe("fuente de partículas dentro: abre (ratio>1.25)",
    /fuente de partículas está dentro/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(9, 10, 900, true);         // ratio 0.9, fuera limpio, CO2 viciado
  debe("fuera limpio + CO2 viciado: abre para ventilar",
    /para bajar el CO₂/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(5, 10, 500, true);         // ratio 0.5 < 0.8, sin CO2 viciado
  debe("dentro claramente mejor: mantén cerrada",
    /Mantén cerrada/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(20, 20, 500, true);        // ratio 1.0, ninguno malo/viciado
  debe("parejos: da igual, no urge",
    /Da igual/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(10, 20, 1300, true);       // fuera en aviso (15<=pe<35), CO2 muy viciado
  debe("fuera en aviso + CO2 muy viciado: avisa igual, no 'da igual' (nivel 1)",
    /Ventila poco y rápido/.test(v.innerHTML) && /aunque fuera no esté tan mal/.test(v.innerHTML)
    && v.className === "panel v1", v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(40, 0.5, 500, true);       // pe casi cero, pi por encima de PM25_MALO
  debe("pe casi cero + pi muy alto: usa umbral absoluto, no ratio (abre, nivel 0)",
    /fuente de partículas está dentro/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  v = await conVentana(5, 0.5, 500, true);        // pe casi cero, pi bajo: no hay urgencia real
  debe("pe casi cero + pi bajo: no fuerza un veredicto de apertura",
    /Da igual/.test(v.innerHTML) && v.className === "panel v0",
    v.className + " | " + limpio(v.innerHTML));

  // Falta dato interior: PMS caído.
  const nSinPms = now(true); nSinPms.pms.ok = false; nSinPms.pms.estado = 2;
  await conExt(EXT);
  ctx.pintarAhora(nSinPms);
  debe("PMS caído: pide dato interior, no inventa un veredicto",
    /Falta dato interior \(PMS5003\)/.test(els["#ventana"].innerHTML),
    limpio(els["#ventana"].innerHTML));

  // Falta dato exterior: sin localización o sondeo fallido.
  await conExt({ configurado: false, lugar: "sin elegir", valido: false });
  ctx.pintarAhora(now(true));
  debe("sin dato exterior: pide dato exterior, no inventa un veredicto",
    /Falta dato exterior/.test(els["#ventana"].innerHTML),
    limpio(els["#ventana"].innerHTML));

  // Dato exterior rancio (caducado): aunque valido siga en true, no debe
  // usarse para el veredicto — mismo trato que "sin dato exterior".
  const nRancio = now(true); nRancio.pms.pm25 = 40; // interior muy sucio
  await conExt(Object.assign({}, EXT, { valido: true, rancio: true, pm25: 5 }));
  ctx.pintarAhora(nRancio);
  debe("dato exterior rancio: pide dato exterior, no da un veredicto con dato caducado",
    /Falta dato exterior/.test(els["#ventana"].innerHTML),
    limpio(els["#ventana"].innerHTML));

  sinBasura("ventana", els["#ventana"].innerHTML);

  // Primero, sin que /api/now haya publicado nunca "umbrales": UMBRAL sigue
  // en el valor de arranque en frío (co2_aviso=800) y CO2=700 no dispara nada.
  const nDefecto = now(true);
  nDefecto.pms.pm25 = 9; nDefecto.scd.co2 = 700;
  await conExt(Object.assign({}, EXT, { valido: true, rancio: false, pm25: 10 }));
  ctx.pintarAhora(nDefecto);
  debe("sin umbrales publicados, se mantienen los de arranque en frío (da igual)",
    /Da igual/.test(els["#ventana"].innerHTML) && els["#ventana"].className === "panel v0",
    els["#ventana"].className + " | " + limpio(els["#ventana"].innerHTML));

  // Ahora /api/now publica umbrales distintos: con co2_aviso=600 el mismo
  // CO2 de 700 (que antes no disparaba nada) ya debe decir "abre para bajar
  // el CO2". Confirma que el JS toma los umbrales del firmware, no los propios.
  const nUmbral = now(true);
  nUmbral.pms.pm25 = 9; nUmbral.scd.co2 = 700;
  nUmbral.umbrales = { pm25_aviso: 15, pm25_malo: 35, co2_aviso: 600, co2_malo: 1200 };
  await conExt(Object.assign({}, EXT, { valido: true, rancio: false, pm25: 10 }));
  ctx.pintarAhora(nUmbral);
  debe("umbrales publicados por el firmware desplazan a los de arranque en frío",
    /para bajar el CO₂/.test(els["#ventana"].innerHTML) && els["#ventana"].className === "panel v0",
    els["#ventana"].className + " | " + limpio(els["#ventana"].innerHTML));

  // --- El SSID viene de fuera: no puede inyectar HTML ---
  console.log("\n--- Escapado del SSID ---");
  const SSID_MALO = '<img src=x onerror="alert(1)">';
  ctx.fetch = async (url) => ({
    ok: true, status: 200, json: async () => {
      if (/\/api\/health/.test(url)) return {
        uptime_s: 60, reset: "POWERON", heap_libre: 100000, heap_min: 90000,
        heap_total: 300000, hora_ok: true,
        wifi: { ok: true, ssid: SSID_MALO, ip: "192.168.1.5", rssi: -55, recon: 0 },
        pms: { ok: true, estado: 1, frames: 10, hace_s: 3, durmiendo: false },
        scd41: { ok: true, estado: 1, i2c: true, reads: 20, errores: 0,
                 ultimo_error: 0, etapa: "midiendo", rancio: false,
                 congelado: false, hace_s: 3, reinicios: 0, asc: 1,
                 selftest: 65535, serie: "ABC", altitud: 650 },
        flash: { usado: 1000000, libre: 300000, escrituras: 3 },
        oled: { ok: true, addr: 60 },
        historial: { muestras: 5, capacidad: 1440 },
        log: { eventos: 2, capacidad: 80 }
      };
      if (/\/api\/log/.test(url)) return { eventos: [] };
      return {};
    }
  });
  await ctx.cargarSis();
  const htmlSalud = els["#salud"].innerHTML;
  debe("el SSID no cuela una etiqueta <img>", !/<img/i.test(htmlSalud),
    limpio(htmlSalud).slice(0, 120));
  debe("el SSID sigue siendo legible, escapado",
    /&lt;img/.test(htmlSalud), limpio(htmlSalud).slice(0, 120));

  // --- Pestaña Sistema: cargarSis() pinta flash (uso + escrituras) ---
  console.log("\n--- Sistema: flash ---");
  const HEALTH = {
    uptime_s: 3600, reset: "POWERON",
    heap_libre: 180000, heap_min: 150000, heap_total: 320000,
    wifi: { ok: true, ssid: "casa", ip: "192.168.1.5", rssi: -55, recon: 0 },
    hora_ok: true,
    pms: { ok: true, estado: 1, frames: 900, hace_s: 3, durmiendo: false },
    scd41: {
      ok: true, estado: 1, i2c: true, reads: 5100, errores: 0, ultimo_error: 0,
      etapa: "midiendo", rancio: false, congelado: false, hace_s: 3, reinicios: 0,
      asc: 1, selftest: 65535, serie: "ABCDEF1234", altitud: 600
    },
    flash: { usado: 1153434, libre: 245350, escrituras: 7 },
    oled: { ok: true, addr: 60 },
    historial: { muestras: 812, capacidad: 1440 },
    log: { eventos: 12, capacidad: 100 }
  };
  ctx.fetch = async url => {
    if (/\/api\/health/.test(url)) return { ok: true, status: 200, json: async () => HEALTH };
    if (/\/api\/log/.test(url)) return { ok: true, status: 200, json: async () => ({ eventos: [] }) };
    if (/\/api\/exterior/.test(url)) return { ok: true, status: 200, json: async () => EXT };
    return { ok: true, status: 200, json: async () => ({}) };
  };
  await ctx.cargarSis();
  // usado=1153434, libre=245350 -> 82% ; 1153434/1024=1126 KB, total/1024=1366 KB
  debe("flash: pinta el porcentaje usado y el detalle en KB",
    /82% \(1126 KB \/ 1366 KB\)/.test(els["#flashUso"].textContent),
    els["#flashUso"].textContent);
  debe("flash: pinta el contador de escrituras",
    els["#flashEscrituras"].textContent === 7, els["#flashEscrituras"].textContent);

  // --- Pestaña Sistema: cargarCloudEstado() pinta el backup en la nube ---
  console.log("\n--- Sistema: backup en la nube ---");
  const conCloud = async estado => {
    ctx.fetch = async url => {
      if (/\/api\/cloud\/estado/.test(url)) return { ok: true, status: 200, json: async () => estado };
      return { ok: true, status: 200, json: async () => ({}) };
    };
    await ctx.cargarCloudEstado();
  };

  await conCloud({ activo: true, configurado: true, hay_token: true, en_cola: 5,
    envios_ok: 40, envios_fallo: 1, hace_s: 12,
    url: "https://influx-prod.grafana.net/write", usuario: "12345" });
  debe("cloud: activo", els["#cloudEstado"].textContent === "activo",
    els["#cloudEstado"].textContent);
  debe("cloud: hay token guardado", /guardado/.test(els["#cloudTokenEstado"].textContent),
    els["#cloudTokenEstado"].textContent);
  debe("cloud: cuenta la cola", els["#cloudCola"].textContent === 5,
    els["#cloudCola"].textContent);
  debe("cloud: repone URL y usuario sin exponer el token",
    els["#cloudUrl"].value === "https://influx-prod.grafana.net/write" &&
    els["#cloudUser"].value === "12345",
    els["#cloudUrl"].value + " / " + els["#cloudUser"].value);
  debe("cloud: repone el interruptor activo", els["#cloudOn"].checked === true,
    els["#cloudOn"].checked);

  await conCloud({ activo: false, configurado: true, hay_token: true, en_cola: 0,
    envios_ok: 0, envios_fallo: 0, hace_s: 0 });
  debe("cloud: configurado pero inactivo", els["#cloudEstado"].textContent === "configurado, inactivo",
    els["#cloudEstado"].textContent);

  await conCloud({ activo: false, configurado: false, hay_token: false, en_cola: 0,
    envios_ok: 0, envios_fallo: 0, hace_s: 0 });
  debe("cloud: sin configurar", els["#cloudEstado"].textContent === "sin configurar",
    els["#cloudEstado"].textContent);
  debe("cloud: sin token guardado", /no hay ninguno/.test(els["#cloudTokenEstado"].textContent),
    els["#cloudTokenEstado"].textContent);

  console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
  process.exit(fallos ? 1 : 0);
})();
