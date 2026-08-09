#ifndef PORTON_H
#define PORTON_H

#include <Arduino.h>

/* Salida LED / BUZZER. Activo en LOW. */
#define LED         2
/* Cruce por cero de la AC. Sincroniza el disparo del TRIAC por angulo de fase (ver
   RAMPA_ARRANQUE_MS mas abajo). */
#define ZCROSS      3

/* Entradas del receptor RF 433MHz.
   D0 = abrir/cerrar/invertir/detener (control normal). D2 = salir de ERROR unicamente.
   D1/D3 sin uso por ahora. */
#define D3          9
#define D2          10
#define D1          11
#define D0          12

/* Encoder del motor. Por ahora solo se usa ENCA, como pulso de "el motor se esta moviendo". */
#define ENCA        13
#define ENCB        14

/* Finales de carrera (reed switch). Normalmente en alto; activos en LOW. */
#define FC_OPEN     15
#define FC_CLOSE    16

/* Disparo del TRIAC (BT138-800). Activo en LOW. Pulso de gate corto (PULSO_TRIAC_US), disparado
   por angulo de fase desde ZCROSS -- no se mantiene fijo en LOW como antes. */
#define TRIGGER     21

/* Relevos de sentido. Activos en LOW. NUNCA ambos activos a la vez (ver seleccionarSentido()
   en porton.cpp: siempre escribe los dos pines juntos, nunca uno solo). */
#define RELAY_OP    38
#define RELAY_CL    39

/* Tiempos de la maquina de estados, en milisegundos. */
#define RELAY_SETTLE_MS     500   // entre elegir sentido (relevo) y habilitar potencia (TRIAC)
#define REVERSE_STOP_MS     500   // detenido obligatorio antes de invertir sentido
#define ENCODER_TIMEOUT_MS  2500  // TEMPORAL (subido para diagnosticar el encoder con
                                    // osciloscopio) -- volver a un valor real (unos segundos)
                                    // antes de dar por buena la deteccion de atasco.
#define BOTON_DEBOUNCE_MS   80    // antirrebote de D0/D2
#define DOBLE_PULSACION_MS  2000  // ventana para detectar doble pulsacion de D0 en movimiento
                                   // (detiene y deja el porton a medio camino, sin invertir)

/* Antirrebote del pulso de ZCROSS, en microsegundos. El comparador analogico puede generar mas
   de un flanco de subida muy cercano en el mismo cruce (confirmado con osciloscopio: la senal
   real es limpia, ~120,8Hz, pero el firmware seguia contando ~2x con 1000us de antirrebote). Se
   ignoran flancos que lleguen antes de este tiempo desde el ultimo aceptado. 3000us sigue con
   margen de sobra frente a los ~8333us entre cruces reales a 60Hz, y evita que un flanco espurio
   arme el disparo del TRIAC dos veces para el mismo cruce real. */
#define ZCROSS_DEBOUNCE_US  3000

/* --- Disparo del TRIAC por angulo de fase (arranque suave) ---
   SEMICICLO_US: duracion aproximada de un semiciclo de red a 60Hz (confirmado: ZCROSS mide
   ~120,8Hz = 2x60Hz). Sirve de referencia para los otros valores, no se usa como limite duro en
   el codigo.
   PULSO_TRIAC_US: ancho del pulso de gate del BT138-800 (confirmado por el usuario, ya probado
   en proyectos anteriores).
   DISPARO_US_MAX: retardo de disparo en potencia de crucero (maxima potencia util) -- chico,
   apenas por encima de PULSO_TRIAC_US.
   DISPARO_US_MIN: retardo de disparo al INICIO de la rampa de arranque (potencia baja). ~50% del
   semiciclo es un punto de partida conservador -- ya da bastante menos potencia que el crucero
   sin acercarse a que el motor no tenga torque para arrancar. Requiere ajuste empirico con el
   motor real: si no arranca, bajar (mas potencia); si el arranque sigue brusco, subir.
   RAMPA_ARRANQUE_MS: duracion de la rampa (interpola DISPARO_US_MIN -> DISPARO_US_MAX). Despues
   de este tiempo el disparo queda fijo en DISPARO_US_MAX, igual que el diseno anterior (todo/
   nada), hasta que el motor se detiene. La PARADA sigue siendo instantanea -- no hay rampa de
   bajada todavia (queda para una sesion futura, con calibracion de recorrido por ENCA2). */
#define SEMICICLO_US        8333
#define PULSO_TRIAC_US      200
#define DISPARO_US_MAX      300
#define DISPARO_US_MIN      4200
#define RAMPA_ARRANQUE_MS   1200

/* --- Diagnostico temporal de pulsos (ENCA/ENCB/ZCROSS/ENCA2) ---
   Para quitarlo: poner DEBUG_PULSOS en 0 (no hace falta borrar codigo). */
#define DEBUG_PULSOS        1
#define DEBUG_PULSOS_MS     1000  // cada cuanto se imprime el conteo, en ms

/* ENCA2: sensor de efecto Hall, alternativa a ENCA/ENCB (esas tienen un problema de pull-up en
   el circuito de entrada). Usa el conector auxiliar de UART0 -- GPIO44 (U0RXD) en vez de GPIO43
   (U0TXD): el ESP32-S3 solo transmite boot log / paniques por GPIO43, nunca por GPIO44, asi que
   GPIO44 queda "limpio" como entrada desde el arranque. Ninguno de los dos se usa como UART real
   (Serial usa el USB CDC nativo). Confirmado en banco (sesion 006): 0 pulsos con el motor
   quieto, pulsos reales con el motor girando -- alimenta la deteccion de atasco en paralelo con
   ENCA (ver isrEncA2 en porton.cpp), no es solo diagnostico. */
#define ENCA2       44

#if DEBUG_PULSOS
void mostrarPulsos();
#endif

enum EstadoPuerta {
  DETENIDA = 1,  // motor detenido, a la espera de un comando
  ABRIENDO = 2,
  CERRANDO = 3,
  ERROR    = 4,  // motor detenido por falla (hoy: atasco). Se sale solo con D2 (ver porton.cpp).
};

extern char comando;
extern char doorStd;
extern char doorLastStd;

void inicializarPines();
void procesarComandoSerial();
void actualizarEstadoPuerta();

#endif
