# Air Script — estación de calidad del aire con ESP32

Air Script es un firmware para construir una estación doméstica de calidad del
aire con un ESP32. Mide partículas, CO₂, temperatura y humedad; muestra el
estado en una pantalla OLED y sirve un dashboard web completo desde la memoria
flash del propio dispositivo.

El equipo sigue midiendo sin WiFi. La red solo se usa para el dashboard, la
comparación opcional con el aire exterior y el backup opcional compatible con
Influx Line Protocol.

> Este es un proyecto doméstico y experimental. No es un instrumento médico,
> industrial ni un equipo de medida certificado.

## Funciones

- PM1.0, PM2.5 y PM10 con Plantower PMS5003.
- CO₂, temperatura y humedad con Sensirion SCD41.
- Pantalla OLED SSD1306 de 128 × 64 píxeles.
- Historial de 24 horas en RAM y dashboard responsive sin dependencias externas.
- Alertas, medias, percentil 95, tendencias y estimación de renovaciones de aire
  por hora (ACH).
- Comparación interior/exterior mediante AQICN y Open-Meteo.
- Vigilancia de datos rancios, recuperación automática del SCD41 y diagnóstico
  desde la web.
- Duty-cycle del PMS5003 para reducir el desgaste del ventilador y del láser.
- Backup opcional a Grafana Cloud mediante Influx Line Protocol.

## Hardware compatible

- ESP32-D DevKit V1 de 38 pines (WROOM-32).
- Plantower PMS5003.
- Sensirion SCD41.
- OLED SSD1306 I2C de 0,96", 128 × 64.

Conexiones principales:

| ESP32 | Dispositivo | Función |
|---|---|---|
| GPIO21 | OLED + SCD41 | I2C SDA |
| GPIO22 | OLED + SCD41 | I2C SCL |
| GPIO16 | PMS5003 TXD | UART2 RX |
| GPIO17 | PMS5003 RXD | UART2 TX |
| GPIO25 | PMS5003 SET | Duty-cycle |
| VIN | PMS5003 VCC | 5 V |
| 3V3 | OLED + SCD41 | 3,3 V |
| GND | Todos | Masa común |

Consulta [docs/HARDWARE.md](docs/HARDWARE.md) antes de alimentar el montaje.
Incluye el cableado completo, advertencias eléctricas y la configuración de
altitud del SCD41.

## Inicio rápido

### Arduino IDE

1. Clona o descarga este repositorio.
2. Instala el soporte **esp32 by Espressif Systems** y las bibliotecas indicadas
   en [docs/HARDWARE.md](docs/HARDWARE.md#software-necesario).
3. Abre `air-script.ino`.
4. Selecciona **ESP32 Dev Module**.
5. Selecciona el esquema de particiones
   **Huge APP (3MB No OTA/1MB SPIFFS)**.
6. Elige el puerto del ESP32 y sube el sketch.

### arduino-cli

`sketch.yaml` fija la placa, la partición y las versiones de las dependencias.
Con Arduino CLI 0.35 o posterior:

```bash
git clone https://github.com/<usuario>/qair-script.git
cd qair-script
arduino-cli compile --profile esp32 .
arduino-cli upload --profile esp32 -p <PUERTO> .
```

El esquema `Huge APP` es obligatorio: el servidor web embebido no cabe en la
partición de aplicación predeterminada.

## Primer arranque

Si el ESP32 no conoce una red, crea durante 180 segundos el punto de acceso
abierto `AirScript-Setup`:

1. Conéctate a `AirScript-Setup` desde un móvil u ordenador.
2. Abre `192.168.4.1` si el portal no aparece automáticamente.
3. Selecciona una red WiFi de 2,4 GHz y guarda sus credenciales.

Después se puede entrar en `http://air.local` o en la dirección IP mostrada en
el OLED y en el monitor serie. Las credenciales quedan guardadas en la NVS del
ESP32.

La configuración diaria, las métricas, la calibración y el diagnóstico se
explican en [docs/USAGE.md](docs/USAGE.md). La referencia HTTP completa está en
[docs/API.md](docs/API.md).

## Configuración

Los pines, umbrales, intervalos y tamaños de buffer viven en `config.h`. Antes
de instalar el equipo en otro lugar conviene revisar especialmente:

- `ALTITUD_M`: la compensación de presión del SCD41; el valor incluido es
  específico del montaje original.
- `TZ_MADRID`: zona horaria usada para NTP.
- `HOSTNAME` y `AP_SSID`: nombre local y punto de acceso de configuración.
- Umbrales de PM, CO₂, temperatura y humedad.

No añadas credenciales, tokens ni coordenadas privadas al código fuente. La
configuración operativa se introduce desde el dashboard y se persiste en NVS.

## Pruebas

La suite usa únicamente módulos incluidos en Node.js:

```bash
node tools/run-tests.cjs
# o
pnpm test
```

Si se modifica `webpage.h`, hay que regenerar y comprobar la versión comprimida
que realmente sirve el firmware:

```bash
node tools/gzip-web.cjs
node tools/gzip-web.cjs --check
```

Las instrucciones para trabajar en el código están en
[CONTRIBUTING.md](CONTRIBUTING.md).

## Seguridad y privacidad

Air Script está pensado para una red doméstica de confianza:

- El dashboard y la API usan HTTP local y no tienen autenticación.
- El portal inicial es un punto de acceso abierto mientras está activo.
- La ubicación, las credenciales WiFi y los tokens se guardan en la NVS del
  dispositivo; no deben considerarse cifrados frente a acceso físico.
- Las conexiones HTTPS validan el servidor con las raíces ISRG Root X1
  (AQICN/Open-Meteo) y DigiCert Global Root G2 (Grafana Cloud). Esas raíces
  caducan en 2035 y 2038 respectivamente y deberán actualizarse si los
  proveedores cambian de autoridad certificadora.
- El backup solo acepta URLs `https://` bajo `*.grafana.net`; cambiar su URL o
  usuario obliga a introducir de nuevo el token para que una credencial antigua
  no se envíe accidentalmente a otro destino.
- La búsqueda de ubicaciones la realiza el navegador contra Nominatim y el
  ESP32 consulta AQICN/Open-Meteo con las coordenadas seleccionadas.

No expongas el puerto 80 del dispositivo a Internet ni uses tokens de escritura
con más permisos de los imprescindibles.

## Documentación

- [Montaje, alimentación e instalación](docs/HARDWARE.md)
- [Uso, métricas, calibración y diagnóstico](docs/USAGE.md)
- [API HTTP](docs/API.md)
- [Desarrollo y contribuciones](CONTRIBUTING.md)

## Licencia

Este proyecto se distribuye bajo la [licencia MIT](LICENSE).
