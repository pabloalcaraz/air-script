#include <Wire.h>
#include "config.h"
#include "i2cbus.h"

BusI2c bus_i2c = {false, false, false, 0};

void i2cInit() {
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);  // margen para que los modulos arranquen antes del escaneo
}

bool i2cResponde(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void i2cEscanear() {
  bus_i2c = {false, false, false, 0};
  Serial.printf("[I2C] Escaneando bus (SDA=%d, SCL=%d)...\n", SDA_PIN, SCL_PIN);

  for (uint8_t addr = 1; addr < 127; addr++) {
    if (!i2cResponde(addr)) continue;

    const char *quien = "";
    if (addr == OLED_ADDR)          { bus_i2c.oledEn3C = true; quien = "  <- OLED"; }
    else if (addr == OLED_ADDR_ALT) { bus_i2c.oledEn3D = true; quien = "  <- OLED (addr alternativa)"; }
    else if (addr == SCD41_ADDR)    { bus_i2c.scd41    = true; quien = "  <- SCD41"; }
    Serial.printf("[I2C] 0x%02X responde%s\n", addr, quien);
    bus_i2c.total++;
  }

  if (bus_i2c.total == 0) {
    Serial.println("[I2C] NADIE contesta. Revisa SDA/SCL/3V3/GND del bus.");
    return;
  }
  if (!bus_i2c.scd41) {
    Serial.println("[I2C] SCD41 (0x62) NO esta en el bus.");
    Serial.println("      -> si el modulo tiene 5 pines, alimenta por 3V3, no por VIN.");
    Serial.println("      -> revisa SDA/SCL del SCD41 y la continuidad de su fila.");
  }
  if (!bus_i2c.oledEn3C && !bus_i2c.oledEn3D) {
    Serial.println("[I2C] OLED no esta ni en 0x3C ni en 0x3D.");
  }
}
