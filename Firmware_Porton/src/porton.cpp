#include "porton.h"

char comando = 0;

char doorStd     = STOP;
char doorLastStd = STOP;

void inicializarPines() {
  pinMode(ZCROSS, INPUT);
  pinMode(D0, INPUT);
  pinMode(D1, INPUT);
  pinMode(D2, INPUT);
  pinMode(D3, INPUT);
  pinMode(ENCA, INPUT_PULLUP);
  pinMode(ENCB, INPUT_PULLUP);
  pinMode(FC_OPEN, INPUT_PULLUP);
  pinMode(FC_CLOSE, INPUT_PULLUP);

  pinMode(TRIGGER, OUTPUT);
  pinMode(RELAY_OP, OUTPUT);
  pinMode(RELAY_CL, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(TRIGGER, HIGH);
  digitalWrite(RELAY_OP, HIGH);
  digitalWrite(RELAY_CL, HIGH);
  digitalWrite(LED, HIGH);
}

void procesarComandoSerial() {
  switch (comando) {
    case '0': {
      digitalWrite(LED, HIGH);
      delay(500);
      digitalWrite(LED, LOW);
      delay(500);
      Serial.println("Testing LED");
    } break;

    case '1': {
      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, HIGH);
      delay(100);

      digitalWrite(RELAY_OP, LOW);
      digitalWrite(RELAY_CL, HIGH);
      comando = 0;
      delay(1000);
      Serial.println("Activa Relay Open ");
    } break;

    case '2': {
      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, HIGH);
      delay(100);

      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, LOW);
      comando = 0;
      delay(1000);
      Serial.println("Activa Relay Close ");
    } break;

    case '3': {
      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, HIGH);
      digitalWrite(TRIGGER, HIGH);
      delay(100);
      comando = 0;
      Serial.println("Desactiva all relay ");
    } break;

    case '4': {
      Serial.println("Reading Digital Inputs...");
      // bool inOpen     = digitalRead(FC_OPEN);
      // bool inClose    = digitalRead(FC_CLOSE);
      // bool inEncA     = digitalRead(ENCA);
      // bool inEncB     = digitalRead(ENCB);

      bool inD0 = digitalRead(D0);
      bool inD1 = digitalRead(D1);
      bool inD2 = digitalRead(D2);
      bool inD3 = digitalRead(D3);

      // Serial.println("inOpen:  " + String(inOpen));
      // Serial.println("inClose: " + String(inClose));
      // Serial.println("inEncA:  " + String(inEncA));
      // Serial.println("inEncB:  " + String(inEncB));

      Serial.println("inD0:  " + String(inD0));
      Serial.println("inD1:  " + String(inD1));
      Serial.println("inD2:  " + String(inD2));
      Serial.println("inD3:  " + String(inD3));

      delay(200);
    } break;

    case '5': {
      digitalWrite(TRIGGER, HIGH);
      delay(100);
      comando = 0;
      Serial.println(" Trigger HIHG ");
    } break;

    case '6': {
      digitalWrite(TRIGGER, LOW);
      delay(100);
      comando = 0;
      Serial.println(" Trigger LOW ");
    } break;

    case '7': {
      comando = 0;
    } break;

    default: {
    } break;
  }
}

void actualizarEstadoPuerta() {
  switch (doorStd) {

    case STOP: {
      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, HIGH);
      digitalWrite(TRIGGER, HIGH);
      Serial.println("Door Stoped ");
      doorStd = WAITING;
    } break;

    case OPENING: {
      /* Abriendo puerta */
      delay(10);
      digitalWrite(RELAY_OP, LOW);
      digitalWrite(RELAY_CL, HIGH);
      doorLastStd = OPENING;

      if (digitalRead(D0) || digitalRead(D1) || digitalRead(D2) || digitalRead(D3)) {
        digitalWrite(RELAY_OP, HIGH);
        digitalWrite(RELAY_CL, HIGH);
        Serial.println("Change to Closinig Door");
        delay(3000);
        doorStd = CLOSING;
        break;
      }

      if (!digitalRead(FC_OPEN) and digitalRead(FC_CLOSE)) {
        digitalWrite(RELAY_OP, HIGH);
        digitalWrite(RELAY_CL, HIGH);
        Serial.println(" Puerta se abrio ");
        doorLastStd = STOP;
        doorStd = STOP;
      }
    } break;

    case CLOSING: {
      /* Cerrando puerta */
      delay(10);
      digitalWrite(RELAY_OP, HIGH);
      digitalWrite(RELAY_CL, LOW);
      doorLastStd = CLOSING;

      if (digitalRead(D0) || digitalRead(D1) || digitalRead(D2) || digitalRead(D3)) {
        digitalWrite(RELAY_OP, HIGH);
        digitalWrite(RELAY_CL, HIGH);
        Serial.println("Change to Opening Door");
        delay(3000);
        doorStd = OPENING;
        break;
      }

      if (digitalRead(FC_OPEN) and !digitalRead(FC_CLOSE)) {
        digitalWrite(RELAY_OP, HIGH);
        digitalWrite(RELAY_CL, HIGH);
        Serial.println(" Puerta se cerro ");
        doorLastStd = STOP;
        doorStd = STOP;
      }
    } break;

    case WAITING: {
      if (digitalRead(D0) || digitalRead(D1) || digitalRead(D2) || digitalRead(D3)) {

        while (!digitalRead(D0) && !digitalRead(D1) && !digitalRead(D2) && !digitalRead(D3)) {
          delay(50);
        }
        delay(1000);

        if (digitalRead(FC_OPEN) and digitalRead(FC_CLOSE)) {
          Serial.println("Opening Door");
          doorStd = OPENING;
          break;
        }

        // if(doorLastStd == OPENING){
        //   Serial.println("Change to Opening Door");
        //   doorStd = OPENING;
        //   break;
        // }

        // if(doorLastStd == CLOSING){
        //   Serial.println("Change to Closing Door");
        //   doorStd = CLOSING;
        //   break;
        // }

        /* Puerta cerrada */
        if (digitalRead(FC_OPEN) and !digitalRead(FC_CLOSE)) {
          Serial.println("Opening Door");
          doorStd = OPENING;
          break;
        }
        /* Puerta abierta */
        if (!digitalRead(FC_OPEN) and digitalRead(FC_CLOSE)) {
          Serial.println("Closing Door");
          doorStd = CLOSING;
          break;
        }
      }
    } break;

    case ERROR: {
    } break;

    default: {
    } break;
  }
}
