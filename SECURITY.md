# Seguridad

## Versiones mantenidas

Las correcciones de seguridad se aplican a la rama `main`. Este proyecto no
mantiene por ahora ramas antiguas ni versiones con soporte prolongado.

## Notificar una vulnerabilidad

Usa **Security → Report a vulnerability** en GitHub para enviar el informe de
forma privada. No abras una incidencia pública si el problema puede exponer
credenciales, ubicaciones o permitir modificar un dispositivo.

Incluye, si es posible, la versión o commit afectado, el escenario necesario
para reproducirlo, su impacto y una prueba de concepto sin datos personales.
No pruebes el fallo en dispositivos o redes que no te pertenezcan.

## Modelo de amenaza

Air Script es un proyecto doméstico y experimental para una LAN de confianza,
no un equipo preparado para quedar expuesto a Internet:

- El dashboard y la API local usan HTTP y no tienen autenticación. Cualquier
  cliente de la misma red puede leer datos y cambiar la configuración.
- El portal de provisión `AirScript-Setup` es un punto de acceso abierto durante
  un máximo de 180 segundos cuando el equipo no puede conectarse.
- WiFi, ubicación y tokens se guardan en la NVS del ESP32. No se consideran
  protegidos frente a alguien con acceso físico al dispositivo.
- Las conexiones salientes de AQICN, Open-Meteo y Grafana Cloud usan HTTPS y
  validan certificados mediante autoridades raíz incluidas en el firmware.
  Hay que actualizarlas si caducan o los proveedores cambian de cadena.
- Las credenciales cloud se restringen a endpoints `https://*.grafana.net`,
  pero el token se introduce inicialmente desde el dashboard HTTP local.

No redirijas el puerto 80 desde el router, no sitúes el dispositivo en una red
hostil y usa tokens dedicados con el alcance mínimo. Borra o reflashea la NVS
antes de vender, regalar o desechar el ESP32.
