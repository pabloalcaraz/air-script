// Sketch TEMPORAL de diagnostico del bus I2C. No forma parte del firmware.
//
//   arduino-cli compile --fqbn esp32:esp32:esp32 tools/diag-i2c
//   arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PUERTO> tools/diag-i2c
//
// Para volver al firmware real:
//   arduino-cli upload --profile esp32 -p <PUERTO>
//
// Corre en bucle a proposito: permite sacar y poner el sensor con la mano y
// ver el cambio en vivo, sin tener que reflashear entre medidas.

#include <Wire.h>

#define SDA_A 21
#define SCL_A 22
// Par alternativo: GPIO32/33 son ADC1, ahi si se puede leer tension real.
#define SDA_B 32
#define SCL_B 33

// Lee el pin en alta impedancia y luego con la pullup interna (~45k). Un pin
// sano en un bus con pullups da HIGH en ambos casos.
static void nivelesPin(const char *nombre, uint8_t pin) {
  pinMode(pin, INPUT);
  delay(5);
  int flotante = digitalRead(pin);

  pinMode(pin, INPUT_PULLUP);
  delay(5);
  int conPullup = digitalRead(pin);

  pinMode(pin, INPUT);
  Serial.printf("  %s (GPIO%2d): flotante=%s  pullup_interna=%s\n",
                nombre, pin, flotante ? "HIGH" : "LOW", conPullup ? "HIGH" : "LOW");
}

// Fuerza el pin en push-pull y comprueba si el nivel se sostiene. Es la unica
// forma sin multimetro de distinguir "sin pullup" de "cortocircuito duro":
// una linea al aire acepta los dos niveles, una en corto solo acepta uno.
static void pruebaCorto(const char *nombre, uint8_t pin) {
  pinMode(pin, OUTPUT);

  digitalWrite(pin, HIGH);
  delayMicroseconds(2000);
  pinMode(pin, INPUT);
  delayMicroseconds(50);
  int sostieneAlto = digitalRead(pin);

  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(2000);
  pinMode(pin, INPUT);
  delayMicroseconds(50);
  int sostieneBajo = digitalRead(pin);

  pinMode(pin, INPUT);

  const char *veredicto;
  if (!sostieneAlto && sostieneBajo)      veredicto = "CORTO A GND (o pin muerto en LOW)";
  else if (sostieneAlto && !sostieneBajo) veredicto = "CORTO A 3V3";
  else if (sostieneAlto && sostieneBajo)  veredicto = "linea libre, sin corto";
  else                                    veredicto = "linea muerta en ambos sentidos";

  Serial.printf("  %s (GPIO%2d): forzado HIGH->%s  LOW->%s  => %s\n",
                nombre, pin, sostieneAlto ? "HIGH" : "LOW",
                sostieneBajo ? "HIGH" : "LOW", veredicto);
}

// GPIO32/33 son ADC1: aqui si sale tension real, no solo nivel logico.
static void tensionAdc(const char *nombre, uint8_t pin) {
  pinMode(pin, INPUT);
  delay(5);
  // Rango completo 0-3.3V; la ADC del ESP32 no es lineal, vale como orientacion.
  analogSetPinAttenuation(pin, ADC_11db);
  int mv = analogReadMilliVolts(pin);
  Serial.printf("  %s (GPIO%2d): %d mV\n", nombre, pin, mv);
}

// 9 pulsos de reloj liberan a un esclavo que se quedo colgado a mitad de byte
// tirando de SDA. Si tras esto SDA sube, el problema era el sensor enganchado.
static void recuperarBus(uint8_t sda, uint8_t scl) {
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
  }
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  pinMode(scl, INPUT_PULLUP);
  delay(5);
  Serial.printf("  tras 9 pulsos de reloj: SDA=%s\n",
                digitalRead(sda) ? "HIGH (bus liberado)" : "LOW (sigue tirado)");
  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
}

static void escanear(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl, 100000);
  delay(20);
  int total = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("  0x%02X responde%s\n", addr, addr == 0x62 ? "  <- SCD41" : "");
      total++;
    }
  }
  Serial.printf("  total: %d dispositivo(s)\n", total);
  Wire.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== DIAGNOSTICO I2C ===");
  Serial.println("Saca y pon el SCD41 con la mano; cada ciclo son 4 s.\n");
}

void loop() {
  Serial.println("--- par A: SDA=21 SCL=22 (pines del firmware) ---");
  nivelesPin("SDA", SDA_A);
  nivelesPin("SCL", SCL_A);
  pruebaCorto("SDA", SDA_A);
  pruebaCorto("SCL", SCL_A);
  recuperarBus(SDA_A, SCL_A);
  escanear(SDA_A, SCL_A);

  Serial.println("--- par B: SDA=32 SCL=33 (solo util si mueves los cables) ---");
  tensionAdc("SDA", SDA_B);
  tensionAdc("SCL", SCL_B);
  escanear(SDA_B, SCL_B);

  Serial.println();
  delay(4000);
}
