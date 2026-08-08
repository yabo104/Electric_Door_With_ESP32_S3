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
> controladora en KiCad (`PCB_puerta/`, sin código — su estructura la define el propio proyecto
> KiCad).

## Stack

- C++ sobre framework **Arduino**, gestionado con **PlatformIO** (`Firmware_Porton/platformio.ini`).
- Build: `pio run` (desde `Firmware_Porton/`). Carga: `pio run -t upload`. Monitor:
  `pio device monitor -b 115200`.

## Estructura de módulos

- `Firmware_Porton/platformio.ini` — configuración del entorno (`esp32-s3-devkitc-1`, `arduino`).
- `Firmware_Porton/include/porton.h` — definiciones de pines, estados de la puerta
  (`EstadoPuerta`) y prototipos de las funciones de `porton.cpp`.
- `Firmware_Porton/src/porton.cpp` — implementación: `inicializarPines()` (configura pines y deja
  las salidas de potencia en estado seguro), `procesarComandoSerial()` (interfaz de pruebas por
  `Serial`, comandos `'0'`-`'7'`), `actualizarEstadoPuerta()` (máquina de estados del portón).
- `Firmware_Porton/src/main.cpp` — `setup()`/`loop()`: inicializa y en cada iteración lee un
  comando de `Serial` si hay, y llama a `procesarComandoSerial()` + `actualizarEstadoPuerta()`.
- `Firmware_Porton/lib/`, `Firmware_Porton/test/` — carpetas estándar de PlatformIO, vacías por
  ahora (`README` de PlatformIO Library Dependency Finder / Unit Testing respectivamente).

## Patrones reutilizables ⭐

- **Máquina de estados de la puerta** — `actualizarEstadoPuerta()` en `src/porton.cpp`, sobre el
  enum `EstadoPuerta` (`STOP`, `OPENING`, `CLOSING`, `ERROR`, `WAITING`) declarado en
  `include/porton.h`. Estado global en `doorStd`/`doorLastStd` (variables `extern` compartidas
  entre `main.cpp` y `porton.cpp`). Cualquier nuevo comportamiento de movimiento del portón se
  agrega como transición dentro de este switch, no como lógica paralela en `main.cpp`.
- **Interfaz de comandos por Serial** — `procesarComandoSerial()`, un switch sobre caracteres
  ASCII recibidos por `Serial`. Pensado para pruebas manuales de banco (activar un relevo, probar
  el LED, leer entradas), no como protocolo de producción.

## Flujos clave

1. **Arranque:** `main.cpp:setup()` → `inicializarPines()` deja relevos y `TRIGGER` en `HIGH`
   (desactivados) y abre `Serial` a 115200.
2. **Ciclo principal:** `main.cpp:loop()` → si hay dato en `Serial`, lo guarda en `comando` →
   `procesarComandoSerial()` (comandos de prueba) → `actualizarEstadoPuerta()` (avanza la máquina
   de estados según finales de carrera `FC_OPEN`/`FC_CLOSE` y canales de control remoto
   `D0`-`D3`).
3. **Ciclo de apertura/cierre:** `WAITING` detecta una entrada activa en `D0`-`D3` → decide
   `OPENING` o `CLOSING` según qué final de carrera está activo → en movimiento, una nueva
   pulsación invierte el sentido; al llegar al final de carrera contrario, vuelve a `STOP` →
   `WAITING`.

## Brecha conocida (antes de tocar `ZCROSS`/encoder/finales de carrera)

El contrato de hardware (`requirements.md` §2, detalle en
[`temas/hardware-esp32-s3.md`](temas/hardware-esp32-s3.md)) describe `ZCROSS`, `ENCA`/`ENCB` y
`FC_OPEN`/`FC_CLOSE` como fuentes de **interrupción**. El firmware actual no usa interrupciones en
ninguno: los finales de carrera se leen por *polling* dentro de `actualizarEstadoPuerta()`,
`ZCROSS` no se usa (no hay disparo de TRIAC sincronizado a cruce por cero), y `ENCA`/`ENCB` no se
leen. Pasar a un diseño con ISRs (sobre todo para `ZCROSS`, que es una señal de temporización) va
a requerir separar estado compartido con `volatile` y sacar el disparo del TRIAC de la lógica de
`loop()`. No implementado todavía — ver preguntas de diseño planteadas en la sesión 003.
