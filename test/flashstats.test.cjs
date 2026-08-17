// Port del contador de escrituras de flashstats.cpp. Es RAM pura (arranca en
// 0, nunca se persiste) asi que el "algoritmo" es solo un incremento — el
// test existe para que quede documentado el contrato: nunca decrece, nunca
// se resetea salvo por un reinicio del aparato.

let contador;

function reset() { contador = 0; }
function contarEscrituraFlash() { contador++; }
function escriturasFlash() { return contador; }

function assert(cond, msg) {
  if (!cond) { console.error("FALLO:", msg); process.exitCode = 1; }
  else console.log("OK:", msg);
}

reset();
assert(escriturasFlash() === 0, "arranca en 0");

contarEscrituraFlash();
assert(escriturasFlash() === 1, "una escritura -> 1");

for (let i = 0; i < 99; i++) contarEscrituraFlash();
assert(escriturasFlash() === 100, "100 escrituras -> 100");

reset();
assert(escriturasFlash() === 0, "reset (simula reinicio) vuelve a 0");

console.log("flashstats.test.cjs: todo OK");
