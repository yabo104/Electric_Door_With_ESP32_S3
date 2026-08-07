# Estado actual

> Última sesión: Sesión `2` (`2026-08-07`) — ver `sesion-002-2026-08-07.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- El marco **stele** está instalado (`.stele/`) y bootstrapeado con layout `agrupado`
  (`base = bitacora`).
- `Firmware_Porton/` es un proyecto **PlatformIO** (entorno `esp32-s3-devkitc-1`, módulo real
  `ESP32-S3-WROOM-1U-N8R8`). `pio run` compila correctamente.
- Repositorio git vinculado a `origin` → `https://github.com/yabo104/Electric_Door_With_ESP32_S3.git`,
  rama `main`. Primer commit (`bbcb7d3`) hecho y **pusheado**.

## Próximo paso inmediato

- Cuando haya placa disponible: `pio run -t upload` + `pio device monitor` para validar en
  hardware real, y ajustar `board`/`board_build.*` en `Firmware_Porton/platformio.ini` si hace
  falta.

## Pendientes operativos

- Varias secciones de `design.md`, `requirements.md` y `memory.md` quedaron marcadas `ADAPTAR`
  (restricciones, modelo de datos, glosario ampliado) — pendientes de que el usuario las complete.

## Referencias

- `design.md` § Decisiones estructurales — 2026-08-07, migración a PlatformIO.
- `architecture.md` — mapa de `Firmware_Porton/`.
- `memory.md` — gotchas de relevos/TRIAC activos en LOW, board sin confirmar contra hardware real.
