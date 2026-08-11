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
- **Disparo del TRIAC por ángulo de fase, con dos `esp_timer` en cascada** — `isrZcross()` (ISR
  real de `ZCROSS`, con antirrebote) arma `timerDisparo` con el retardo de fase actual
  (`delayDisparoActualUS`); su callback (`onDisparoTriac`) pulsa `TRIGGER` y arma `timerPulso`
  para soltarlo tras `PULSO_TRIAC_US`. Ninguna ISR hace trabajo pesado: solo lee una variable y
  arma un temporizador. `actualizarRampa()` (llamada desde `manejarMovimiento()`, fuera de
  cualquier ISR) es la única que **calcula** el retardo objetivo, interpolando con `millis()` —
  mismo patrón no bloqueante que el resto de la máquina de estados. Ver `requirements.md` §5.
- **Cancelar temporizadores pendientes al detener** — `detenerMotor()` llama `esp_timer_stop()`
  sobre `timerDisparo` antes de forzar `TRIGGER` a `HIGH`, para que no quede un disparo de fase
  ya programado que dispare el gate después de haber decidido parar. Mismo principio que "cortar
  `TRIGGER` antes de liberar los relevos" — parar significa parar todo, no solo la salida visible.
- **Contador de posición relativa (suma/resta según sentido)** — el contador de trayecto
  (`contadorTrayecto`, `requirements.md` §6) no cuenta pulsos crudos: compara el sentido actual
  (`doorStd`) contra el sentido con el que arrancó el trayecto (`sentidoTrayecto`) en cada pulso
  de `isrEncA2()`, sumando si coinciden y restando si no. Es el mismo patrón que usaría cualquier
  odómetro relativo sin posición absoluta — reutilizable si en el futuro hace falta estimar
  posición en otro contexto del proyecto.
- **Máquina de fases para efectos no bloqueantes cortos** — el parpadeo del LED
  (`actualizarParpadeoLed()`, `requirements.md` §7) usa un patrón chico de estados (`0` inactivo,
  `1` pausa, `2` encendido) con su propio timestamp, revisado cada iteración desde
  `actualizarEstadoPuerta()`. Mismo enfoque no bloqueante que el resto del código; sirve de
  plantilla para cualquier otra señal temporizada corta que se necesite más adelante (no hace
  falta un RTOS task ni un `esp_timer` para esto).

## Flujos clave

1. **Arranque:** `main.cpp:setup()` → `inicializarPines()` apaga WiFi/Bluetooth (`apagarRadios()`,
   `requirements.md` §8), deja relevos, `TRIGGER` y `LED` en su estado seguro/apagado, arma las
   interrupciones de encoder (`ENCA` y `ENCA2`, `CHANGE`) y de `ZCROSS` (`RISING`), crea los dos
   `esp_timer` del disparo del TRIAC, y abre `Serial`.
2. **Ciclo principal:** `main.cpp:loop()` → si hay dato en `Serial`, lo guarda en `comando` →
   `procesarComandoSerial()` → `actualizarEstadoPuerta()` (que además revisa el botón de reset y
   el parpadeo del LED en cada vuelta, antes del switch por estado).
3. **Apertura/cierre:** en `DETENIDA`, un flanco de `D0` decide sentido según la posición real
   (`FC_OPEN`/`FC_CLOSE`, o la última llegada confirmada si ninguno está activo —
   `requirements.md` §4) → si el origen es un final de carrera confirmado, arma el contador de
   trayecto (§6) → `iniciarMovimiento()` activa el relevo y enciende el LED fijo → tras
   `RELAY_SETTLE_MS` se habilita el TRIAC a potencia fija (`POTENCIA_MOTOR_PCT`, sin rampa por
   ahora — `requirements.md` §5) → en movimiento se vigilan fin de carrera de destino, timeout de
   encoder (atasco) y un nuevo flanco de `D0` (pide reversa o, si es la segunda pulsación dentro
   de `DOBLE_PULSACION_MS`, detiene y se queda a medio camino) → al llegar al fin de carrera,
   `detenerMotor()` (instantáneo), se guarda la llegada confirmada, se reporta el trayecto si era
   válido, y arranca el parpadeo del LED (§7).
4. **Atasco:** sin pulsos de `ENCA`/`ENCA2` durante `ENCODER_TIMEOUT_MS` con el TRIAC ya activo →
   `detenerMotor()` + `ERROR` (invalida el trayecto en curso). Única salida: un flanco de `D2`,
   que vuelve a `DETENIDA` sin reintento automático (ver `requirements.md` §3).

## Brecha conocida / pendiente

- **Parada suave y arranque suave "de verdad" (con rampa).** El release (sesión 008) corre a
  potencia fija — ver `requirements.md` §5 para el porqué (enganche inconsistente del TRIAC cerca
  del cruce por cero con esta carga inductiva). El código de la rampa
  (`RAMPA_ARRANQUE_MS`/`actualizarRampa()`) sigue en `porton.cpp` sin usar. Retomar ambas cosas
  queda pospuesto a una sesión futura, con calibración de recorrido por conteo de pulsos (§6)
  como paso previo — a especificar por el usuario.
- **`ESP_TIMER_ISR` no disponible en esta build** — los dos `esp_timer` del disparo del TRIAC
  corren en dispatch `ESP_TIMER_TASK` (por defecto), no en una ISR real; puede haber más jitter
  del ideal en el ángulo de disparo. No impidió que el release funcionara en el portón real. Ver
  `memory.md`.
