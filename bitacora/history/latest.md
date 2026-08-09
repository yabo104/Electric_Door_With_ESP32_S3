# Estado actual

> Última sesión: Sesión `6` (`2026-08-08`) — ver `sesion-006-2026-08-08.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- **Primera validación completa en hardware real.** Con la placa física conectada: apertura/
  cierre, finales de carrera, interlock de relevos, TRIAC/relevos a máxima potencia, detección de
  atasco y recuperación de `ERROR` — todo confirmado funcionando.
- `ENCA`/`ENCB` tienen un problema real de hardware (pull-up) y no aportan pulsos. La detección
  de atasco corre hoy sobre `ENCA2`, un sensor Hall en `GPIO44`, confirmado limpio.
- Control remoto rediseñado: `D0` hace todo el control normal (abrir/cerrar/invertir/detener
  parcial con doble pulsación); `D2` es un botón dedicado, exclusivo, para salir de `ERROR`.
- Board de PlatformIO (`esp32-s3-devkitc-1`) validado contra el módulo real
  (`ESP32-S3-WROOM-1U-N8R8`) — ya no es un pendiente.
- `ZCROSS` sigue con un conteo de diagnóstico aproximado (no exacto) pero no bloquea nada, porque
  todavía no dispara el TRIAC.

## Próximo paso inmediato

- **Control de potencia con el TRIAC** (a pedido del usuario): retomar la rampa de arranque suave
  sincronizada con `ZCROSS`, pospuesta desde la sesión 004.

## Pendientes operativos

- Resolver la carpeta de KiCad renombrada (`PCB_Door_Controller/`, con archivos internos que no
  coinciden con el nombre de la carpeta) — sigue sin tocar, sin commitear.
- Qué hacer con el diagnóstico de pulsos (`DEBUG_PULSOS` en `porton.h`, hoy activo) cuando ya no
  haga falta — apagarlo o quitarlo.
- `ZCROSS`: el conteo de diagnóstico sigue sin ser exacto (antirrebote insuficiente) — no urgente.

## Referencias

- `requirements.md` §3/§4 — máquina de estados, detección de atasco y control remoto (D0/D2).
- `architecture.md` — patrones (interlock, reset dedicado, doble pulsación) y brecha de `ZCROSS`.
- `memory.md` — gotchas de hardware (ENCA/ENCB, ENCA2, ZCROSS, incidente del osciloscopio).
- `design.md` → Decisiones estructurales, 2026-08-08.
