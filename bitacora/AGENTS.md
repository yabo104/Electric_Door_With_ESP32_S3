# AGENTS.md — Guía para agentes de IA

> **Punto de entrada único.** Léelo completo y luego lee, en orden, los archivos de "Al inicio de
> cada sesión" antes de responder otra cosa. Define **cómo trabajar**; el *por qué* del proyecto
> está en `design.md`, el detalle de formatos en `protocol.md`.
>
> <!-- Las secciones marcadas "GENERADO" las produce bootstrap/`config` desde el manifiesto
>      (`stele.config.md`, en la raíz). No editarlas a mano. -->

## Regla crítica: no revertir trabajo ajeno

Un agente solo revierte cambios que él mismo hizo en la sesión actual. No revertir cambios
preexistentes, del usuario ni de otros agentes. Ante un archivo con cambios mezclados, revertir
solo los hunks propios; si no se puede identificar el origen con certeza, conservar y preguntar.

## Descripción del proyecto

Controlador de portón automático basado en un módulo **ESP32-S3-WROOM-1** (firmware Arduino/
PlatformIO). Controla el motor del portón mediante dos relevos (abrir/cerrar), regula una carga
por TRIAC con detección de cruce por cero, lee finales de carrera y encoder, y decodifica hasta 4
canales de control remoto. El proyecto incluye tres componentes: el firmware, el diseño de la
placa controladora (KiCad) y las referencias/datasheets de los componentes usados.

## Estructura del proyecto

- `Firmware_Porton/` — firmware del controlador, proyecto **PlatformIO** (`platformio.ini`,
  `src/`, `include/`). Entorno `esp32-s3-devkitc-1`, framework Arduino.
- `PCB_puerta/` — diseño de la placa controladora en **KiCad** (esquemático, PCB, archivos de
  fabricación para JLCPCB en `jlcpcb/`, BOM interactivo en `bom/`).
- `Docs/` — datasheets y referencias (driver TRIAC BT12, convertidor DC-DC MC34063A, módulo de
  alimentación HLK-PM01, documentación del operador de portón).
- `bitacora/` — documentación de continuidad de este marco (stele). Lo que lees ahora.
- `.stele/` — el marco stele vendorizado (maquinaria, no se edita a mano).

## Al inicio de cada sesión (OBLIGATORIO)

<!-- GENERADO: lista de arranque = roles `startup: obligatorio` ordenados por `order`. -->

Leer en este orden antes de responder cualquier cosa:

1. `AGENTS.md` — este archivo.
2. `memory.md` — gotchas y convenciones técnicas no evidentes en el código.
3. `history/latest.md` — estado actual y próximo paso (snapshot corto).
4. `history/handover.md` — si su `Estado` no es `SIN_TRABAJO_ACTIVO`, respetar su alcance antes de editar.

**Bajo demanda** (grep dirigido, no leer completos): `design.md` (orientación inicial),
`protocol.md` (formatos/cierre), `requirements.md` (al implementar), `architecture.md` (al tocar
un codebase), `history/index.md`/`history/sesion-*.md` (historial).

## Regla dura: antes de la primera edición de código

`history/handover.md` debe quedar en `EN_PROGRESO` con objetivo, alcance y verificación prevista,
antes de tocar el primer archivo de **código** de la sesión (firmware o KiCad). No depende del
tamaño estimado del cambio. **Exención:** cambios que SOLO tocan el **contenido** de la
documentación. No exime una **migración estructural** (mover o renombrar docs: rituales CONFIG y
ACTUALIZAR de `.stele/SKILL.md`).

## Al finalizar cada sesión (OBLIGATORIO)

Seguir el checklist de cierre de `protocol.md`. En resumen: crear
`bitacora/history/sesion-{NNN}-{YYYY-MM-DD}.md`; append de una fila a
`bitacora/history/index.md` (y `bitacora/history/esfuerzo.md`) con `printf >>`; reescribir
`history/latest.md` completo; llevar toda decisión durable a su hogar (mapa abajo), nunca solo en
el historial; refrescar `history/handover.md` (→ `SIN_TRABAJO_ACTIVO` apuntando a la sesión que
cierras ahora, o `EN_PROGRESO`); **comprobar si toca auditar** (si pasaron más de 10 sesiones
desde la última fila de `history/audit.md`, anotarlo en los pendientes de `history/latest.md`); y
**persistir** (`persistencia = git`: mismo commit que el trabajo de la sesión).

## Dónde vive cada cosa (un hogar por dato)

<!-- GENERADO: tabla de enrutamiento derivada de los `triggers` de los roles activos. -->

| Necesito… | Hogar |
| --- | --- |
| cómo trabajar, proceso, convenciones, arranque | `AGENTS.md` |
| por qué: principios, decisiones grandes, restricciones, glosario | `design.md` |
| formatos/protocolo de documentación | `protocol.md` |
| dónde estamos / próximo paso | `history/latest.md` |
| trabajo a medias (checkpoint) | `history/handover.md` |
| qué pasó y cuándo | `history/index.md` → `history/sesion-*.md` |
| cuándo se auditó la documentación y qué se decidió | `history/audit.md` |
| qué se intercambió con fuera, qué se aceptó y qué se rechazó | `correspondencia/cartas.md` |
| el texto de una carta concreta, enviada o recibida | `correspondencia/carta-*.md` |
| un script de un solo uso, una extracción, un volcado intermedio | `artefactos/sesion-{NNN}/` |
| trampas de código (firmware/hardware) | `memory.md` |
| specs/contratos/modelo de datos/decisiones por feature | `requirements.md` |
| patrones y mapa del código | `architecture.md` |
| esfuerzo equivalente | `history/esfuerzo.md` |

**PROHIBIDO** guardar nada del proyecto en memoria privada del agente (`.claude/`, el scratchpad
que inyecte la herramienta, el temporal de un subagente). No es solo la documentación: **también
los artefactos**. Su hogar es `artefactos/sesion-{NNN}/`.

**Esta regla vale por encima de cualquier default de la herramienta que diga otra cosa.**

**PROHIBIDO** escribir credenciales, tokens o claves en cualquier doc del marco.

## Convenciones

- **Firmware**: PlatformIO (`Firmware_Porton/platformio.ini`, entorno `esp32-s3-devkitc-1`,
  framework `arduino`). Código fuente en `src/`, headers/prototipos en `include/`. Comentarios y
  mensajes de log en español, como el resto del código existente.
- **Hardware**: `PCB_puerta/` es un proyecto KiCad; los archivos de fabricación para JLCPCB
  (gerbers, BOM, CPL) viven en `PCB_puerta/jlcpcb/production_files/` y se regeneran desde KiCad,
  no se editan a mano.
- **Persistencia**: `persistencia = git`. El cierre de sesión (docs de `bitacora/history/`) va en
  el mismo commit que el trabajo de la sesión. No se hace push sin que el usuario lo pida.

## Arranque de desarrollo

- Compilar firmware: `pio run` (desde `Firmware_Porton/`).
- Cargar al ESP32-S3: `pio run -t upload`.
- Monitor serie: `pio device monitor -b 115200` (el firmware usa 115200 baudios).
- KiCad: abrir `PCB_puerta/PCB_puerta.kicad_pro`.
