# Tema: Contrato de hardware — sistema de potencia y pines ESP32-S3

> Extraído de `requirements.md` §2 (superó el umbral de ~50 líneas). Fuente: descripción del
> usuario, sesión `003` (`2026-08-08`). Es el **contrato autoritativo** de qué hace cada pin y
> cómo está compuesto el sistema de potencia — el firmware debe respetarlo.

## Sistema de potencia (motor del portón)

- **Motor AC de 110V, fase partida, un solo motor.** Tres terminales: uno **común** y dos de
  **control de giro** (uno por sentido). Entre los dos terminales de control hay un
  **capacitor de arranque/marcha de 33 µF / 400 V** — es lo que da el desfase para que el motor
  gire en un sentido u otro según cuál terminal de control se energiza.
- **Dos relevos (`RELAY_OP`, `RELAY_CL`) conmutan cuál terminal de control queda activo** — deciden
  el **sentido** de giro (abrir/cerrar), no la potencia entregada.
- **Un TRIAC regula la potencia** entregada al motor (fase controlada, sincronizada con el cruce
  por cero de la AC vía `ZCROSS`). Es el elemento que controla **cuánta** potencia recibe el
  motor (p. ej. arranque suave, limitar torque), independiente del sentido que ya fijaron los
  relevos.
- **Encoder de dos canales (`ENCA`, `ENCB`) en el eje del motor** — pulsos en cuadratura: permiten
  saber si el motor **está girando o no**, y en qué **dirección** (por el desfase entre A y B).
  Es la única realimentación de movimiento real (sin él, el firmware no puede distinguir "el
  relevo está activado" de "el motor efectivamente se está moviendo").
- **Dos finales de carrera tipo *reed switch*** (`FC_OPEN`, `FC_CLOSE`) — un imán solidario a la
  parte móvil acciona el reed switch correspondiente al llegar a cada extremo del recorrido.
- **Receptor RF 433 MHz** para los controles remotos — decodifica la señal de aire y expone el
  resultado como hasta 4 líneas digitales (`D0`-`D3`) hacia el ESP32-S3; el ESP32-S3 no decodifica
  RF directamente, solo lee esas salidas ya decodificadas.

## Pines ESP32-S3 (contrato)

| GPIO | Nombre | Tipo | Descripción |
| --- | --- | --- | --- |
| 2 | `LED` | Salida / digital | LED piloto. **Activo en `LOW`** (nivel 0 = encendido). |
| 3 | `ZCROSS` | Entrada / interrupción | Cruce por cero de la AC: pulso en **alto** en cada cruce. Pensado para disparar por interrupción, no por polling (es una señal de temporización). |
| 9 | `D3` | Entrada / digital / RF | Canal decodificado del receptor RF. Activo en **alto**. |
| 10 | `D2` | Entrada / digital / RF | Ídem. |
| 11 | `D1` | Entrada / digital / RF | Ídem. |
| 12 | `D0` | Entrada / digital / RF | Ídem. |
| 13 | `ENCA` | Entrada / interrupción / encoder | Canal A del encoder de movimiento del motor. |
| 14 | `ENCB` | Entrada / interrupción / encoder | Canal B del encoder de movimiento del motor. |
| 15 | `FC_OPEN` | Entrada / interrupción / final de carrera | **Normalmente en alto**; flanco de bajada cuando el imán llega (portón abierto). |
| 16 | `FC_CLOSE` | Entrada / interrupción / final de carrera | **Normalmente en alto**; flanco de bajada cuando el imán llega (portón cerrado). |
| 21 | `TRIGGER` | Salida / digital / disparo TRIAC | Dispara el TRIAC con nivel **bajo**. |
| 38 | `RELAY_OP` | Salida / digital | Activa el relevo de apertura con nivel **bajo**. |
| 39 | `RELAY_CL` | Salida / digital | Activa el relevo de cierre con nivel **bajo**. |

## Consistencia con el firmware actual (`Firmware_Porton/`)

Confirmado contra `include/porton.h` / `src/porton.cpp`:

- Polaridades activas en `LOW` de `RELAY_OP`, `RELAY_CL`, `TRIGGER`, y activo en alto de `D0`-`D3`
  y de los finales de carrera vía `INPUT_PULLUP` (`!digitalRead(FC_OPEN)` = detectado) — **ya
  coinciden** con el código existente.
- **Nuevo dato, no reflejado aún en el firmware:** polaridad de `LED` (activo en `LOW`) — el
  código actual ya arranca en `HIGH` (apagado) en `inicializarPines()`, consistente por
  casualidad de diseño, no porque el firmware lo supiera documentado.
- **Brecha real:** `ZCROSS`, `ENCA`/`ENCB` y `FC_OPEN`/`FC_CLOSE` están pensados como fuentes de
  **interrupción**, pero el firmware actual solo hace `pinMode(..., INPUT[_PULLUP])` y lee
  `FC_OPEN`/`FC_CLOSE` por *polling* dentro de `actualizarEstadoPuerta()`. `ZCROSS` no se usa en
  absoluto todavía (no hay disparo de TRIAC sincronizado a cruce por cero: los comandos `5`/`6` de
  prueba fuerzan `TRIGGER` a mano). `ENCA`/`ENCB` tampoco se leen en ningún lado (estaban
  comentados en el `.ino` original). Ver preguntas de diseño abiertas en `design.md` →
  Decisiones estructurales / sesión 003.
