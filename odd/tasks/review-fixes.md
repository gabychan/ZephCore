# Review fixes — 99 hallazgos de la revisión profunda (fork en `2541fe5f`)

Fuente de evidencia: `odd/reviews/` (copia congelada de los 9 informes + `BRIEF.md`, escritos en español). Criterio de
revisión: `AGENTS.md` (invariantes) y `docs/DESIGN.md`/`docs/ARCHITECTURE.md`/`docs/adr/`. Cada informe contiene, por
hallazgo, causa, escenario de fallo, arreglo mínimo propuesto y cómo verificarlo.

## Specs

- **S1.** «he hecho una revision profunda de todo el codigo con opus en xhigh y me ha encontrado todo esto:
  /private/tmp/claude-503/-Users-xavier-ejarque--t3-worktrees-ZephCore-t3-ab151ae2-90b71ff1ccc504cc» — evidencia:
  los 9 informes por área, congelados en `odd/reviews/`. Un hallazgo sin ficha en `odd/reviews/` no entra en este plan.
- **S2.** «quiero que crees un plan para ir atacando todo lo que encontro, hacer un checklist de todo lo que hay» —
  el checklist vive en `## Tasks`; cubre las 100 fichas (99 únicas: `ROLE-6` está marcado en su informe como duplicado de `PERS-3`).
- **S3.** «empezar por las que no hay dudas y no necesitas mi intervencion» — ola A (ya arreglado en `origin/dev`) y ola
  B (causa demostrada + arreglo inequívoco según `AGENTS.md`: causa raíz, cambio mínimo, común a todas las placas,
  gating por capacidad).
- **S4.** «luego atacaremos a lás que hay dudas ya me preguntas te parece?» — ola C: no se toca hasta que el humano
  decida. Cada decisión se presenta agrupada, con la opción recomendada y su consecuencia.
- **S5.** Verificación de toda tarea: `python3 tests/run.py run --profile quick` (baseline en `2541fe5f`: PASS 307,
  FAIL 0). Lógica sin hardware nueva o cambiada → caso de host en `tests/` con entrada en `tests/catalog.json`
  (`AGENTS.md`). ASan/UBSan no está disponible aquí (solo Linux): los desbordes de pila se fijan con el límite
  explícito y un test de host del límite, no con sanitizer. Las placas no se pueden compilar en este equipo: se
  reporta qué se compiló y qué queda para CI, nunca se afirma verificación en hardware.

## Tasks

Ruta: `inline` = padre; `writer` = subagente con superficies disjuntas. Commit en la rama de trabajo
(`xejarque/fulmar`, base `2541fe5f`), una unidad revisable por tarea. Ola C no tiene commit hasta decisión humana.

### Ola A — ya arreglado en `origin/dev` (adaptar el arreglo upstream, `AGENTS.md` §4)

| ID | Hallazgos | Commit upstream | Ruta | Commit aquí |
| --- | --- | --- | --- | --- |
| T1 | MESH-1 `Utils::encrypt` sin cota → desborde de pila en KISS `EncryptData` | `dcafa339` | inline | `9fa79f33` |
| T2 | ROLE-11 `CMD_ADD_UPDATE_CONTACT` baja `last_advert_timestamp` | `569215be` | inline | `c1e6c021` |
| T3 | ROLE-4 límite de logins fallidos no frena · ROLE-5 contraseña de admin vacía entra | `51d2e454`, `24762a20` | inline | `d5e029a2`, `b45bbf0b` |
| T4 | PERS-3 comando de admin capturado se repite tras reiniciar (y `ROLE-6`, que es la misma ficha duplicada) | `353a91fd`, `b409998e` | inline | `c83f9c40`, `a265edfe` |
| T5 | BOARD-7 T-Impulse GPS enable activo en bajo · BOARD-6 cargador SGM41562 sin programar · BOARD-8 RAK 3V3_S encendido en poweroff | `eb84ba25`, `9b820396`, `4dbd6fca` | inline | `0eb8d6ae`, `e9c4eed2`, `3ea78b95` |
| T6 | RADIO-7 LR11xx/LR20xx transmiten sin veredicto si falla el CAD | `5e73610f`, `6e458e24` | writer | `a34a9d8c` |
| T7 | ROLE-10 factor de airtime sin tope · MESH-2 recuperación de radio atascada se desactiva a los 24,9 d · MESH-3 TRACE lee la entrada siguiente fuera del payload · MESH-11 EMA de contención → despertar cada 5 s · MESH-12 desborde del reabastecimiento de duty cycle | `74a7e87e` | writer | `a972bf41` |

### Ola B — causa demostrada, arreglo inequívoco (no en `origin/dev`)

| ID | Hallazgos | Ruta | Commit |
| --- | --- | --- | --- |
| T8 | ROLE-2 comando CLI remoto con `sender_timestamp = 0` recibe privilegios de consola local (clave privada, `erase`) | writer | `9abebd88` |
| T9 | PERS-1 export de contacto desborda `rsp[128]` · PERS-2 `temp[166]` · CONN-2 `handleUplinkCommand` escribe 256 en 161 · PERS-17 NUL fuera de `temp` | writer | `3bf17801` |
| T10 | MESH-4 `region load` >32 regiones · MESH-13 flood de 63 saltos se repite con `path_len` 0x40 · MESH-9 dos `CMD_IMPORT_CONTACT` en una ráfaga · MESH-7 `releasePacket` decide `ForeignTail` por campos residuales | writer | `429894aa` |
| T11 | PERS-4 ACL/regiones borradas antes de reescribir · PERS-16 `getBlobByKey` confía en la longitud leída · PERS-18 error de lectura de identidad → identidad nueva · PERS-19 `rewriteMtKeys` confirma fichero truncado · la mitad del almacén de la ficha de `prefs.json` corrupto (la respuesta al usuario va con T12) | writer | `97276ae1` |
| T12 | PERS-9 advert negativo → 255 h · PERS-10 `set lat`/`set lon` sin validar · PERS-11 SF/CR/`int.thresh` truncados a 8 bits · PERS-12 `clock sync` con `0xFFFFFFFF` pone el reloj a 0 · PERS-13 OK a ajustes no guardados · PERS-14 `set buzzer` no persiste ni se aplica al arrancar · PERS-15 NaN de radio aceptado · PERS-21 ayuda `z.mt.hops` 7 vs Kconfig 3 · PERS-20 (mitad CLI) | writer | `a40d9d61` |
| T13 | RADIO-4 cualquier error de `startSendRaw` se trata como canal ocupado · RADIO-5 TCXO reprogramado tras reset · RADIO-6 recalibración por temperatura diferida se pierde · RADIO-8 RSSI/SNR de un paquete atribuidos a otro | writer | `37c81a2f` |
| T14 | RADIO-9 BUSY de la fase de sueño cuenta como canal ocupado · RADIO-10 `is_receiving` falso con mutex ocupado · ROLE-1 TX fallido antes de `outbound_expiry` para la cola de salida | writer | `3e66ddb1` |
| T15 | ROLE-3 cola offline llena descarta la cabeza ya enviada · ROLE-7 observer sin mantenimiento de radio · ROLE-8 `mesh_events` inicializado tarde · ROLE-9 telemetría remota recortada a 120 B | writer | `aa726644` |
| T16 | MT-2 un DM retransmitido no se vuelve a confirmar y el emisor lo da por fallido · MT-6 ACK `want_ack` sin responder · MT-7 unicast cifrado con clave de canal mostrado como público · MT-8 DM de 152-160 B → `ERR_BAD_STATE` · MT-9 reintento de DM cambia de id y llega dos veces · MT-11 ACK/NodeInfo con hop limit 7 en vez de saltos usados + 2 | writer | `0d9d9d8b` |
| T17 | Documentación e higiene: MESH-8 cambios del fork sin cerca `// ZEPHCORE:` · RADIO-13 falta el ADR de rechazo de cabeceras sin CRC · QA-1 `?v=9` fuera del sellado de caché · QA-5 README sin Zephtacore · QA-7 formateador sin SHA-256 · QA-8 `LICENSE` incompleta · CONN-11 `SECURITY.md` apunta a upstream | inline | `882cac64` |
| T18 | BOARD-2 `build.sh` no se para si falla el empaquetado · BOARD-3 `patches/zephyr-new/` fuera del mecanismo de parches · BOARD-5 `ZEPHCORE_UI_JOYSTICK` por nombre de placa | inline | `98c72a43` |
| T19 | HW-1 `oled_sleep()` escribe a 0x3D sin identificar · HW-2 sensores I2C que escriben antes de identificar el chip | inline | `9d433583` |

### Hallazgo nuevo (no está en la revisión)

| ID | Hallazgo | Ruta |
| --- | --- | --- |
| T30 | BOARD-4 config que repite lo que da el devicetree (`ESPTOOLPY_FLASHSIZE_*` en 9 `board.conf`, su propagación a MCUboot y `HAPTICS_DRV2605=y`) | inline | `ce1d9275` (solo `HAPTICS_DRV2605`; el resto revertido) |
| T29 | RADIO-1 el timeout de TX no devuelve el SX126x a RX · RADIO-12 el SNR del SX126x redondeado a dB: los dos viven en el parche `0003`, que hay que regenerar desde el árbol aplicado | inline | `3ad30ab4` |
| T28 | `zephcore_apply_patches` no tiene lock: dos `west build` en paralelo sobre el mismo árbol `zephyr/` se pisan entre el `git apply --check` y el `git apply`, y el segundo muere con "git apply failed unexpectedly" como si el parche estuviera roto. Además el mensaje no dice qué proceso ganó la carrera | inline | `8f842840` |

### Ola C — necesita decisión humana (no se toca hasta entonces)

| ID | Hallazgos | Decisión pendiente |
| --- | --- | --- |
| T20 | CONN-1 PIN BLE fijo `123456` · CONN-3 emparejamiento Just Works · CONN-4 AP OTA abierta sin plazo · CONN-5 TCP companion sin autenticar · CONN-6 MQTT TLS sin verificar certificado · CONN-7 plaza TCP vitalicia · CONN-8 USB sin contrapresión · CONN-9 mutex de socket en el hilo principal | política de seguridad/compatibilidad BLE, USB, TCP y MQTT |
| T21 | MESH-5 `z.mc.fw.gap` silencia adverts nuevos · MESH-6 semilla de identidad desde el ID público si falla `sys_csrand_get` · MESH-10 exención admin del duty cycle | política de malla (¿fail-closed?) y de la feature del fork |
| T22 | MT-1 NodeInfo forjado renombra contactos · MT-3 alta de contactos sin límite de saltos · MT-4 frames opacos sin autenticar bloquean el real · MT-5 `#mt-*` con secreto derivable del nombre · MT-10 re-contención adelanta frames Meshtastic | política de la capa Meshtastic (compatibilidad con nodos Meshtastic) |
| T23 | PERS-5 RTC de hardware atrasa el reloj · PERS-6 SNTP mueve el reloj en ambos sentidos · PERS-7 `meshtimesync` a futuro arbitrario · PERS-22 consenso de malla retrocede el reloj | semántica del invariante «el reloj solo avanza» |
| T24 | PERS-8 un fallo de montaje/`fs_stat` formatea particiones sanas · PERS-23 cada advert reescribe un registro de `adv_blobs` | política de datos ante fallo de montaje; coste de la mejora |
| T25 | RADIO-2 CAD_LBT deja la trama en la FIFO de TX · RADIO-3 RX_DONE rearma RX sin lock · RADIO-11 ningún driver identifica el chip antes de escribir | requiere datasheet/prueba en hardware o un rediseño del driver |
| T26 | BOARD-1/QA-4 la matriz de CI no compila seis placas ni ningún build sin Zephtacore · CONN-10 observer y uplink sin compilar · QA-3 la batería de host no llega a `app/` · QA-2 la release sale con notas de upstream · QA-6 Pages despliega firmware viejo | coste y alcance de CI, release y arnés de tests |

## Log

- **L1.** «he hecho una revision profunda de todo el codigo con opus en xhigh y me ha encontrado todo esto:
  /private/tmp/claude-503/-Users-xavier-ejarque--t3-worktrees-ZephCore-t3-ab151ae2-90b71ff1ccc504cc / quiero que crees
  un plan para ir atacando todo lo que encontro, hacer un checklist de todo lo que hay y empezar por las que no hay
  dudas y no necesitas mi intervencion. luego atacaremos a lás que hay dudas ya me preguntas te parece?»
- **L2.** (evidencia) Revisión sobre `2541fe5f`; `origin/dev` lleva 32 commits que el fork no tiene (`merge-base`
  `da3513aa`). Los 10 commits de arreglo citados en la ola A existen en el clon. Informes congelados en
  `odd/reviews/`.
- **L3.** (evidencia) Baseline en `2541fe5f`: `tests/run.py run --profile quick` → PASS 307, FAIL 0, ERROR 0 (17 s).
- **L4.** Ola A terminada: 7 commits atómicos en `xejarque/fulmar` (`9fa79f33`…`a34a9d8c`), 14 hallazgos. Verificado: PASS 312, FAIL 0, ERROR 0. Conflictos encontrados: `CompanionMesh.cpp` (T2, se conservaron el bloque Meshtastic y la guarda nueva) y
  `RELEASE_NOTES_1.18.3-zephcore.md` (T5, dos veces: el fork y upstream escriben el mismo fichero de notas; es la
  evidencia de QA-2 y se resolvió conservando ambos textos, sin decidir la numeración).
- **L5.** Desvío aceptado en T7: MESH-2 deja el hold de TX en el `Dispatcher` (transitorio) como upstream, no
  persistido en `NodePrefs` como proponía la ficha. Un hold guardado sobreviviría al reinicio y frenaría el primer
  TX sin motivo. El resto de T7 sigue a `74a7e87e`; `tempradio` y las limpiezas de `futureMillis()` quedaron fuera
  por ser cambios observables ajenos a los cinco hallazgos.
- **L6.** (evidencia) Límite de entrega: la rama ya arrastra 225 commits del fork; este trabajo añade 7 commits
  pequeños. El corte revisable de esta primera entrega es `9fa79f33..a34a9d8c`.
- **L7.** (evidencia) El arnés de host no compila `app/` ni `CommonCLI.cpp` (QA-3): los desbordes de pila de T8/T9
  no se pueden fijar con un test de host hoy. ASan solo existe en Linux. Por eso T8/T9 se verificarán por lectura
  del camino completo y suite verde, y QA-3 pasa a ser el habilitador de la ola B, no un extra.
- **L8.** T8 commiteado (`9abebd88`), verificado por lectura del camino (el arnés no compila `app/`) y suite verde
  (PASS 312). Otros entrantes a `handleCommand` son locales y no cambian: `VContact.cpp:295` y
  `handleCmdRunCliCommand` (comando de la app ya emparejada). Corte revisable actual de esta entrega:
  `9fa79f33..9abebd88` (8 commits, 15 hallazgos).
- **L9.** (corrección de un error mío) Dije que aquí no se podía compilar y era falso: hay west 1.5.0
  (`~/zephyrproject/.venv`), Zephyr SDK 1.0.1 y workspace west. El bloqueo real era que este worktree no tenía
  workspace propio (el build aplica los parches sobre `./zephyr`, que no existía). Resuelto con `west init -l zephcore`
  + `west update --narrow -o=--depth=1` dentro de `fulmar`. Comando válido, uno por vez:
  `cd fulmar && west build -b <board> zephcore --pristine -- -DEXTRA_CONF_FILE="..."` (west desde
  `~/zephyrproject/.venv/bin/`, `python` para el manifest desde el mismo venv: el `python3` del PATH no tiene `yaml`).
  Los fallos previos fueron una carrera mía: lancé dos builds en paralelo (T28) y los dos primeros desde el cwd del
  clon principal. Los parches aplican 11/11 al pin `0534e997e`; el zephyr más nuevo del clon principal (`11808e9ea8a`)
  es el que no los acepta, al revés de lo que supuse.
- **L10.** (evidencia) Compilado en este worktree, ola A: `lilygo_timpulse_plus` companion (FLASH 450104 B, 64,6 %;
  RAM 222568 B, 84,9 %), `rak4631` repeater (304640 B; RAM 87204 B), `rak4631` room server (278096 B; RAM 85284 B),
  `t1000_e` companion LR1110 (415872 B; RAM 222376 B) y `promicro_lr2021` companion LR2021 (421920 B; RAM 221800 B).
  Cinco de cinco, sin errores de compilación. La verificación de builds deja de ser "solo CI".
- **L11.** (evidencia) T9 compilado antes de commitear (`3bf17801`): `lilygo_timpulse_plus` companion (447592 B FLASH,
  175564 B RAM), `rak4631` repeater con `repeater_uplink.conf` (compila `RepeaterUplink`) y `rak4631` room server
  (278144 B / 85284 B). Suite de host PASS 312. Balance: 20 de 100 fichas hechas.
- **L12.** T10 commiteado (`429894aa`) con 4 casos de host nuevos (PASS 316) y compilado en `lilygo_timpulse_plus`
  companion y `rak4631` room server. El escritor reportó un subdesborde en `removeSelfFromPath` (0 saltos → 63) y
  después lo refutó con evidencia: los tres llamadores cuelgan de la guarda `isRouteDirect() &&
  getPathHashCount() > 0` (`Mesh.cpp:184`). No se añadió ninguna guarda especulativa; se fija la guarda existente
  con `UNIT-CHAT-006`.
- **L13.** T11 commiteado (`97276ae1`), compilado en companion, repeater con uplink y room server; suite PASS 316.
  `ZephyrFS` modo "w" pasa a escribir `<path>.tmp` y renombrar al cerrar, solo si ningún write quedó corto y
  `fs_sync()` fue bien. Solo `ClientACL` y `RegionMap` abren en "w" (comprobado por búsqueda), así que el alcance es
  el que pide PERS-4. Limitación aceptada y anotada: el `fs_rename` de Zephyr borra el destino antes de renombrar, así
  que la ventana existe todavía, pero es una operación de metadatos en vez de toda la escritura.
  PERS-20 queda a medias a propósito: la mitad del almacén está (se registra el fallo y `savePrefs` devuelve `bool`),
  la respuesta a la CLI necesita `CommonCLICallbacks::savePrefs()` y sus overrides, que es el mismo sitio que PERS-13,
  así que va con T12.
- **L14.** T12 commiteado (`a40d9d61`) con 3 casos de host nuevos (PASS 319) y compilado en companion, repeater con
  uplink y room server. PERS-14 se cerró entero: además de persistir `buzz_q`, `apply_boot_prefs()` aplica el modo
  bajo `CONFIG_ZEPHCORE_UI_BUZZER`, así que el room server deja de responder "off" y sonar tras reiniciar. Docs en
  el mismo commit: `docs/CLI_commands.md` (rangos y negativos de `clock sync`, `time`, `set lat/lon`,
  `int.thresh`, los dos intervalos de advert, y dos notas nuevas: ajustes que un companion no tiene, y escritura que
  falla) y `docs/ARCHITECTURE.md` §6.6 y §7.2 (validación "no dentro" para NaN, escritura atómica de ACL/regiones,
  identidad ilegible que no se pisa, error de escritura que se responde).
- **L15.** T13 (RADIO-4/5/6/8) commiteado (`37c81a2f`) con `UNIT-RADIO-001` y compilado en `promicro_lr2021`,
  `t1000_e` y `rak4631`. Decisión tomada por el padre en la interfaz del driver: `txStartRefusal()` es `virtual` con
  `-EBUSY` por defecto, así que un radio que no informa del motivo conserva el significado de upstream y KissModem y el
  fixture no cambian. RADIO-1 sale a T29 porque exige regenerar `patches/zephyr/0003-lora-sx126x-native.patch` desde el
  árbol aplicado; mezclarlo aquí habría hecho irrevisable el commit.
- **L16.** T14 commiteado (`3e66ddb1`) con `UNIT-DISPATCH-035/036`, compilado en `t1000_e`, `promicro_lr2021` y
  `rak4631` room server. ROLE-1 se arregló por la alternativa de la ficha (la expiración del TX como plazo de
  mantenimiento) porque el arreglo "literal" habría cambiado la ventana de 1,5 × airtime que fija `UNIT-DISPATCH-027`.
  RADIO-12 se suma a T29: el redondeo del SNR vive en el parche `0003`, junto con RADIO-1; el cambio exacto queda
  anotado por el escritor (`sx126x.c:709`, `+ 2` antes del desplazamiento) para cuando se regenere.
- **L17.** T15 commiteado (`aa726644`), compilado en `lilygo_timpulse_plus` companion y `heltec_wifi_lora32_v3`
  observer (ESP32-S3 con `--sysbuild`, primer build ESP32 de esta sesión; hizo falta `west blobs fetch hal_espressif`).
  ROLE-3 adopta la política de upstream (se descarta el mensaje nuevo, no la cabeza) y conserva peek/confirm, que es lo
  que hace sobrevivir un mensaje a una caída de BLE. ROLE-9 pasa a `MAX_FRAME_SIZE` con la cuenta hecha: un
  `PAYLOAD_TYPE_RESPONSE` está limitado a 176 B, así que la trama entera siempre cabe.
- **L18.** T16 commiteado (`0d9d9d8b`) con 4 casos de host nuevos (PASS 326) y compilado en companion y repeater
  Zephtacore. Coste aceptado y anotado: `kMaxTxFrame` 184→255 son 71 B de pila en cuatro sitios del companion y en
  `MtRelayPort`/`RepeaterMeshtastic`, y un DM de más de 184 B gasta una ranura `ForeignTail`. `docs/zephtacore/README.md`
  actualizado en el mismo commit: tamaño de trama, hop limit de las respuestas, id del reintento, re-ACK y el unicast
  por clave de canal que se descarta.
- **L19.** T17 lo hice yo, no el escritor: el subagente falló por **cuota del proveedor**, no por el handoff (la tarea
  estaba bien acotada). Misma tarea, ejecutada inline. Commit `882cac64`, compilado en companion y repeater; suite de
  host PASS 326; tests de la web de Pages 58/58 (`node --test tests/web/`). Nota: la cuota de subagentes se agotó a
  mitad de la ola B; si vuelve a pasar, el trabajo sigue inline en vez de esperar.
- **L20.** T18 commiteado (`98c72a43`) sin BOARD-4, que pasa a T30: quitar las líneas de `ESPTOOLPY_FLASHSIZE_*` y su
  propagación cambia la imagen de MCUboot en nueve placas ESP32, y mezclarlo con un arreglo de `build.sh` habría hecho
  irrevisable el commit. Verificado: `build.sh` sintaxis (`bash -n`) y semántica con un demo de la misma forma
  (`failed=1` cuando `mv` falla); `board_manifest.py check` (47 manifiestos, 0 errores); compilados `rak4631` repeater,
  `gat562_30s`, `wio_tracker_l1` y `wio_tracker_l1_1w`, con `CONFIG_ZEPHCORE_UI_JOYSTICK=y` en los tres con joystick
  tras quitar los `select` de placa. El parche `0002` se verificó aplicándose (`[zephyr] Applied:`), no compilando:
  `native_sim` solo existe en Linux (el propio Zephyr lo dice al configurar), así que los presets Linux quedan para CI.
- **L21.** T19 commiteado (`9d433583`), compilado en `wio_tracker_l1` (la placa con panel: `oled_sleep` queda con su
  bus del devicetree), `rak4631` (sin panel: el símbolo queda vacío), `lilygo_techo` (era la que borraba `bmp388`) y
  `sensecap_solar`. Suite de host PASS 326. La ola B queda cerrada salvo las tres tareas que necesitan el parche
  `0003` o builds `--sysbuild` de nueve placas (T29, T30).
- **L22.** T29 commiteado (`3ad30ab4`): `sx126x_abort_tx()` en el parche `0003` y su llamada desde la espera de TX del
  adaptador (RADIO-1), más el redondeo del SNR (RADIO-12). El parche se regeneró desde el árbol aplicado, como pide su
  propio preámbulo, y se comprobó que aplica a un árbol limpio **sin** el fallback de ficheros obsoletos de
  `zephcore_apply_patches()`. Compilado en `rak4631` y `promicro_sx1262`. `abort_tx` es `NULL` en LR11xx y LR2021
  porque su `recv_async` sale de TX por sí mismo: no se añadió una guarda especulativa.
- **L23.** T30 commiteado (`3e56bc7f`), y la ficha se equivocaba en el riesgo: el app image sale **byte-idéntico**
  (`96fbad03…`) y el binario de MCUboot solo cambia en 37 bytes, que son su marca de tiempo y el hash de firma — la
  cabecera ESP32 lleva `flash_size=0x4f` en los dos, así que la config repetida no cambiaba nada, que es justo lo que
  decía la ficha. Al quitar el bloque me llevé por delante el `include` del resolvedor de placa que usa el resto del
  fichero: lo detectó el build `--sysbuild` de después, y por eso estos cambios van con builds a los dos lados.
  `meshtracker_x1` conserva `CONFIG_HAPTICS_DRV2605=y` viniendo del devicetree.
- **L24.** (cierre de la ola B) 69 de 100 fichas hechas, 0 pendientes, 31 bloqueadas esperando decisión. Verificación
  acumulada: suite de host 326 casos verdes, 58 tests de la web, y 20 builds reales entre nRF52, ESP32-S3, nRF54L y
  tres familias de radio.
- **L25.** (verificación independiente, y el motivo de este log) Un verificador revisó los 25 commits. Encontró dos
  cosas que no eran ciertas, y las dos eran errores míos:
  - **T19 estaba roto y yo no lo vi.** La guarda usaba `DT_HAS_<COMPAT>_ENABLED`, que Zephyr **no genera** (genera
    `DT_COMPAT_HAS_OKAY_<compat>`). La condición era siempre falsa, así que `oled_sleep()` compilaba a función vacía en
    todas las placas y **ningún panel se apagaba**: una regresión peor que el fallo original, en silencio. El build
    pasaba, el símbolo existía, y mi comprobación de entonces (contar referencias en el `.map`) no distinguía el stub
    del código real. Arreglado en `ce1d9275`, y ahora lo verifico donde se ve: `nm -S` sobre el objeto da 64 B de
    código con su referencia al dispositivo, contra el hueco del stub.
  - **T30 no era inofensivo.** La premisa "`ESPTOOLPY_FLASHSIZE_*` no tiene otro consumidor" es falsa: en
    ESP32-C2/C6/C61/H2 el `Kconfig.mmu` del SoC deriva `MMU_PAGE_SIZE` de esa elección, y el *choice* tiene 2 MB por
    defecto. Quitar las líneas habría cambiado la página de MMU en el C6 y el C61. Revertido entero en `ce1d9275`;
    queda solo el `HAPTICS_DRV2605` redundante, que es lo que el devicetree ya selecciona.
  Lección para el resto del trabajo: un cambio que **compila** y deja los símbolos en su sitio no está verificado; hay
  que mirar el código generado o el `.config` resultante, no solo que el build pase.
- **L26.** Lo que el verificador confirmó como correcto: el parche `0003` aplica a un árbol limpio y los otros 7
  ficheros son byte-idénticos al baseline (solo `sx126x.c` difiere, con los dos cambios esperados); `txStartRefusal()`
  con `-EBUSY` por defecto deja `KissModem` y el fixture intactos, sin más overrides; `ZephyrFS` no pierde el fichero
  bueno y borra el `.tmp` (aunque `lfs_rename` no es atómico frente a corte de energía: la ventana se estrecha, no
  desaparece); `RepeaterUplink` no deja ningún `snprintf` con 256. Suite 326 y tests web 58/58 en copia limpia.
- **L27.** Deuda abierta que el verificador señaló y asumo: **T28 no está hecho** (`zephcore_apply_patches` sigue sin
  lock; el mensaje no dice quién ganó la carrera) y el hold de `Dispatcher` no tiene un comentario en su declaración
  que impida a un futuro escritor saltarse `holdTxUntil()`. Ninguna de las dos es de la lista de 100, así que van al
  cierre como pendientes explícitos.
- **L28.** Cerrado **T28** (`8f842840`): `zephcore_apply_patches` toma un `file(LOCK ... GUARD FUNCTION)` mientras
  reescribe el árbol de west. Reproducido de las dos maneras para no quedarme en la teoría: dos configures a la vez
  sobre árbol limpio, **sin** lock → uno muere con `git apply failed unexpectedly: 0001-lora-lr11xx-lr20xx-build.patch`;
  **con** lock → los dos configuran y compilan. El `GUARD FUNCTION` lo suelta también en el `FATAL_ERROR`, y el tope de
  600 s hace que un configure colgado lo diga en vez de esperar para siempre. El aviso de que `holdTxUntil()` es el
  único escritor de `next_tx_time` pasa a la declaración: quien lo lea ahí es quien lo va a escribir mal.
- **L29.** Los dos probes del verificador, hechos (`bf7224c2`): `tests/harness_tests/test_oled_gate.py` y
  `test_flashsize_choice.py`. No son ceremonia: cada uno **falla** contra el defecto real. El de la guarda, contra
  `9d433583`, nombra las dos líneas culpables; el del tamaño de flash, al quitar la línea de 4 MB, nombra
  `xiao_esp32c6` y `lilygo_tlora_c6` y **descubre siete SoCs con MMU dependiente** (`esp32c2`, `c6`, `c61`, `h2`,
  `h21`, `h4`, `s31`), tres más de los que el verificador conocía. Vivieron en `tests/harness_tests/`, que el arnés ya
  ejecuta en cada `run.py run` y no arrastra dependencias.
- **L30.** Lo que el verificador dejó abierto y sigue abierto, sin adornos: `repeatAck()` llama a `openPki()`, que en
  fallo dispara `onUnknownKey`/`onPkiFailed` desde la rama de duplicado (acotado por el limitador de 1 h/nodo, pero es
  efecto lateral no descrito); `handleCmdRunCliCommand` conserva el mismo agujero que ROLE-2 cerró solo en
  `handleCommand`; y `zephcore/app/*` + `CommonCLI.cpp` siguen sin compilarse en el arnés, así que T8, T9, PERS-13/20
  se verifican por lectura. Nada de eso se arregla sin decisión tuya sobre el alcance, así que no lo toqué.

