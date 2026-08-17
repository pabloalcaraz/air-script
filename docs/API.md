# API HTTP

El servidor escucha en el puerto 80 del ESP32. Los ejemplos usan
`http://air.local`; puede sustituirse por la IP mostrada en el OLED.

Todas las respuestas de la API son JSON. Los campos que no se pueden calcular
o que proceden de un sensor sin datos se devuelven como `null`, no como cero.

## Seguridad de las rutas POST

Las rutas que cambian estado comprueban la cabecera `Origin`. Si está presente,
debe coincidir con el `Host` del propio dispositivo; una petición de navegador
desde otro origen recibe `403`. Las peticiones sin `Origin`, como las de `curl`
o una integración local, se aceptan.

Esta comprobación reduce el riesgo de CSRF, pero **no es autenticación**. Cualquier
equipo con acceso directo a la red local puede invocar la API. No publiques el
puerto 80 en Internet.

## Resumen de rutas

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/` | Dashboard HTML comprimido con gzip |
| GET | `/api/now` | Lecturas actuales, alertas y estado de sensores |
| GET | `/api/health` | Diagnóstico, WiFi, memoria, flash y contadores |
| GET | `/api/log` | Eventos, del más reciente al más antiguo |
| GET | `/api/history?range=1h\|6h\|24h` | Series históricas con downsampling |
| GET | `/api/stats?range=1h\|6h\|24h` | Resumen estadístico y métricas derivadas |
| GET | `/api/exterior` | Última lectura del aire exterior |
| POST | `/api/exterior` | Guarda la localización exterior |
| POST | `/api/exterior/token` | Guarda o borra el token AQICN |
| POST | `/api/calibrar` | Calibración forzada del SCD41 |
| POST | `/api/scd/reset` | Reinicio manual del SCD41 |
| POST | `/api/scd/autotest` | Autotest interno del SCD41 |
| POST | `/api/scd/asc` | Activa o desactiva ASC |
| POST | `/api/cloud/config` | Configura el backup Influx |
| GET | `/api/cloud/estado` | Estado del backup y de su cola |

Un valor de `range` distinto de `1h` o `6h` se normaliza a `24h`.

## Lecturas actuales

```bash
curl http://air.local/api/now
```

La respuesta incluye:

- `ts`, `hora_ok` y `uptime_s`;
- bloque `pms` con PM1, PM2.5, PM10, alertas y estado;
- bloque `scd` con CO₂, temperatura, humedad y diagnóstico;
- bloque `m24` con medias móviles de partículas;
- `umbrales`, para que el navegador use la misma configuración que el
  firmware;
- `peor`, con el nivel global más alto.

Los estados numéricos de salud son `0` esperando, `1` correcto y `2` fallo.

## Salud del sistema

```bash
curl http://air.local/api/health
```

Publica uptime y causa del último reinicio, heap libre/mínimo/total, WiFi, hora,
estado detallado de los sensores, OLED, ocupación del historial y del log.

El bloque `flash` contiene:

- `usado`: bytes usados por el sketch;
- `libre`: espacio libre en la partición de aplicación;
- `escrituras`: operaciones NVS/EEPROM contadas desde el último arranque.

## Historial

```bash
curl "http://air.local/api/history?range=6h"
```

La respuesta contiene `range`, número de puntos `n`, disponibilidad de hora
NTP (`hora_ok`), eje temporal `t` y las series `pm1`, `pm25`, `pm10`, `co2`,
`temp` y `hum`.

El dispositivo conserva una muestra por minuto durante 24 horas. Para limitar
el JSON y el trabajo del navegador, agrupa las muestras hasta devolver un
máximo de 240 puntos. Un bloque sin ningún dato válido aparece como `null`.

## Estadísticas

```bash
curl "http://air.local/api/stats?range=24h"
```

Cada entrada de `series` contiene:

- `n`, `min`, `media`, `p95` y `max`;
- `tend_h`, pendiente reciente expresada por hora;
- `pct`, porcentajes en nivel normal/aviso/malo, o `null` si la magnitud no
  tiene alerta activa.

Fuera de `series` se publican `pm25_24h`, `pm10_24h`, sus niveles, `ach`,
`sin_ventilar_min`, `co2_ext`, `co2_ventilado` y el mínimo de muestras exigido
para las medias móviles.

## Log de eventos

```bash
curl http://air.local/api/log
```

`eventos` se ordena de más reciente a más antiguo. Cada entrada contiene
timestamp `ts`, nivel `n`, repeticiones agrupadas `r` y mensaje `m`.

## Aire exterior

### Consultar

```bash
curl http://air.local/api/exterior
```

La respuesta distingue si existe configuración, si el dato es válido o rancio,
su edad y el último código HTTP. Incluye partículas, O₃, NO₂, temperatura,
humedad y AQI cuando están disponibles.

`fuente` vale `aqicn`, `modelo` o `ninguna`. Solo con AQICN tienen contenido
`estacion` y `distancia_km`.

### Guardar una localización

```bash
curl -X POST http://air.local/api/exterior \
  --data-urlencode "lat=40.4168" \
  --data-urlencode "lon=-3.7038" \
  --data-urlencode "nombre=Madrid"
```

La latitud debe estar entre -90 y 90 y la longitud entre -180 y 180. Si se
omite el nombre, se genera uno a partir de las coordenadas. Guardar una nueva
ubicación invalida el dato anterior y dispara un sondeo.

### Token AQICN

```bash
curl -X POST http://air.local/api/exterior/token \
  --data-urlencode "token=<TOKEN_AQICN>"
```

Un token vacío lo borra y deja Open-Meteo como fuente. El límite es de 39
caracteres útiles. El token queda en NVS y no se devuelve desde la API.

## SCD41

### Calibración forzada

```bash
curl -X POST http://air.local/api/calibrar --data "ppm=420"
```

La referencia admitida está entre 350 y 2000 ppm. El sensor debe llevar al
menos tres minutos midiendo sin interrupción. Una calibración correcta devuelve
la corrección aplicada; los fallos de condición o comunicación usan `409`.

### Reinicio

```bash
curl -X POST http://air.local/api/scd/reset
```

Interrumpe la medición aproximadamente un segundo. La primera lectura nueva
suele tardar unos cinco segundos.

### Autotest

```bash
curl -X POST http://air.local/api/scd/autotest
```

Bloquea el dispositivo alrededor de 10 segundos. La respuesta diferencia la
ejecución de la prueba (`ok`) del resultado del sensor (`sano`).

### Autocalibración ASC

```bash
curl -X POST http://air.local/api/scd/asc --data "on=1"
curl -X POST http://air.local/api/scd/asc --data "on=0"
```

El cambio persiste en la EEPROM del SCD41. No debe usarse como conmutador
periódico.

## Backup en la nube

### Configurar

```bash
curl -X POST http://air.local/api/cloud/config \
  --data-urlencode "url=https://<INSTANCIA>.grafana.net/api/v1/push/influx/write" \
  --data-urlencode "user=<USUARIO>" \
  --data-urlencode "token=<TOKEN_DE_ESCRITURA>" \
  --data "on=1"
```

Límites de almacenamiento:

| Campo | Máximo útil |
|---|---:|
| URL | 159 caracteres |
| Usuario | 23 caracteres |
| Token | 170 caracteres |

Solo se aceptan URLs HTTPS cuyo host termine exactamente en `.grafana.net`, sin
credenciales embebidas, puerto alternativo, query ni fragmento. Un `token`
vacío conserva el guardado únicamente mientras no cambien URL ni usuario; si
cambia alguno de ellos hay que introducir un token nuevo. Para desactivar el
envío sin borrar la configuración usa `on=0` manteniendo la URL y usuario.

La configuración persiste en NVS. El endpoint del ESP32 sigue siendo HTTP
local, pero el cliente HTTPS saliente valida Grafana con DigiCert Global Root
G2, fijada en `cloud_ca.h` y válida hasta enero de 2038. Usa una red local de
confianza y un token dedicado con el mínimo alcance posible.

### Estado

```bash
curl http://air.local/api/cloud/estado
```

| Campo | Significado |
|---|---|
| `activo` | Envío habilitado |
| `configurado` | Hay URL, usuario y token |
| `hay_token` | Existe token, sin revelarlo |
| `en_cola` | Muestras pendientes en RAM |
| `envios_ok` | Envíos correctos desde el arranque |
| `envios_fallo` | Envíos fallidos desde el arranque |
| `hace_s` | Segundos desde el último envío correcto; cero si nunca hubo uno |
| `url` | Endpoint guardado, sin credenciales embebidas |
| `usuario` | Usuario o Instance ID guardado |

El formato enviado es Influx Line Protocol:

```text
aire,dispositivo=airscript pm1=4,pm25=7,pm10=9,co2=612,temp=22.4,hum=48.1 1786900000000000000
```

Los campos de sensores caídos se omiten. Si NTP no está sincronizado, también
se omite el timestamp y el servidor usa la hora de ingesta. Se considera
correcta una respuesta HTTP 200 o 204.
