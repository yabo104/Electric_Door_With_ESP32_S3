#include "porton.h"
#include "esp_timer.h"
#include <WiFi.h>

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
static char ultimaLlegadaConfirmada = 0;   // ultimo ABRIENDO/CERRANDO que llego a su fin de
                                            // carrera de verdad (0 = ninguna todavia)

/* --- Contador de trayecto (ENCA2): solo valido entre dos finales de carrera confirmados --- */

static portMUX_TYPE muxTrayecto = portMUX_INITIALIZER_UNLOCKED;
static volatile long contadorTrayecto = 0;   // suma en el sentido del trayecto, resta al reves
static volatile bool trayectoActivo = false; // false = conteo invalido (origen no confirmado)
static volatile char sentidoTrayecto = 0;    // sentido con el que arranco el trayecto actual

/* --- Parpadeo no bloqueante del LED al detectar un fin de carrera --- */
/* Fases: 0 = inactivo. 1 = pausa apagado (para que se note el flanco: el LED ya viene encendido
   fijo desde que el motor se movia, si se prendiera de nuevo sin una pausa visible no se
   distingue del "encendido por movimiento"). 2 = parpadeo encendido. */

static uint8_t faseParpadeoLed = 0;
static unsigned long tFaseParpadeoLed = 0;

/* --- Disparo del TRIAC por angulo de fase, sincronizado a ZCROSS --- */

static portMUX_TYPE muxRampa = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t delayDisparoActualUS = DISPARO_US_MAX;  // leido por isrZcross()
static unsigned long tInicioRampa = 0;      // cuando arranco el TRIAC en este movimiento
static volatile unsigned long tUltimoZcrossUS = 0;  // antirrebote de ZCROSS (real, no solo diag.)
static esp_timer_handle_t timerDisparo = NULL;  // dispara el gate tras el retardo de fase
static esp_timer_handle_t timerPulso = NULL;    // apaga el pulso de gate tras PULSO_TRIAC_US

// Se llama solo desde manejarMovimiento(), mientras triacActivo. Interpola el retardo de disparo
// entre DISPARO_US_MIN (al arrancar) y DISPARO_US_MAX (potencia de crucero) durante
// RAMPA_ARRANQUE_MS; despues queda fijo en DISPARO_US_MAX. No bloqueante.
static void actualizarRampa() {
  unsigned long elapsed = millis() - tInicioRampa;
  uint32_t delay;
  if (elapsed >= RAMPA_ARRANQUE_MS) {
    delay = DISPARO_US_MAX;
  } else {
    delay = DISPARO_US_MIN -
            (uint32_t)(((uint64_t)(DISPARO_US_MIN - DISPARO_US_MAX) * elapsed) / RAMPA_ARRANQUE_MS);
  }
  portENTER_CRITICAL(&muxRampa);
  delayDisparoActualUS = delay;
  portEXIT_CRITICAL(&muxRampa);
}

// Callbacks de los esp_timer (corren en la tarea de esp_timer, no en una ISR real -- ver la nota
// en inicializarPines()). Se mantienen minimos igual: un digitalWrite y, en el primero, programar
// el segundo.
static void IRAM_ATTR onDisparoTriac(void* arg) {
  digitalWrite(TRIGGER, LOW);
  esp_timer_start_once(timerPulso, PULSO_TRIAC_US);
}

static void IRAM_ATTR onFinPulsoTriac(void* arg) {
  digitalWrite(TRIGGER, HIGH);
}

/* --- Interrupcion del encoder: solo registra que hubo un pulso, no cuenta ni decodifica --- */

#if DEBUG_PULSOS
static portMUX_TYPE muxPulsos = portMUX_INITIALIZER_UNLOCKED;
static volatile unsigned long pulsosZcross = 0;
static volatile unsigned long pulsosEncA2 = 0;
static unsigned long tUltimoReportePulsos = 0;
#endif

static void IRAM_ATTR isrEncoder() {
  ultimoPulsoEncoderISR = millis();
}

// ENCA2: sensor Hall en GPIO44 (ver porton.h), alternativa a ENCA mientras ENCA/ENCB tengan el
// problema de pull-up conocido en hardware. Confirmado en banco (sesion 006): 0 pulsos con el
// motor quieto, pulsos reales con el motor girando. Alimenta la MISMA deteccion de atasco que
// ENCA (ultimoPulsoEncoderISR) -- cualquiera de los dos canales que pulse resetea el timeout.
// Tambien alimenta el contador de trayecto (suma en el sentido del trayecto, resta al reves) --
// ver manejarDetenida()/manejarMovimiento() para donde se arma/lee/invalida. Por eso esta ISR
// existe siempre, no solo bajo DEBUG_PULSOS (ahi solo esta condicionado el conteo de diagnostico).
static void IRAM_ATTR isrEncA2() {
  ultimoPulsoEncoderISR = millis();
#if DEBUG_PULSOS
  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosEncA2++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
#endif

  portENTER_CRITICAL_ISR(&muxTrayecto);
  if (trayectoActivo) {
    if (doorStd == sentidoTrayecto) {
      contadorTrayecto++;
    } else {
      contadorTrayecto--;
    }
  }
  portEXIT_CRITICAL_ISR(&muxTrayecto);
}

// ZCROSS: sincroniza el disparo del TRIAC por angulo de fase. Real desde esta sesion (antes era
// solo diagnostico) -- por eso existe siempre, igual que isrEncA2. Antirrebote obligatorio (no
// solo para el conteo): el comparador analogico genera mas de un flanco de subida muy cercano en
// el mismo cruce real, y armar el disparo dos veces por el mismo cruce es justo lo que este
// antirrebote evita.
static void IRAM_ATTR isrZcross() {
  unsigned long ahoraUS = micros();
  if (ahoraUS - tUltimoZcrossUS < ZCROSS_DEBOUNCE_US) return;
  tUltimoZcrossUS = ahoraUS;

#if DEBUG_PULSOS
  portENTER_CRITICAL_ISR(&muxPulsos);
  pulsosZcross++;
  portEXIT_CRITICAL_ISR(&muxPulsos);
#endif

  if (!triacActivo) return;  // motor detenido: no hay nada que disparar

  uint32_t delay = delayDisparoActualUS;  // lectura de 32 bits, atomica en este chip
  esp_timer_stop(timerDisparo);           // por si quedaba uno armado del cruce anterior
  esp_timer_start_once(timerDisparo, delay);
}

#if DEBUG_PULSOS
// Se llama desde loop(). No bloqueante: solo imprime cuando pasaron DEBUG_PULSOS_MS.
void mostrarPulsos() {
  unsigned long ahora = millis();
  if (ahora - tUltimoReportePulsos < DEBUG_PULSOS_MS) return;
  tUltimoReportePulsos = ahora;

  portENTER_CRITICAL(&muxPulsos);
  unsigned long z  = pulsosZcross;
  unsigned long a2 = pulsosEncA2;
  pulsosZcross = 0;
  pulsosEncA2 = 0;
  portEXIT_CRITICAL(&muxPulsos);

  Serial.print("[DIAG] pulsos/");
  Serial.print(DEBUG_PULSOS_MS);
  Serial.print("ms -> ZCROSS=");
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
  triacActivo = false;           // primero: isrZcross() ya no arma disparos nuevos
  esp_timer_stop(timerDisparo);  // cancela un disparo de fase que ya haya quedado programado
  digitalWrite(TRIGGER, HIGH);   // corta potencia
  seleccionarSentido(0);         // y libera el sentido (los dos relevos a HIGH)
  digitalWrite(LED, HIGH);       // LED activo en LOW: HIGH = apagado (motor detenido)
}

static void iniciarMovimiento(char sentido) {
  seleccionarSentido(sentido);
  tInicioSentido = millis();
  triacActivo = false;
  doorStd = sentido;
  doorLastStd = sentido;
  digitalWrite(LED, LOW);   // LED encendido fijo mientras el motor esta en movimiento
  faseParpadeoLed = 0;      // cancela un parpadeo pendiente: ahora manda el "encendido fijo"
}

// Arranca la secuencia de parpadeo (ver declaracion de faseParpadeoLed mas arriba). No escribe el
// LED de una -- lo primero es la pausa apagado, para que el flanco se note.
static void iniciarParpadeoLed() {
  faseParpadeoLed = 1;
  tFaseParpadeoLed = millis();
}

// No bloqueante: se llama cada iteracion desde actualizarEstadoPuerta().
static void actualizarParpadeoLed() {
  unsigned long ahora = millis();
  switch (faseParpadeoLed) {
    case 1:  // pausa apagado
      if (ahora - tFaseParpadeoLed >= LED_PAUSA_PARPADEO_MS) {
        digitalWrite(LED, LOW);
        tFaseParpadeoLed = ahora;
        faseParpadeoLed = 2;
      }
      break;
    case 2:  // parpadeo encendido
      if (ahora - tFaseParpadeoLed >= LED_PARPADEO_MS) {
        digitalWrite(LED, HIGH);
        faseParpadeoLed = 0;
      }
      break;
    default:
      break;
  }
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
    portENTER_CRITICAL(&muxTrayecto);
    trayectoActivo = false;  // movimiento parcial: el trayecto queda invalido, no se reporta
    portEXIT_CRITICAL(&muxTrayecto);
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

  // El sentido se decide por la posicion REAL (finales de carrera) cuando hay uno activo.
  bool enFinOpen  = !digitalRead(FC_OPEN);
  bool enFinClose = !digitalRead(FC_CLOSE);

  char sentido;
  if (enFinOpen) {
    sentido = CERRANDO;
  } else if (enFinClose) {
    sentido = ABRIENDO;
  } else if (ultimaLlegadaConfirmada == CERRANDO) {
    // Ningun fin de carrera activo, pero la ultima llegada confirmada fue "cerrada". Pasa en la
    // practica: por la inercia del motor, el iman puede rebasar el sensor al frenar y el fin de
    // carrera se desactiva de nuevo aunque el porton este efectivamente cerrado. Sin esto, el
    // codigo volveria a intentar CERRAR contra el tope -- atasco mecanico real, visto en banco.
    sentido = ABRIENDO;
  } else if (ultimaLlegadaConfirmada == ABRIENDO) {
    sentido = CERRANDO;
  } else {
    // Nunca se confirmo una llegada (arranque en frio, posicion realmente desconocida) -> el
    // default seguro sigue siendo cerrar.
    sentido = CERRANDO;
  }

  // Contador de trayecto: solo arranca (en 0) si el origen es un final de carrera confirmado.
  // Desde posicion intermedia el trayecto queda invalido -- no hay desde donde contar.
  portENTER_CRITICAL(&muxTrayecto);
  if (enFinOpen || enFinClose) {
    contadorTrayecto = 0;
    sentidoTrayecto = sentido;
    trayectoActivo = true;
  } else {
    trayectoActivo = false;
  }
  portEXIT_CRITICAL(&muxTrayecto);

  iniciarMovimiento(sentido);
}

static void manejarMovimiento(char sentido, uint8_t pinFinCarreraDestino) {
  // 1) Habilitar el TRIAC recien despues del asentamiento del relevo, arrancando la rampa de
  //    arranque suave (DISPARO_US_MIN -> DISPARO_US_MAX en RAMPA_ARRANQUE_MS). El disparo real
  //    lo hace isrZcross(), sincronizado a cada cruce por cero -- aqui solo se arma el estado.
  if (!triacActivo && (millis() - tInicioSentido >= RELAY_SETTLE_MS)) {
    tInicioRampa = millis();
    portENTER_CRITICAL(&muxRampa);
    delayDisparoActualUS = DISPARO_US_MIN;
    portEXIT_CRITICAL(&muxRampa);
    triacActivo = true;
    // El timeout de atasco cuenta desde que hay potencia real, no desde que arranco el settle.
    ultimoPulsoEncoderISR = millis();
  }

  if (triacActivo) {
    actualizarRampa();
  }

  // 2) Fin de carrera de destino alcanzado -> detener. Prioridad estricta: primero se corta la
  //    salida (detenerMotor(), instantaneo), recien despues el indicador optico (parpadeo del
  //    LED) -- nunca al reves.
  if (!digitalRead(pinFinCarreraDestino)) {
    detenerMotor();
    doorStd = DETENIDA;
    tEntradaDetenida = millis();
    Serial.println(sentido == ABRIENDO ? "Puerta abierta" : "Puerta cerrada");

    ultimaLlegadaConfirmada = sentido;  // ver manejarDetenida(): cubre el rebote del iman por
                                        // inercia, cuando el fin de carrera se desactiva de nuevo

    portENTER_CRITICAL(&muxTrayecto);
    bool trayectoCompleto = trayectoActivo;
    long pulsos = contadorTrayecto;
    trayectoActivo = false;
    portEXIT_CRITICAL(&muxTrayecto);
    if (trayectoCompleto) {
      Serial.print("[TRAYECTO] completo, sentido=");
      Serial.print(sentido == ABRIENDO ? "ABRIENDO" : "CERRANDO");
      Serial.print(", pulsos=");
      Serial.println(pulsos);
    }

    iniciarParpadeoLed();  // confirma que se detecto el iman del fin de carrera
    return;
  }

  // 3) Atasco: el TRIAC ya esta activo y no llegan pulsos de encoder hace demasiado tiempo.
  if (triacActivo && (millis() - ultimoPulsoEncoderISR > ENCODER_TIMEOUT_MS)) {
    detenerMotor();
    doorStd = ERROR;
    portENTER_CRITICAL(&muxTrayecto);
    trayectoActivo = false;  // atasco: el trayecto queda invalido, no se reporta
    portEXIT_CRITICAL(&muxTrayecto);
    Serial.println("ERROR: atasco detectado (sin pulsos de encoder)");
    return;
  }

  // 4) Boton D0 de nuevo: nunca se invierte el sentido en caliente (ver
  //    manejarPulsacionEnMovimiento -- ademas resuelve la doble pulsacion = detener y quedarse).
  if (botonD0Presionado()) {
    manejarPulsacionEnMovimiento(sentido == ABRIENDO ? CERRANDO : ABRIENDO);
  }
}

// Este proyecto no usa WiFi ni Bluetooth. Apagar los dos radios de entrada evita que consuman
// energia/generen calor en vano y reduce la superficie de interferencia -- sin la complejidad de
// un modo de bajo consumo (deep sleep), que se descarto para este equipo por estar alimentado de
// red, no a bateria (ver bitacora/design.md).
static void apagarRadios() {
  WiFi.mode(WIFI_OFF);
  btStop();
}

void inicializarPines() {
  apagarRadios();

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

  // Temporizadores del disparo del TRIAC por angulo de fase (ver isrZcross()/actualizarRampa()).
  // Dispatch por defecto (ESP_TIMER_TASK): esta build no tiene habilitado ESP_TIMER_ISR
  // (dispatch en contexto de interrupcion) -- el callback corre en la tarea de esp_timer, de
  // prioridad alta pero no una ISR real. Puede haber algo mas de jitter que el ideal; a
  // confirmar con el osciloscopio si el disparo queda lo bastante preciso.
  const esp_timer_create_args_t argsDisparo = {
    .callback = &onDisparoTriac,
    .arg = nullptr,
    .name = "disparo_triac",
  };
  esp_timer_create(&argsDisparo, &timerDisparo);

  const esp_timer_create_args_t argsPulso = {
    .callback = &onFinPulsoTriac,
    .arg = nullptr,
    .name = "fin_pulso_triac",
  };
  esp_timer_create(&argsPulso, &timerPulso);

  // ZCROSS: real desde esta sesion (ver isrZcross), no solo diagnostico. Un pulso en alto por
  // cada cruce por cero -> RISING.
  attachInterrupt(digitalPinToInterrupt(ZCROSS), isrZcross, RISING);
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
  actualizarParpadeoLed();

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
