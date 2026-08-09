# architecture.md — Firmware_Porton

> Mapa de módulos y **patrones reutilizables** de este producto. Objetivo: que ningún agente
> tenga que releer el fuente para redescubrir un patrón ya entendido. Se **lee** al inicio de
> cualquier sesión que toque esta área y se **actualiza** al cerrar si se descubre un patrón
> nuevo.
>
> Frontera: aquí va *cómo está organizado el producto* (estructura + patrones). Las *trampas
> puntuales* van en `memory.md`; las *decisiones de producto* en `requirements.md`.
>
> Este proyecto tiene dos productos con estructura propia: el firmware (este archivo) y la placa
> controladora en KiCad (`PCB_puerta/` — en proceso de rename, ver `latest.md`).

## Stack

- C++ sobre framework **Arduino**, gestionado con **PlatformIO** (`Firmware_Porton/platformio.ini`).
- Build: `pio run` (desde `Firmware_Porton/`). Carga: `pio run -t upload`. Monitor:
  `pio device monitor -b 115200` (por USB CDC nativo del ESP32-S3, ver `platformio.ini`).

## Estructura de módulos

- `Firmware_Porton/platformio.ini` — entorno `esp32-s3-devkitc-1`, framework `arduino`, USB CDC
  habilitado desde el arranque (`ARDUINO_USB_MODE`/`ARDUINO_USB_CDC_ON_BOOT`).
- `Firmware_Porton/include/porton.h` — pines, tiempos de la máquina de estados (`#define`, todos
  en milisegundos) y prototipos públicos de `porton.cpp`.
- `Firmware_Porton/src/porton.cpp` — toda la lógica: inicialización, interlock de salidas de
  potencia, máquina de estados del portón, y la interfaz de pruebas por `Serial`. El detalle de
  funciones internas (`static`, no expuestas en el header) está comentado en el propio archivo.
- `Firmware_Porton/src/main.cpp` — `setup()`/`loop()`, delgado: inicializa, lee `Serial` si hay
  dato, y llama a `procesarComandoSerial()` + `actualizarEstadoPuerta()` en cada iteración.
- `Firmware_Porton/lib/`, `Firmware_Porton/test/` — carpetas estándar de PlatformIO, vacías por
  ahora.

## Patrones reutilizables ⭐

- **Interlock de salidas de potencia centralizado** — `seleccionarSentido(sentido)` en
  `src/porton.cpp` es el **único** punto del código que escribe `RELAY_OP`/`RELAY_CL`; siempre
  escribe los dos pines juntos, así que "ambos relevos activos a la vez" es estructuralmente
  imposible, no una convención a recordar. `detenerMotor()` corta `TRIGGER` primero y después
  llama a `seleccionarSentido(0)`. **Cualquier código nuevo que necesite mover el motor debe pasar
  por estas dos funciones, nunca por `digitalWrite` directo a los relevos/TRIGGER.**
- **Máquina de estados no bloqueante, con `millis()`** — `actualizarEstadoPuerta()`, sobre el enum
  `EstadoPuerta` (`DETENIDA`, `ABRIENDO`, `CERRANDO`, `ERROR`). Sin `while()`/`delay()` largos: los
  tiempos de asentamiento (`RELAY_SETTLE_MS`), de parada antes de invertir sentido
  (`REVERSE_STOP_MS`) y de timeout de atasco (`ENCODER_TIMEOUT_MS`) se miden comparando
  `millis()` contra un timestamp guardado, nunca bloqueando el `loop()`. Ver `requirements.md` §3.
- **Botón por flanco, no por nivel** — `botonD0Presionado()`/`botonResetPresionado()` solo
  disparan en la transición de no-presionado a presionado (con antirrebote), así que mantener el
  botón apretado no genera comandos repetidos. Cada botón tiene su propio estado de flanco/
  antirrebote — no se comparten, porque son semánticamente independientes (`D0` opera, el reset
  en `D2` solo reconoce una falla) y podrían presionarse casi a la vez.
- **Reset de falla como botón dedicado, no como gesto** — la salida de `ERROR` usa un pin físico
  aparte (`D2`) en vez de una pulsación larga del mismo botón de operar. Más simple de
  implementar (sin temporizar "cuánto es largo") y más seguro: una pulsación normal de `D0` nunca
  puede sacar de `ERROR` por accidente. Ver `requirements.md` §3/§4.
- **Pulsación única vs. doble durante el movimiento** — `manejarPulsacionEnMovimiento()`
  centraliza la decisión: una sola pulsación de `D0` programa una reversa segura (detiene,
  espera `REVERSE_STOP_MS`, invierte); una segunda pulsación dentro de `DOBLE_PULSACION_MS`
  cancela esa reversa y deja el portón donde está (apertura/cierre parcial). Es la misma función
  para los dos casos donde puede llegar la segunda pulsación (todavía en movimiento, o ya
  detenido esperando la reversa) — evita duplicar la lógica de "es doble o no".
- **Interfaz de comandos por Serial** — `procesarComandoSerial()`, un switch sobre caracteres
  ASCII. Reusa `seleccionarSentido()`/`detenerMotor()` en vez de escribir los pines de potencia
  por su cuenta, así el interlock también aplica en modo de prueba manual.

## Flujos clave

1. **Arranque:** `main.cpp:setup()` → `inicializarPines()` deja relevos y `TRIGGER` en `HIGH`
   (desactivados), arma las interrupciones de encoder (`ENCA` y `ENCA2`, `CHANGE`) y abre
   `Serial`.
2. **Ciclo principal:** `main.cpp:loop()` → si hay dato en `Serial`, lo guarda en `comando` →
   `procesarComandoSerial()` → `actualizarEstadoPuerta()`.
3. **Apertura/cierre:** en `DETENIDA`, un flanco de `D0` decide sentido según la posición real
   (`FC_OPEN`/`FC_CLOSE`) → `iniciarMovimiento()` activa el relevo → tras `RELAY_SETTLE_MS` se
   habilita el TRIAC (`TRIGGER` en `LOW`, sin sincronismo a `ZCROSS` todavía) → en movimiento se
   vigilan fin de carrera de destino, timeout de encoder (atasco) y un nuevo flanco de `D0`
   (pide reversa o, si es la segunda pulsación dentro de `DOBLE_PULSACION_MS`, detiene y se queda
   a medio camino) → al llegar al fin de carrera, `detenerMotor()` y vuelta a `DETENIDA`.
4. **Atasco:** sin pulsos de `ENCA`/`ENCA2` durante `ENCODER_TIMEOUT_MS` con el TRIAC ya activo →
   `detenerMotor()` + `ERROR`. Única salida: un flanco de `D2`, que vuelve a `DETENIDA` sin
   reintento automático (ver `requirements.md` §3).

## Brecha conocida (pendiente, no bloqueante)

El contrato de hardware (`requirements.md` §2) describe `ZCROSS` como fuente de interrupción para
sincronizar el disparo del TRIAC con el cruce por cero. **Decisión del usuario (sesión 004):**
por ahora el TRIAC se maneja como interruptor todo/nada sin usar `ZCROSS` en absoluto; la rampa de
arranque suave con disparo por ángulo de fase queda para una sesión futura. No es un olvido: es
el alcance acordado. El diagnóstico de pulsos de `ZCROSS` (`DEBUG_PULSOS` en `porton.h`) sigue
sobre-contando incluso con antirrebote — ver `memory.md` — pero no bloquea nada porque `ZCROSS`
no dispara el TRIAC todavía.
