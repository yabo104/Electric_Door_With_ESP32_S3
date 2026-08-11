# Estado actual

> Última sesión: Sesión `8` (`2026-08-11`) — ver `sesion-008-2026-08-11.md`
> Índice completo de sesiones: [index.md](./index.md)
> Protocolo de documentación: [protocol.md](../protocol.md)

## Dónde estamos

- **El portón funciona de punta a punta en hardware real**, validado por el usuario: abrir/
  cerrar con un botón, inversión de sentido, detenido parcial, detección de atasco con reset
  manual, indicador LED, y el TRIAC a potencia fija calibrada (85%, ver `requirements.md` §5).
- Corregido un bug real de posición (rebote del imán del fin de carrera por inercia del motor) —
  ver `ultimaLlegadaConfirmada`, `requirements.md` §4.
- Agregado un contador de trayecto (`requirements.md` §6) como insumo para una futura calibración
  de recorrido, y apagadas las radios Wi-Fi/Bluetooth (`requirements.md` §8, sin deep sleep — el
  equipo va a red, no a batería).
- Escrito el primer `README.md` de portada del repositorio, con la vista puesta en un release.
- **Pendiente de verificar en hardware:** los últimos cambios de esta sesión (apagado de radios,
  dos fixes menores de la revisión de código) compilan pero no se cargaron a la placa — no estaba
  conectada al cierre.

## Próximo paso inmediato

- Cargar la última versión a la placa real y confirmar que el comportamiento no cambió (apagado
  de radios + fixes de revisión).
- Crear el tag/release en GitHub para esta versión (pedido explícito del usuario, en curso).

## Pendientes operativos

- Verificar en hardware real la última carga (ver arriba).
- Retomar arranque/parada suaves (rampa real del TRIAC) en una sesión futura — el 85% de potencia
  fija ya es una base fiable para partir, en vez de valores sin probar.
- Sigue sin resolver la carpeta de KiCad renombrada (`PCB_Door_Controller/`) — nombre de archivos
  internos que no coincide con la carpeta, gerbers/producción desactualizados. El usuario la
  sigue trabajando por su cuenta; no se toca desde acá.
- El usuario va a iniciar otro proyecto de sistemas embebidos — sin acción pendiente en este
  proyecto, queda anotado como contexto.

## Referencias

- `requirements.md` §4/§5/§6/§7/§8 — posición inferida, TRIAC en potencia fija, contador de
  trayecto, LED, radios apagadas.
- `design.md` → Decisiones estructurales, 2026-08-11.
- `memory.md` — hallazgos técnicos de esta sesión (enganche del TRIAC, mutex del contador, etc.).
- `README.md` (raíz) — portada del repositorio.
