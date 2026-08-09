# index.md — Índice de sesiones (append-only)

> Una fila por sesión. Al cerrar, agregar con Bash (sin leer el archivo completo):
> `printf '| N | YYYY-MM-DD | resumen | <session> |\n' >> bitacora/history/index.md`

| Sesión | Fecha | Resumen | Archivo |
| --- | --- | --- | --- |
| 1 | 2026-08-07 | Bootstrap de stele (layout agrupado) + firmware migrado a PlatformIO | sesion-001-2026-08-07.md |
| 2 | 2026-08-07 | Vinculado repo remoto (origin/main) + primer commit y push | sesion-002-2026-08-07.md |
| 3 | 2026-08-08 | Capturado contrato de hardware (pines ESP32-S3 + sistema de potencia) | sesion-003-2026-08-08.md |
| 4 | 2026-08-08 | Interlock de relevos + maquina de estados no bloqueante + timeout de encoder + USB CDC | sesion-004-2026-08-08.md |
| 5 | 2026-08-08 | Confirmaciones de diseno (control remoto, UART, idioma futuro) | sesion-005-2026-08-08.md |
| 6 | 2026-08-08 | Validacion en hardware real: control remoto D0/D2, ENCA2, atasco confirmado | sesion-006-2026-08-08.md |
| 7 | 2026-08-08 | Arranque suave del TRIAC (fase por ZCROSS), sin confirmar efecto en motor real | sesion-007-2026-08-08.md |
