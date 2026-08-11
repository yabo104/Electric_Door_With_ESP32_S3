# Controlador de Portón — ESP32-S3

Controlador electrónico para automatizar la apertura y cierre de un portón residencial, con
firmware propio (ESP32-S3) y placa controladora diseñada a medida (KiCad).

Mueve el motor mediante dos relevos (sentido) y un TRIAC (potencia, por ángulo de fase
sincronizado al cruce por cero de la red), lee finales de carrera y un sensor de efecto Hall para
detectar movimiento/atasco, y decodifica un control remoto RF 433MHz.

## Estado actual

**Funcional en hardware real** — validado de punta a punta en un portón físico:

- Apertura y cierre con un solo botón del control remoto, con inversión de sentido y detención
  parcial (doble pulsación) para dejar pasar personas o vehículos sin abrir del todo.
- Interlock de seguridad: nunca ambos relevos activos a la vez, el motor siempre se detiene antes
  de invertir sentido.
- Detección de atasco por ausencia de pulsos del sensor de movimiento, con estado de falla y
  reset manual por un botón dedicado.
- Disparo del TRIAC por ángulo de fase, sincronizado al cruce por cero de la red — hoy en un
  **nivel de potencia fijo**, calibrado empíricamente en el portón real (ver *Limitaciones*).
- Indicador LED (encendido en movimiento, parpadeo al confirmar cada final de carrera) y un
  contador de pulsos por trayecto, pensado como base para una futura calibración de recorrido.
- Sin Wi-Fi ni Bluetooth: los dos radios se apagan explícitamente al arrancar.

### Limitaciones conocidas

- **Sin arranque/parada suaves todavía.** El disparo por ángulo de fase está implementado y
  funcionando, pero corre a potencia constante — la rampa gradual (arranque suave) quedó
  pospuesta: en las pruebas, disparar muy cerca del cruce por cero no enganchaba el TRIAC de
  forma confiable con este motor (carga inductiva), y la rampa pasaba por esa zona. La parada
  sigue siendo instantánea.
- El `timeout` de detección de atasco sigue en un valor ajustado empíricamente, no derivado
  analíticamente.
- El diseño de la placa (carpeta de KiCad) está en proceso de renombrado; los archivos de
  fabricación (gerbers/BOM) pueden no reflejar la última revisión del esquemático.

## Estructura del repositorio

- **`Firmware_Porton/`** — firmware, proyecto [PlatformIO](https://platformio.org/) (entorno
  `esp32-s3-devkitc-1`, framework Arduino).
- **`PCB_Door_Controller/`** — diseño de la placa controladora en KiCad.
- **`Docs/`** — datasheets y referencias de los componentes usados (driver TRIAC, convertidor
  DC-DC, módulo de alimentación).
- **`bitacora/`** — documentación de continuidad del proyecto (decisiones, arquitectura,
  histórico de sesiones de desarrollo) bajo el marco [stele](https://github.com/emezav/stele).

## Hardware

| Componente | Detalle |
| --- | --- |
| Módulo | ESP32-S3-WROOM-1U-N8R8 |
| Motor | AC 110V, fase partida (con capacitor de arranque) |
| Potencia | TRIAC BT138-800, disparo por ángulo de fase sincronizado a cruce por cero |
| Sentido | 2 relevos (interlock por software: nunca ambos activos) |
| Posición | 2 finales de carrera (reed switch) + sensor de efecto Hall |
| Control remoto | Receptor RF 433MHz (decodificado externamente, el ESP32-S3 lee canales digitales) |

## Arranque de desarrollo

```bash
cd Firmware_Porton
pio run                              # compilar
pio run -t upload --upload-port COMx # cargar a la placa
pio device monitor -p COMx -b 115200 # monitor serie (o el USB CDC nativo del ESP32-S3)
```

## Licencia

Sin definir todavía.
