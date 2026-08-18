# Uso y diagnóstico

## Puesta en marcha

En el primer arranque, o cuando no puede conectar a una red conocida, el ESP32
crea el punto de acceso abierto `AirScript-Setup`. El portal permanece activo
180 segundos.

1. Conéctate a `AirScript-Setup`.
2. Abre `192.168.4.1` si el portal cautivo no aparece solo.
3. Selecciona una red WiFi de 2,4 GHz e introduce sus credenciales.

Las credenciales se guardan en NVS. En los arranques siguientes el equipo se
conecta automáticamente. Si no hay WiFi, los sensores y el OLED siguen
funcionando; solo quedan indisponibles el dashboard, el aire exterior y el
backup remoto.

Con red disponible, abre `http://air.local` o la IP que aparece durante unos
segundos en el OLED y en el monitor serie.

## Dashboard

El dashboard se sirve desde el propio ESP32 y no necesita CDN ni acceso a
Internet para representar los datos locales.

- **Ahora:** últimas lecturas, niveles de alerta, tendencia, valores derivados
  y comparación con el exterior.
- **Histórico:** series de 1, 6 o 24 horas, con un máximo de 240 puntos.
- **Datos:** resumen estadístico, reparto por niveles, ACH y tiempo sin
  ventilar.
- **Sistema:** sensores, WiFi, memoria, flash, eventos, ubicación, calibración y
  backup.

El historial contiene 1440 muestras, una por minuto, y vive solo en RAM. Se
borra al reiniciar. Los sensores ausentes se representan con `null`, nunca con
cero, para no confundir un fallo con una lectura real.

## Alertas

Los umbrales se definen en `config.h`. Nivel 0 significa normal, 1 aviso y 2
malo.

| Magnitud | Aviso | Malo |
|---|---:|---:|
| PM2.5 | ≥ 15 µg/m³ | ≥ 35 µg/m³ |
| PM10 | ≥ 45 µg/m³ | ≥ 100 µg/m³ |
| CO₂ | ≥ 800 ppm | ≥ 1200 ppm |
| Temperatura | fuera de 18-27 °C | fuera de 15-30 °C |
| Humedad | fuera de 30-60 % | fuera de 20-70 % |

PM1.0 no tiene alerta. La alerta de temperatura está desactivada por defecto
con `ALERTA_TEMP=0`, aunque la magnitud se sigue midiendo y guardando.

Los valores de partículas inspirados en las guías de la OMS corresponden a
medias de 24 horas, no a lecturas instantáneas. El indicador instantáneo sirve
para detectar un episodio actual; la media móvil de 24 horas es la comparación
adecuada para exposición diaria. Esa media no se publica hasta reunir al menos
60 muestras válidas.

## Estadísticas

`GET /api/stats` calcula todo al solicitarlo, a partir del historial en RAM.

Para cada magnitud y ventana de 1, 6 o 24 horas devuelve número de muestras,
mínimo, media, percentil 95 y máximo. El P95 se interpola como
`PERCENTILE.INC`, por lo que es menos sensible que el máximo a un único frame
anómalo.

También calcula:

- Porcentaje del tiempo en nivel normal, aviso y malo, solo para magnitudes con
  umbrales activos.
- Tendencia de las últimas 30 muestras por mínimos cuadrados, expresada por
  hora y calculada con sus timestamps reales. Con menos de cinco puntos válidos
  se devuelve `null`.
- Punto de rocío y humedad absoluta mediante la fórmula de Magnus; estos dos
  valores se calculan en el navegador.
- Minutos desde la última lectura de CO₂ inferior a `CO2_VENTILADO` (600 ppm).

### Renovaciones de aire por hora

Durante una ventilación, el CO₂ se aproxima al exterior de forma exponencial:

```text
C(t) = C_ext + (C_0 - C_ext) · e^(-ACH · t)
```

El firmware busca la bajada sostenida más reciente y ajusta una recta a
`ln(C - C_ext)`. Usa `C_ext=430 ppm` como fondo de referencia. El resultado se
descarta cuando no hay evidencia suficiente:

- menos de 10 minutos de bajada continua;
- huecos del sensor o repuntes mayores de 12 ppm;
- salto inicial inferior a 200 ppm sobre el fondo;
- descenso total inferior a 60 ppm;
- ajuste con R² inferior a 0,80;
- resultado no positivo o superior a 20 renovaciones por hora.

Un hueco superior a 90 segundos corta el tramo: el firmware no une dos
periodos separados ni supone intervalos perfectos si el loop se retrasó.

Que aparezca `null` la mayor parte del tiempo es normal: solo se estima ACH
cuando existe un decaimiento medible.

## Aire exterior

La localización se elige en **Sistema → Localización del aire exterior**. El
buscador se ejecuta en el navegador y consulta Nominatim; el ESP32 recibe las
coordenadas y el nombre elegidos y los guarda en NVS. No existe una ubicación
predeterminada.

La consulta sigue este orden:

1. Si hay token, intenta obtener una estación real cercana desde AQICN.
2. Si no hay token, la estación queda demasiado lejos o la consulta falla, usa
   el modelo CAMS a través de Open-Meteo.

AQICN publica el nombre y la distancia de la estación. El modelo CAMS ofrece
una estimación regional y no debe tratarse como una estación física del barrio.
La respuesta de la API identifica siempre la fuente usada.

El dato se vuelve a consultar cada 30 minutos. Un fallo conserva temporalmente
la última lectura y reintenta a los dos minutos; pasadas dos horas sin dato
nuevo, se marca como rancio y deja de mostrarse en el OLED.

La columna de CO₂ exterior usa la constante de fondo `CO2_EXTERIOR_PPM`, no una
medición remota. El ratio PM2.5 interior/exterior solo se calcula si ambos datos
existen y el exterior es al menos 1 µg/m³.

### Recomendación de ventilación

El banner «¿Abrir la ventana?» se calcula en el navegador con PM2.5 interior,
PM2.5 exterior y CO₂ interior. Nunca recomienda abrir si el exterior supera
`PM25_MALO`; con CO₂ muy alto puede sugerir una ventilación breve. Si falta o
ha caducado cualquiera de los datos necesarios, muestra esa limitación en vez
de emitir un veredicto.

### Privacidad y transporte

Las coordenadas elegidas se envían a AQICN/Open-Meteo y las búsquedas de texto
a Nominatim. El firmware valida las consultas HTTPS de ambos proveedores con
ISRG Root X1. Esta raíz pública caduca en junio de 2035; si el proveedor cambia
de cadena antes, habrá que actualizar `outdoor_ca.h`. No uses un token AQICN
con más permisos de los necesarios.

## PMS5003: duty-cycle

El pin `SET`, conectado a GPIO25, reduce el tiempo de funcionamiento del
ventilador y del láser:

1. Despierta el sensor.
2. Descarta los primeros 30 segundos de calentamiento.
3. Promedia los frames recibidos durante 5 segundos.
4. Pone el sensor en reposo hasta el siguiente ciclo de un minuto.

Mientras duerme, el OLED conserva el último promedio y muestra `zzz`. El
silencio esperado durante el reposo no cuenta como dato rancio. Al despertar,
si no llega ningún frame durante `PMS_TIMEOUT_MS`, el sistema sí lo marca como
fallo. Ese estado se conserva durante el reposo y no se borra hasta completar
un ciclo posterior con al menos un frame válido; nunca se recupera por el mero
hecho de volver a dormir el sensor.

## SCD41: vigilancia y calibración

El firmware vigila tres situaciones:

| Control | Umbral | Respuesta |
|---|---:|---|
| Sin lectura válida | 30 s | Marca datos ausentes |
| CO₂, temperatura y humedad congelados | 10 min | Marca posible dato cacheado |
| Fallo persistente | Reintento cada 60 s | Ejecuta apagado, despertar y reinicio |

El contador de recuperaciones aparece en **Sistema → Diagnóstico del SCD41**.
Una recuperación aislada puede ser normal; un contador que crece continuamente
suele apuntar a alimentación inestable.

### Autotest

El autotest se lanza manualmente desde el dashboard. Bloquea el dispositivo
unos 10 segundos porque así lo exige el SCD41. Un resultado de avería interna
indica un problema de hardware; una prueba correcta no descarta problemas de
alimentación o calibración.

### Calibración forzada

Antes de calibrar, el dispositivo exige al menos tres minutos de medición
continua. Colócalo en aire exterior estable, sin personas cerca, e introduce el
valor de referencia desde **Sistema → Calibración del CO₂**. La operación
escribe la EEPROM del sensor.

La autocalibración ASC solo es adecuada si el sensor ve aire fresco con
regularidad. Cambiar ASC también persiste la configuración en la EEPROM, por lo
que no debe conmutarse de forma periódica.

## Flash y backup

`Sistema → Flash` muestra el tamaño usado y libre de la partición, además de un
contador en RAM de escrituras NVS/EEPROM realizadas desde el último arranque.
El contador sirve para detectar bucles de escritura y se reinicia al encender.

El backup opcional encola una muestra por minuto mediante Influx Line Protocol.
Una tarea de FreeRTOS separada realiza los POST, por lo que una resolución DNS,
negociación TLS o respuesta lenta no detiene sensores, OLED ni API. Si el
envío falla, conserva hasta 120 muestras en una cola circular de RAM y aplica
una espera exponencial de 5 segundos a 5 minutos. Al volver la conexión, drena
como máximo una muestra por segundo y en orden FIFO. La cola no sobrevive a un
reinicio y se vacía intencionadamente si cambia el destino o el usuario para
no enviar medidas antiguas a otra cuenta. Consulta la configuración y los
campos exactos en [API.md](API.md#backup-en-la-nube).

El token de escritura se guarda en NVS y nunca se devuelve desde la API, pero
se introduce a través del dashboard HTTP local. Usa esta función solo en una
red de confianza y con un token de alcance mínimo.

## Diagnóstico rápido

| Síntoma | Comprobación recomendada |
|---|---|
| `BROWNOUT` como causa de reinicio | Fuente y cable USB; usa una fuente estable de 1 A o más |
| `SCD41: SIN DATOS` | Alimentación, VDD/GND y bus I2C |
| `Valor congelado: SÍ` | Alimentación; revisa si aumentan los reinicios automáticos |
| El SCD41 responde pero no mide | Prueba una fuente mejor: el emisor infrarrojo genera picos de consumo |
| `PMS5003: sin frames` | GPIO16, masa común y alimentación de 5 V |
| `Memoria mínima` baja continuamente | Posible fuga o fragmentación de memoria |
| PM10 coincide con PM2.5 | Puede ser normal; el PMS estima las fracciones por dispersión óptica |

Para problemas I2C utiliza `tools/diag-i2c`. El montaje y los comandos están en
[HARDWARE.md](HARDWARE.md#comprobación-del-bus-i2c).
