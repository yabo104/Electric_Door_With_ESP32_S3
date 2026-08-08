# Estado actual

> Última sesión: Sesión `4` (`2026-08-08`) — ver `sesion-004-2026-08-08.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- El marco **stele** está instalado (`.stele/`) y bootstrapeado con layout `agrupado`
  (`base = bitacora`). Repo vinculado a `origin/main`, sincronizado.
- `Firmware_Porton/` ya tiene una **primera implementación real** de la máquina de estados del
  portón (no solo la plantilla de pruebas migrada): interlock de relevos, no bloqueante,
  detección de atasco por encoder, USB CDC nativo. `pio run` compila limpio. **Nada probado en
  hardware real todavía** (no hay placa conectada en este entorno).
- Contrato de hardware completo en `requirements.md` §2 / `bitacora/temas/hardware-esp32-s3.md`.
  Diseño de la máquina de estados en `requirements.md` §3/§4 y `architecture.md`.

## Próximo paso inmediato

- **Confirmar con el usuario** la interpretación de la lógica de sentido del control remoto
  (`requirements.md` §4 — el mensaje original tenía "abrir"/"cerrar" aparentemente invertidos
  respecto de lo funcionalmente sensato; se implementó la versión intuitiva, pendiente de que el
  usuario la valide).
- Cuando haya placa disponible: `pio run -t upload` + prueba real del interlock de relevos, el
  timeout de atasco del encoder, y el USB CDC.

## Pendientes operativos

- Confirmación pendiente de la lógica abrir/cerrar del control remoto (ver arriba) — punto de
  seguridad real, no cosmético.
- Qué hacer al entrar/salir del estado `ERROR` (atasco) — el usuario dijo que lo define más
  adelante; hoy solo corta potencia y queda inerte.
- Rampa de arranque suave del TRIAC vía `ZCROSS` — pospuesta a una sesión futura (decisión
  explícita del usuario).
- Rename externo de `PCB_puerta/` → `PCB_Door_Controller`/`PCB_Door_Controller_` detectado en la
  sesión 3, todavía sin resolver con el usuario (no se tocó, ver sesión 003).
- Board de PlatformIO (`esp32-s3-devkitc-1`) sin confirmar contra el módulo real
  (`ESP32-S3-WROOM-1U-N8R8`).

## Referencias

- `requirements.md` §3/§4 — máquina de estados y política de control remoto.
- `architecture.md` — patrones (interlock, no bloqueante, botón por flanco) y brecha de `ZCROSS`.
- `memory.md` — gotchas de polaridad y de la ISR del encoder.
