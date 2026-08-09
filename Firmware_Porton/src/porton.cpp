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
static bool estadoD0Anterior = false;      // para detectar flanco de subida de D0
static unsigned long tUltimoD0 = 0;        // antirrebote de D0
static bool estadoResetAnterior = false;   // para detectar flanco de subida del boton de reset
static unsigned long tUltimoReset = 0;     // antirrebote del boton de reset
static unsigned long tUltimaPulsacionMovimiento = 0;  // para detectar doble pulsacion de D0

/* --- Interrupcion del encoder: solo registra que hubo un pulso, no cuenta ni decodifica --- */

#if DEBUG_PULSOS
static portMUX_TYPE muxPulsos = portMUX_INITIALIZER_UNLOCKED;
static volatile unsigned long pulsosEncA = 0;
static volatile unsigned long pulsosEncB = 0;
static volatile unsigned long pulsosZcross = 0;
static volatile unsigned long pulsosEncA2 = 0;
static volatile unsigned long tUltimoZcrossUS = 0;  // para el antirrebote de ZCROSS
static unsigned long tUltimoReportePulsos = 0;
#endif

static void IRAM_ATTR isrEncoder() {
  ultimoPulsoEncoderISR = millis();
#if DEBUG_PULSOS
  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosEncA++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
#endif
}

// ENCA2: sensor Hall en GPIO44 (ver porton.h), alternativa a ENCA mientras ENCA/ENCB tengan el
// problema de pull-up conocido en hardware. Confirmado en banco (sesion 006): 0 pulsos con el
// motor quieto, pulsos reales con el motor girando. Alimenta la MISMA deteccion de atasco que
// ENCA (ultimoPulsoEncoderISR) -- cualquiera de los dos canales que pulse resetea el timeout.
// Por eso esta ISR existe siempre, no solo bajo DEBUG_PULSOS (solo el conteo para el diagnostico
// esta condicionado).
static void IRAM_ATTR isrEncA2() {
  ultimoPulsoEncoderISR = millis();
#if DEBUG_PULSOS
  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosEncA2++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
#endif
}

#if DEBUG_PULSOS
// Diagnostico temporal: ENCB y ZCROSS no se usan todavia para nada mas que contar pulsos aqui.
static void IRAM_ATTR isrEncBDiag() {
  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosEncB++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
}

static void IRAM_ATTR isrZcrossDiag() {
  // Antirrebote en la propia ISR: el comparador analogico genera mas de un flanco de subida
  // muy cercano en el mismo cruce (confirmado con osciloscopio + este mismo contador). Se
  // descarta un flanco si llego a menos de ZCROSS_DEBOUNCE_US del ultimo aceptado.
  unsigned long ahoraUS = micros();
  if (ahoraUS - tUltimoZcrossUS < ZCROSS_DEBOUNCE_US) return;
  tUltimoZcrossUS = ahoraUS;

  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosZcross++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
}

// Se llama desde loop(). No bloqueante: solo imprime cuando pasaron DEBUG_PULSOS_MS.
void mostrarPulsos() {
  unsigned long ahora = millis();
  if (ahora - tUltimoReportePulsos < DEBUG_PULSOS_MS) return;
  tUltimoReportePulsos = ahora;

  portENTER_CRITICAL(&muxPulsos);
  unsigned long a  = pulsosEncA;
  unsigned long b  = pulsosEncB;
  unsigned long z  = pulsosZcross;
  unsigned long a2 = pulsosEncA2;
  pulsosEncA = 0;
  pulsosEncB = 0;
  pulsosZcross = 0;
  pulsosEncA2 = 0;
  portEXIT_CRITICAL(&muxPulsos);

  Serial.print("[DIAG] pulsos/");
  Serial.print(DEBUG_PULSOS_MS);
  Serial.print("ms -> ENCA=");
  Serial.print(a);
  Serial.print("  ENCB=");
  Serial.print(b);
  Serial.print("  ZCROSS=");
  Serial.print(z);
  Serial.print("  ENCA2=");
  Serial.println(a2);
}
#endif

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

/* --- Control remoto: D0 mueve, D2 solo saca de ERROR. Deteccion de flanco (no de nivel) --- */
/* Un boton mantenido presionado no debe generar comandos repetidos: por eso se dispara solo
   en la transicion de "no presionado" a "presionado", nunca mientras se lo mantiene. */

static bool botonD0Presionado() {
  bool actual = digitalRead(D0);
  bool flanco = actual && !estadoD0Anterior;
  estadoD0Anterior = actual;

  if (!flanco) return false;

  unsigned long ahora = millis();
  if (ahora - tUltimoD0 < BOTON_DEBOUNCE_MS) return false;  // rebote, se ignora
  tUltimoD0 = ahora;
  return true;
}

// Boton de reset de ERROR. Pin fisico en D2 (ver porton.h) -- el nombre de la funcion no lleva
// el pin a proposito, para que reasignarlo a futuro no obligue a un rename.
static bool botonResetPresionado() {
  bool actual = digitalRead(D2);
  bool flanco = actual && !estadoResetAnterior;
  estadoResetAnterior = actual;

  if (!flanco) return false;

  unsigned long ahora = millis();
  if (ahora - tUltimoReset < BOTON_DEBOUNCE_MS) return false;  // rebote, se ignora
  tUltimoReset = ahora;
  return true;
}

/* Se llama con cada pulsacion de D0 detectada mientras la puerta se mueve (o esta a punto de
   reanudar tras una reversa pendiente). Primera pulsacion: detiene y programa la reversa
   (sentidoOpuesto). Segunda pulsacion dentro de DOBLE_PULSACION_MS: cancela la reversa y deja
   el porton a medio camino (apertura/cierre parcial), sin invertir. */
static void manejarPulsacionEnMovimiento(char sentidoOpuesto) {
  unsigned long ahora = millis();
  bool esDoble = (ahora - tUltimaPulsacionMovimiento) < DOBLE_PULSACION_MS;
  tUltimaPulsacionMovimiento = ahora;

  detenerMotor();
  doorStd = DETENIDA;
  tEntradaDetenida = ahora;

  if (esDoble) {
    sentidoPendiente = 0;
    Serial.println("Detenida por doble pulsacion (movimiento parcial)");
  } else {
    sentidoPendiente = sentidoOpuesto;
  }
}

/* --- Estados --- */

static void manejarDetenida() {
  // Reversa pendiente: hay que esperar el tiempo de parada obligatorio antes de arrancar en el
  // sentido contrario. Una nueva pulsacion de D0 durante esta espera cuenta como "doble
  // pulsacion" (ver manejarPulsacionEnMovimiento) y cancela la reversa.
  if (sentidoPendiente != 0) {
    if (botonD0Presionado()) {
      manejarPulsacionEnMovimiento(sentidoPendiente);
      return;
    }
    if (millis() - tEntradaDetenida >= REVERSE_STOP_MS) {
      char sentido = sentidoPendiente;
      sentidoPendiente = 0;
      iniciarMovimiento(sentido);
    }
    return;
  }

  if (!botonD0Presionado()) return;

  // El sentido se decide por la posicion REAL (finales de carrera), no por el ultimo movimiento:
  // abierta -> el boton cierra; cerrada -> el boton abre; posicion intermedia/desconocida
  // (ningun fin de carrera activo, incluido un movimiento parcial anterior) -> por defecto cierra.
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

  // 4) Boton D0 de nuevo: nunca se invierte el sentido en caliente (ver
  //    manejarPulsacionEnMovimiento -- ademas resuelve la doble pulsacion = detener y quedarse).
  if (botonD0Presionado()) {
    manejarPulsacionEnMovimiento(sentido == ABRIENDO ? CERRANDO : ABRIENDO);
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

  // ENCA2: sensor Hall en GPIO44 (U0RXD, libre: no se usa la UART fisica). Alimenta la deteccion
  // de atasco en paralelo con ENCA (ver isrEncA2). Asumido open-collector (INPUT_PULLUP) como un
  // sensor Hall tipico -- confirmar contra el datasheet del sensor puntual que se conecte.
  pinMode(ENCA2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCA2), isrEncA2, CHANGE);

#if DEBUG_PULSOS
  // Diagnostico temporal (ver porton.h -> DEBUG_PULSOS): cuenta pulsos de ENCB y ZCROSS, que hoy
  // no se usan para nada mas. ZCROSS es un pulso en alto por cada cruce por cero -> RISING.
  attachInterrupt(digitalPinToInterrupt(ENCB), isrEncBDiag, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ZCROSS), isrZcrossDiag, RISING);
#endif
}

void procesarComandoSerial() {
  switch (comando) {
    case '0': {  // prueba de LED (activo en LOW)
      digitalWrite(LED, LOW);
      delay(500);
      digitalWrite(LED, HIGH);
      delay(500);
      Serial.println("Testing LED");
      comando = 0;
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
      comando = 0;
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
  // El boton de reset (D2) se consume siempre (para no perder flancos), pero solo hace algo si
  // estamos en ERROR.
  bool reset = botonResetPresionado();

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
      // Motor ya detenido (se llamo detenerMotor() al entrar). Unica salida: D2. Vuelve a
      // DETENIDA "como recien reiniciado" -- el proximo D0 decide el sentido por posicion real.
      if (reset) {
        sentidoPendiente = 0;
        doorStd = DETENIDA;
        tEntradaDetenida = millis();
        Serial.println("ERROR: reset manual (D2). Esperando D0.");
      }
      break;

    default:
      break;
  }
}
