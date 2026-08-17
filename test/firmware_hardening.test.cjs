// Comprobaciones de cableado que no se pueden ejercer desde los ports JS:
// creación de primitivas FreeRTOS, persistencia y uso de valores escapados.

const fs = require("fs"), path = require("path");
const raiz = path.join(__dirname, "..");
const lee = f => fs.readFileSync(path.join(raiz, f), "utf8");
const webapi = lee("webapi.cpp"), outdoor = lee("outdoor.cpp"), net = lee("net.cpp"),
  sensors = lee("sensors.cpp");

let fallos = 0;
function debe(nombre, cond) {
  console.log((cond ? "  ok    " : "  FALLO ") + nombre);
  if (!cond) fallos++;
}

console.log("--- Endurecimiento de firmware ---");
debe("/api/health escapa el SSID antes de interpolarlo",
  /escapar\(red_estado\.ssid,\s*ssidEsc/.test(webapi) &&
  /ssidEsc,\s*red_estado\.ip/.test(webapi));
debe("leerCoord exige fin de cadena tras posibles espacios",
  /while\s*\(\*fin\s*&&\s*isspace[\s\S]*if\s*\(\*fin\)\s*return false/.test(webapi));
debe("el watchdog SCD no abandona antes de la primera medida",
  !/void sensoresRevisarScd\(\)[\s\S]{0,220}if\s*\(!scd_estado\.valido\)\s*return/.test(sensors));
debe("solo busca congelación después de una primera medida",
  /scd_estado\.congelado\s*=\s*scd_estado\.valido\s*&&/.test(sensors));
debe("mDNS publica el puerto configurado",
  /MDNS\.addService\("http",\s*"tcp",\s*PUERTO_HTTP\)/.test(net));
debe("el fallo de creación del mutex desactiva exterior",
  /candado\s*=\s*xSemaphoreCreateMutex\(\);\s*if\s*\(!candado\)/s.test(outdoor));
debe("la tarea exterior comprueba pdPASS y el handle",
  /xTaskCreatePinnedToCore[\s\S]*creada\s*==\s*pdPASS\s*&&\s*tarea/.test(outdoor));
debe("la ubicación comprueba los tres put de Preferences",
  /nLat\s*!=\s*sizeof lat[\s\S]*nLon\s*!=\s*sizeof lon[\s\S]*nLugar\s*==\s*0/.test(outdoor));
debe("el token verifica la NVS incluso cuando se borra con cadena vacia",
  /p\.putString\("aqicn",\s*token\)[\s\S]*p\.getString\("aqicn",\s*"\\x01"\)\s*==\s*token/.test(outdoor));
debe("las descargas exteriores validan TLS con una CA",
  /setCACert\s*\(\s*OUTDOOR_ROOT_CA\s*\)/.test(outdoor) &&
  !/setInsecure\s*\(\s*\)/.test(outdoor));

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
