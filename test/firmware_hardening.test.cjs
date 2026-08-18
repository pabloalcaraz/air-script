// Comprobaciones de cableado que no se pueden ejercer desde los ports JS:
// creación de primitivas FreeRTOS, persistencia y uso de valores escapados.

const fs = require("fs"), path = require("path");
const raiz = path.join(__dirname, "..");
const lee = f => fs.readFileSync(path.join(raiz, f), "utf8");
const webapi = lee("webapi.cpp"), outdoor = lee("outdoor.cpp"), net = lee("net.cpp"),
  sensors = lee("sensors.cpp"), cloud = lee("cloud.cpp"), stats = lee("stats.cpp");

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
debe("el token verifica la NVS y una cadena vacia elimina la clave",
  /p\.putString\("aqicn",\s*token\)[\s\S]*p\.getString\("aqicn",\s*"\\x01"\)\s*==\s*token/.test(outdoor) &&
  /p\.remove\("aqicn"\)/.test(outdoor));
debe("las descargas exteriores validan TLS con una CA",
  /setCACert\s*\(\s*OUTDOOR_ROOT_CA\s*\)/.test(outdoor) &&
  !/setInsecure\s*\(\s*\)/.test(outdoor));
debe("cloud y exterior comparten un mutex para los handshakes TLS",
  /redTlsTomar\s*\(\s*RED_TLS_ESPERA_MS\s*\)/.test(outdoor) &&
  /xSemaphoreCreateMutex\(\)/.test(net));
debe("un cambio de ubicación invalida respuestas exteriores en vuelo",
  /revision\s*!=\s*revisionCfg/.test(outdoor) &&
  /revisionCfg\+\+/.test(outdoor));
debe("las respuestas exteriores tienen un limite estricto de bytes",
  /class\s+StringLimitada/.test(outdoor) &&
  /EXT_RESPUESTA_MAX\s*-\s*dst\.length/.test(outdoor));
debe("TLS comprueba memoria total y el mayor bloque contiguo",
  /heap_caps_get_largest_free_block\(MALLOC_CAP_8BIT\)/.test(net) &&
  /redTlsHayMemoria\(CLOUD_HEAP_MIN\)/.test(cloud));
debe("los comandos de configuracion del SCD41 comprueban errores",
  /error\s*=\s*scd4x\.setSensorAltitude/.test(sensors) &&
  /error\s*=\s*scd4x\.getAutomaticSelfCalibrationEnabled/.test(sensors) &&
  /error\s*=\s*scd4x\.getSerialNumber/.test(sensors));
debe("las estadisticas temporales usan timestamps reales",
  /m->ts\s*-\s*baseTs/.test(stats) &&
  /HIST_HUECO_MAX_S/.test(stats));

console.log(fallos ? "\n" + fallos + " FALLO(S)" : "\nTodo correcto");
process.exit(fallos ? 1 : 0);
