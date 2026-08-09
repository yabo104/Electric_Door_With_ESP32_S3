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
- **Board real:** el módulo es `ESP32-S3-WROOM-1U-N8R8`. **Confirmado contra hardware real**
  (sesión 006): `pio run -t upload --upload-port COM8` cargó sin problemas; `esptool` identificó
  el chip como ESP32-S3 (QFN56, rev v0.2) con **8MB PSRAM embebida**, consistente con el módulo
  real. El entorno `esp32-s3-devkitc-1` (placa de referencia genérica) funciona tal cual, sin
  necesidad de ajustar `board_build.*` — no se detectaron problemas de tamaño de flash/PSRAM.
- **USB nativo confirmado:** el puerto enumera con VID:PID `303A:1001` (Espressif) — el USB CDC
  configurado en `platformio.ini` funciona; no es un puente serie genérico.

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
- **`D0` mueve, `D2` resetea `ERROR`** (`D1`/`D3` sin rol, ver `requirements.md` §4). Cada botón
  tiene su propio detector de flanco/antirrebote (`botonD0Presionado()`/`botonResetPresionado()`
  en `porton.cpp`) — no comparten estado, a propósito: son semánticamente independientes (uno
  opera, el otro solo reconoce una falla) y podrían presionarse casi al mismo tiempo. `D2` se
  consume siempre (todas las iteraciones), aunque solo actúa estando en `ERROR` — si no se
  consumiera igual, se perdería el flanco si el usuario lo presiona mientras el portón está en
  movimiento.
- **`ENCA`/`ENCA2` se leen por interrupción (`attachInterrupt`, `CHANGE`)**, ambos alimentan el
  mismo timestamp de detección de atasco (`ultimoPulsoEncoderISR`, `requirements.md` §3) — no
  decodifican dirección ni cuentan posición, solo "hubo un pulso". Las ISR llaman `millis()` —
  patrón estándar en el core Arduino-ESP32 (a diferencia de AVR clásico).
- **Hardware: `ENCA`/`ENCB` tienen un problema real de resistencias de pull-up en el circuito de
  entrada** (encontrado por el usuario en la sesión 006, en banco). No es un problema de
  firmware; hoy no aportan pulsos. **`ENCA2` (sensor Hall en `GPIO44`) ya es la fuente real y
  confirmada** de detección de atasco (sesión 006: 0 pulsos con el motor quieto, pulsos reales
  con el motor girando, y la detección de atasco ya se probó en hardware). Se eligió `GPIO44` y
  no `GPIO43` a propósito: el ESP32-S3 solo *transmite* por `GPIO43` (boot log, volcados de
  excepción/crash), nunca por `GPIO44` — como entrada, `GPIO44` queda más "silencioso" desde el
  arranque. Ninguno de los dos interfiere con `Serial` (usa el USB CDC nativo, no esta UART).
  `INPUT_PULLUP` asumido como para un sensor Hall típico de salida open-collector — a confirmar
  contra el datasheet del sensor puntual. Si se arregla el hardware de `ENCA`, vuelve a sumar
  sin más cambios (las dos ISR alimentan el mismo timestamp).
- **`ZCROSS`: contrato de hardware confirmado por osciloscopio (PicoScope 2208, sesión 006),
  señal limpia a ~120,8 Hz** (un pulso ascendente por semiciclo — coherente con 60Hz de red). El
  método de conteo por firmware (interrupción `RISING` simple) medía ~400-440 pulsos/s, ~3.4x más
  — **no es la señal, es una interrupción disparándose varias veces muy cerca en el mismo cruce**
  (ruido/rebote de umbral, invisible a 5ms/div en el osciloscopio). Se agregó antirrebote por
  software (`ZCROSS_DEBOUNCE_US`, en `porton.h`) en la ISR de diagnóstico; con 1000us el conteo
  bajó a ~231-237/s (~2x lo real) — **sigue sin quedar en el número correcto**, probablemente hay
  un segundo flanco válido más allá de esa ventana. Pendiente: probar subiendo
  `ZCROSS_DEBOUNCE_US` (con margen de sobra respecto a los ~8300us reales entre pulsos) antes de
  dar el conteo por bueno. `ZCROSS` sigue sin usarse para disparar el TRIAC (eso sigue pospuesto,
  decisión de la sesión 004) — este trabajo es solo para tener el conteo de diagnóstico correcto.
- **UART de depuración: confirmado en `GPIO43` (TXD0) / `GPIO44` (RXD0)**, hacia un conector
  auxiliar en la placa (confirmado por el usuario, sesión 005) — **no** hay conflicto con
  `TRIGGER` (`GPIO21`). El comentario del `.ino` original (`// HW UART TXD pin IO21`) estaba
  desactualizado/equivocado; no se arrastró a `porton.h`. Con el USB CDC habilitado
  (`ARDUINO_USB_CDC_ON_BOOT`), `Serial` usa el USB nativo, así que esta UART física queda libre
  para depuración aparte si hace falta (no está en uso desde el firmware todavía).
- **`comando` se maneja por caracter ASCII** (`'0'`-`'7'`) vía `Serial`; es la interfaz de pruebas
  manuales, no un protocolo binario. Los comandos `1`/`2`/`3` reusan `seleccionarSentido()`/
  `detenerMotor()`, así que el interlock de relevos aplica también en modo manual.
- **Todo case de `procesarComandoSerial()` debe terminar con `comando = 0;`.** Si no, el comando
  queda "pegado" y se reejecuta en cada iteración del `loop()` hasta que llegue otro. Bug real
  encontrado en hardware (sesión 006): los cases `'0'` (LED) y `'4'` (leer D0-D3) no lo hacían —
  heredado del `.ino` original — y quedaban repitiéndose solos. Ya corregido en los dos; si se
  agrega un case nuevo, no olvidar el `comando = 0;` al final.

## Seguridad

- El firmware maneja directamente relevos y disparo de TRIAC sobre carga de línea (motor 110VAC).
  Reglas duras pedidas por el usuario y ya implementadas en `porton.cpp` — cualquier cambio futuro
  debe preservarlas:
  1. Nunca ambos relevos activos a la vez (garantizado por construcción en `seleccionarSentido()`).
  2. El motor se detiene siempre antes de invertir sentido — nunca se conmuta el relevo con el
     motor energizado (`detenerMotor()` corta `TRIGGER` antes de liberar los relevos;
     `REVERSE_STOP_MS` de pausa antes de arrancar en el sentido contrario).
  3. Arranque siempre en estado seguro (`inicializarPines()`: relevos y `TRIGGER` en `HIGH`).
- El estado `ERROR` (atasco detectado) corta la potencia al entrar y **se sale solo con `D2`**
  (confirmado en hardware, sesión 006) — nunca automáticamente. Decisión deliberada: un atasco
  puede ser una obstrucción real, y reintentar por temporizador movería el motor de nuevo sin
  verificación humana (mismo criterio que UL 325 para fallas de obstrucción en portones
  automáticos). No agregar un reintento automático sin que el usuario lo pida explícitamente.
- **Cuidado al medir con osciloscopio de PC no aislado (p. ej. PicoScope 2208) sobre `ZCROSS`**,
  que está referenciado al lado de potencia (AC de red) a través del TRIAC. La pinza de tierra de
  la sonda puede crear un lazo de tierra entre la red eléctrica y el USB de la PC si toca un punto
  del circuito de potencia. Incidente en la sesión 006: tras medir así, la placa dejó de emitir
  cualquier salida por Serial (aunque seguía respondiendo a nivel de bootloader/USB); se
  recuperó sola al desconectar el osciloscopio, sin daño permanente aparente. Para medir `ZCROSS`
  con seguridad: sonda diferencial/aislada, o un transformador de aislación para el osciloscopio.
