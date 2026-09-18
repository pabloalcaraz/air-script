## Qué cambia y por qué

## Comprobaciones

- [ ] `node tools/run-tests.cjs` pasa
- [ ] Si tocó `webpage.h`: `node tools/gzip-web.cjs` y `node tools/gzip-web.cjs --check`
- [ ] `arduino-cli compile --profile esp32 --warnings all .` sin warnings nuevos
- [ ] Si afecta a sensores, red, NVS o tiempos: validado en hardware real
- [ ] No añade credenciales, tokens ni ubicaciones privadas
