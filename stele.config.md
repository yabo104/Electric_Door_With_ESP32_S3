# stele.config — Configuración del marco

> Fuente única de la configuración de ESTE proyecto. La editas a mano o con el ritual `config`
> (`.stele/SKILL.md`). El **auto-arranque** y el **mapa de documentación** se GENERAN de aquí —
> no los edites por separado. Todo lo accionable va en **tablas**; la prosa solo explica.
>
> **Contrato de parseo:** los headers `##` son secciones canónicas y fijas, en este orden:
> `Meta` · `Rutas` · `Nombres` · `Features` · `Presupuestos` · `Wording de rituales`. Se
> referencian por posición, no por su texto. En cada tabla, col1 = clave, col2 = valor; columnas
> y filas extra se ignoran. `—` en un nombre = rol desactivado. Fila ausente = default del
> rol/feature (ver `.stele/core/roles.md` y `.stele/modules/<mód>/`). Al aplicar un cambio,
> `config` reescribe la tabla afectada **completa** y regenera los derivados.

## Meta

| Parámetro | Valor |
| --- | --- |
| idioma | es |
| módulos | software |
| persistencia | git |
| persistencia_cmd | — |
| kit_origen | `https://github.com/emezav/stele` |
| remitente | — |
| remitente_publico | — |

## Rutas

| Ruta | Valor | Qué es |
| --- | --- | --- |
| kit | .stele | El marco vendorizado (`SKILL.md`, `guide.md`, `core/`, `modules/`). |
| base | bitacora | Raíz de los docs instanciados (layout `agrupado`). |
| loader | CLAUDE.md | Loader de auto-arranque, siempre en la raíz. GENERADO. |

## Nombres (rol → archivo)

| Rol | Archivo | Origen |
| --- | --- | --- |
| entry | AGENTS.md | núcleo |
| charter | design.md | núcleo |
| protocol | protocol.md | núcleo |
| state | latest.md | núcleo |
| handover | handover.md | núcleo |
| index | index.md | núcleo |
| history_dir | history/ | núcleo |
| session | sesion-{NNN}-{YYYY-MM-DD}.md | núcleo |
| audit | audit.md | núcleo |
| correspondence | cartas.md | núcleo |
| letter | carta-{NNN}-{YYYY-MM-DD}.md | núcleo |
| correspondence_dir | correspondencia/ | núcleo |
| artifacts_dir | artefactos/ | núcleo |
| gotchas | memory.md | software |
| specs | requirements.md | software |
| specs_dir | temas/ | software |
| architecture | architecture.md | software |
| effort | esfuerzo.md | software |

## Features (toggles)

| Feature | Valor |
| --- | --- |
| effort_log | on |
| effort_unit | horas-ingeniero |
| session_greeting | on |
| audit_log | on |
| audit_every_n_sessions | 10 |
| correspondence_log | on |

## Presupuestos

| Doc (rol) | Máx. líneas |
| --- | --- |
| state | 100 |
| handover | 50 |

## Wording de rituales

| Ritual | Parámetro | Texto |
| --- | --- | --- |
| checkpoint | trigger | antes de la primera edición de código |
| abrir | saludo | 1-3 líneas: última sesión + estado handover + próximo paso |
