# design.md — Decisiones y principios generales del proyecto

> **Por qué `Controlador_Porton` es como es**, a gran escala. Estable, se lee para orientarse y
> rara vez cambia. Frontera: aquí van *principios y apuestas grandes transversales*; las *specs y
> decisiones por feature* van en `requirements.md`; los *patrones de código* en
> `architecture.md`; las *trampas de código* en `memory.md`. Tope objetivo ~200 líneas: si una
> decisión crece, se extrae a su tema de `requirements.md` y aquí queda 1-3 líneas + link.

## Norte

Controlador electrónico para automatizar la apertura y cierre de un portón, con su propio
firmware y placa controladora diseñada a medida.

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

(Repetir por decisión. Las que dejan de ser relevantes se podan; su rastro queda en el historial.)

## Glosario

- **Portón** — la puerta/reja automatizada que controla este sistema.
- **FC_OPEN / FC_CLOSE** — finales de carrera (reed switch) que indican portón totalmente abierto
  / cerrado.
- **D0-D3** — canales ya decodificados del receptor RF 433MHz de los controles remotos.
- **TRIGGER** — señal de disparo del TRIAC. Diseño final: sincronizada con `ZCROSS` (cruce por
  cero de la AC) para regular potencia por ángulo de fase. **Hoy (provisional):** sin `ZCROSS`,
  todo/nada. El TRIAC regula la **potencia** entregada al motor; no decide el sentido de giro.
- **RELAY_OP / RELAY_CL** — relevos que eligen el **sentido** de giro del motor (qué terminal de
  control queda energizado), no la potencia.
- **Motor de fase partida (110VAC)** — motor con un terminal común y dos terminales de control,
  con un capacitor (33µF/400V) entre ambos que produce el desfase necesario para girar en un
  sentido u otro según cuál terminal se energiza.
- **ENCA / ENCB** — canales en cuadratura del encoder del motor. **Hoy solo se usa `ENCA`**, como
  pulso de "el motor se está moviendo" (detección de atasco); no se decodifica dirección/posición
  todavía.
- ADAPTAR: agregar términos de dominio adicionales según surjan (p. ej. nombre/modelo del
  operador de portón, protocolo del control remoto).

## Mapa de documentación

Dónde vive cada tipo de información: ver `AGENTS.md` → "Dónde vive cada cosa" (no duplicar aquí).
