#ifndef PORTON_H
#define PORTON_H

#include <Arduino.h>

/* Salida LED / BUZZER */
#define LED         2
/* Pin interrupcion cruce por cero */
#define ZCROSS      3

/* Pines salida control remoto */
#define D3          9
#define D2          10
#define D1          11
#define D0          12

/* Pines encoder */
#define ENCA        13
#define ENCB        14

/* Pines finales de carrera */
#define FC_OPEN     15
#define FC_CLOSE    16

/* TRIAC trigger */
#define TRIGGER     21

/* Control relevos */
#define RELAY_OP    38
#define RELAY_CL    39

/* HW UART RXD pin IO20 */
/* HW UART TXD pin IO21 */

enum EstadoPuerta {
  STOP    = 1,
  OPENING = 2,
  CLOSING = 3,
  ERROR   = 4,
  WAITING = 5,
};

extern char comando;
extern char doorStd;
extern char doorLastStd;

void inicializarPines();
void procesarComandoSerial();
void actualizarEstadoPuerta();

#endif
