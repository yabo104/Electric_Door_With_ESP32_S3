# memory.md — Gotchas técnicos no evidentes en el código

> **Hogar único** de las trampas y convenciones no evidentes al leer el código (las que un agente
> debe respetar al escribir). Lectura obligatoria al inicio de sesión, tras `AGENTS.md`.
> NO lleva: contexto de negocio / decisiones de producto (→ `requirements.md`), principios (→
> `design.md`), proceso (→ `AGENTS.md`). Para eso, apunta.
>
> Se edita incrementalmente pero se **cura**: una entrada resuelta u obsoleta se **borra** (su
> rastro queda en el historial). Si una sección de subsistema supera ~150-200 líneas, extraerla a
> un tema de `requirements.md` con un resumen de 1-3 líneas + link aquí.

---

## Stack y versiones

- **PlatformIO**, entorno `esp32-s3-devkitc-1`, framework `arduino`, USB CDC nativo habilitado
  desde el arranque (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1` en `platformio.ini`).
  Verificado en esta sesión: `pio run` compila (Flash 7.9%, RAM 5.9%).
- **Board real:** el módulo montado en la placa es `ESP32-S3-WROOM-1U-N8R8` (ver BOM). El entorno
  `esp32-s3-devkitc-1` es una placa de referencia genérica usada como aproximación — **no se ha
  confirmado** contra hardware real (`pio run -t upload`) ni ajustado la config de flash/PSRAM al
  módulo exacto (N8R8 = 8MB flash / 8MB PSRAM octal). Si aparecen problemas de tamaño al cargar,
  revisar `board_build.*` en `Firmware_Porton/platformio.ini`.

## Firmware (`Firmware_Porton/`)

**Contrato de pines confirmado por el usuario** (sesión 003): tabla completa y sistema de potencia
en [`temas/hardware-esp32-s3.md`](temas/hardware-esp32-s3.md), resumen en `requirements.md` §2.
Diseño de la máquina de estados (sesión 004) en `requirements.md` §3/§4 y `architecture.md`.

- **Los relevos son activos en LOW** y **solo se escriben desde `seleccionarSentido()`** (en
  `src/porton.cpp`) — nunca con `digitalWrite` directo en otro lado. Esa función es el interlock:
  escribe los dos pines juntos, así que nunca quedan ambos en `LOW`. Si se agrega código nuevo que
  necesite mover el motor, pasar por ahí (o por `detenerMotor()`), no escribir los relevos aparte.
- **`TRIGGER` (disparo del TRIAC) es activo en LOW.** Por ahora se maneja como interruptor
  todo/nada (sin sincronismo a `ZCROSS`): `LOW` = máxima potencia, `HIGH` = apagado. Se habilita
  recién `RELAY_SETTLE_MS` después de elegir el relevo, nunca junto con él.
- **`LED` es activo en LOW** (nivel 0 = encendido).
- **`FC_OPEN`/`FC_CLOSE` usan `INPUT_PULLUP`.** Final de carrera activado se lee como `LOW`; la
  lógica comprueba con `!digitalRead(FC_OPEN)`. Se leen por *polling* (no por interrupción) — a
  propósito: al eliminarse los bloqueos del `loop()`, el polling es lo bastante rápido y evita la
  complejidad de debounce en ISR para un reed switch mecánico.
- **`D0`/`D1` son los únicos canales de control remoto cableados** (D2/D3 quedan configurados
  pero sin leerse en la lógica). Se leen con `digitalRead` simple, **por flanco** (no por nivel)
  vía `botonPresionado()`: un botón mantenido presionado genera un solo comando, no uno por
  iteración. Si se necesita usar D2/D3 a futuro, hay que decidir primero si conviene agregarles
  pull-down (hoy son `INPUT` simple, sin pull, y como no se leen no importa que floten).
- **`ENCA` se lee por interrupción (`attachInterrupt`, `CHANGE`)**, solo para saber "hubo un
  pulso" (detección de atasco vía timeout, `requirements.md` §3) — no decodifica dirección ni
  cuenta posición. `ENCB` queda sin usar. La ISR (`isrEncoder()`) llama `millis()` — es el patrón
  estándar en el core Arduino-ESP32 (a diferencia de AVR clásico), pero no se ha verificado contra
  hardware real en este entorno; si a futuro hay pulsos perdidos o timestamps erráticos, revisar
  primero esto.
- **`ZCROSS` sigue sin usarse.** Decisión explícita del usuario (sesión 004): queda para una
  sesión futura, cuando se implemente la rampa de arranque suave del TRIAC.
- **UART de depuración: confirmado en `GPIO43` (TXD0) / `GPIO44` (RXD0)**, hacia un conector
  auxiliar en la placa (confirmado por el usuario, sesión 005) — **no** hay conflicto con
  `TRIGGER` (`GPIO21`). El comentario del `.ino` original (`// HW UART TXD pin IO21`) estaba
  desactualizado/equivocado; no se arrastró a `porton.h`. Con el USB CDC habilitado
  (`ARDUINO_USB_CDC_ON_BOOT`), `Serial` usa el USB nativo, así que esta UART física queda libre
  para depuración aparte si hace falta (no está en uso desde el firmware todavía).
- **`comando` se maneja por caracter ASCII** (`'0'`-`'7'`) vía `Serial`; es la interfaz de pruebas
  manuales, no un protocolo binario. Los comandos `1`/`2`/`3` reusan `seleccionarSentido()`/
  `detenerMotor()`, así que el interlock de relevos aplica también en modo manual.

## Seguridad

- El firmware maneja directamente relevos y disparo de TRIAC sobre carga de línea (motor 110VAC).
  Reglas duras pedidas por el usuario y ya implementadas en `porton.cpp` — cualquier cambio futuro
  debe preservarlas:
  1. Nunca ambos relevos activos a la vez (garantizado por construcción en `seleccionarSentido()`).
  2. El motor se detiene siempre antes de invertir sentido — nunca se conmuta el relevo con el
     motor energizado (`detenerMotor()` corta `TRIGGER` antes de liberar los relevos;
     `REVERSE_STOP_MS` de pausa antes de arrancar en el sentido contrario).
  3. Arranque siempre en estado seguro (`inicializarPines()`: relevos y `TRIGGER` en `HIGH`).
- El estado `ERROR` (atasco detectado) corta la potencia al entrar pero **no tiene recuperación
  definida todavía** — no asumir que hay un reintento automático ni una forma de salir de `ERROR`
  sin que el usuario lo especifique.
