#ifndef PORTON_H
#define PORTON_H

#include <Arduino.h>

/* Salida LED / BUZZER. Activo en LOW. */
#define LED         2
/* Cruce por cero de la AC. No se usa todavia: el TRIAC se maneja como interruptor on/off. */
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

/* Disparo del TRIAC. Activo en LOW. */
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
   de un flanco de subida muy cercano en el mismo cruce (confirmado: ~400/s medidos por firmware
   contra ~120/s reales medidos en el osciloscopio). Se ignoran flancos que lleguen antes de este
   tiempo desde el ultimo aceptado. 1000us da margen de sobra frente a los ~8.3ms entre cruces
   reales a 60Hz. */
#define ZCROSS_DEBOUNCE_US  1000

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
