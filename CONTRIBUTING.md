# Desarrollo y contribuciones

Gracias por mejorar Air Script. Este documento resume la arquitectura, las
convenciones y las comprobaciones necesarias para cambiar el firmware sin
desincronizar el dashboard ni sus pruebas.

## Entorno

- Arduino CLI 1.x (probado con 1.5.1), o Arduino IDE compatible.
- Core `esp32:esp32` y bibliotecas declaradas en `sketch.yaml`.
- Node.js para la suite de pruebas y la compresión del dashboard.
- Hardware real para validar sensores, WiFi, NVS y consumo.

La configuración reproducible usa:

```bash
arduino-cli compile --profile esp32 .
node tools/run-tests.cjs
node tools/gzip-web.cjs --check
```

No hay paquetes npm de runtime ni es necesario ejecutar `pnpm install` para las
pruebas actuales.

## Arquitectura

Arduino compila los `.ino`, `.cpp` y `.h` situados en la carpeta del sketch. Por
eso los módulos de firmware se mantienen en la raíz.

| Archivo | Responsabilidad |
|---|---|
| `air-script.ino` | `setup()`, `loop()` y planificación de tareas periódicas |
| `config.h` | Pines, intervalos, umbrales y límites de memoria |
| `i2cbus.*` | Escaneo y diagnóstico del bus I2C |
| `sensors.*` | PMS5003, SCD41, estados y recuperación |
| `alerts.*` | Conversión de lecturas a niveles de alerta |
| `screen.*` | Renderizado y paginación del OLED |
| `net.*` | WiFiManager, mDNS y sincronización NTP |
| `history.*` | Buffer circular de muestras de 24 horas |
| `stats.*` | Estadísticas, tendencias, medias móviles y ACH |
| `outdoor.*` | AQICN/Open-Meteo y configuración de ubicación |
| `eventlog.*` | Buffer circular de eventos |
| `monitor.*` | Traducción de cambios de estado a eventos |
| `flashstats.*` | Contador en RAM de escrituras persistentes |
| `cloud.*` | Cola de backup y serialización Influx Line Protocol |
| `webapi.*` | Servidor HTTP, JSON y protección de rutas POST |
| `webpage.h` | Fuente HTML, CSS y JavaScript del dashboard |
| `webpage_gz.h` | Dashboard generado y comprimido que sirve el firmware |

El `loop()` usa comparaciones por resta sobre `millis()` para tolerar su
desbordamiento. Las consultas de aire exterior se ejecutan en una tarea de
FreeRTOS separada porque `HTTPClient` es síncrono; el estado compartido se lee
mediante snapshots protegidos.

## Convenciones

- Comentarios en `.ino`, `.cpp` y `.h`: español sin tildes, siguiendo la
  convención histórica del firmware y de sus salidas CP437.
- Documentación y pruebas JavaScript: español normal, con tildes.
- Los comentarios deben explicar decisiones y restricciones, no repetir el
  código.
- Los datos ausentes se representan con los centinelas del proyecto y se
  serializan como `null`; nunca deben convertirse en un cero inventado.
- Las operaciones que escriben NVS/EEPROM deben contabilizarse con
  `contarEscrituraFlash()`.
- Toda ruta POST con efectos debe comenzar con `postPermitido()`.
- No añadas credenciales, tokens ni ubicaciones privadas al repositorio.

## Dashboard generado

El firmware no sirve directamente `webpage.h`, sino `webpage_gz.h`. Después de
cualquier cambio en HTML, CSS o JavaScript ejecuta:

```bash
node tools/gzip-web.cjs
node tools/gzip-web.cjs --check
```

El archivo generado incluye el SHA-256 y el tamaño de su fuente. No lo edites a
mano.

## Pruebas

Ejecuta la suite completa con:

```bash
node tools/run-tests.cjs
# o
pnpm test
```

También se puede lanzar un archivo individual:

```bash
node test/stats.test.cjs
node test/web.test.cjs
node test/outdoor.test.cjs
```

Las pruebas cubren alertas, backup, eventos, flash, historial, aire exterior,
duty-cycle, pantalla, estadísticas, API y dashboard.

Parte de la suite porta algoritmos C++ a JavaScript puro. Esto permite probar
la lógica sin simular todo el runtime de Arduino, pero no ejecuta el binario
real. Si cambia un algoritmo en C++, revisa y actualiza su port correspondiente
en `test/`.

`test/web.test.cjs` extrae el script de `webpage.h` y lo ejecuta con DOM y red
simulados. `test/webapi.test.cjs` usa inspección estructural para algunas
garantías, como la presencia de `postPermitido()`, y tampoco sustituye una
prueba HTTP sobre el dispositivo.

## Comprobación antes de entregar un cambio

1. Ejecuta `node tools/run-tests.cjs`.
2. Si cambió `webpage.h`, regenera el gzip y ejecuta su comprobación.
3. Compila con `arduino-cli compile --profile esp32 --warnings all .`.
4. Revisa los warnings de compilación y el tamaño de flash/RAM.
5. Si afecta a sensores, red, NVS o tiempos, valida el caso en hardware real.
6. Comprueba que no se hayan añadido artefactos locales ni secretos.

Para cambios de comportamiento público, actualiza también `README.md` o el
documento correspondiente de `docs/`.
