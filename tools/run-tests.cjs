// Lanza todos los test/*.test.cjs y devuelve codigo != 0 si alguno falla.
//   node tools/run-tests.cjs
const fs = require("fs"), path = require("path"), { execFileSync } = require("child_process");
const dir = path.join(__dirname, "..", "test");
const ficheros = fs.readdirSync(dir).filter(f => f.endsWith(".test.cjs")).sort();
let fallos = 0;
for (const f of ficheros) {
  console.log("\n=== " + f + " ===");
  try { execFileSync(process.execPath, [path.join(dir, f)], { stdio: "inherit" }); }
  catch (e) { fallos++; }
}
console.log(fallos ? "\n" + fallos + " fichero(s) con fallos" : "\nTodos los tests OK");
process.exit(fallos ? 1 : 0);
