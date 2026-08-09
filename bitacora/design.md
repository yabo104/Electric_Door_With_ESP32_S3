# design.md — Decisiones y principios generales del proyecto

> **Por qué `Controlador_Porton` es como es**, a gran escala. Estable, se lee para orientarse y
> rara vez cambia. Frontera: aquí van *principios y apuestas grandes transversales*; las *specs y
> decisiones por feature* van en `requirements.md`; los *patrones de código* en
> `architecture.md`; las *trampas de código* en `memory.md`. Tope objetivo ~200 líneas: si una
> decisión crece, se extrae a su tema de `requirements.md` y aquí queda 1-3 líneas + link.

## Norte

Controlador electrónico para automatizar la apertura y cierre de un portón, con su propio
firmware y placa controladora diseñada a medida. A futuro, este proyecto forma parte de un pool
de proyectos open-source del usuario, pensados como referencia de trabajos realizados.

## Principios de diseño (no negociables)

- **Un hogar por dato** — cada hecho vive en un solo documento; los demás apuntan.
- **Fail-safe en las salidas de potencia** — relevos y disparo de TRIAC arrancan en `HIGH`
  (desactivados) en `setup()`; ADAPTAR si esto deja de ser cierto al cambiar el hardware.
- ADAPTAR: agregar aquí otros tenets durables (p. ej. tiempos de seguridad, comportamiento ante
  falla de sensor, requisitos normativos si aplica) a medida que se decidan.

## Restricciones y no-negociables

- **Sin Wi-Fi ni Bluetooth por ahora.** El ESP32-S3 los tiene disponibles pero no se usan todavía.
  A futuro: Bluetooth para controlar el portón desde una app móvil (no hay diseño de ese
  protocolo todavía — cuando se aborde, documentar en `requirements.md`).
- **Idioma de la documentación (decisión del usuario, sesión 005):** durante el desarrollo activo
  se mantiene en español (es lo que ya hay y lo que configura `stele.config.md`). **Cuando el
  proyecto se dé por terminado**, se traduce todo a inglés, porque el destino es un pool de
  proyectos open-source de referencia. No es un cambio a aplicar ahora — es una tarea de cierre
  de proyecto (traducir `bitacora/`, comentarios de código, `README` si se agrega uno, etc.),
  a recordar cuando se llegue a esa etapa.
- ADAPTAR: agregar otros límites (normativos, presupuesto, requisitos de seguridad eléctrica al
  operar TRIAC/relevos de línea) a medida que se decidan.

## Decisiones estructurales (ADR-lite)

> Decisiones grandes y transversales. Formato corto y fechado; el detalle va a `requirements.md`.

### 2026-08-07 — Firmware migrado de sketch `.ino` a proyecto PlatformIO

- **Contexto:** el firmware vivía como un único `Firmware_Porton.ino` (Arduino IDE), sin
  separación entre inicialización, lógica de puerta y manejo de comandos.
- **Decisión:** reestructurar `Firmware_Porton/` como proyecto PlatformIO (`platformio.ini`,
  `src/main.cpp`, `src/porton.cpp`, `include/porton.h`), entorno `esp32-s3-devkitc-1`.
- **Alternativas descartadas:** mantener el `.ino` (Arduino IDE) — descartado porque el proyecto
  pasa a usar PlatformIO como herramienta de build/flash a partir de ahora.
- **Consecuencias / detalle:** ver `architecture.md`.

### 2026-08-08 — Máquina de estados del portón: interlock de seguridad + no bloqueante

- **Contexto:** el firmware original (plantilla de pruebas) no garantizaba por diseño que los dos
  relevos nunca estuvieran activos juntos, invertía sentido con `delay()` bloqueantes, y no tenía
  forma de detectar un atasco del motor.
- **Decisión:** interlock de relevos centralizado en una sola función (`seleccionarSentido()`,
  ver `architecture.md`), máquina de estados no bloqueante basada en `millis()`, y detección de
  atasco por timeout de pulsos del encoder (`ENCA` por interrupción). El TRIAC se trata como
  interruptor todo/nada (sin `ZCROSS`) hasta una sesión futura. Detalle en `requirements.md` §3/§4.
- **Alternativas descartadas:** mantener el diseño con `while()`/`delay()` bloqueantes — se
  descartó porque impide medir timeouts (atasco) y responder a `Serial` mientras la puerta se
  mueve.
- **Consecuencias / detalle:** ver `architecture.md` → Patrones reutilizables y Brecha conocida.

### 2026-08-08 — Validación en hardware real: control remoto D0/D2 y recuperación de ERROR

- **Contexto:** primera sesión con la placa física conectada. Se confirmó en hardware que abrir/
  cerrar, finales de carrera, interlock de relevos, TRIAC a máxima potencia y detección de atasco
  funcionan. Aparecieron dos problemas de hardware (no de firmware): `ENCA`/`ENCB` con pull-ups
  defectuosos, y el conteo de `ZCROSS` con ruido de umbral (parcialmente corregido por software).
  El usuario pidió además: separar el botón de operar del de reset de falla, una forma de detener
  el portón a medio camino (apertura parcial para personas/motos), y una política de salida de
  `ERROR`.
- **Decisión:** `D0` concentra todo el control normal (abrir/cerrar/invertir/detener parcial);
  `D2` es un botón dedicado, exclusivo, para salir de `ERROR` — nunca reintento automático tras
  un atasco (mismo criterio que UL 325 para fallas de obstrucción: la salida de una falla es una
  acción humana deliberada). Detección de atasco migrada a `ENCA2` (sensor Hall en `GPIO44`),
  confirmada limpia en banco, en paralelo con `ENCA` por si se repara. Detalle completo en
  `requirements.md` §3/§4, patrones en `architecture.md`.
- **Alternativas descartadas:** pulsación larga del mismo botón (`D0`) para resetear `ERROR` — se
  descartó por un botón dedicado (`D2`): más simple de implementar y evita que una pulsación
  normal saque de `ERROR` por accidente. Reversa automática alejándose de la obstrucción al
  entrar en `ERROR` — se descartó por ahora: sin sensor de obstrucción dedicado, no hay forma de
  saber hacia dónde es seguro moverse.
- **Consecuencias / detalle:** ver `requirements.md` §3/§4 y `architecture.md`.

### 2026-08-08 — Arranque suave del TRIAC: alcance acotado a solo arranque

- **Contexto:** el portón toma velocidad y golpea fuerte contra el tope al cerrar, con riesgo
  para los engranajes. El usuario pidió arranque y parada suaves por ángulo de fase del TRIAC.
  Antes de tocar código se armó un plan (vía plan mode) y se lo discutió con el usuario.
- **Decisión:** acotar esta ronda a **solo arranque suave**, para validar primero si el mecanismo
  de disparo por fase regula bien la potencia real (confirmado el TRIAC: `BT138-800`, pulso de
  gate de 200us). La parada sigue siendo instantánea. Parada suave con zona de desaceleración
  necesita conteo de pulsos de recorrido completo y un modo de calibración — el usuario lo va a
  especificar en una sesión futura, no se improvisa ahora. Detalle en `requirements.md` §5.
- **Alternativas descartadas:** implementar arranque y parada juntos en la misma ronda — se
  descartó porque la parada depende de una pieza no diseñada todavía (calibración de recorrido
  por pulsos de `ENCA2`, con un solo canal de encoder — no hay dirección, solo conteo).
- **Estado al cierre de la sesión 007:** implementado y subido a la placa, pero **sin confirmar
  que el arranque suave se perciba en el motor real** — el usuario no notó diferencia en la
  primera prueba. Pendiente diagnóstico con osciloscopio (sesión futura) antes de dar la
  funcionalidad por buena. Ver `requirements.md` §5 y `memory.md` para el detalle técnico y las
  dos hipótesis abiertas (bug vs. limitación física del motor de inducción).
- **Consecuencias / detalle:** ver `requirements.md` §5 y `architecture.md`.

(Repetir por decisión. Las que dejan de ser relevantes se podan; su rastro queda en el historial.)

## Glosario

- **Portón** — la puerta/reja automatizada que controla este sistema.
- **FC_OPEN / FC_CLOSE** — finales de carrera (reed switch) que indican portón totalmente abierto
  / cerrado.
- **D0-D3** — canales ya decodificados del receptor RF 433MHz de los controles remotos. `D0` =
  control normal (abrir/cerrar/invertir/detener parcial); `D2` = reset de `ERROR` únicamente;
  `D1`/`D3` sin rol asignado.
- **TRIGGER** — señal de disparo del TRIAC. Diseño final: sincronizada con `ZCROSS` (cruce por
  cero de la AC) para regular potencia por ángulo de fase. **Hoy (provisional):** sin `ZCROSS`,
  todo/nada. El TRIAC regula la **potencia** entregada al motor; no decide el sentido de giro.
- **RELAY_OP / RELAY_CL** — relevos que eligen el **sentido** de giro del motor (qué terminal de
  control queda energizado), no la potencia.
- **Motor de fase partida (110VAC)** — motor con un terminal común y dos terminales de control,
  con un capacitor (33µF/400V) entre ambos que produce el desfase necesario para girar en un
  sentido u otro según cuál terminal se energiza.
- **ENCA / ENCB** — canales en cuadratura del encoder del motor. Tienen un problema real de
  hardware (pull-up del circuito de entrada) y hoy no aportan pulsos.
- **ENCA2** — sensor de efecto Hall alternativo (`GPIO44`), confirmado funcionando en banco: es
  la fuente real de detección de atasco hoy, en paralelo con `ENCA` por si se repara.
- ADAPTAR: agregar términos de dominio adicionales según surjan (p. ej. nombre/modelo del
  operador de portón, protocolo del control remoto).

## Mapa de documentación

Dónde vive cada tipo de información: ver `AGENTS.md` → "Dónde vive cada cosa" (no duplicar aquí).
