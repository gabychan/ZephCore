# Revisión área `placas` (BOARD)

Estado: terminado

Revisado hasta ahora:
- Material de la ronda anterior en `work-placas/` (workspace west sincronizado con HEAD; su matriz no llegó a correr
  y su `cfgjob.sh` partía mal las filas sin conf por el `IFS` de tabulador). Matriz nueva: `cmake` (solo
  configuración) de 44 placas x companion/repetidor (con y sin Zephtacore)/room server/observer, 267 trabajos,
  salidas en `work-placas/o2/` (`.config` y `zephyr.dts` por trabajo). Scripts: `pincheck.py`, `partcheck.py`,
  `aliascheck.py`, `buscheck.py`, `radiotab.py`, `cfgsum.sh`.
- `build.yml`, `build.sh`, `board_manifest.py`, `gen_provider_catalog.py` (variantes publicadas).
- `zephcore/patches/`: un parche por fichero, aplicación contra el Zephyr fijado, `zephyr-new/`.
- Pines, buses SPI, alias/chosen, particiones (nRF52, nRF54L, ESP32), radio (TCXO, DIO2, RF switch) por placa.
- Raíles que quedan encendidos en System OFF (`hogcheck.py`), `origin/dev` frente al fork en placas.
- Pila del hilo principal con Zephtacore (análisis estático con `-fcallgraph-info=su`, rak4631 y heltec V4).

Resumen: 1 alta, 3 medias, 2 bajas, 2 mejoras.

---

### BOARD-6. LilyGo T-Impulse Plus publicada sin configurar su cargador SGM41562: no carga la batería y en USB se reinicia cada pocos minutos
- Severidad: alta
- Certeza: probable (el fork no escribe nunca al cargador, comprobado; el comportamiento del chip es el que
  documenta upstream en su arreglo, no lo he medido)
- Origen: upstream, arreglado después de la base del fork · ¿arreglado en origin/dev?: sí (`9b820396`,
  2026-11-11) · ¿ya conocido?: no (no figura en la memoria del proyecto)
- Ubicación: `zephcore/boards/nrf52840/lilygo_timpulse_plus/` (no hay `board_init.c`; el DTS, línea 17-18, dice
  "charger ... present but NOT used by ZephCore, so that bus is left disabled"); `zephcore.yml` la publica como
  companion y repetidor Zephtacore.
- Causa: según `origin/dev:zephcore/boards/nrf52840/lilygo_timpulse_plus/board_init.c`, el SGM41562 sale del reset
  con la carga desactivada (REG01 CEB=1) y con el watchdog I2C armado a 160 s cuando hay alimentación de
  entrada; si nadie lo programa, no carga, y con USB el chip corta la alimentación del sistema cada vez que vence
  el watchdog. El fork deja ese bus I2C deshabilitado y no escribe nada en 0x03.
- Escenario de fallo: un usuario flashea el release del fork en una T-Impulse Plus nueva o descargada. La
  batería no carga por USB (solo carga si antes corrió el firmware de LilyGo, y deja de hacerlo al agotarse), y
  conectado a USB el reloj se reinicia cada ~160 s, lo que corta el BLE con la app. Las notas de 1.18.3 de
  upstream describen exactamente esto.
- Arreglo mínimo propuesto: traer `9b820396` (board_init.c, pinctrl del segundo I2C, nodo en el DTS y la línea
  de `CMakeLists.txt`) tal cual; no hay conflicto con código del fork en esa placa.
- Cómo verificarlo: en una T-Impulse Plus con la batería baja, con el firmware actual del fork: USB conectado
  20 min, `get z.info`/uptime se reinicia y el voltaje no sube; con el arreglo, uptime continuo y la batería sube.

### BOARD-2. `build.sh` ya no se para si falla el empaquetado: un release puede salir sin el `.zip`, `.uf2` o `-merged.bin` de una placa y CI en verde
- Severidad: media
- Certeza: confirmado (semántica de bash reproducida en `work-placas/errexit_demo.sh`)
- Origen: fork (`build.sh`, commit del bucle `failed+=`) · ¿arreglado en origin/dev?: no (upstream llama a `build_row` sin `||`) · ¿ya conocido?: no
- Ubicación: `build.sh:100-116` (y `package()` en `build.sh:30-80`)
- Causa: el fork llama a `build_row ... || failed+=(...)`. Bash desactiva `set -e` en todo el cuerpo de una
  función invocada a la izquierda de `||`, también en las funciones que llame. Solo el `west build` tiene su
  `|| return 1`; los `mv`, `python -m esptool merge-bin`, `dts_flash_size.py` y `cp` de `package()` pueden fallar
  sin efecto, y `package` devuelve el estado de su última orden.
- Escenario de fallo: en una placa ESP32-S3 falla `esptool merge-bin` (o `dts_flash_size.py` devuelve vacío y
  `--flash-size ""` lo rompe); el `cp` siguiente tiene éxito, `build_row` devuelve 0, `failed` queda vacío y el
  job sube `firmware/` sin `-merged.bin`. `create-release` publica, y `gen_provider_catalog.py:files_for()` omite
  en silencio los ficheros que no encuentra, así que el configurador deja de ofrecer la instalación completa de
  esa placa. En nRF, si falla el primer `mv` (`zephyr.uf2`) y el segundo no, pasa lo mismo.
  Reproducido: `bash work-placas/errexit_demo.sh` imprime el error de `mv`, "failed=0" y sale con 0.
- Arreglo mínimo propuesto: que cada orden de `package` corte con `|| return 1` (o encadenarlas con `&&`), y en
  `build_row` `package ... || return 1`. Un `( set -e; ... )` no basta: bash también ignora `set -e` dentro de un
  subshell que está a la izquierda de `||`.
- Cómo verificarlo: el script de demostración con el arreglo debe dar "failed=1"; en local, `build.sh nrf` con
  un `zephyr.zip` borrado a mano tras el build debe terminar con `::error::`.

### BOARD-7. T-Impulse Plus: el enable del GPS (P1.10) está declarado activo en alto y es activo en bajo; el GPS nunca arranca
- Severidad: media
- Certeza: probable (el valor del fork está comprobado; la polaridad la corrige upstream tras verlo en el
  hardware; los ejemplos públicos de LilyGo, `examples/GPS*/`, ni siquiera tocan `GPS_EN`, lo que encaja con un
  enable activo en bajo que en reposo deja el GPS encendido)
- Origen: upstream, arreglado después de la base · ¿arreglado en origin/dev?: sí (`eb84ba25`, 2026-11-12) · ¿ya conocido?: no
- Ubicación: `zephcore/boards/nrf52840/lilygo_timpulse_plus/lilygo_timpulse_plus.dts:151-157`
  (`gpios = <&gpio1 10 GPIO_ACTIVE_HIGH>`)
- Causa: `ZephyrGPSManager` enciende el GPS poniendo activa la línea del alias `gps-enable`; con
  `GPIO_ACTIVE_HIGH` eso es nivel alto en P1.10, que en esta placa apaga el MIA-M10Q.
- Escenario de fallo: con `gps on`, el GPS queda apagado, la sonda de arranque no ve NMEA y la app informa de
  que no hay GPS; sin posición ni hora por GPS en el repetidor (que lo usa para sincronizar hora).
- Arreglo mínimo propuesto: traer `eb84ba25` (`GPIO_ACTIVE_LOW` y la documentación de la placa).
- Cómo verificarlo: en la placa, `gps on` y esperar NMEA por UART0 a 38400; hoy no llega.

### BOARD-8. RAK4631 y RAK3401 1W dejan encendido el raíl 3V3_S durante System OFF: el apagado por batería baja no protege la celda
- Severidad: media
- Certeza: confirmado en código (el pin queda latcheado: `zephyr_poweroff.c` no lo toca); consumo según la
  medida de upstream (29,7 mA frente a 1,30 mA), no medido por mí
- Origen: upstream, arreglado después de la base · ¿arreglado en origin/dev?: sí (`4dbd6fca`, 2026-11-12) · ¿ya conocido?: no
- Ubicación: `zephcore/boards/nrf52840/rak4631/board.overlay:144-171` (hogs `sx1262_power_en` P1.05 y `v3v3s_en`
  P1.02), `zephcore/boards/nrf52840/rak3401_1watt/rak3401_1watt.dts` (regulador `v3v3s-enable` P1.02 con
  `regulator-boot-on`), `zephcore/adapters/board/zephyr_poweroff.c:106-122,175-240`
- Causa: el nRF52 conserva el estado de salida de los GPIO en System OFF. `power_off()` solo apaga los
  reguladores `sensor_power` y `buzzer_enable` y solo suelta los pines de un nodo `zephcore,poweroff-gpios`;
  estas dos placas no tienen ese nodo, así que 3V3_S (GPS del slot, sensores, OLED) sigue alimentado con el
  SoC "apagado". Comprobado de forma mecánica con `work-placas/hogcheck.py` sobre los DTS generados.
- Escenario de fallo: un RAK4631 companion con GPS RAK12500 baja a 3200 mV, el auto-apagado entra en System OFF
  para proteger la celda y el nodo sigue gastando ~30 mA: una 18650 de 3000 mAh con ~5 % restante se vacía en
  unas 5 h hasta el corte del BMS o por debajo. Lo mismo con `poweroff` desde la UI o la CLI.
- Arreglo mínimo propuesto: traer `4dbd6fca` (nodo `poweroff-gpios` con P1.05 y P1.02 en las dos placas, y la
  aclaración del binding). El mismo script lista otros raíles con hog `output-high` que no se sueltan al apagar
  (`thinknode_m3`: gps/pwr/bat/eeprom/led-power; `thinknode_m6`: gps-standby; `meshtracker_x1`: rtc-power-on;
  `gat562_30s`: P1.05) y reguladores `regulator-boot-on` que `power_regulators_off()` no apaga (`rak_wismesh_tag`,
  `lilygo_techo`, `thinknode_m1/m6`, `heltec_t1/t096`, `promicro_*`...): no sé cuáles de esos consumen de verdad;
  merece una medida por placa antes de tocarlos.
- Cómo verificarlo: amperímetro en la batería de un RAK4631 con RAK12500, `poweroff`: hoy ~30 mA, con el arreglo
  ~1,3 mA (cifras de upstream).

### BOARD-1. CI ya no compila el companion ni el repetidor sin Zephtacore en ninguna placa, ni el room server, ni nada de C6, STM32WL y Linux
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: fork · ¿arreglado en origin/dev?: n.a. · ¿ya conocido?: no (CONN-10 cubre solo observer y uplink)
- Ubicación: `zephcore/scripts/board_manifest.py:175-205`, `.github/workflows/build.yml:166-175`
- Causa: `3c4a2ff9` hizo que `matrix()` solo emita las variantes de `published_variants()` (id `zephtacore`);
  la fila base (`builds = [("", base_confs)]`) desapareció. `e87c5646` quitó la variante a `lilygo_tlora_c6`,
  `xiao_esp32c6` y `lora_e5_mini`, y `54617540` borró el job de Linux. Los pasos compile-only solo compilan
  `kiss_modem` (rak4631, xiao_esp32c3) y dos builds Zephtacore de rak4631 que ya son filas de release.
- Escenario de fallo: un cambio que rompe una rama `#if !CONFIG_ZEPHTACORE_MESHTASTIC` (o el build por defecto
  `west build -b <placa> zephcore`, que es el que documentan `README` y `docs/`), la variante `noscreen` de
  heltec_t114, cualquier placa ESP32-C6 (RISC-V), la única STM32WL o un preset Linux, pasa CI en verde y llega a
  `master`. Contradice `AGENTS.md` ("A role or fragment no `zephcore.yml` row builds goes into the compile-only
  roles step"). Tampoco compila nada el room server (`room_server.conf`, sin fila ni en upstream ni en el fork,
  aunque el fork le añadió `PowerPolicy.cpp` y los `z.*` de `CommonCLI`), ni `healthcheck.conf`,
  `packet_logging.conf`, `hangdebug.conf`, `gps_debug.conf`. Los dos pasos "compile-only zephtacore" de rak4631
  repiten dos filas que el release ya construye, y su comentario ("variant on t1000_e and wio_tracker_l1; rak4631
  covers the rest") está desfasado. Hoy compilan: lo comprobé con builds completos de rak4631 companion sin
  Zephtacore (FLASH 422164 B, RAM 216488 B) y room server (277636 B / 85284 B); el hallazgo es de cobertura, no
  de un fallo de build actual. Además `board_manifest.py docs` sigue contando `release:` como "con firmware publicado": el
  `README.md:104-109` dice 14 ESP32, 1 STM32WL y 3 Linux publicados, y el release no trae ninguno de esos 5.
- Arreglo mínimo propuesto: cambiar los dos pasos "compile-only zephtacore" de rak4631 (redundantes) por rak4631
  sin fragmento y con `room_server.conf`; añadir un build sin fragmento por familia (p. ej.
  `xiao_esp32c6`, `lora_e5_mini`, un preset Linux) y la variante `noscreen`; o que `matrix` siga emitiendo la fila
  base con una marca "no publicar" que `build.sh` compile sin empaquetar. En `summary()`/`docs()`, contar como
  publicada solo una placa con `published_variants(b)` no vacío.
- Cómo verificarlo: `python3 zephcore/scripts/board_manifest.py matrix esp32 | grep c6` y `... matrix stm32wl`
  devuelven vacío; tras el arreglo, el job compile-only debe fallar si se rompe adrede una rama no Zephtacore.

### BOARD-3. `patches/zephyr-new/` sustituye entero un fichero de Zephyr (`fake_entropy_native_sim.c`), fuera del mecanismo de parches
- Severidad: baja
- Certeza: confirmado (seguido en código y en el árbol aplicado)
- Origen: upstream (ZephCore; el fork solo cambió la licencia en `5b8245d9`) · ¿arreglado en origin/dev?: no · ¿ya conocido?: no
- Ubicación: `zephcore/patches/zephyr-new/drivers/entropy/fake_entropy_native_sim.c`, `zephcore/cmake/zephyr_new_files.cmake:40-62`
- Causa: `zephyr-new/` está pensado para ficheros que no existen en Zephyr y se copia con `configure_file(... COPYONLY)`
  sin comprobar nada. `drivers/entropy/fake_entropy_native_sim.c` sí existe en el Zephyr fijado (está versionado
  en `0534e997`) y se reemplaza entero (+70/-57 respecto al original). Es el único caso: los otros 57 ficheros de
  `zephyr-new/` no existen arriba, y ningún `.patch` crea ni borra ficheros ni comparte fichero con otro.
- Escenario de fallo: en el siguiente salto de Zephyr, si upstream cambia ese driver (opciones `--seed-random`,
  `fake_entropy_native_bottom.h`, `CONFIG_FAKE_ENTROPY_NATIVE_SIM_SEED_BY_DEFAULT`), no hay `git apply --check` que
  falle: la copia vieja pisa la nueva sin aviso y los presets Linux (que sacan de aquí la entropía de la
  identidad) compilan contra una API que ya no es la suya o pierden el arreglo de upstream. Y si un día se quita
  de `zephyr-new/`, la limpieza de `zephyr_new_files.cmake:47-57` no lo restaura porque está versionado: el árbol se
  queda con la copia modificada hasta un `git checkout` manual. Va contra la regla de `AGENTS.md` de que cada
  fichero de Zephyr lo posee un parche.
- Arreglo mínimo propuesto: convertirlo en `patches/zephyr/00xx-entropy-native-sim-urandom.patch` (diff contra el
  original) y borrarlo de `zephyr-new/`; o que `zephyr_new_files.cmake` dé `FATAL_ERROR` si el destino está
  versionado en el árbol de Zephyr.
- Cómo verificarlo: `git -C <zephyr> cat-file -e HEAD:drivers/entropy/fake_entropy_native_sim.c` devuelve 0 hoy;
  tras el arreglo, el preset Linux compila y `git apply --check` vigila el fichero.

### BOARD-4. Config que repite lo que ya da el devicetree: `ESPTOOLPY_FLASHSIZE_*` en 9 `board.conf` y su propagación a MCUboot, y `HAPTICS_DRV2605=y`
- Severidad: mejora
- Certeza: confirmado (búsqueda de consumidores en Zephyr, hal_espressif y MCUboot fijados)
- Origen: upstream · ¿arreglado en origin/dev?: no · ¿ya conocido?: no
- Ubicación: `zephcore/sysbuild/CMakeLists.txt:13-30`, `zephcore/sysbuild/mcuboot_flashsize_*.conf`,
  `sysbuild/mcuboot.conf` (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`), `board.conf` de heltec_wifi_lora32_v3/v4/v43/v4_r8,
  heltec_wireless_tracker(_v2), meshnology_w12, station_g2, thinknode_m9; `boards/nrf52840/meshtracker_x1/board.conf`
- Causa: en este Zephyr el tamaño de flash de `esptool elf2image` sale del nodo `zephyr,flash` del devicetree
  (`soc/espressif/common/CMakeLists.txt:61-63`). `CONFIG_ESPTOOLPY_FLASHSIZE_*` no tiene ningún consumidor salvo
  un `#error` para ≥32 MB en `hal_espressif/components/spi_flash/esp_flash_spi_init.c:45`. Comprobado que el
  devicetree ya da el mismo tamaño en las 9 placas (8 MB y 16 MB, `dts_flash_size.py`). `HAPTICS_DRV2605` tiene
  `default y` con `depends on DT_HAS_TI_DRV2605_ENABLED`.
- Escenario de fallo: ninguno hoy; quien cambie el tamaño en `board.conf` creerá que cambia la imagen y no cambia
  nada (lo que manda es el `reg` del flash en el DTS). Va contra "`board.conf` does not repeat what ... the
  devicetree already provides".
- Arreglo mínimo propuesto: borrar esas líneas, `mcuboot_flashsize_*.conf` y el bloque que las propaga en
  `sysbuild/CMakeLists.txt` (MCUboot ya recibe `sysbuild/mcuboot.conf` por convención de sysbuild).
- Cómo verificarlo: build de heltec_wifi_lora32_v4 con `--sysbuild` antes y después: `zephyr.signed.bin` y el
  `merged.bin` deben salir idénticos.

### BOARD-5. `ZEPHCORE_UI_JOYSTICK` se activa por nombre de placa (`BOARD_WIO_TRACKER_L1`), no por el joystick del devicetree
- Severidad: mejora
- Certeza: confirmado (seguido en Kconfig y en los `.config` de la matriz)
- Origen: upstream · ¿arreglado en origin/dev?: no · ¿ya conocido?: no
- Ubicación: `zephcore/Kconfig:1351-1354`; las otras dos placas con joystick lo hacen con `select` en su Kconfig de
  placa (`boards/nrf52840/gat562_30s/Kconfig.gat562_30s:6`, `wio_tracker_l1_1w/Kconfig.wio_tracker_l1_1w:7`)
- Causa: "In-tree Zephyr board (no board Kconfig of ours to select from)": se usa el nombre de la placa como
  sustituto de "tiene joystick". `AGENTS.md` regla 7 pide un nodo o propiedad del devicetree.
- Escenario de fallo: hoy el resultado es correcto en las tres placas con joystick (L1, L1 1W, GAT562; comprobado
  en `o2/*/zephcore.config`). Una placa nueva con joystick, o una revisión del L1 con otro nombre de board en
  Zephyr, no lo recibe hasta que alguien añada su nombre o un `select`.
- Arreglo mínimo propuesto: `default y if $(dt_nodelabel_enabled,joystick_up)` (o un compatible/alias propio para
  el joystick) y quitar los `select` de placa.
- Cómo verificarlo: la matriz de configuración debe seguir dando `ZEPHCORE_UI_JOYSTICK=y` exactamente en esas tres.

## Alcance

A fondo:
- Diff del fork en el área: `build.yml`, `build.sh`, `board_manifest.py`, `gen_provider_catalog.py` (variantes),
  `Kconfig` (Zephtacore, battery wake, auto-apagado), `CMakeLists.txt` (fuentes MT, recorte de heap WiFi,
  `zephcore,companion-uart`), fragmentos `wifi_companion_*`, `zephtacore.conf`, `zephcore_common.conf`, DTS de
  gat562_30s y promicro_sx1262 (LPCOMP), `thinknode_m9/board.overlay`, todos los `zephcore.yml`.
- Comprobaciones mecánicas sobre los `zephyr.dts`/`.config` generados de 38 placas (todas las nRF52840, nRF54L y
  ESP32 Xtensa): pines repetidos entre nodos activos (gpios, pinctrl, AIN de SAADC y LPCOMP, hogs), CS de SPI,
  alias y chosen a nodos deshabilitados, particiones solapadas o fuera de la flash, SoftDevice v6/v7 frente al
  inicio de la aplicación, tamaño de flash del DTS frente a `board.conf`, MCUboot, stacks y heap por rol,
  `board.conf` que repite padres, avisos Kconfig de asignaciones que no se aplican.
- `patches/`: los 11 parches de Zephyr y el de loramac-node aplican sobre `0534e997` (con offsets, sin fuzz),
  ningún fichero lo tocan dos parches, ningún parche crea ni borra ficheros; `zephyr-new/` (BOARD-3).
- Builds completos verificados: rak4631 companion sin Zephtacore, room server, companion Zephtacore; heltec V4
  companion Zephtacore (sin sysbuild).

Sin hallazgo (comprobado):
- Pines compartidos: todos los que salen son intencionados y documentados (LED gpio + PWM en T096/Heltec V3/V4,
  `gps-enable` + `on-off-gpios` sobre el mismo pin con `LED_GPIO=n`, hog + `antenna-enable` en P1.05 de rak4631,
  `poweroff-gpios` de r1neo, CS hardware + `cs-gpios` en el TFT del Wireless Tracker).
- Particiones nRF52/nRF54L sin solapes; SoftDevice v7 solo en placas Seeed/MeshTracker/Ikoka. En ESP32 quedan los
  nodos `slot*_appcpu_partition` del dtsi AMP de Zephyr solapando slot1/storage, pero solo los usa una imagen
  APPCPU (`soc/espressif/common/loader.c:104`), que aquí no existe.
- Pila del hilo principal con Zephtacore (pendiente que dejó `meshtastic.md`): la cadena más profunda estática es
  un texto de canal MT que dispara la respuesta del bot de ping (`main → loop → checkRecv → onRecvForeign →
  MtRelayPort::onFrame → Bridge::handleFrame → MtSink::onText → deliverChannelMessage → pingReply →
  sendGroupMessage → HMAC-SHA256`): ~4,4 KB en nRF52 y ~4,6 KB en ESP32-S3, de 6144. Margen ~1,3-1,5 KB. Es un
  análisis estático: no sigue llamadas indirectas (virtuales, callbacks BLE) ni cuenta el marco de excepción.
- Recorte de heap WiFi + Zephtacore + UART: el orden de fragmentos da 63/68/69/72 KB como dicen los comentarios.
- `ZEPHTACORE_MESHTASTIC` en placas SX127x: solo ttgo_lora32, que no se publica.
- Gating por nombre en código nuevo del fork: ninguno (el único `default` nuevo usa `DT_HAS_NORDIC_NRF_LPCOMP_ENABLED`).

Por encima:
- Valores de TCXO, DIO2 y conmutadores RF de cada placa: tabulados (`radiotab.py`) y cotejados con los comentarios
  de cada DTS, que citan las variantes de MeshCore/Meshtastic; no los he cotejado uno a uno con hojas de datos
  públicas (solo T-Impulse Plus, en BOARD-7).
- Otros arreglos de `origin/dev` en placas que el fork no tiene y no he analizado a fondo: `1758f4ad` (RAK4631 con
  RAK13302: dormir el SX1262 de la placa base), `eee3a429` (deep sleep del e-paper al apagar), `af0d0b58` y
  `b1caf8c1` (radio, ya en el área de `radio.md`).

No revisado o no verificado:
- Configuración de ESP32-C3/C6, MG24, STM32WL y presets Linux: mi entorno no tiene el toolchain RISC-V, ni
  `hal_silabs`/`hal_stm32` en el workspace, y POSIX no compila en macOS.
- Nada probado en hardware; las cifras de consumo y el comportamiento del SGM41562 vienen de upstream.
- `custom_builder_web/`, `pages.yml`, `tests/`.
- Nimios vistos y no listados como hallazgos: la línea `battery-wake-mv` del DTS de gat562 está indentada con
  espacios; la cabecera del DTS de gat562 dice multiplicador 6232 y el valor es 6259; la ayuda de
  `ZEPHCORE_AUTO_SHUTDOWN_MILLIVOLTS` dice "Defaults ON (3300)" y el default es 3200; la ayuda de
  `ZEPHCORE_BATTERY_WAKE` dice "Every role then runs auto-shutdown" y el observer no lo hace.
