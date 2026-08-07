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

### §1 — Interfaz de comandos por Serial (pruebas de banco)

- **Contrato:** `Firmware_Porton` expone por `Serial` (115200 baudios) una interfaz de un solo
  carácter para pruebas manuales, manejada en `procesarComandoSerial()`
  (`Firmware_Porton/src/porton.cpp`):
  - `0` — parpadea el LED (prueba de LED/buzzer).
  - `1` — activa el relevo de apertura (`RELAY_OP` en `LOW`) ~1s.
  - `2` — activa el relevo de cierre (`RELAY_CL` en `LOW`) ~1s.
  - `3` — desactiva todos los relevos y el disparo del TRIAC.
  - `4` — imprime por Serial el estado de las entradas `D0`-`D3`.
  - `5` — fuerza `TRIGGER` en `HIGH` (disparo TRIAC inhibido).
  - `6` — fuerza `TRIGGER` en `LOW` (disparo TRIAC activo).
  - `7` — limpia el comando pendiente (no-op).
  - Esta interfaz **no** reemplaza a la máquina de estados de la puerta (`actualizarEstadoPuerta()`),
    que corre en paralelo cada iteración del `loop()`.
- **Por qué:** pensada para validar cableado/hardware de banco antes de operar el portón de forma
  autónoma. No es un protocolo pensado para integrarse con otro sistema.

## Modelo de datos

ADAPTAR: no aplica lógica de datos persistentes todavía; el firmware es puramente reactivo sobre
entradas digitales. Si se agrega persistencia (p. ej. posición del portón, configuración por
NVS/EEPROM), documentar aquí el modelo.

## Contratos de API / integración

ADAPTAR: hoy no hay integración de red (Wi-Fi/BLE/MQTT, etc.). Si se agrega, documentar aquí el
contrato (endpoints, payloads, autenticación).
