# Estado actual

> Última sesión: Sesión `3` (`2026-08-08`) — ver `sesion-003-2026-08-08.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- El marco **stele** está instalado (`.stele/`) y bootstrapeado con layout `agrupado`
  (`base = bitacora`).
- `Firmware_Porton/` es un proyecto **PlatformIO** (entorno `esp32-s3-devkitc-1`, módulo real
  `ESP32-S3-WROOM-1U-N8R8`). `pio run` compila correctamente. Todavía no se ha modificado la
  lógica desde la migración inicial.
- Repositorio git vinculado a `origin` → `https://github.com/yabo104/Electric_Door_With_ESP32_S3.git`,
  rama `main`, sincronizado.
- **Contrato de hardware completo capturado**: sistema de potencia (motor de fase partida, TRIAC,
  relevos, encoder, finales de carrera, receptor RF) y tabla de los 13 pines del ESP32-S3, en
  `requirements.md` §2 / `bitacora/temas/hardware-esp32-s3.md`. Confirmado que coincide con las
  polaridades que ya asume el firmware.
- **Brecha detectada** (ver `architecture.md` → "Brecha conocida" y `memory.md`): `ZCROSS`,
  `ENCA`/`ENCB` y `FC_OPEN`/`FC_CLOSE` están pensados como interrupciones y el firmware actual
  solo hace polling / no los usa. Hay preguntas de diseño abiertas con el usuario antes de
  implementar esa parte (ver el chat de la sesión 3; si se responden, la decisión va a
  `requirements.md`/`design.md`, no solo aquí).

## Próximo paso inmediato

- Resolver con el usuario las preguntas de diseño sobre `ZCROSS`/TRIAC (sincronización de
  disparo), encoder (uso para detectar movimiento/dirección/atasco) y si `FC_OPEN`/`FC_CLOSE`
  pasan a manejarse por interrupción.
- Recién con eso decidido, empezar la implementación real de firmware (hoy la máquina de estados
  sigue siendo la que vino del `.ino` original, sin usar TRIAC/encoder).

## Pendientes operativos

- Preguntas de diseño de hardware/firmware sin responder (ver arriba) — bloquean la siguiente
  implementación real, no son bloqueo para seguir documentando.
- Cuando haya placa disponible: `pio run -t upload` + `pio device monitor` para validar en
  hardware real, y ajustar `board`/`board_build.*` en `Firmware_Porton/platformio.ini` si hace
  falta.
- Varias secciones de `design.md`/`requirements.md` quedaron marcadas `ADAPTAR` (restricciones,
  modelo de datos) — pendientes de que el usuario las complete.

## Referencias

- `requirements.md` §2 / `bitacora/temas/hardware-esp32-s3.md` — contrato de hardware y pines.
- `architecture.md` → "Brecha conocida" — qué falta para que el firmware respete ese contrato.
- `memory.md` — gotchas de polaridad (LED, relevos, TRIGGER, finales de carrera).
