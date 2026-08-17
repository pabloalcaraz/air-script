# Montaje e instalación

Esta guía corresponde al montaje probado con un ESP32-D DevKit V1 de 38 pines,
un PMS5003, un SCD41 y una pantalla OLED SSD1306 I2C. Comprueba siempre la
serigrafía de tus módulos: el orden de los pines puede variar entre fabricantes.

## Componentes

- ESP32-D DevKit V1 (WROOM-32), 38 pines.
- Sensor Plantower PMS5003 con cable JST.
- Sensor Sensirion SCD41 en placa de desarrollo.
- Pantalla OLED SSD1306 I2C de 0,96", 128 × 64.
- Protoboard y cables Dupont.
- Fuente USB estable de al menos 1 A recomendada.

## Alimentación

El montaje se alimenta por el USB del ESP32:

- `VIN`/5 V del ESP32 → `VCC` del PMS5003.
- `3V3` del ESP32 → `VCC`/`VDD` del OLED y del SCD41.
- Todas las masas deben estar unidas.

El PMS5003 se alimenta a 5 V, pero sus señales UART son de 3,3 V y se conectan
directamente al ESP32. No alimentes el OLED o el SCD41 a 5 V salvo que la placa
concreta indique expresamente que su entrada `VIN` lo admite.

Antes de dar corriente:

1. Comprueba que los carriles positivo y negativo de la protoboard no están en
   cortocircuito.
2. Verifica si los carriles están partidos a mitad de la placa; algunos modelos
   necesitan un puente.
3. Confirma el orden real de `VCC`, `GND`, `SDA` y `SCL` en cada módulo.

Los picos del WiFi y del emisor infrarrojo del SCD41 pueden superar lo que
entrega con estabilidad un puerto USB débil. Un SCD41 puede seguir respondiendo
por I2C y, aun así, no completar mediciones si la alimentación cae durante esos
picos.

## Mapa de pines

### Bus I2C compartido

El OLED y el SCD41 comparten bus porque tienen direcciones diferentes: `0x3C`
para el OLED y `0x62` para el SCD41. El firmware también prueba `0x3D` para
pantallas OLED que usan esa dirección.

| ESP32 | Función | OLED | SCD41 |
|---|---|---|---|
| GPIO21 | SDA | `SDA` | `SDA` |
| GPIO22 | SCL | `SCL` | `SCL` |
| 3V3 | 3,3 V | `VCC` | `VDD`/`3V3` |
| GND | Masa | `GND` | `GND` |

### PMS5003

| ESP32 | Función | PMS5003 |
|---|---|---|
| GPIO16 | UART2 RX | `TXD` |
| GPIO17 | UART2 TX | `RXD` |
| GPIO25 | Control de reposo | `SET` |
| VIN/5 V | Alimentación | `VCC` |
| GND | Masa | `GND` |
| Sin conectar | — | `RST`, `NC`, `NIC` |

`SET` alto activa el sensor; bajo detiene el ventilador y el láser. `RST` puede
quedar sin conectar porque el módulo incorpora su propio pull-up.

## Uso de la protoboard

En el bloque central solo están conectados los cinco agujeros de cada mitad de
una fila (`a-e` y `f-j`). Los carriles laterales recorren la placa en vertical,
aunque algunos modelos los interrumpen por la mitad.

Una distribución sencilla es:

- Carril positivo izquierdo: 3,3 V.
- Carril negativo izquierdo: masa común.
- Una fila central para repartir SDA entre ESP32, OLED y SCD41.
- Otra fila central distinta para repartir SCL.
- PMS5003 conectado directamente al ESP32, salvo su masa común.

No coloques dos pines diferentes de un módulo en la misma mitad de una fila:
quedarían unidos eléctricamente.

## Software necesario

### Arduino IDE

Añade esta URL en **Preferencias → URLs adicionales del gestor de placas**:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Instala **esp32 by Espressif Systems** y selecciona **ESP32 Dev Module**.
Después instala estas bibliotecas desde el gestor:

| Biblioteca | Versión del perfil reproducible |
|---|---:|
| Adafruit GFX Library | 1.12.6 |
| Adafruit SSD1306 | 2.5.17 |
| Adafruit BusIO | 1.17.4 |
| PMS Library | 1.1.0 |
| Sensirion Core | 0.7.3 |
| Sensirion I2C SCD4x | 1.1.0 |
| WiFiManager | 2.0.17 |

El perfil de `sketch.yaml` usa el core ESP32 3.3.11. Otras versiones pueden
funcionar, pero no forman parte de la configuración reproducible del proyecto.

### Particiones

Selecciona obligatoriamente:

**Tools/Herramientas → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)**

El dashboard se almacena comprimido en PROGMEM. Con el esquema predeterminado
la aplicación no dispone de espacio suficiente; este proyecto no usa OTA ni
SPIFFS.

### Compilar y subir

Con Arduino IDE, abre `air-script.ino`, selecciona la placa, el esquema de
particiones y el puerto.

Con Arduino CLI 0.35 o posterior:

```bash
arduino-cli compile --profile esp32 .
arduino-cli upload --profile esp32 -p <PUERTO> .
```

El monitor serie funciona a 115200 baudios. Para capturar una sesión durante
30 segundos se puede usar `tools/serial-capture.py`, que requiere `pyserial`:

```bash
python -m pip install pyserial
python tools/serial-capture.py <PUERTO> 30
```

## Comprobación del bus I2C

Si el OLED o el SCD41 no aparecen, utiliza el sketch de diagnóstico incluido:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 tools/diag-i2c
arduino-cli upload --fqbn esp32:esp32:esp32 -p <PUERTO> tools/diag-i2c
```

El escaneo debe encontrar el SCD41 en `0x62` y el OLED en `0x3C` o `0x3D`.

## Ajustes específicos del lugar

Revisa `config.h` antes de dejar el aparato instalado:

- `ALTITUD_M` compensa la presión en las mediciones de CO₂. El valor incluido,
  650 m, pertenece al montaje original y no es un valor universal.
- `TZ_MADRID` configura la zona horaria de NTP.
- `HOSTNAME` determina la dirección `.local`.
- `AP_SSID` determina el nombre del portal de configuración.

El offset térmico del SCD41 depende del encapsulado y del montaje. Para
ajustarlo, deja el sensor estabilizarse junto a un termómetro de referencia y
aplica el nuevo offset entre `stopPeriodicMeasurement()` y
`startPeriodicMeasurement()`, tal como exige el sensor.
