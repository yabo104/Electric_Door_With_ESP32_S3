<!-- STELE:INICIO — bloque GENERADO. No editar a mano: se reescribe entero en `config` y al
     actualizar. Lo que esté FUERA de estas marcas es del proyecto y el marco no lo toca. -->

Este proyecto usa el marco **stele** (`.stele/`). El agente carga este archivo al iniciar cada
sesión, así que el ritual de apertura se ejecuta **automáticamente**: el contexto mínimo viene
importado abajo. No hace falta pedir "lee AGENTS.md".

**Reglas de sesión (resumen — detalle en `.stele/SKILL.md`):**

- **En tu PRIMERA respuesta de la sesión, empieza con 1-3 líneas de orientación** que confirmen el
  arranque: última sesión (N + título), si quedó trabajo a medias (`bitacora/history/handover.md`),
  y próximo paso propuesto — sea cual sea el primer mensaje del usuario.
- **Al usuario se le habla en llano, y se nombra el archivo entre paréntesis** — vale para el
  saludo, los informes y los resúmenes, no para lo que escribes en los docs. Tabla de
  equivalencias en `.stele/SKILL.md` → "Cómo se le habla al usuario".
- **El habla va en español, con acentos y ñ.** Una regla de "solo ASCII" gobierna lo que se
  escribe a un archivo, no la prosa que lees tú.
- Si `bitacora/history/handover.md` está en `EN_PROGRESO`, respeta su alcance antes de editar.
- **Antes de la primera edición de código**, deja `bitacora/history/handover.md` en
  `EN_PROGRESO` con objetivo + alcance (regla dura; exención: editar el **contenido** de un doc —
  mover o renombrar docs **no** exime).
- **Al cerrar**, sigue el checklist de cierre de `bitacora/protocol.md` (sesion-NNN,
  `bitacora/history/index.md`, `bitacora/history/esfuerzo.md`, reescribir
  `bitacora/history/latest.md`, decisiones a su hogar, refrescar
  `bitacora/history/handover.md`, y el **aviso de auditoría** si toca).
- **Un hogar por dato:** consulta el *mapa de documentación* en `bitacora/AGENTS.md`.
- Lee lo demás **bajo demanda con `grep`**; no abras archivos grandes completos.

---

## Contexto de arranque (auto-importado — GENERADO desde la lista de arranque)

@bitacora/AGENTS.md
@bitacora/memory.md
@bitacora/history/latest.md
@bitacora/history/handover.md

<!-- STELE:FIN -->
