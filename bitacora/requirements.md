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
- **TRIAC sin sincronismo a `ZCROSS` por ahora** — se maneja como interruptor de estado sólido
  todo/nada (`TRIGGER` en `LOW` = máxima potencia). La rampa de arranque suave con disparo por
  ángulo de fase queda para una sesión futura (decisión del usuario).
- **Detección de atasco vía encoder:** una interrupción en `ENCA` (`CHANGE`) actualiza un
  timestamp (`ultimoPulsoEncoderISR`); si el TRIAC lleva más de `ENCODER_TIMEOUT_MS` (3000 ms)
  activo sin ningún pulso, se detiene el motor y se pasa a `ERROR`. Solo se usa `ENCA` (no hace
  falta decodificar dirección con `ENCA`/`ENCB` para esto); `ENCB` queda cableado pero sin leer.
- **Estado `ERROR`:** hoy solo corta la potencia al entrar (vía `detenerMotor()`) y queda inerte
  — sin recuperación automática. Qué debe pasar después (reintentar, esperar reset manual, cómo
  se sale de `ERROR`) **queda pendiente de que el usuario lo defina**.
- **No bloqueante:** se eliminaron los `while()`/`delay()` largos que había en el diseño anterior
  dentro de la máquina de estados (bloqueaban `loop()` y hubieran roto el timeout de atasco). Los
  tiempos (`RELAY_SETTLE_MS`, `REVERSE_STOP_MS`, `ENCODER_TIMEOUT_MS`) se miden con `millis()`
  sin bloquear.

### §4 — Política del control remoto (D0/D1)

- **Solo `D0` y `D1` están cableados** en esta tarjeta (`D2`/`D3` quedan configurados como
  entrada pero sin leerse en la lógica — evita reaccionar a ruido de un pin flotante).
- **Detección por flanco, no por nivel:** un botón mantenido presionado genera un solo comando
  (en la transición de no-presionado a presionado), no uno por iteración del `loop()`. Con
  antirrebote (`BOTON_DEBOUNCE_MS`, 50 ms).
- **El sentido se decide por la posición real** (finales de carrera), no por el último
  movimiento hecho:
  - Portón en el fin de carrera de **abierto** → el botón **cierra**.
  - Portón en el fin de carrera de **cerrado** → el botón **abre**.
  - **Posición intermedia** (ningún fin de carrera activo, típicamente al arrancar con el portón
    a medio camino) → el botón **cierra**, por defecto.
  - Esta lógica se evalúa en cada pulsación, no solo al arrancar — si alguien mueve el portón a
    mano estando el sistema apagado, al reconectar la próxima pulsación lee la posición real.
  - **Nota sobre una posible contradicción:** el pedido original decía *"si el portón está
    abierto solo se puede abrir, si está cerrado solo se puede cerrar"*, que es lo opuesto de lo
    de arriba y no tiene sentido funcional (un botón que solo permite "abrir" cuando ya está
    abierta no hace nada). Se interpretó como un lapsus de redacción y se implementó la lógica
    intuitiva (arriba), que además es la que ya tenía el firmware original. **Pendiente de
    confirmación del usuario.**

## Modelo de datos

ADAPTAR: no aplica lógica de datos persistentes todavía; el firmware es puramente reactivo sobre
entradas digitales. Si se agrega persistencia (p. ej. posición del portón, configuración por
NVS/EEPROM), documentar aquí el modelo.

## Contratos de API / integración

ADAPTAR: hoy no hay integración de red (Wi-Fi/BLE/MQTT, etc.). Si se agrega, documentar aquí el
contrato (endpoints, payloads, autenticación).
