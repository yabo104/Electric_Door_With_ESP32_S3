#include <Arduino.h>
#include "porton.h"

void setup() {
  inicializarPines();

  Serial.begin(115200);
  delay(500);

  Serial.println("Inicializacion Completa");
}

void loop() {
  if (Serial.available()) {
    comando = Serial.read();
  }

  procesarComandoSerial();
  actualizarEstadoPuerta();
}
