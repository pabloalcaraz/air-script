# Captura el arranque del ESP32 por puerto serie.
# Existe porque `arduino-cli monitor` sale de inmediato cuando stdin no es un TTY
# (caso de las sesiones automatizadas), y el buffer se pierde al matarlo.
#
#   python tools/serial-capture.py <puerto> [segundos]
#   python tools/serial-capture.py COM3 30

import sys
import time

import serial

if len(sys.argv) < 2:
    raise SystemExit("Uso: python tools/serial-capture.py <puerto> [segundos]")

puerto = sys.argv[1]
segundos = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0

s = serial.Serial(puerto, 115200, timeout=0.2)

# Pulso de reset por DTR/RTS: sin esto se engancha a mitad del loop y nunca se
# ven ni el bootloader ROM ni las lineas de setup(), que es justo lo que interesa.
s.setDTR(False)
s.setRTS(True)
time.sleep(0.15)
s.setRTS(False)
time.sleep(0.05)

t0 = time.time()
while time.time() - t0 < segundos:
    datos = s.read(4096)
    if datos:
        sys.stdout.write(datos.decode("utf-8", "replace"))
        sys.stdout.flush()

s.close()
