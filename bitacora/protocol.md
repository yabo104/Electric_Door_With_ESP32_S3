# protocol.md — Protocolo de documentación entre sesiones

> **Cómo** se documenta (formatos, convenciones de edición, topes de tamaño) para que ningún
> archivo de estado crezca sin límite y para minimizar tokens/roundtrips de cualquier agente.
> Los rituales condensados están en `.stele/SKILL.md`; el *por qué*, en `.stele/guide.md`.
> Los nombres de archivo abajo son los del manifiesto (`stele.config.md`, en la raíz).

## Principios

1. Los archivos de **estado** no crecen: se **sobrescriben** (formato fijo + tope).
2. El **historial** vive en archivos por sesión y **no se reabre** (se referencia por link).
3. Los **apéndices de una línea** usan `printf '...' >> archivo`, no `Read`+`Edit`.
4. Ninguna sección de un doc fuente-de-verdad supera **~150-200 líneas**; si crece, se extrae a
   un tema de `requirements.md` + link.
5. Nada de esto vive en memoria privada del agente; todo en el proyecto. **Tampoco los
   artefactos** —scripts de un solo uso, extracciones, volcados—: su hogar es
   `bitacora/artefactos/sesion-{NNN}/`, y esa regla vale por encima de cualquier default del
   harness (`.stele/SKILL.md` → Precedencia).
6. **Un hogar por dato** (mapa en `.stele/SKILL.md` y en `AGENTS.md`).

## Rutas: comando contra enlace

Dos clases, y no se resuelven igual:

- **Ruta de comando** (`printf '...' >> …`, `grep`, `git log --`): siempre **desde la raíz del
  proyecto**, incluyendo `bitacora/history/` delante del nombre del archivo (p. ej.
  `bitacora/history/index.md`).
- **Enlace Markdown clicable** (`[index.md](./index.md)`): relativo **al archivo que lo
  contiene**, que es como lo resuelve cualquier visor.

El `printf >>` del cierre es el que más vigilar: si la ruta está mal, **no da error** — crea el
archivo que falta y el bueno se queda sin la fila.

## Archivos y su rol

Ver `.stele/guide.md` → "Roles y fronteras". Aquí solo los **formatos**.

### `latest.md` — formato fijo, se SOBREESCRIBE (~100 líneas)

```markdown
# Estado actual
> Última sesión: Sesión N (YYYY-MM-DD) — ver sesion-{NNN}-{YYYY-MM-DD}.md
> Índice completo: index.md

## Dónde estamos
- (3-8 bullets del estado REAL, no histórico)
## Próximo paso inmediato
- (lo que haría la siguiente sesión; reemplaza, no acumula)
## Pendientes operativos
- Procesos en background / decisiones abiertas / trabajo sin persistir
## Referencias
- requirements.md §X — … / design.md § … / tema de requirements.md
```

Al cerrar: reescribir COMPLETO con `Write`. Nunca `Edit` para prepend/rename de "anterior".
En *Pendientes operativos* no anotes "push pendiente" por el commit que lleva este mismo cierre.

### `index.md` — tabla append-only

`| Sesión | Fecha | Resumen | Archivo |`. Al cerrar:
`printf '| N | YYYY-MM-DD | resumen | <session> |\n' >> bitacora/history/index.md`.

### `esfuerzo.md` — tabla append-only (OPCIONAL, feature `effort_log`)

`| Sesión | Fecha | horas-ingeniero | Funcionalidades clave |`. Estimar lo que le tomaría a UN
ingeniero senior el mismo trabajo con calidad de producción (investigación + implementación +
validación + docs), en rango. Detalle por funcionalidad en el `sesion-{NNN}-{YYYY-MM-DD}.md`.
Append: `printf '...' >> bitacora/history/esfuerzo.md`.

### `audit.md` — tabla append-only (OPCIONAL, feature `audit_log`)

`| Audit | Fecha | Sesiones | Alcance | Comprobadas | Hallazgos | Desenlace |`. Una fila por
auditoría (ritual AUDITAR). **Lo crea la primera auditoría, no el bootstrap.**

### `sesion-{NNN}-{YYYY-MM-DD}.md` — uno por sesión

Detalle completo: qué se hizo, decisiones, archivos tocados, verificación, notas para retomar,
y `## Esfuerzo equivalente`. `NNN` con padding a 3 dígitos. No se reabre; se lee con grep.

**No lleva su propio hash de commit.** El cierre viaja en el mismo commit que el trabajo, y un
commit no puede contener su propio hash. Para recuperarlo:
`git log --diff-filter=A -- bitacora/history/<archivo de sesión>`.

### `cartas.md` + `carta-{NNN}-{YYYY-MM-DD}.md` — la correspondencia (OPCIONAL, `correspondence_log`)

Misma forma que el historial: un índice (`cartas.md`), un archivo por carta
(`carta-{NNN}-{YYYY-MM-DD}.md`, en `bitacora/correspondencia/`) y su carpeta. Lo crea la primera
carta, no el bootstrap. Índice: `| # | Fecha | Dir | Corresponsal | Asunto | Desenlace |`, con
`Dir` = `->` sale · `<-` entra.

### `bitacora/artefactos/` — artefactos por sesión (se crea con el primero, no en bootstrap)

Un subdirectorio por sesión: `bitacora/artefactos/sesion-{NNN}/`. Dentro va lo que la sesión
produjo y **no es documentación**: scripts de un solo uso, extracciones, volcados intermedios.
Con `persistencia = git` no se versiona por defecto (ver `.gitignore`), salvo lo que el
`sesion-{NNN}-{YYYY-MM-DD}.md` marque como sostén de un cambio irreversible.

**El agente nunca limpia por su cuenta.** Borrar es decisión del usuario, siempre.

### `handover.md` — checkpoint de trabajo en curso (~50 líneas)

Estados: `SIN_TRABAJO_ACTIVO` | `EN_PROGRESO` | `COMPLETADO`. **Regla dura:** antes de la primera
edición de código, `EN_PROGRESO` con objetivo/alcance/verificación. Al cerrar, siempre refrescar
el puntero a la sesión que se cierra AHORA. Plantilla en `.stele/core/templates/handover.md`.

### `requirements.md` (+ temas)

Una sección por decisión o feature, **autocontenida**. Se lee con `grep` y por rango, nunca
entera.

**Umbral de extracción: ~50 líneas.** Por debajo, la decisión vive aquí. Por encima, se mueve a
`bitacora/temas/<TEMA>.md` y aquí queda un resumen de 2-3 líneas + link; si un tema supera
~600-800 líneas, se divide en sub-temas. `bitacora/temas/` se crea **cuando la primera decisión
cruce el umbral**, no en el scaffold.

### `memory.md`

Hogar único de gotchas de código (firmware y hardware). Se edita incrementalmente pero se
**cura** (se poda lo obsoleto). Una sección de subsistema que supera ~150-200 líneas se extrae a
un tema de `requirements.md`.

## Acuerdos de auditoría

Lo que el ritual AUDITAR señaló y el usuario decidió **no** cambiar. Se registra aquí para no
rediscutirlo en cada auditoría, y **siempre con umbral**.

| Fecha | Doc | Acuerdo | Umbral de revisión |
| --- | --- | --- | --- |

Un **tope de tamaño** de un rol no va aquí: es un **presupuesto** y su hogar es el manifiesto
(`stele.config.md`, sección Presupuestos, con el ritual `config`).

## Convenciones de texto (y dónde se escapan)

Este proyecto no impone una convención adicional de texto (más allá de escribir en español, como
el resto del código y la documentación). Si eso cambia, la regla vive en `AGENTS.md` →
Convenciones, y aquí solo se documenta **cómo se comprueba**.

## Checklist de inicio / cierre

Condensados en `.stele/SKILL.md` (rituales ABRIR / CERRAR). Este archivo es la referencia de
formato cuando haya dudas.

## Operaciones de bajo coste (preferir)

- Apéndice de fila → `printf '...' >> archivo`.
- Archivo pequeño de formato fijo → un `Write`.
- Buscar en archivo grande → `grep -n` + lectura por rango.
- Si vas a EDITAR, léelo con el tool `Read` (no `cat`/`sed`) o el `Edit` se bloquea.
- Volumen mecánico grande → delegar a un subagente.
