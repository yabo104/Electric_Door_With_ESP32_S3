# Estado actual

> Última sesión: Sesión `5` (`2026-08-08`) — ver `sesion-005-2026-08-08.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- El marco **stele** está instalado y bootstrapeado (`base = bitacora`), repo sincronizado con
  `origin/main`.
- `Firmware_Porton/` tiene una primera implementación completa de la lógica del portón (interlock
  de relevos, no bloqueante, timeout de atasco por encoder, USB CDC), con la lógica de control
  remoto **ya confirmada por el usuario**. `pio run` compila limpio. **Todavía nada probado en
  hardware real** — la próxima sesión es con la placa física en mano.
- Aclarado: no hay conflicto de pines entre `TRIGGER` (GPIO21) y la UART de depuración (confirmada
  en GPIO43/44, hacia un conector auxiliar).
- Decisión de largo plazo registrada: la documentación se traduce a inglés recién al finalizar el
  proyecto (destino: pool de proyectos open-source de referencia del usuario). Por ahora sigue en
  español — no hay nada que traducir todavía.

## Próximo paso inmediato

- **Sesión con la placa física:** `pio run -t upload`, validar en hardware real el interlock de
  relevos, el timeout de atasco del encoder, los finales de carrera, y el USB CDC. Ajustar
  `board_build.*` en `platformio.ini` si aparecen problemas de flash/PSRAM contra el módulo real
  (`ESP32-S3-WROOM-1U-N8R8`).

## Pendientes operativos

- **Trackear en git el proyecto de KiCad renombrado.** En disco ya quedó una sola carpeta,
  `PCB_Door_Controller/` (la otra variante con guion bajo, `PCB_Door_Controller_/`, ya no está),
  pero sigue sin commitear — git todavía tiene `PCB_puerta/` como borrado pendiente. Ojo: dentro
  de `PCB_Door_Controller/` los archivos del proyecto KiCad se llaman `PCB_Door_Controller_.*`
  (con guion bajo al final, distinto del nombre de la carpeta) — parece un rename incompleto;
  confirmar con el usuario antes de commitear el rename.
- Qué hacer al entrar/salir del estado `ERROR` (atasco) — el usuario lo define más adelante.
- Rampa de arranque suave del TRIAC vía `ZCROSS` — pospuesta a una sesión futura.
- Traducción de la documentación a inglés — tarea de **cierre de proyecto**, no de ahora.

## Referencias

- `requirements.md` §3/§4 — máquina de estados y control remoto (ya confirmados).
- `design.md` → Restricciones — decisión de idioma de documentación al finalizar el proyecto.
- `memory.md` — gotchas de polaridad, ISR del encoder, UART de depuración (GPIO43/44).
