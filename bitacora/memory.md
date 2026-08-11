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
- **Enlazar la librería `WiFi.h` infla el binario notablemente aunque solo se use para apagar el
  radio** (`WiFi.mode(WIFI_OFF)` en `apagarRadios()`, sesión 008): Flash pasó de ~8% a ~20%, RAM
  de ~6% a ~13% en esta build. Sigue sobrando margen, pero si en algún momento el espacio se pone
  ajustado, este es el primer lugar a revisar.

## Firmware (`Firmware_Porton/`)

**Contrato de pines confirmado por el usuario** (sesión 003): tabla completa y sistema de potencia
en [`temas/hardware-esp32-s3.md`](temas/hardware-esp32-s3.md), resumen en `requirements.md` §2.
Diseño de la máquina de estados (sesión 004) en `requirements.md` §3/§4 y `architecture.md`.

- **Los relevos son activos en LOW** y **solo se escriben desde `seleccionarSentido()`** (en
  `src/porton.cpp`) — nunca con `digitalWrite` directo en otro lado. Esa función es el interlock:
  escribe los dos pines juntos, así que nunca quedan ambos en `LOW`. Si se agrega código nuevo que
  necesite mover el motor, pasar por ahí (o por `detenerMotor()`), no escribir los relevos aparte.
- **`TRIGGER` (disparo del TRIAC, `BT138-800`) es activo en LOW.** Desde la sesión 007 ya **no**
  se mantiene fijo en `LOW`: se pulsa por `PULSO_TRIAC_US` (200us) en cada semiciclo, en el
  momento que decide el retardo de fase (`delayDisparoActualUS`) — ver "Disparo del TRIAC" más
  abajo. Se habilita recién `RELAY_SETTLE_MS` después de elegir el relevo, nunca junto con él.
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
- **`ZCROSS`: resuelto el sobre-conteo (sesión 007).** Señal real limpia a ~120,8 Hz (confirmada
  por osciloscopio, sesión 006). El firmware contaba ~231-237/s con `ZCROSS_DEBOUNCE_US = 1000` —
  **subir el antirrebote a 3000us lo dejó en 120/s exactos.** El motivo de subirlo no fue el
  conteo en sí: con el disparo de fase real (sesión 007), un flanco espurio cercano al real podía
  armar el `esp_timer` del TRIAC dos veces para el mismo cruce, y 1000us no alcanzaba para
  filtrarlo.
- **Disparo del TRIAC por ángulo de fase, sincronizado a `ZCROSS` (sesión 007) — CONFIRMADO
  funcionando en el portón real (sesión 008), en potencia fija.** Mecanismo: `isrZcross()` arma
  `timerDisparo` (`esp_timer`) con el retardo de fase actual; su callback pulsa `TRIGGER` y arma
  `timerPulso` para soltarlo tras `PULSO_TRIAC_US`. Llamar `esp_timer_start_once()`/
  `esp_timer_stop()` desde una ISR de GPIO real (`isrZcross()`) **sí funciona** en esta build —
  quedaba como "sin verificar" en la sesión 007, ya validado con el motor real moviéndose.
  - **`ESP_TIMER_ISR` (dispatch en contexto de interrupción real) sigue sin estar disponible en
    esta build** de Arduino-ESP32 (falla la compilación al pedirlo) — los dos `esp_timer` corren
    en dispatch `ESP_TIMER_TASK` (por defecto). El jitter que eso pueda meter **no impidió** que
    el mecanismo funcionara bien en la práctica.
  - **Hallazgo clave (sesión 008): disparar muy cerca del cruce por cero (~100% de potencia, ~6,5°)
    no engancha el TRIAC de forma confiable con esta carga inductiva** — el motor directamente no
    se movía a ese nivel. A partir de ~90% (~46°) sí enganchaba. Es la explicación más probable de
    por qué la rampa de arranque original (que pasaba por la zona cercana al 100% al llegar a
    velocidad de crucero) generaba paradas falsas por "atasco": enganche inconsistente ciclo a
    ciclo = pulsos de torque irregulares, no potencia pareja. **La hipótesis de la sesión 007**
    (el motor de inducción no muestra el corte de fase en velocidad, solo en torque) **queda sin
    resolver y ya no es la explicación principal** — este hallazgo de enganche es más concreto y
    concuerda con los datos.
  - **`PULSO_TRIAC_US` subido de 200 a 500us (sesión 008)** — pulso de gate más largo, para ayudar
    al enganche cerca del cruce por cero. El retardo de disparo y el ancho del pulso son
    independientes (el pulso puede durar más que el retardo).
  - **Release (sesión 008): rampa de arranque DESACTIVADA, potencia fija al 85%**
    (`POTENCIA_MOTOR_PCT` en `porton.h`) — primer nivel que movió el portón de punta a punta sin
    falsos atascos. La rampa (`RAMPA_ARRANQUE_MS`/`actualizarRampa()`) sigue en el código, sin
    usar. Detalle completo en `requirements.md` §5.
- **UART de depuración: confirmado en `GPIO43` (TXD0) / `GPIO44` (RXD0)**, hacia un conector
  auxiliar en la placa (confirmado por el usuario, sesión 005) — **no** hay conflicto con
  `TRIGGER` (`GPIO21`). El comentario del `.ino` original (`// HW UART TXD pin IO21`) estaba
  desactualizado/equivocado; no se arrastró a `porton.h`. Con el USB CDC habilitado
  (`ARDUINO_USB_CDC_ON_BOOT`), `Serial` usa el USB nativo, así que esta UART física queda libre
  para depuración aparte si hace falta (no está en uso desde el firmware todavía).
- **`comando` se maneja por caracter ASCII** (`'0'`-`'7'`) vía `Serial`; es la interfaz de pruebas
  manuales, no un protocolo binario. Los comandos `1`/`2`/`3` reusan `seleccionarSentido()`/
  `detenerMotor()`, así que el interlock de relevos aplica también en modo manual.
- **Contador de trayecto (`contadorTrayecto`/`trayectoActivo`/`sentidoTrayecto`) tiene su propio
  `portMUX_TYPE` (`muxTrayecto`), separado de `muxPulsos`** — a propósito: `muxPulsos` solo existe
  si `DEBUG_PULSOS` está activo, y el contador de trayecto es una función real, no diagnóstica.
  **Todas** las escrituras a `trayectoActivo` deben pasar por `muxTrayecto` (se encontraron y
  corrigieron dos que no lo hacían, en la sesión 008) — aunque sea un `bool` de una sola escritura
  "atómica" en la práctica, romper el patrón ahí invita a que el próximo cambio sí introduzca una
  condición de carrera real.
- **`ultimaLlegadaConfirmada`** (sesión 008) guarda el último `ABRIENDO`/`CERRANDO` que llegó de
  verdad a su fin de carrera — no confundir con `doorLastStd` (que se actualiza en **cada**
  arranque de movimiento, llegue o no a destino, y hoy no se lee en ningún lado). Es la que decide
  el sentido cuando no hay ningún fin de carrera activo (`requirements.md` §4).
- **El parpadeo del LED necesita una pausa apagada antes de encenderse** (`LED_PAUSA_PARPADEO_MS`)
  — si se enciende de una tras `detenerMotor()` (que ya apaga el LED), la transición apagado→
  encendido pasa en microsegundos y el ojo humano no la distingue del "encendido fijo" que tenía
  el LED un instante antes por el movimiento. Bug real de la primera versión (sesión 008): el
  parpadeo estaba implementado pero nadie lo veía.
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
