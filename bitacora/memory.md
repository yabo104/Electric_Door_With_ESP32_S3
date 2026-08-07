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

- **Los relevos son activos en LOW.** `RELAY_OP`/`RELAY_CL` en `LOW` es lo que **activa** el
  motor (abre/cierra); `HIGH` los deja desenergizados. Escribir `HIGH` a ambos es el estado
  seguro/parado.
- **`TRIGGER` (disparo del TRIAC) también es activo en LOW.** `HIGH` = disparo inhibido.
- **`FC_OPEN`/`FC_CLOSE` usan `INPUT_PULLUP`.** El final de carrera activado se lee como `LOW`;
  por eso la lógica de puerta comprueba con `!digitalRead(FC_OPEN)` para "portón abierto".
- **`D0`-`D3` son las salidas ya decodificadas del receptor de control remoto** (no los pines
  crudos del RF). Se leen con `digitalRead` simple (sin pull-up): cualquiera en `HIGH` dispara un
  comando de apertura/cierre.
- **El estado `WAITING` bloquea el `loop()`.** Tiene un `while(...)` que espera a que se suelten
  los canales `D0`-`D3` antes de seguir — mientras un botón del control remoto se mantiene
  presionado, el resto del firmware (lectura serie, LED, etc.) no corre. Tenerlo en cuenta antes
  de agregar lógica que deba correr siempre (p. ej. un watchdog o un timeout).
- **`comando` se maneja por caracter ASCII** (`'0'`-`'7'`) vía `Serial` a 115200 baudios; es la
  interfaz de pruebas manuales, no un protocolo binario.

## Seguridad

- El firmware maneja directamente relevos y disparo de TRIAC sobre lo que probablemente es carga
  de línea (motor del portón / iluminación). Cualquier cambio a la lógica de `RELAY_OP`/
  `RELAY_CL`/`TRIGGER` debe preservar el arranque en estado seguro (`HIGH` = todo desactivado) que
  ya hace `inicializarPines()`.
