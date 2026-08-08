# handover.md

## Estado

SIN_TRABAJO_ACTIVO

## Última sesión cerrada

Sesión `4` (`2026-08-08`) — Interlock de relevos, máquina de estados no bloqueante, detección de
atasco por encoder y USB CDC nativo. Ver `sesion-004-2026-08-08.md` en `bitacora/history/`.
No hay trabajo a medias, pero queda una confirmación pendiente del usuario (lógica abrir/cerrar
del control remoto, ver `latest.md`) antes de dar la funcionalidad por cerrada.

<!-- =========================================================================
     Cuando arranca un cambio interrumpible, SOBRESCRIBIR con la forma EN_PROGRESO:

## Estado
EN_PROGRESO

## Salto actual
Objetivo en una frase + decisiones ya tomadas que el siguiente agente debe respetar.

## Alcance permitido / No tocar
- permitido: <archivo/dir>
- no tocar: <archivo/dir fuera de alcance>

## Trampas de este salto
- Lo que sabes que puede salir mal en LO QUE VAS A HACER, no trampas generales del proyecto — esas
  viven en su hogar. Aquí van las que dispararían en las próximas horas.
- Es el sitio donde una advertencia llega a tiempo: un doc que se lee al arrancar informa; esto
  detiene. (Ver `.stele/SKILL.md` → la regla dura del checkpoint.)

## Estado intermedio
- Qué quedó a medias (p. ej. "X hecho, Y pendiente -> inconsistente hasta Y").
- Qué está sin persistir (sin commitear, si `persistencia = git`).

## Pendiente inmediato (en orden)
- Paso 1 concreto para retomar...

## Si fui interrumpido
Retomar desde: ...   No repetir: ...
========================================================================= -->
