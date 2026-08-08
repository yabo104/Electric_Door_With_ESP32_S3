#ifndef PORTON_H
#define PORTON_H

#include <Arduino.h>

/* Salida LED / BUZZER. Activo en LOW. */
#define LED         2
/* Cruce por cero de la AC. No se usa todavia: el TRIAC se maneja como interruptor on/off. */
#define ZCROSS      3

/* Entradas del receptor RF 433MHz. Solo D0 y D1 estan cableados en esta tarjeta. */
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
#define RELAY_SETTLE_MS     100   // entre elegir sentido (relevo) y habilitar potencia (TRIAC)
#define REVERSE_STOP_MS     500   // detenido obligatorio antes de invertir sentido
#define ENCODER_TIMEOUT_MS  3000  // sin pulsos de encoder en movimiento => atasco (pasa a ERROR)
#define BOTON_DEBOUNCE_MS   50    // antirrebote de D0/D1

enum EstadoPuerta {
  DETENIDA = 1,  // motor detenido, a la espera de un comando
  ABRIENDO = 2,
  CERRANDO = 3,
  ERROR    = 4,  // motor detenido por falla (hoy: atasco). Recuperacion: pendiente de definir.
};

extern char comando;
extern char doorStd;
extern char doorLastStd;

void inicializarPines();
void procesarComandoSerial();
void actualizarEstadoPuerta();

#endif
