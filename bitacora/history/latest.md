# Estado actual

> Última sesión: Sesión `1` (`2026-08-07`) — ver `sesion-001-2026-08-07.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- El marco **stele** quedó instalado (`.stele/`) y bootstrapeado con layout `agrupado`
  (`base = bitacora`). Docs de arranque escritos: `AGENTS.md`, `memory.md`, `latest.md` (este
  archivo), `handover.md`.
- `Firmware_Porton/` se reestructuró de sketch `.ino` a proyecto **PlatformIO** (entorno
  `esp32-s3-devkitc-1`), sin cambiar la lógica. `pio run` compila correctamente.
- Repositorio git inicializado en la raíz (no existía antes). **Sin ningún commit todavía.**
- Módulo real de la placa: `ESP32-S3-WROOM-1U-N8R8` — el board de PlatformIO usado
  (`esp32-s3-devkitc-1`) es una aproximación sin confirmar contra hardware real.

## Próximo paso inmediato

- Decidir con el usuario si se hace el primer commit (todo el trabajo de esta sesión está sin
  persistir) y, si corresponde, el mensaje/alcance.
- Cuando haya placa disponible: `pio run -t upload` + `pio device monitor` para validar en
  hardware real, y ajustar `board`/`board_build.*` en `Firmware_Porton/platformio.ini` si hace
  falta.

## Pendientes operativos

- Repositorio git sin commits: todo el árbol de trabajo está sin persistir.
- Varias secciones de `design.md`, `requirements.md` y `memory.md` quedaron marcadas `ADAPTAR`
  (restricciones, modelo de datos, glosario ampliado) — pendientes de que el usuario las complete.

## Referencias

- `design.md` § Decisiones estructurales — 2026-08-07, migración a PlatformIO.
- `architecture.md` — mapa de `Firmware_Porton/`.
- `memory.md` — gotchas de relevos/TRIAC activos en LOW, board sin confirmar.
