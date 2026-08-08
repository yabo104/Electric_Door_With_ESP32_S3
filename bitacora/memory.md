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

- **PlatformIO**, entorno `esp32-s3-devkitc-1`, framework `arduino`. Verificado en esta sesión:
  `pio run` compila (Flash 8.3%, RAM 5.8% con el firmware actual).
- **Board real:** el módulo montado en `PCB_puerta` es `ESP32-S3-WROOM-1U-N8R8` (ver BOM en
  `PCB_puerta/jlcpcb/production_files/BOM-PCB_puerta.csv`). El entorno `esp32-s3-devkitc-1` es
  una placa de referencia genérica usada como aproximación razonable — **no se ha confirmado**
  contra hardware real (`pio run -t upload` + prueba en la placa) ni ajustado la config de
  flash/PSRAM al módulo exacto (N8R8 = 8MB flash / 8MB PSRAM octal). Si al cargar el firmware a la
  placa real aparecen problemas de tamaño de flash o de PSRAM, revisar `board_build.*` en
  `Firmware_Porton/platformio.ini`.

## Firmware (`Firmware_Porton/`)

**Contrato de pines confirmado por el usuario** (sesión 003): tabla completa y sistema de potencia
en [`temas/hardware-esp32-s3.md`](temas/hardware-esp32-s3.md), resumen en `requirements.md` §2.
Lo de abajo son las trampas puntuales que ese contrato implica para el código actual.

- **Los relevos son activos en LOW.** `RELAY_OP`/`RELAY_CL` en `LOW` es lo que **activa** el
  relevo (elige el sentido de giro); `HIGH` los deja desenergizados. Escribir `HIGH` a ambos es el
  estado seguro/parado. **Los relevos solo eligen sentido — la potencia la regula el TRIAC.**
- **`TRIGGER` (disparo del TRIAC) también es activo en LOW.** `HIGH` = disparo inhibido.
- **`LED` es activo en LOW** (nivel 0 = encendido). El firmware actual ya arranca en `HIGH`
  (apagado) por diseño de `inicializarPines()`, pero antes de este contrato esa polaridad no
  estaba documentada — no asumir lo contrario al tocar el LED.
- **`FC_OPEN`/`FC_CLOSE` usan `INPUT_PULLUP`.** El final de carrera activado se lee como `LOW`
  (flanco de bajada al llegar el imán); por eso la lógica de puerta comprueba con
  `!digitalRead(FC_OPEN)` para "portón abierto".
- **`D0`-`D3` son las salidas ya decodificadas del receptor RF 433MHz** (no los pines crudos del
  RF). Se leen con `digitalRead` simple (sin pull-up): cualquiera en `HIGH` dispara un comando de
  apertura/cierre.
- **`ZCROSS`, `ENCA`/`ENCB` y `FC_OPEN`/`FC_CLOSE` están pensados como fuentes de interrupción**
  (así los describe el contrato de hardware), pero el firmware actual **no usa interrupciones en
  ninguno**: `FC_OPEN`/`FC_CLOSE` se leen por *polling* dentro de `actualizarEstadoPuerta()`;
  `ZCROSS` no se usa (no hay disparo de TRIAC sincronizado a cruce por cero todavía, solo los
  comandos de prueba `5`/`6` que fuerzan `TRIGGER` a mano); `ENCA`/`ENCB` no se leen en ningún
  lado. Es una brecha de diseño conocida, no un bug — ver preguntas abiertas antes de tocar esto.
- **El estado `WAITING` bloquea el `loop()`.** Tiene un `while(...)` que espera a que se suelten
  los canales `D0`-`D3` antes de seguir — mientras un botón del control remoto se mantiene
  presionado, el resto del firmware (lectura serie, LED, etc.) no corre. Si `ZCROSS` pasa a
  manejarse por interrupción, este bloqueo del loop principal deja de ser un problema para el
  disparo del TRIAC (la ISR sigue corriendo), pero sigue siéndolo para todo lo demás.
- **`comando` se maneja por caracter ASCII** (`'0'`-`'7'`) vía `Serial` a 115200 baudios; es la
  interfaz de pruebas manuales, no un protocolo binario.

## Seguridad

- El firmware maneja directamente relevos y disparo de TRIAC sobre lo que probablemente es carga
  de línea (motor del portón / iluminación). Cualquier cambio a la lógica de `RELAY_OP`/
  `RELAY_CL`/`TRIGGER` debe preservar el arranque en estado seguro (`HIGH` = todo desactivado) que
  ya hace `inicializarPines()`.
