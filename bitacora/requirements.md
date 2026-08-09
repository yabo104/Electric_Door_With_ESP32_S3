# requirements.md — Qué hace el producto (specs, contratos, decisiones por feature)

> **Fuente de verdad del producto**: specs, contratos de API, modelo de datos, decisiones por
> feature, contexto de negocio detallado. Es lo que perdura y comparten todos los agentes y
> sesiones. Frontera: los *principios y apuestas grandes transversales* van en `design.md`; los
> *patrones de código* en `architecture.md`; las *trampas de código* en `memory.md`.
>
> **Estructura:** una sección `§` por decisión o feature, **autocontenida**. Este archivo se lee
> con `grep` y por rango, nunca entero. **Umbral de extracción: ~50 líneas** — por encima, la
> decisión se mueve a `bitacora/temas/<TEMA>.md` y aquí queda un resumen de 2-3 líneas + link.

## Cómo usar este archivo

- **Antes de implementar**, `grep -n` la sección relevante y lee solo esa parte.
- **Toda decisión** de producto/arquitectura/integración/negocio que deba perdurar se documenta
  aquí *en el momento en que se toma*.

## § Índice de decisiones

- §1 — Interfaz de comandos por Serial (pruebas de banco)
- §2 — Contrato de hardware: sistema de potencia y pines ESP32-S3
- §3 — Máquina de estados del portón (segura, no bloqueante)
- §4 — Política del control remoto (D0/D1)
- §5 — Arranque suave del TRIAC (disparo por ángulo de fase)

### §1 — Interfaz de comandos por Serial (pruebas de banco)

- **Contrato:** `Firmware_Porton` expone por `Serial` (USB CDC nativo del ESP32-S3, 115200
  baudios) una interfaz de un solo carácter para pruebas manuales, manejada en
  `procesarComandoSerial()` (`Firmware_Porton/src/porton.cpp`):
  - `0` — parpadea el LED.
  - `1` — activa el relevo de apertura, **sin habilitar el TRIAC** (no mueve el motor).
  - `2` — activa el relevo de cierre, **sin habilitar el TRIAC**.
  - `3` — detiene todo (relevos + TRIAC), vía la misma función `detenerMotor()` que usa la
    máquina de estados.
  - `4` — imprime por Serial el estado de las entradas `D0`-`D3`.
  - `5` — fuerza `TRIGGER` en `HIGH` (TRIAC apagado).
  - `6` — fuerza `TRIGGER` en `LOW` (TRIAC encendido, máxima potencia).
  - `7` — limpia el comando pendiente (no-op).
  - Los comandos `1`/`2` reusan `seleccionarSentido()`, la misma función que usa la máquina de
    estados automática — así el interlock de "nunca ambos relevos activos" aplica también en modo
    de prueba manual.
  - Esta interfaz **no** reemplaza a la máquina de estados de la puerta (`actualizarEstadoPuerta()`),
    que corre en paralelo cada iteración del `loop()`. Si se usan comandos manuales mientras la
    puerta está en movimiento automático, pueden interferir entre sí (es una interfaz de banco,
    no pensada para correr junto con operación autónoma).
- **Por qué:** pensada para validar cableado/hardware de banco antes de operar el portón de forma
  autónoma. No es un protocolo pensado para integrarse con otro sistema.

### §2 — Contrato de hardware: sistema de potencia y pines ESP32-S3

- **Qué es:** motor AC 110V de fase partida (común + 2 terminales de control + capacitor 33µF/
  400V entre ellos), 2 relevos que eligen el sentido, 1 TRIAC que regula la potencia (sincronizado
  a `ZCROSS`), encoder de 2 canales para detectar movimiento/dirección, 2 finales de carrera *reed
  switch*, receptor RF 433MHz que expone 4 canales digitales (`D0`-`D3`). Tabla completa de pines
  ESP32-S3 con tipo/polaridad de cada señal.
- **Por qué se extrajo:** contrato largo (tabla de 13 pines + descripción de sistema) y es la
  referencia autoritativa antes de escribir cualquier lógica de firmware nueva.
- Detalle completo, con la brecha entre este contrato y lo que el firmware actual ya implementa:
  [`bitacora/temas/hardware-esp32-s3.md`](./temas/hardware-esp32-s3.md).

### §3 — Máquina de estados del portón (segura, no bloqueante)

- **Estados** (`EstadoPuerta` en `include/porton.h`): `DETENIDA`, `ABRIENDO`, `CERRANDO`, `ERROR`.
  Reemplaza al diseño anterior (`STOP`/`WAITING` separados) — `DETENIDA` cubre tanto el arranque
  como el reposo entre movimientos.
- **Reglas de seguridad no negociables** (pedidas por el usuario, sesión 004):
  1. **Nunca ambos relevos activos a la vez.** Se garantiza por construcción:
     `seleccionarSentido()` escribe siempre los dos pines juntos (uno `LOW`, el otro `HIGH`, o los
     dos `HIGH`); ningún otro punto del código escribe `RELAY_OP`/`RELAY_CL` por separado.
  2. **El motor se detiene ante cualquier cambio de sentido** — nunca se invierte "en caliente".
     Al detectar un pedido de reversa en movimiento, se llama `detenerMotor()` (corta `TRIGGER`
     primero, después libera los relevos) y se guarda el sentido pedido en `sentidoPendiente`; el
     nuevo sentido recién arranca tras `REVERSE_STOP_MS` (500 ms) detenido.
  3. **La potencia (TRIAC) se habilita después del relevo**, no junto con él:
     `RELAY_SETTLE_MS` (100 ms) entre elegir sentido y encender `TRIGGER`. Evita conmutar el
     relevo con el motor ya energizado.
- **TRIAC con disparo por ángulo de fase, sincronizado a `ZCROSS`, desde la sesión 007** — ver §5.
  Reemplaza al diseño anterior (interruptor todo/nada).
- **Detección de atasco vía encoder — confirmada en hardware real (sesión 006):** una
  interrupción actualiza un timestamp (`ultimoPulsoEncoderISR`); si el TRIAC lleva más de
  `ENCODER_TIMEOUT_MS` (2500 ms, ajustado por el usuario tras ver la velocidad real del motor)
  activo sin ningún pulso, se detiene el motor y se pasa a `ERROR`. Dos fuentes en paralelo
  alimentan el mismo timestamp: `ENCA` (con un problema de hardware conocido — pull-up del
  circuito de entrada — hoy no aporta pulsos) y `ENCA2`, un sensor Hall en `GPIO44` que sí
  funciona (confirmado: 0 pulsos con el motor quieto, pulsos reales con el motor girando). Si se
  arregla el hardware de `ENCA`, vuelve a sumar sin más cambios de código.
- **Salida de `ERROR` — implementada y confirmada en hardware real (sesión 006):** botón
  dedicado (`D2`, ver §4) saca de `ERROR` y vuelve a `DETENIDA` "como recién arrancado" — el
  siguiente `D0` decide el sentido por la posición real de los finales de carrera. **Sin
  reintento automático**, a propósito: un atasco puede ser una obstrucción real, y reintentar
  por temporizador movería el motor otra vez sin que nadie lo haya verificado (mismo criterio
  que usan los estándares de portones automáticos tipo UL 325 para fallas de obstrucción). Una
  reversa automática alejándose de la obstrucción queda como posible mejora futura, pospuesta
  hasta tener un sensor de obstrucción dedicado.
- **No bloqueante:** se eliminaron los `while()`/`delay()` largos que había en el diseño anterior
  dentro de la máquina de estados (bloqueaban `loop()` y hubieran roto el timeout de atasco). Los
  tiempos (`RELAY_SETTLE_MS`, `REVERSE_STOP_MS`, `ENCODER_TIMEOUT_MS`) se miden con `millis()`
  sin bloquear.

### §4 — Política del control remoto (D0 mueve, D2 resetea fallas)

> Confirmado en hardware real, sesión 006. `D1`/`D3` están cableados en la tarjeta pero sin rol
> asignado por ahora.

- **`D0` — control normal (abrir/cerrar/invertir/detener parcial).** Detección por flanco, no
  por nivel: un botón mantenido presionado genera un solo comando, no uno por iteración del
  `loop()`. Con antirrebote (`BOTON_DEBOUNCE_MS`, 80 ms).
  - **El sentido se decide por la posición real** (finales de carrera), no por el último
    movimiento hecho: portón en el fin de carrera de **abierto** → `D0` **cierra**; en el de
    **cerrado** → `D0` **abre**; **posición intermedia** (ningún fin de carrera activo — al
    arrancar a medio camino, o tras un detenido parcial) → `D0` **cierra**, por defecto. Se
    evalúa en cada pulsación, no solo al arrancar.
  - **Una pulsación de `D0` durante el movimiento invierte el sentido** — nunca en caliente:
    detiene, espera `REVERSE_STOP_MS` (500 ms), recién ahí arranca en el sentido contrario.
  - **Dos pulsaciones de `D0` dentro de `DOBLE_PULSACION_MS` (2000 ms) durante el movimiento:
    detiene y se queda a medio camino, sin invertir.** Pensado para apertura/cierre parcial (dejar
    pasar una persona o una moto sin abrir del todo). Implementado en
    `manejarPulsacionEnMovimiento()` — una segunda pulsación que caiga dentro de la ventana de
    `REVERSE_STOP_MS` (mientras espera para iniciar la reversa) también cuenta como "doble".
- **`D2` — reset manual de `ERROR` únicamente.** No mueve el portón. Ver §3 para la política de
  por qué no hay reintento automático tras un atasco.
- **`D1`/`D3` sin uso.** Se leen igualmente en el comando de prueba `4` (`requirements.md` §1)
  pero no participan de la lógica automática.

### §5 — Arranque suave del TRIAC (disparo por ángulo de fase)

> Implementado en la sesión 007, a partir de un plan discutido y aprobado con el usuario antes de
> tocar código (`C:\Users\Yabo\.claude\plans\nifty-cooking-stardust.md` — plan local, no versionado
> en el repo). **Motivo:** el portón tomaba velocidad y golpeaba fuerte contra el tope al cerrar,
> con riesgo de dañar los engranajes.

- **Alcance de esta ronda, acotado a propósito:** solo arranque suave (rampa de potencia ascendente
  al iniciar un movimiento). **La parada sigue siendo instantánea** — sin rampa de bajada. Parada
  suave con zona de desaceleración por conteo de pulsos de `ENCA2` (con un modo de calibración de
  recorrido) queda pospuesta a una sesión futura, a especificar por el usuario.
- **Mecanismo:** `ZCROSS` pasa de "solo diagnóstico" a interrupción real. En cada cruce por cero
  válido (con antirrebote), si el TRIAC está activo, arma un `esp_timer` con el retardo de fase
  actual (`delayDisparoActualUS`); al vencer, pulsa `TRIGGER` (gate del TRIAC `BT138-800`) por
  `PULSO_TRIAC_US` (200µs, confirmado por el usuario en proyectos anteriores) con un segundo
  `esp_timer`.
- **Rampa:** al activar el TRIAC (mismo punto donde antes se fijaba `TRIGGER` en `LOW` una sola
  vez), arranca una interpolación lineal de `RAMPA_ARRANQUE_MS` (1200 ms) entre `DISPARO_US_MIN`
  (retardo grande = potencia baja, al empezar) y `DISPARO_US_MAX` (retardo chico = potencia de
  crucero, la misma potencia plena de antes). Después de `RAMPA_ARRANQUE_MS`, el disparo queda
  fijo en `DISPARO_US_MAX` hasta que el motor se detiene.
- **Valores puestos a criterio conservador, pendientes de ajuste empírico** con el usuario mirando
  el motor real: `DISPARO_US_MIN` (potencia inicial — no puede ser tan baja como para no vencer la
  fricción estática del motor), `RAMPA_ARRANQUE_MS` (duración).
- **Estado al cierre de la sesión 007: implementado, compila y sube sin error, pero SIN CONFIRMAR
  que produzca un arranque perceptiblemente más suave.** El usuario probó un movimiento real y no
  notó diferencia respecto del arranque anterior (a máxima potencia). Sin diagnóstico con
  osciloscopio todavía — pendiente para la próxima sesión. Dos hipótesis abiertas, no
  descartadas: (a) un bug real en el mecanismo de disparo (el retardo no varía como debería), o
  (b) el motor es de fase partida (inducción) y reducir el voltaje RMS por corte de fase reduce
  sobre todo el **torque**, no la **velocidad**, de forma menos perceptible que en un motor
  universal — en ese caso el arranque suave podría estar funcionando eléctricamente sin notarse
  al oído/vista. Ninguna de las dos se da por buena ni por descartada sin el dato del osciloscopio.
- **Detalle de hallazgos técnicos y gotchas de esta implementación:** `memory.md`.

## Modelo de datos

ADAPTAR: no aplica lógica de datos persistentes todavía; el firmware es puramente reactivo sobre
entradas digitales. Si se agrega persistencia (p. ej. posición del portón, configuración por
NVS/EEPROM), documentar aquí el modelo.

## Contratos de API / integración

ADAPTAR: hoy no hay integración de red (Wi-Fi/BLE/MQTT, etc.). Si se agrega, documentar aquí el
contrato (endpoints, payloads, autenticación).
