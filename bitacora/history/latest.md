# Estado actual

> Última sesión: Sesión `7` (`2026-08-08`) — ver `sesion-007-2026-08-08.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- Arranque suave del TRIAC implementado: `ZCROSS` dispara el gate del TRIAC (`BT138-800`) por
  ángulo de fase, con una rampa de potencia al iniciar cada movimiento (`requirements.md` §5).
  Compila y corre sin crash en la placa real.
- **Sin confirmar que se perciba en el motor real.** El usuario probó un movimiento y no notó
  diferencia respecto del arranque anterior a máxima potencia. Dos hipótesis abiertas, ninguna
  confirmada: (a) un bug en el mecanismo de disparo, (b) el motor de inducción responde poco en
  velocidad a un corte de fase moderado (reduce más el torque). Falta el dato del osciloscopio.
- La parada sigue siendo instantánea a propósito — parada suave queda pospuesta a una sesión
  futura, junto con calibración de recorrido por pulsos de `ENCA2` (el usuario la va a
  especificar).
- Efecto lateral bueno: el diagnóstico de `ZCROSS` (`DEBUG_PULSOS`) ya cuenta exacto (120/s),
  tras subir el antirrebote a 3000us — ya no es un pendiente.

## Próximo paso inmediato

- **El usuario va a probar con el osciloscopio** (`ZCROSS` vs `TRIGGER` durante un movimiento
  real) y va a traer el resultado. Con eso se decide si hay que corregir el mecanismo de disparo
  o si el arranque suave ya funciona pero no es perceptible con estos parámetros.

## Pendientes operativos

- Diagnóstico de osciloscopio del arranque suave (ver arriba) — bloquea seguir ajustando la
  rampa a ciegas.
- Resolver la carpeta de KiCad renombrada (`PCB_Door_Controller/`) — sigue sin tocar.
- Parada suave + calibración de recorrido — diseño pendiente, a especificar por el usuario.
- `ESP_TIMER_ISR` no disponible en esta build — los timers del TRIAC corren en dispatch
  `ESP_TIMER_TASK` (más jitter que el ideal) — no bloqueante, pero anotado.

## Referencias

- `requirements.md` §5 — arranque suave del TRIAC, alcance y parámetros.
- `architecture.md` — patrón de disparo por fase (dos `esp_timer` en cascada) y brecha conocida.
- `memory.md` — gotchas técnicos y las dos hipótesis abiertas sobre por qué no se percibe.
- `design.md` → Decisiones estructurales, 2026-08-08 ("Arranque suave del TRIAC...").
