// Banco de pruebas del parseo de outdoor.cpp. Es un port a JS de bloqueActual()
// y campo(): valida el METODO, no el binario compilado (en esta maquina no hay
// compilador de host, solo el cruzado de xtensa). Tocar el .cpp obliga a tocar
// esto, o las dos versiones dejan de decir lo mismo.

// --- port de outdoor.cpp -----------------------------------------------------

// strstr(json, "\"current\":{"). El "_" de "current_units" impide que ese
// bloque haga de ancla por accidente.
function bloqueActual(json) {
  const i = json.indexOf('"current":{');
  return i < 0 ? null : json.slice(i);
}

// strtof: salta espacios, lee un numero y, si no consume nada, deja el puntero
// donde estaba. Ese es el caso que aqui devuelve null en vez de 0.
function campo(obj, clave) {
  const pat = '"' + clave + '":';
  const i = obj.indexOf(pat);
  if (i < 0) return null;
  const p = obj.slice(i + pat.length);
  const m = /^[ \t\n\r]*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/.exec(p);
  return m ? parseFloat(m[0]) : null;
}

// --- arnes ------------------------------------------------------------------

let fallos = 0;
function debe(nombre, cond, visto) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre + (cond ? "" : "  <-- " + visto));
  if (!cond) fallos++;
}
function igual(nombre, a, b) {
  debe(nombre + " = " + b, a === b, String(a));
}

// --- respuestas reales de Open-Meteo (capturadas 2026-07-29) ----------------

const AIRE = '{"latitude":40.4,"longitude":-3.6999989,"generationtime_ms":0.192,' +
  '"utc_offset_seconds":7200,"timezone":"Europe/Madrid","timezone_abbreviation":' +
  '"GMT+2","elevation":666.0,"current_units":{"time":"iso8601","interval":' +
  '"seconds","pm10":"μg/m³","pm2_5":"μg/m³","ozone":' +
  '"μg/m³","nitrogen_dioxide":"μg/m³","european_aqi":"EAQI"},' +
  '"current":{"time":"2026-07-29T18:00","interval":3600,"pm10":25.3,"pm2_5":13.7,' +
  '"ozone":116.0,"nitrogen_dioxide":5.7,"european_aqi":51}}';

const METEO = '{"latitude":40.4375,"longitude":-3.6875,"generationtime_ms":0.03,' +
  '"utc_offset_seconds":0,"timezone":"GMT","timezone_abbreviation":"GMT",' +
  '"elevation":666.0,"current_units":{"time":"iso8601","interval":"seconds",' +
  '"temperature_2m":"°C","relative_humidity_2m":"%"},"current":{"time":' +
  '"2026-07-29T16:30","interval":900,"temperature_2m":37.5,' +
  '"relative_humidity_2m":19}}';

console.log("--- Respuestas reales ---");
const a = bloqueActual(AIRE);
debe("el bloque current existe", a !== null, "no encontrado");
// La trampa de este endpoint: "current_units" trae LAS MISMAS claves y va
// ANTES. Buscar "pm2_5" sin anclar devolveria la cadena de la unidad.
debe("el ancla se salta current_units", !a.includes("EAQI"), a.slice(0, 60));
igual("pm2_5", campo(a, "pm2_5"), 13.7);
igual("pm10", campo(a, "pm10"), 25.3);
igual("ozone", campo(a, "ozone"), 116);
igual("nitrogen_dioxide", campo(a, "nitrogen_dioxide"), 5.7);
igual("european_aqi", campo(a, "european_aqi"), 51);

const m = bloqueActual(METEO);
igual("temperature_2m", campo(m, "temperature_2m"), 37.5);
igual("relative_humidity_2m", campo(m, "relative_humidity_2m"), 19);

console.log("\n--- La unidad nunca puede colarse como valor ---");
// Aunque el ancla fallase, strtof sobre "μg/m³" no consume nada: null, no 0.
debe("una unidad no se parsea como numero",
  campo('{"pm2_5":"μg/m³"}', "pm2_5") === null,
  String(campo('{"pm2_5":"μg/m³"}', "pm2_5")));
// Y si algun dia Open-Meteo pusiera "current" antes que "current_units", el
// ancla sigue encontrando primero el valor bueno.
const INVERTIDO = '{"current":{"pm2_5":13.7},"current_units":{"pm2_5":"ug"}}';
igual("con current antes que current_units", campo(bloqueActual(INVERTIDO), "pm2_5"), 13.7);

console.log("\n--- Datos que faltan ---");
// Un 0 aqui se leeria en la web como \"hoy no hay contaminacion fuera\", que es
// la mentira mas cara que puede contar este panel.
debe("null se traduce a 'no hay dato', no a 0",
  campo('{"pm2_5":null,"pm10":4}', "pm2_5") === null,
  String(campo('{"pm2_5":null,"pm10":4}', "pm2_5")));
debe("una clave ausente no inventa un valor",
  campo('{"pm10":4}', "pm2_5") === null, String(campo('{"pm10":4}', "pm2_5")));
debe("un JSON de error no tiene bloque current",
  bloqueActual('{"error":true,"reason":"Latitude must be in range"}') === null,
  "lo encontro igualmente");
debe("una respuesta vacia no revienta el parseo",
  bloqueActual("") === null, "algo devolvio");

console.log("\n--- Formatos numericos ---");
igual("temperatura bajo cero", campo('{"current":{"temperature_2m":-3.4}}', "temperature_2m"), -3.4);
igual("entero sin decimales", campo('{"relative_humidity_2m":19}', "relative_humidity_2m"), 19);
igual("cero es un valor legitimo", campo('{"nitrogen_dioxide":0.0}', "nitrogen_dioxide"), 0);
igual("notacion cientifica", campo('{"ozone":1.16e2}', "ozone"), 116);

console.log("\n--- AQICN: estacion real (respuesta capturada 2026-07-30) ---");
// El token publico "demo" ignora las coordenadas del geo: y siempre devuelve
// Shanghai, sea cual sea el lat;lon pedido. No es un bug de este parser ni
// del calculo de distancia: es el comportamiento documentado del token de
// pruebas. La resolucion por geo real solo se puede probar a mano con un
// token propio (gratis, aqicn.org/data-platform/register).
const AQICN = '{"status":"ok","data":{"aqi":96,"idx":1437,' +
  '"city":{"geo":[31.2047372,121.4489017],"name":"Shanghai (上海)",' +
  '"url":"https://aqicn.org/city/shanghai","location":""},' +
  '"dominentpol":"o3",' +
  '"iaqi":{"co":{"v":6.4},"h":{"v":42},"no2":{"v":5.1},"o3":{"v":96.2},' +
  '"p":{"v":1009},"pm10":{"v":35},"pm25":{"v":72},"so2":{"v":3.6},' +
  '"t":{"v":37.5},"w":{"v":1.5}},' +
  '"time":{"s":"2026-07-30 13:00:00","tz":"+08:00","v":1785416400},' +
  '"forecast":{"daily":{"pm25":[{"avg":149,"day":"2026-07-29","max":159,"min":138}]}}}}';

function statusOk(json) {
  return json.indexOf('"status":"ok"') >= 0;
}
// "<clave>":{"v":NUM} — la llave y "v" en el propio patron evitan confundir
// iaqi.pm25.v con forecast.daily.pm25, que es un array con otra forma.
function campoAqicn(json, clave) {
  const pat = '"' + clave + '":{"v":';
  const i = json.indexOf(pat);
  if (i < 0) return null;
  const p = json.slice(i + pat.length);
  const m = /^[ \t\n\r]*[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/.exec(p);
  return m ? parseFloat(m[0]) : null;
}
function bloqueCiudad(json) {
  const i = json.indexOf('"city":{');
  return i < 0 ? null : json.slice(i);
}
function campoTexto(obj, clave) {
  const pat = '"' + clave + '":"';
  const i = obj.indexOf(pat);
  if (i < 0) return null;
  const start = i + pat.length;
  const end = obj.indexOf('"', start);
  return end < 0 ? null : obj.slice(start, end);
}
function geoCiudad(obj) {
  const i = obj.indexOf('"geo":[');
  if (i < 0) return null;
  const p = obj.slice(i + 7);
  const num = '[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?';
  const m = new RegExp('^(' + num + '),(' + num + ')').exec(p);
  return m ? [parseFloat(m[1]), parseFloat(m[4])] : null;
}
// Aproximacion plana: valida con error <0.5% a la escala de España (<1000 km
// entre dos puntos cualesquiera). El mismo metodo se usa en outdoor.cpp.
function distanciaKm(lat1, lon1, lat2, lon2) {
  const R = 6371;
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const mlat = (lat1 + lat2) / 2 * Math.PI / 180;
  const x = dLon * Math.cos(mlat);
  const y = dLat;
  return R * Math.sqrt(x * x + y * y);
}

debe("status ok se reconoce", statusOk(AQICN), "no lo detecto");
igual("pm2.5 desde iaqi, no desde el pronostico", campoAqicn(AQICN, "pm25"), 72);
igual("pm10", campoAqicn(AQICN, "pm10"), 35);
igual("co (monoxido)", campoAqicn(AQICN, "co"), 6.4);
igual("nombre de estacion", campoTexto(bloqueCiudad(AQICN), "name"), "Shanghai (上海)");
debe("coordenadas de la estacion",
  JSON.stringify(geoCiudad(bloqueCiudad(AQICN))) === JSON.stringify([31.2047372, 121.4489017]),
  JSON.stringify(geoCiudad(bloqueCiudad(AQICN))));

console.log("\n--- AQICN: casos de fallo ---");
debe("status error no se da por bueno",
  !statusOk('{"status":"error","data":"Invalid key"}'), "lo dio por bueno");
debe("iaqi sin pm25 no inventa un valor",
  campoAqicn('{"iaqi":{"pm10":{"v":35}}}', "pm25") === null,
  String(campoAqicn('{"iaqi":{"pm10":{"v":35}}}', "pm25")));
debe("un JSON sin bloque city no inventa una ciudad",
  bloqueCiudad('{"status":"ok","data":{"aqi":96}}') === null,
  String(bloqueCiudad('{"status":"ok","data":{"aqi":96}}')));
debe("un objeto sin geo no inventa coordenadas",
  geoCiudad('{"name":"Shanghai"}') === null,
  String(geoCiudad('{"name":"Shanghai"}')));
debe("un objeto sin la clave no inventa un texto",
  campoTexto('{"other":"x"}', "name") === null,
  String(campoTexto('{"other":"x"}', "name")));

console.log("\n--- Distancia estacion-coordenadas ---");
// 1 grado de latitud son ~111.19 km en cualquier punto del globo: referencia
// facil de comprobar a mano, sin fiarse de memoria de una distancia real
// entre dos ciudades.
debe("1 grado de latitud son ~111.2 km",
  Math.abs(distanciaKm(0, 0, 1, 0) - 111.19) < 0.1,
  distanciaKm(0, 0, 1, 0).toFixed(2));
igual("mismo punto, distancia cero", distanciaKm(40.4, -3.7, 40.4, -3.7), 0);
// Madrid -> Barcelona: a diferencia de los dos tests anteriores, aqui dLat y
// dLon son ambos distintos de cero, asi que este test si ejercita el termino
// "* Math.cos(mlat)". Si el port a C++ (Task 4) pusiera el signo mal, se
// olvidara del coseno o intercambiara lat/lon, este test lo detectaria: el
// error resultante seria de decenas de km, muy por encima de la tolerancia.
// Distancia real ~504.6 km; formula plana da ~505.2 km (<0.5% de error a
// escala de España, ver comentario de distanciaKm).
debe("Madrid-Barcelona, ~505.2 km",
  Math.abs(distanciaKm(40.4168, -3.7038, 41.3874, 2.1686) - 505.2) < 1,
  distanciaKm(40.4168, -3.7038, 41.3874, 2.1686).toFixed(2));

console.log("\n--- Guard de distancia: se descarta la estacion lejana ---");
// Port de la decision de sondearAqicn: solo se rechaza si SABEMOS que esta
// lejos. distKm NAN (la estacion no trajo geo) = no hay con que juzgar = se
// acepta. El umbral es AQICN_MAX_KM (30 km en config.h).
const AQICN_MAX_KM = 30.0;
function estacionDemasiadoLejos(distKm) {
  return Number.isFinite(distKm) && distKm > AQICN_MAX_KM;
}
// Caso real observado: se pide Madrid y AQICN devuelve Bailen a ~258 km.
debe("Bailen (~258 km) desde Madrid se descarta",
  estacionDemasiadoLejos(distanciaKm(40.4168, -3.7038, 38.0972, -3.7739)),
  distanciaKm(40.4168, -3.7038, 38.0972, -3.7739).toFixed(2) + " km");
debe("una estacion en la misma ciudad (~2 km) se acepta",
  !estacionDemasiadoLejos(2.0), "2 km rechazada");
debe("justo en el umbral (30 km) se acepta",
  !estacionDemasiadoLejos(30.0), "30 km rechazada");
debe("un pelo por encima del umbral se descarta",
  estacionDemasiadoLejos(30.01), "30.01 km aceptada");
// Sin geo en la respuesta no se puede medir: se acepta, no se penaliza a la
// estacion por un dato de forma que falta.
debe("distancia desconocida (NAN) se acepta",
  !estacionDemasiadoLejos(NaN), "NAN rechazada");

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
