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
#define ENCODER_TIMEOUT_MS  2000  // TEMPORAL (subido para diagnosticar el encoder con
                                    // osciloscopio) -- volver a un valor real (unos segundos)
                                    // antes de dar por buena la deteccion de atasco.
#define BOTON_DEBOUNCE_MS   80    // antirrebote de D0/D2
#define DOBLE_PULSACION_MS  2000  // ventana para detectar doble pulsacion de D0 en movimiento
                                   // (detiene y deja el porton a medio camino, sin invertir)
#define LED_PAUSA_PARPADEO_MS 100 // apagado visible antes del parpadeo (para que se note el
                                   // flanco: el LED ya viene encendido fijo por el movimiento)
#define LED_PARPADEO_MS     150   // duracion del parpadeo del LED al detectar un fin de carrera

/* Antirrebote del pulso de ZCROSS, en microsegundos. El comparador analogico puede generar mas
   de un flanco de subida muy cercano en el mismo cruce (confirmado con osciloscopio: la senal
   real es limpia, ~120,8Hz, pero el firmware seguia contando ~2x con 1000us de antirrebote). Se
   ignoran flancos que lleguen antes de este tiempo desde el ultimo aceptado. 3000us sigue con
   margen de sobra frente a los ~8333us entre cruces reales a 60Hz, y evita que un flanco espurio
   arme el disparo del TRIAC dos veces para el mismo cruce real. */
#define ZCROSS_DEBOUNCE_US  3000

/* --- Disparo del TRIAC por angulo de fase ---
   SEMICICLO_US: duracion aproximada de un semiciclo de red a 60Hz (confirmado: ZCROSS mide
   ~120,8Hz = 2x60Hz). Referencia para los otros valores, no es un limite duro en el codigo.
   PULSO_TRIAC_US: ancho del pulso de gate del BT138-800. 500us (subido desde 200us en la sesion
   008: un pulso mas largo ayuda a enganchar con esta carga inductiva, sobre todo cerca del cruce
   por cero). El retardo (DISPARO_US_MAX/MIN) y el ancho del pulso son independientes: el pulso
   puede durar mas que el retardo sin problema, el TRIAC solo necesita el gate en LOW el tiempo
   suficiente para enganchar.
   DISPARO_US_MAX: retardo de disparo en la potencia mas alta que se usa (chico = cerca del cruce
   por cero = mas conduccion).

   ESTADO ACTUAL (sesion 008): **release en potencia fija, sin rampa de arranque suave.** La
   version original de esta seccion armaba una rampa (DISPARO_US_MIN bajo -> DISPARO_US_MAX)
   durante RAMPA_ARRANQUE_MS. En las pruebas de banco con el porton real, disparar demasiado cerca
   del cruce por cero (potencias ~100%) no siempre enganchaba el TRIAC con esta carga inductiva, y
   la rampa pasaba por esa zona marginal -- eso generaba paradas falsas por "atasco" durante el
   arranque. Se decidio, con el usuario, dejar el release con **potencia fija** en el nivel que sí
   demostro enganchar de forma confiable (ver POTENCIA_MOTOR_PCT abajo) y posponer la rampa de
   arranque suave a una sesion futura, ya con este piso de potencia fiable como punto de partida.
   RAMPA_ARRANQUE_MS y la logica de actualizarRampa() en porton.cpp se dejan en el codigo (no se
   usan mientras DISPARO_US_MIN == DISPARO_US_MAX) para no tener que rehacerlas desde cero. */
#define SEMICICLO_US        8333
#define PULSO_TRIAC_US      500
#define DISPARO_US_MAX      300
#define RAMPA_ARRANQUE_MS   1200

/* --- Nivel de potencia del release (fijo, sin rampa) ---
   POTENCIA_MOTOR_PCT: 0-100. Cambiar este numero y volver a compilar/cargar para ajustar la
   potencia fija que usa el porton (por ejemplo, si cambian las condiciones mecanicas del
   recorrido). Se traduce a un retardo de disparo por interpolacion LINEAL en tiempo entre
   DISPARO_US_MAX (100%) y SEMICICLO_US (0%, disparo tan tarde que casi no hay conduccion) -- OJO:
   no es un porcentaje exacto de potencia RMS (esa relacion es no lineal: el disparo cerca del
   pico de la onda aporta bastante mas energia que cerca del cruce por cero), pero es una perilla
   monotona: mas alto = mas potencia real.
   Historial de pruebas en banco que llevaron al valor actual (motor real, sin rampa):
     100% (300us,  ~6,5 grados desde el cruce) -> el motor NO se movio (el TRIAC no llegaba a
       enganchar tan cerca del cruce, con esta carga inductiva).
     90%  (2150us, ~46 grados) -> el motor SI se movio.
     95%  (1670us, ~36 grados) -> punto intermedio, tambien probado.
     85%  (con PULSO_TRIAC_US ya en 500us) -> nivel elegido para el release: mueve el porton de
       forma confiable en todo el recorrido, sin las paradas falsas por atasco que daban los
       niveles mas cercanos al 100%. */
#define POTENCIA_MOTOR_PCT  85
#define DISPARO_US_FIJO \
  (DISPARO_US_MAX + ((uint32_t)(SEMICICLO_US - DISPARO_US_MAX) * (100 - POTENCIA_MOTOR_PCT)) / 100)
#define DISPARO_US_MIN      DISPARO_US_FIJO

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
