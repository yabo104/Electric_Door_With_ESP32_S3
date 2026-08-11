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
- §5 — Disparo del TRIAC por ángulo de fase (release: potencia fija, sin rampa)
- §6 — Contador de trayecto (pulsos de `ENCA2`)
- §7 — Indicador LED
- §8 — Radios WiFi/Bluetooth apagadas (sin deep sleep)

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
  `ENCODER_TIMEOUT_MS` (2000 ms — ajustado varias veces por el usuario durante las pruebas de la
  sesión 008 en el portón real; sigue marcado `TEMPORAL` en el código, no hay un valor "final"
  derivado analíticamente) activo sin ningún pulso, se detiene el motor y se pasa a `ERROR`. Dos
  fuentes en paralelo alimentan el mismo timestamp: `ENCA` (con un problema de hardware conocido —
  pull-up del circuito de entrada — hoy no aporta pulsos) y `ENCA2`, un sensor Hall en `GPIO44`
  que sí funciona (confirmado: 0 pulsos con el motor quieto, pulsos reales con el motor girando).
  Si se arregla el hardware de `ENCA`, vuelve a sumar sin más cambios de código.
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
    **cerrado** → `D0` **abre**. Se evalúa en cada pulsación, no solo al arrancar.
  - **Posición intermedia (ningún fin de carrera activo) — corregido en la sesión 008:** ya no
    es siempre "por defecto cierra". El firmware recuerda la última llegada **confirmada**
    (`ultimaLlegadaConfirmada`, en `porton.cpp`) y decide el sentido contrario a esa. Esto cubre
    un caso real encontrado en el portón físico: por inercia del motor, el imán del fin de
    carrera puede rebasar el sensor al frenar, y el switch se desactiva de nuevo justo después de
    haber llegado — sin este ajuste, el firmware volvía a intentar cerrar (o abrir) contra el
    tope mecánico ya alcanzado. Solo si **nunca** hubo una llegada confirmada (arranque en frío,
    posición realmente desconocida) se usa el default original: cierra.
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

### §5 — Disparo del TRIAC por ángulo de fase (release: potencia fija, sin rampa)

> Mecanismo implementado en la sesión 007 (plan discutido y aprobado con el usuario antes de tocar
> código). **Motivo original:** el portón tomaba velocidad y golpeaba fuerte contra el tope al
> cerrar, con riesgo de dañar los engranajes. **Decisión de alcance para el release (sesión 008,
> con el usuario):** dejar el TRIAC en **potencia fija** (sin la rampa de arranque suave) — ver el
> porqué más abajo.

- **Mecanismo (sin cambios desde la sesión 007):** `ZCROSS` es una interrupción real (no solo
  diagnóstico). En cada cruce por cero válido (con antirrebote, `ZCROSS_DEBOUNCE_US`), si el TRIAC
  está activo, arma un `esp_timer` con el retardo de fase actual (`delayDisparoActualUS`); al
  vencer, pulsa `TRIGGER` (gate del TRIAC `BT138-800`) por `PULSO_TRIAC_US` con un segundo
  `esp_timer`. `PULSO_TRIAC_US` subió de 200 a 500µs en la sesión 008: un pulso más largo ayuda a
  enganchar con esta carga inductiva.
- **La rampa de arranque suave (interpolar `DISPARO_US_MIN` → `DISPARO_US_MAX` durante
  `RAMPA_ARRANQUE_MS`) quedó DESACTIVADA para el release.** Motivo, encontrado en pruebas en banco
  con el portón real (sesión 008): disparar demasiado cerca del cruce por cero (potencias
  cercanas al 100%) no siempre engancha el TRIAC con esta carga inductiva — al 100% exacto
  (~6,5° desde el cruce) el motor directamente no se movía. La rampa original pasaba por esa zona
  marginal al principio de cada arranque, y eso generaba paradas falsas por "atasco" (§3): el
  enganche inconsistente ciclo a ciclo genera pulsos de torque irregulares, no una rampa suave de
  potencia creciente.
- **Solución para el release: potencia fija, calibrada empíricamente.** `POTENCIA_MOTOR_PCT` (hoy
  85%, en `porton.h`) fija `DISPARO_US_MIN = DISPARO_US_MAX` — el TRIAC dispara al mismo ángulo de
  fase durante todo el movimiento, sin rampa. Es una interpolación **lineal en tiempo** entre
  `DISPARO_US_MAX` (100%) y `SEMICICLO_US` (0%), no un porcentaje exacto de potencia RMS (esa
  relación es curva), pero sirve como perilla monótona para ajustar a mano si hace falta.
  Historial de pruebas que llevaron al 85%: 100% no movía el motor; 90% y 95% sí lo movían pero
  daban `ERROR` falso en tramos del recorrido con más esfuerzo mecánico; 85% (con el pulso de gate
  ya en 500µs) fue el primer nivel que movió el portón de punta a punta sin falsos atascos.
- **La parada sigue siendo instantánea** — sin rampa de bajada, sin cambios en esta sesión.
- **Arranque suave real (con rampa) queda pospuesto**, ahora con un piso de potencia fiable
  (85%, pulso de 500µs) como punto de partida en vez de los valores originales sin probar. El
  código de la rampa (`RAMPA_ARRANQUE_MS`, `actualizarRampa()`) se deja en `porton.cpp` sin usar,
  para no rehacerlo desde cero cuando se retome.
- **Detalle de hallazgos técnicos y gotchas de esta implementación:** `memory.md`.

### §6 — Contador de trayecto (pulsos de `ENCA2`)

> Agregado en la sesión 008 a pedido del usuario, para poder revisar si el recorrido completo del
> portón da una cantidad de pulsos de `ENCA2` medianamente consistente entre corridas — dato de
> entrada para una futura calibración de recorrido (parada suave con zona de desaceleración,
> pospuesta, ver §5).

- **Solo es válido entre dos finales de carrera confirmados.** Arranca en 0 únicamente cuando un
  movimiento empieza con un fin de carrera activo en el origen (abierta→cierra o cerrada→abre) —
  **no** desde una posición intermedia (incluida la inferida por `ultimaLlegadaConfirmada`, §4):
  ahí no hay un punto de partida medido, solo asumido, y contaminaría la comparación entre
  corridas.
- **Suma en el sentido del trayecto, resta en el sentido contrario.** Cada pulso de `ENCA2`
  incrementa el contador si el sentido actual coincide con el que tenía el trayecto al arrancar, y
  lo decrementa si no (hubo una reversa) — así el contador siempre refleja la distancia neta
  recorrida desde el origen confirmado, aunque haya idas y vueltas en el medio.
- **Se invalida en silencio (decisión del usuario) si el trayecto se interrumpe** antes de llegar
  al destino: detenido parcial (doble pulsación) o `ERROR` (atasco). No se imprime nada — solo se
  reportan los recorridos completos, que son el dato comparable.
- **Al llegar al fin de carrera de destino**, si el trayecto era válido, imprime por Serial
  `[TRAYECTO] completo, sentido=…, pulsos=…`.

### §7 — Indicador LED

> Agregado en la sesión 008 a pedido del usuario, pensando en la instalación final del portón
> (sin acceso directo al Serial).

- **Encendido fijo mientras el motor está en movimiento** (`ABRIENDO`/`CERRANDO`), apagado el
  resto del tiempo — centralizado en `iniciarMovimiento()`/`detenerMotor()`, no hay otro punto del
  código que escriba `LED` para la operación automática.
- **Parpadeo al detectar el fin de carrera de destino** (confirma que se detectó el imán) —
  **no** en detenido parcial ni en `ERROR`, para no dar una señal engañosa. Secuencia de dos fases,
  no bloqueante: pausa apagada (`LED_PAUSA_PARPADEO_MS`, 100ms) y luego encendido
  (`LED_PARPADEO_MS`, 150ms). La pausa es necesaria: sin ella, el parpadeo arranca desde el mismo
  "encendido" que ya tenía por el movimiento, y el flanco no se distingue a simple vista (bug
  real encontrado en la primera versión, sesión 008).
- **Prioridad estricta:** siempre se llama primero a `detenerMotor()` (corte de salidas,
  instantáneo) y recién después se dispara el parpadeo — nunca al revés.

### §8 — Radios WiFi/Bluetooth apagadas (sin deep sleep)

> Decisión de la sesión 008, a partir de una pregunta del usuario sobre bajo consumo/calor del
> ESP32-S3. Detalle completo de la discusión en `design.md` → Decisiones estructurales.

- **`WiFi.mode(WIFI_OFF)` + `btStop()`** en `inicializarPines()` (`apagarRadios()`,
  `porton.cpp`) — el proyecto no usa ninguno de los dos radios, así que apagarlos evita consumo/
  calor/interferencia sin costo funcional.
- **Se descartó el deep sleep** (que el usuario planteó primero) porque este equipo está
  alimentado de red, no a batería — el ahorro de energía no justifica el costo: perder toda la
  RAM en cada ciclo de sueño (hay que reconstruir el estado desde memoria RTC), la latencia de
  arranque en frío en cada despertar, que el USB CDC (el `Serial` que se usó para depurar toda
  esta sesión) se cae mientras duerme, y verificar con osciloscopio que los relevos/TRIAC no
  flotan durante la transición de sueño/despertar antes de confiar en él con 110VAC de por medio.
- **Costo de la librería WiFi:** aunque solo se llama para apagarla, enlazarla infla el binario
  notablemente (Flash ~8%→20%, RAM ~6%→13% en esta build) — sigue sobrando margen de sobra, pero
  vale la pena saberlo si en el futuro el espacio se vuelve ajustado.

## Modelo de datos

ADAPTAR: no aplica lógica de datos persistentes todavía; el firmware es puramente reactivo sobre
entradas digitales. Si se agrega persistencia (p. ej. posición del portón, configuración por
NVS/EEPROM), documentar aquí el modelo.

## Contratos de API / integración

ADAPTAR: hoy no hay integración de red (Wi-Fi/BLE/MQTT, etc.). Si se agrega, documentar aquí el
contrato (endpoints, payloads, autenticación).
