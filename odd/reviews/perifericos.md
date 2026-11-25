# Revisión: periféricos, energía e interfaces (HW)

Estado: terminado

Resumen: 2 hallazgos, los dos de severidad baja (invariante "identificar antes de escribir" de `AGENTS.md`, código
heredado de upstream). No he encontrado fallos críticos, altos ni medios en el área. El código propio del fork
(despertar por batería, `AutoShutdownCli.h`, dashboard, `HeapStats.h`, `RxStats.h`) ha resistido las pruebas de
refutación que se describen al final.

## Hallazgos

### HW-1. `oled_sleep()` escribe `0xAE` a 0x3D de `i2c0` en cualquier placa sin identificar el dispositivo
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: sí (anotado de pasada en `roles.md`, sin ficha)
- Ubicación: `zephcore/helpers/oled_power.c:19-38`; llamadas en `zephcore/src/main_companion.cpp:996`,
  `zephcore/src/server_main_common.cpp:708`, `zephcore/src/main_kiss_modem.cpp:195`
- Causa: la única condición es `CONFIG_I2C` y `i2c0` en `okay`. La dirección 0x3D está fija en el código ("Wio Tracker
  L1 SH1106"); no se mira si el devicetree tiene un display en esa dirección ni se identifica el chip antes de escribir.
  Viola dos reglas de `AGENTS.md`: "Never write to a peripheral that was not cleanly identified first" y "Gate on the
  capability" (aquí el gating es por el nombre de una placa en un comentario).
- Escenario de fallo: cualquier build sin `CONFIG_ZEPHCORE_UI_DISPLAY` en una placa con `i2c0` (repetidores headless,
  KISS, companion sin pantalla) manda `{0x00, 0xAE}` a 0x3D. Hoy ningún devicetree del árbol pone otro chip en 0x3D
  (solo el SH1106 de la Wio Tracker L1 y L1 1W), así que el daño es teórico; un módulo I2C añadido por el usuario o una
  placa nueva con un dispositivo en 0x3D recibe una escritura a su registro 0. Al revés, una placa con el OLED en 0x3C
  (la dirección habitual del SSD1306) sin UI no se apaga y sigue consumiendo.
- Arreglo mínimo propuesto: sacar bus y dirección del nodo del display en devicetree (`I2C_DT_SPEC_GET` del nodo
  `solomon,ssd1306fb`/`sinowealth,sh1106` en `okay`) y compilar `oled_sleep()` como no-op si no existe; así la placa
  sin display no lleva código y la que lo tiene en 0x3C también se apaga.
- Cómo verificarlo: compilar un repetidor Wio Tracker L1 sin UI y uno RAK4631; en el primero el log debe decir "OLED
  display put to sleep", en el segundo no debe haber transacción I2C (analizador lógico o `i2c` shell).

### HW-2. La lista común de sensores I2C arranca drivers que escriben antes de identificar el chip; el comentario dice lo contrario
- Severidad: baja
- Certeza: confirmado (escrituras seguidas en los drivers de Zephyr); el daño en un chip concreto es sospecha
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/boards/common/sensors-i2c.dtsi:43-45,86-105` (incluido por 36 placas);
  drivers: `zephyr/drivers/sensor/bosch/bmp388/bmp388.c:463-476`, `ti/ina219/ina219.c:263`,
  `sensirion/sht4x/sht4x.c:191`, `sensirion/shtcx/shtcx.c:203-215`
- Causa: el dtsi declara, sin `zephyr,deferred-init`, nodos candidatos en direcciones que comparten otros chips, y
  cada driver corre su init al arrancar. El comentario afirma que "each driver checks its chip ID, so only the part
  actually fitted comes up", pero `bmp388_init` escribe `0xB6` en el registro `0x7E` (soft reset) antes de leer el
  chip ID; `ina219_init` escribe `RST` (0x8000) en el registro 0 de 0x40 sin identificar nada; `sht4x_init` manda
  `0x94` a 0x44 y `shtcx_init` manda `0x3517` y `0x805D` a 0x70 antes de leer su ID. Va contra "Never write to a
  peripheral that was not cleanly identified first" de `AGENTS.md`.
- Escenario de fallo: una placa con la lista común y un BME280/BME680/SPA06 en 0x77 (la dirección por defecto de las
  placas de Adafruit, y justo la que `bme280_alt` existe para cubrir) recibe la escritura del reset del BMP388 en un
  registro no documentado. La T-Echo ya borra `&bmp388` por este choque (`lilygo_techo.dts:316-320`), pero el resto de
  placas no. Un INA226/INA3221 en 0x40 se resetea con el init del INA219; un mux TCA9548A o un HT16K33 en 0x70 recibe
  los comandos del SHTC3. No he encontrado un chip del árbol que se estropee con eso: el riesgo es de módulos externos.
- Arreglo mínimo propuesto: quitar `bmp388` de la lista común (comparte 0x77 con `bme280_alt`, como ya hizo la
  T-Echo) o, si se quiere mantener, parchear su init para leer el chip ID antes del reset (un parche por fichero de
  Zephyr). Corregir el comentario del dtsi para que no prometa una identificación que los drivers no hacen.
- Cómo verificarlo: placa con la lista común y un BME280 en 0x77 con analizador lógico en el bus: hoy aparece la
  escritura `0x7E <- 0xB6` a 0x77 durante el arranque; tras el cambio, solo lecturas de ID.

## Comprobado y descartado (no son hallazgos)

- Despertar por batería (`zephyr_poweroff.c`, ADR 0017, Zephyr `comparator_nrf_lpcomp.c` y `nrfx_lpcomp.c`): el
  apagado de usuario suspende el LPCOMP (`nrfx_lpcomp_stop` hace `DISABLE` y `STOP`), así que no despierta; el de
  batería baja lo deja arrancado con `DETECT_UP`. Los dos DTS llevan `enable-hyst`, así que no hay tormenta de
  interrupciones con la batería cerca del umbral durante el uso normal. Con el LDO en caída, la entrada queda por
  debajo de la referencia al apagar (ProMicro: el cruce solo ocurre con VDD regulado), coherente con lo medido. La
  GAT562 baja el umbral por defecto a 3000 mV (`board.conf:38`), así que el `static_assert` del techo (3080 mV) pasa.
  Ningún otro camino llega a System OFF sin pasar por `power_off()`. Los servidores sin umbral quedan `idle()` (hooks
  vacíos en `server_main_common.cpp:719`), sin lecturas cada 30 s, como dice el ADR. `z.autoshutdown` responde "not
  available" en placas sin despertar. La prueba en hardware de la memoria (un `poweroff` de usuario que no despierte
  al cruzar ~3,7 V) sigue pendiente; el código la debería pasar.
- `autoshutdown::parseMv`: `strtol` con desbordamiento da `LONG_MAX` y cae en `OUT_OF_RANGE`; negativo imposible
  (exige dígito inicial).
- Curvas de batería: las 9 tablas son estrictamente decrecientes y `num_points` coincide con el array, así que
  `battery_curve_lookup` nunca divide por cero.
- Checksums NMEA y UBX de configuración y sueño del GPS: todos correctos (`work-perifericos/cks.py`).
- Dashboard: `utf8_to_display` se para en el NUL aunque el nombre se corte a mitad de una secuencia UTF-8; los nombres
  se truncan con `%.*s` y el resto de `struct ui_dash` está a cero, así que una copia rota por la carrera
  `ui_set_dashboard` (hilo principal) / render (system workqueue) sigue terminada en NUL; `cfb_draw_point` recorta
  fuera de pantalla; el glifo de 6x8 cubre 32-255. `mc_display_invert_rect` no entra en el hash de EPD, pero hoy solo
  lo usa el dashboard en posiciones fijas y ninguna placa de joystick tiene EPD.
- `RxStats.h`: `formatHops`/`formatRelays` no escriben fuera de `cap` aunque `snprintf` trunque.
- `telemetry_text()` (LPP remoto, entrada no confiable): avance acotado por `len`, tipo desconocido no se queda en
  bucle, salida acotada.
- GPS: acciones pendientes tras `gps_enable(false)` están guardadas por estado en `gps_process_event`; el cerrojo de
  sueño del GPS y el de la pantalla están equilibrados (bool de borde); la UART del GPS se suspende en standby/off
  (`CONFIG_PM_DEVICE=y` global).
- `fatal_reboot.c`: registro con magia y checksum, se consume una vez; sin escrituras fuera de `s_record`.
- SPA06: identifica el chip antes de escribir y sus coeficientes y compensación coinciden con el datasheet (8x, kP
  7864320).

## Cobertura

- A fondo: `zephyr_poweroff.c/.h`, `AutoShutdownCli.h`, `PowerPolicy.cpp` (solo lo que toca al despertar; el resto lo
  cubre `roles.md`), `ZephyrBoard.cpp` (batería, alimentación externa, reinicios, LED de actividad), `oled_power.c`,
  `battery_curve.c` y curvas de placa, `ui_dash.h`, `ui_dash_fmt.h`, render del dashboard en `ui_pages.c`,
  `ui_set_dashboard`/`dash_refresh_handler` en `ui_task.c`, relleno en `ui_mesh_actions.cpp` y `CompanionMesh*.cpp`,
  `utf8_to_display`, `mc_display_text_px`/hash EPD en `display.c`, `HeapStats.h`, `RxStats.h`, `LoopWakeStats.h`,
  `telemetry_text.cpp`, `fatal_reboot.c`, `boot_info.c`, `ZephyrEnvSensors.cpp`, `sensors-i2c.dtsi`, `spa06.c`,
  `t1000e_analog.c` (rail y lectura), `pm_sleep_guard.h` y todos los `zc_pm_block/unblock_sleep`, `led_gate.c`,
  `input_multi_tap.c`.
- Por encima: `ZephyrGPSManager.cpp` (máquina de estados, callbacks GNSS, intervalos), `gps_module_cfg.cpp` (orden de
  comandos; se mandan PMTK/PCAS/UBX a ciegas a cualquier módulo de una placa con `gnss-nmea-generic`, por diseño
  documentado; incluye `UBX-CFG-CFG` en cada arranque y `CFG-PRT` a 9600, que en el u-blox M10 de la T-Impulse Plus a
  38400 no debería aplicarse porque el M10 no admite los CFG heredados, sin verificar), `gps_power.cpp` (solo apagado
  de cierre y UART PM), `pm_esp32_wake.c`, `pm_esp32_console.c`, `power_diag.c` (fragmento de banco sin release: los
  comandos 200/201 manejan pines fijos de QSPI y restauran todo `P0.OUT` de golpe; ningún `zephcore.yml` ni el paso
  "compile-only" de `build.yml` construye `power_diag.conf`), `qspi_probe.c` (solo `bringup.conf` de la MeshTracker
  X1), `ui_common.c` (latido LED), joystick (`joystick_display.h`, copias de texto en `joystick_ui_task.cpp`).
- No revisado: `display.c` fuera de lo citado (init, TFT a color, auto-apagado), `display_mono_tft.c`, `doom_game.c`,
  `buzzer.c` (parser RTTTL; solo recibe melodías constantes), `haptic.c`, `time_sync.c`, `buzzer_gate.c`,
  `PacketLog.h`, `ZephyrSensorManager.cpp` más allá de los ajustes, pantallas de `ui-joystick/screens/`,
  `ui-button/ui_task.c` fuera del dashboard y el apagado. Nada verificado en hardware ni compilado.
