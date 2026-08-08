#include "porton.h"

char comando = 0;

char doorStd     = DETENIDA;
char doorLastStd = DETENIDA;

/* --- Estado interno de la maquina (no expuesto en el header: uso exclusivo de este archivo) --- */

static volatile unsigned long ultimoPulsoEncoderISR = 0;  // se actualiza desde la interrupcion
static unsigned long tInicioSentido = 0;   // cuando se activo el relevo del sentido actual
static bool triacActivo = false;           // si ya se habilito el TRIAC en el movimiento actual
static char sentidoPendiente = 0;          // ABRIENDO/CERRANDO si hay una reversa en espera
static unsigned long tEntradaDetenida = 0; // cuando se entro a DETENIDA (para REVERSE_STOP_MS)
static bool estadoBotonAnterior = false;   // para detectar flanco de subida de D0/D1
static unsigned long tUltimoBoton = 0;     // antirrebote

/* --- Interrupcion del encoder: solo registra que hubo un pulso, no cuenta ni decodifica --- */

static void IRAM_ATTR isrEncoder() {
  ultimoPulsoEncoderISR = millis();
}

/* --- Salidas de potencia, con el interlock centralizado en un solo lugar --- */

// Escribe SIEMPRE los dos relevos juntos: por construccion, nunca quedan los dos en LOW.
static void seleccionarSentido(char sentido) {
  digitalWrite(RELAY_OP, sentido == ABRIENDO ? LOW : HIGH);
  digitalWrite(RELAY_CL, sentido == CERRANDO ? LOW : HIGH);
}

static void detenerMotor() {
  digitalWrite(TRIGGER, HIGH);   // corta potencia primero
  seleccionarSentido(0);         // y libera el sentido (los dos relevos a HIGH)
  triacActivo = false;
}

static void iniciarMovimiento(char sentido) {
  seleccionarSentido(sentido);
  tInicioSentido = millis();
  triacActivo = false;
  doorStd = sentido;
  doorLastStd = sentido;
}

/* --- Control remoto: D0/D1, con deteccion de flanco (no de nivel) --- */
/* Un boton mantenido presionado no debe generar comandos repetidos: por eso se dispara solo
   en la transicion de "no presionado" a "presionado", nunca mientras se lo mantiene. */

static bool botonPresionado() {
  bool actual = digitalRead(D0) || digitalRead(D1);
  bool flanco = actual && !estadoBotonAnterior;
  estadoBotonAnterior = actual;

  if (!flanco) return false;

  unsigned long ahora = millis();
  if (ahora - tUltimoBoton < BOTON_DEBOUNCE_MS) return false;  // rebote, se ignora
  tUltimoBoton = ahora;
  return true;
}

/* --- Estados --- */

static void manejarDetenida() {
  // Reversa pendiente: hay que esperar el tiempo de parada obligatorio antes de arrancar en el
  // sentido contrario. Mientras se espera, no se atienden botones nuevos (evita que mantener
  // presionado el boton reinicie la cuenta).
  if (sentidoPendiente != 0) {
    if (millis() - tEntradaDetenida >= REVERSE_STOP_MS) {
      char sentido = sentidoPendiente;
      sentidoPendiente = 0;
      iniciarMovimiento(sentido);
    }
    return;
  }

  if (!botonPresionado()) return;

  // El sentido se decide por la posicion REAL (finales de carrera), no por el ultimo movimiento:
  // abierta -> el boton cierra; cerrada -> el boton abre; posicion intermedia/desconocida
  // (ningun fin de carrera activo) -> por defecto cierra.
  bool enFinOpen  = !digitalRead(FC_OPEN);
  bool enFinClose = !digitalRead(FC_CLOSE);

  char sentido;
  if (enFinOpen) {
    sentido = CERRANDO;
  } else if (enFinClose) {
    sentido = ABRIENDO;
  } else {
    sentido = CERRANDO;
  }

  iniciarMovimiento(sentido);
}

static void manejarMovimiento(char sentido, uint8_t pinFinCarreraDestino) {
  // 1) Habilitar el TRIAC recien despues del asentamiento del relevo. Por ahora sin sincronismo
  //    a ZCROSS: se trata como interruptor de estado solido todo/nada (maxima potencia).
  if (!triacActivo && (millis() - tInicioSentido >= RELAY_SETTLE_MS)) {
    digitalWrite(TRIGGER, LOW);
    triacActivo = true;
    // El timeout de atasco cuenta desde que hay potencia real, no desde que arranco el settle.
    ultimoPulsoEncoderISR = millis();
  }

  // 2) Fin de carrera de destino alcanzado -> detener.
  if (!digitalRead(pinFinCarreraDestino)) {
    detenerMotor();
    doorStd = DETENIDA;
    tEntradaDetenida = millis();
    Serial.println(sentido == ABRIENDO ? "Puerta abierta" : "Puerta cerrada");
    return;
  }

  // 3) Atasco: el TRIAC ya esta activo y no llegan pulsos de encoder hace demasiado tiempo.
  if (triacActivo && (millis() - ultimoPulsoEncoderISR > ENCODER_TIMEOUT_MS)) {
    detenerMotor();
    doorStd = ERROR;
    Serial.println("ERROR: atasco detectado (sin pulsos de encoder)");
    return;
  }

  // 4) Boton presionado de nuevo: nunca se invierte el sentido en caliente. Se detiene y se dej
  //    pendiente la reversa; manejarDetenida() la arranca despues de REVERSE_STOP_MS.
  if (botonPresionado()) {
    detenerMotor();
    sentidoPendiente = (sentido == ABRIENDO) ? CERRANDO : ABRIENDO;
    doorStd = DETENIDA;
    tEntradaDetenida = millis();
  }
}

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
  digitalWrite(LED, HIGH);   // LED activo en LOW: HIGH = apagado

  attachInterrupt(digitalPinToInterrupt(ENCA), isrEncoder, CHANGE);
}

void procesarComandoSerial() {
  switch (comando) {
    case '0': {  // prueba de LED (activo en LOW)
      digitalWrite(LED, LOW);
      delay(500);
      digitalWrite(LED, HIGH);
      delay(500);
      Serial.println("Testing LED");
    } break;

    case '1': {  // prueba de banco: relevo de apertura, sin habilitar el TRIAC
      seleccionarSentido(ABRIENDO);
      comando = 0;
      Serial.println("Activa Relay Open");
    } break;

    case '2': {  // prueba de banco: relevo de cierre, sin habilitar el TRIAC
      seleccionarSentido(CERRANDO);
      comando = 0;
      Serial.println("Activa Relay Close");
    } break;

    case '3': {  // detiene todo (relevos + TRIAC)
      detenerMotor();
      comando = 0;
      Serial.println("Desactiva all relay");
    } break;

    case '4': {
      Serial.println("Reading Digital Inputs...");
      Serial.println("D0: " + String(digitalRead(D0)));
      Serial.println("D1: " + String(digitalRead(D1)));
      Serial.println("D2: " + String(digitalRead(D2)));
      Serial.println("D3: " + String(digitalRead(D3)));
    } break;

    case '5': {  // fuerza el TRIAC apagado
      digitalWrite(TRIGGER, HIGH);
      comando = 0;
      Serial.println("Trigger HIGH (TRIAC apagado)");
    } break;

    case '6': {  // fuerza el TRIAC encendido
      digitalWrite(TRIGGER, LOW);
      comando = 0;
      Serial.println("Trigger LOW (TRIAC encendido)");
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
    case DETENIDA:
      manejarDetenida();
      break;

    case ABRIENDO:
      manejarMovimiento(ABRIENDO, FC_OPEN);
      break;

    case CERRANDO:
      manejarMovimiento(CERRANDO, FC_CLOSE);
      break;

    case ERROR:
      // Motor ya detenido (se llamo detenerMotor() al entrar). Sin recuperacion automatica
      // todavia: queda a la espera de que se defina que hacer (ver bitacora/requirements.md).
      break;

    default:
      break;
  }
}
