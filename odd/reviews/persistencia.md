# Revisión: persistencia (commit 2541fe5f)

Estado: terminado

Ya revisado: `adapters/datastore/*`, `RepeaterDataStore.*`, `PrefsJson`, `PrefsCodec`, `ConfigSerializer`, `NodePrefs.h`,
`CommonCLI.*`, `ZCommand.h`, `ZInfo.h`, `AutoShutdownCli.h`, `MeshTimeSync.*`, `tz_offset.h`, `adapters/clock/*`,
`helpers/compat/ZephyrFS.*`, `ClientACL` y `RegionMap` (load/save), SNTP, export de contactos, `docs/CLI_commands.md`.

"Upstream" en este informe es ZephCore `origin/dev`; donde el código viene de Arduino MeshCore se dice.

## Hallazgos

### PERS-1. Exportar un contacto o el propio nodo desborda `rsp[128]` en la pila del companion
- Severidad: alta
- Certeza: confirmado (seguido en código)
- Origen: upstream (`8d1823d0`; Arduino MeshCore usa `out_frame[MAX_FRAME_SIZE+1]`)   ·   ¿arreglado en origin/dev?: no (`CompanionMesh.cpp:2358`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMesh.cpp:2528` (export propio) y `:2540` (export de contacto); datos de `zephcore/adapters/datastore/ZephyrDataStore.cpp:826-831`
- Causa: el blob es el paquete advert crudo, guardado en `BaseChatMesh.cpp:164` y `:207` con su ruta flood y hasta `MAX_ADVERT_PKT_LEN` (166 bytes, `ZephyrDataStore.cpp:26`). `exportContact()` lo copia entero en `&rsp[1]`, que solo tiene 127 bytes. El export propio escribe con `writeTo()` hasta 134 bytes (2 + 100 + 32 de app data).
- Escenario de fallo: un paquete advert ocupa 2 + ruta + 100 + app data, y la app data es 1 + 8 (posición) + nombre. Se desborda en cuanto ruta + app data pasa de 25 bytes: un contacto de nombre de 14 caracteres con posición oído a 3 saltos ya lo hace. Un nodo remoto que emita un advert flood con nombre largo y 32 bytes de ruta escribe 39 bytes elegidos por él más allá de `rsp` cuando el usuario pulsa "compartir contacto". Con el export propio basta un nombre de 17 caracteres con posición (7 bytes de más como máximo).
- Arreglo mínimo propuesto: `uint8_t rsp[MAX_FRAME_SIZE + 1]` en las dos ramas, como Arduino MeshCore. Ver también PERS-16.
- Cómo verificarlo: test de host con un advert de 166 bytes en `adv_blobs` y ASan sobre `handleCmdExportContact`; en placa, exportar un contacto de nombre largo y posición con la app.

### PERS-2. Un comando CLI remoto largo desborda la pila en `temp[166]`
- Severidad: alta
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`CommonCLI.cpp:1414` y `:1982`, `temp[166]` en `RepeaterMesh.cpp:915`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:2024` y `:1459`; buffers en `zephcore/app/RepeaterMesh.cpp:965` y `zephcore/app/RoomServerMesh.cpp:548`
- Causa: las ramas finales de `handleSetCmd` y `handleGetCmd` limitan con `CLI_REPLY_SIZE` (256), pero en remoto `reply` apunta a `temp[5]` con 161 bytes útiles. El resto de handlers usa `replyCap()`.
- Escenario de fallo: el payload admite 176 bytes de texto cifrado (184 - 2 hashes - 2 MAC, múltiplo de 16), es decir 171 caracteres de comando. `set ` + 167 caracteres desconocidos produce `unknown config: ` + 167 = 183 caracteres + NUL = 184 bytes desde `temp[5]`: 23 bytes controlados por el emisor más allá de `temp`. Con `get `, 11 bytes. Hace falta sesión de admin, pero la contraseña por defecto es `password` (`zephcore/Kconfig:276-278`). `CONFIG_HW_STACK_PROTECTION` solo protege el final de la pila del hilo, no un buffer local dentro del marco.
- Arreglo mínimo propuesto: `replyCap(sender_timestamp)` en esas dos ramas.
- Cómo verificarlo: test de host con ASan que pase el comando de 171 caracteres a un `reply` de 161 bytes.

### PERS-3. Un comando de admin capturado se puede repetir tras cada reinicio
- Severidad: alta
- Certeza: confirmado (seguido en código)
- Origen: upstream (también Arduino MeshCore)   ·   ¿arreglado en origin/dev?: sí (`353a91fd`, `b409998e`: `_replay_floor`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/ClientACL.h:21,54`, `ClientACL.cpp` (`load`), `zephcore/app/RepeaterMesh.cpp:941`, `zephcore/app/RoomServerMesh.cpp:535`
- Causa: la defensa contra replay es `sender_timestamp >= client->last_timestamp`, pero `last_timestamp` no se guarda: `load()` lo deja a 0.
- Escenario de fallo: un atacante sin claves graba del aire un `reboot` cifrado de un admin. Tras cualquier reinicio lo retransmite: el MAC es válido, `last_timestamp` es 0 y el comando se ejecuta. Como `reboot` vuelve a poner el contador a 0, basta repetirlo en cada arranque para mantener el nodo en bucle. Lo mismo vale para cualquier `set` grabado.
- Arreglo mínimo propuesto: portar `353a91fd` y `b409998e` (suelo = reloj de arranque menos `REPLAY_BOOT_SLACK` de 10 s). Sin reloj válido al arrancar ese suelo protege poco; es un límite a documentar.
- Cómo verificarlo: test de host que recargue la ACL y reinyecte el mismo datagrama; en banco, dos placas y una retransmisión cruda.

### PERS-4. ACL y mapa de regiones se borran antes de reescribirse: un corte de energía los pierde
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`ZephyrFS.cpp:91`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/compat/ZephyrFS.cpp` (`ZephyrFS::open`, modo `"w"`), usado por `ClientACL.cpp:67` y `RegionMap.cpp:120`
- Causa: el modo `"w"` hace `fs_unlink()` y después crea el fichero. LittleFS confirma el unlink de inmediato, y el contenido nuevo solo al cerrar. `saveContacts` evita justo esto, y lo dice en su comentario.
- Escenario de fallo: corte entre el unlink y el `close` durante un guardado de ACL (tras login o `setperm`) o un `region save`. Al arrancar no hay `/acl` o `/regions2`: se pierden permisos de admin y `sync_since`, o todo el mapa de regiones con sus reglas de flood. Un error de escritura a medias también deja un fichero truncado, porque `save` hace `break` y cierra.
- Arreglo mínimo propuesto: en `ZephyrFS`, escribir `"w"` a `<ruta>.tmp` y hacer `fs_rename` en `File::close()` si no hubo error, sin tocar el texto portado de upstream.
- Cómo verificarlo: test de host con un FS simulado que corte tras el unlink; en placa, cortar la alimentación en bucle durante `region save`.

### PERS-5. Al arrancar, el RTC de hardware puede mover el reloj hacia atrás
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`main_companion.cpp:989`, `server_main_common.cpp:668`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/main_companion.cpp:985-986`, `zephcore/src/server_main_common.cpp:691-692`, `zephcore/adapters/clock/ZephyrRTCClock.cpp:60-68`
- Causa: el constructor recupera la hora de la RAM `noinit`, y después `seedCurrentTime()` la sustituye sin comparar por la del chip RTC.
- Escenario de fallo: una escritura al RTC falla (`zephcore_rtc_save` solo registra un aviso) y el chip conserva una hora antigua pero válida. En el siguiente reinicio en caliente el reloj retrocede todo ese desfase, y los pares descartan los adverts del nodo hasta alcanzar su último timestamp. Sin fallo alguno, la deriva entre cristales también produce un retroceso de segundos. Viola el invariante de reloj que solo avanza.
- Arreglo mínimo propuesto: sembrar solo si `rtc_epoch > getCurrentTime()`.
- Cómo verificarlo: test de host del reloj con backup mayor que la hora del RTC.

### PERS-6. SNTP sin autenticar mueve el reloj en cualquier sentido, contra DESIGN §8.2
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (`875e6554`)   ·   ¿arreglado en origin/dev?: no (`RepeaterMesh.cpp:57-60`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/wifi/ZephyrWiFiStation.c:90-94`, `zephcore/app/RepeaterMesh.cpp:55-63`, `zephcore/app/main_observer.cpp:303-308`; `ZephyrRTCClock::setCurrentTime` (`ZephyrRTCClock.cpp:70-77`) es incondicional
- Causa: la hora de `sntp_simple("pool.ntp.org")` llega a `setCurrentTime` sin comparar con la actual. `docs/DESIGN.md` §8.2 solo exceptúa del avance obligatorio el fix GPS y `clkreboot`, y SNTP no figura entre las fuentes.
- Escenario de fallo: en una WiFi con DNS o NTP falsificados, o con un servidor averiado, una respuesta antigua retrasa el reloj del repetidor con uplink o del observer. Sus adverts quedan descartados por los pares hasta alcanzar el último timestamp. Hace falta controlar la red local o el camino a `pool.ntp.org`; SNTP no va autenticado.
- Arreglo mínimo propuesto: decidir y dejarlo escrito. O SNTP aplica solo hacia delante, como el resto de fuentes no GPS, o se añade a la excepción de §8.2 junto al GPS con su motivo.
- Cómo verificarlo: test de host del callback con una hora anterior; en banco, servidor NTP local con fecha pasada.

### PERS-7. Con `meshtimesync on`, tres identidades falsas pueden llevar el reloj a un futuro arbitrario
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (fichero idéntico)   ·   ¿ya conocido?: parcialmente (`docs/ARCHITECTURE.md`, "Accepted limits")
- Ubicación: `zephcore/helpers/MeshTimeSync.cpp:226-252`, `:342-348`
- Causa: mientras el reloj está por debajo del build epoch, el bootstrap acepta el consenso de tres pares frescos sin el tope de ±1 h ni techo alguno.
- Escenario de fallo: un nodo recién arrancado sin hora oye tres adverts firmados por identidades creadas por un atacante con timestamp en 2099. El reloj salta; en los roles de solo avance no vuelve salvo por GPS o `clkreboot`, y sus adverts quedan con fecha futura.
- Arreglo mínimo propuesto: un techo de plausibilidad para el bootstrap, relativo al build epoch, documentado en ARCHITECTURE.
- Cómo verificarlo: ampliar `UNIT-SYNC-006` con tres pares en 2099.

### PERS-8. Un fallo de montaje o de `fs_stat` al arrancar formatea particiones sanas
- Severidad: media
- Certeza: probable (la ruta está seguida en código; el fallo transitorio de lectura no se ha reproducido)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/boards/common/filesystem.dtsi:19`, `qspi-ext.dtsi`, `zephcore/adapters/datastore/ZephyrFsFormat.c:112`, `ZephyrFsUtil.c:34`, `zephcore/app/RepeaterDataStore.cpp:49-60`, `zephcore/src/server_main_common.cpp:661`; Zephyr `subsys/fs/littlefs_fs.c:979-996`
- Causa: dos caminos. Ningún fstab lleva `no-format`, y Zephyr formatea ante cualquier error de `lfs_mount`, no solo con flash en blanco como dice el comentario de `ZephyrFsFormat.c:112`. Además `hasRoleData()` usa `fs_stat() == 0`: cualquier error, no solo `-ENOENT`, cuenta como "volumen de otro rol" y lanza `zephcore_fs_format_all()`, que borra `/lfs`, los bonds en NVS y QSPI.
- Escenario de fallo: en el arranque la QSPI aún no responde bien (alimentación inestable o chip que no salió de deep power-down) y la lectura del superbloque falla. `fs_mount` formatea `/ext` y se pierden contactos y canales. En `/lfs`, un error de lectura en `fs_stat` borra la identidad del nodo.
- Arreglo mínimo propuesto: `no-format` en los fstab, formatear solo cuando se ha visto flash en blanco, y en `hasRoleData()` distinguir `-ENOENT` de los demás errores.
- Cómo verificarlo: test de host con un backend de bloques que devuelva `-EIO` al montar y al hacer `stat`; en placa, inyectar el error en el driver QSPI.

### PERS-9. Intervalos de advert negativos se guardan como 255 h o 510 min
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`CommonCLI.cpp:1607`, `:1621`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:1650-1658` y `:1662-1672`; `cliNum` en `:139-147`
- Causa: `cliNum` acepta negativos (`strtol`), y las comprobaciones solo miran `> 0 && < min` y `> max`. Después se hace `(uint8_t)hours` y `(uint8_t)(mins / 2)`.
- Escenario de fallo: `set flood.advert.interval -1` responde OK y guarda 255 h, fuera del 3-168 que documenta `docs/CLI_commands.md:543`. `set advert.interval -2` guarda 255, es decir 510 min, frente al tope de 240 (`:544`).
- Arreglo mínimo propuesto: rechazar `< 0` en las dos ramas.
- Cómo verificarlo: test de host de `handleSetCmd` con `-1` y `-2`.

### PERS-10. `set lat` y `set lon` no validan nada, aunque la documentación del observer promete un rango
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream y Arduino MeshCore   ·   ¿arreglado en origin/dev?: no (`CommonCLI.cpp:1687`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:1719-1728`; `docs/CLI_commands.md:421` y `:532-533`
- Causa: `atof()` sin comprobar rango ni que el texto sea un número.
- Escenario de fallo: `set lat 3000` se guarda; al codificar el advert, `3000 * 1e6` no cabe en `int32_t` y la conversión es indefinida. `set lat abc` pone 0 y responde OK. La documentación del observer dice `<-90..90>` y `<-180..180>`.
- Arreglo mínimo propuesto: rechazar lo que no sea número o quede fuera de -90..90 y -180..180, como documenta `:421`.
- Cómo verificarlo: test de host con `3000`, `abc` y `-91`.

### PERS-11. SF, CR e `int.thresh` se truncan a 8 bits antes de comprobar el rango
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:1051-1052` (`set radio`), `:662-663` (`tempradio`), `:1023` (`int.thresh`), mensaje fijo en `:1083`
- Causa: `(uint8_t)atoi(...)` y `(uint8_t)v` antes de validar.
- Escenario de fallo: `set radio 869.525 250 263 261` se acepta como SF7 CR5. `set int.thresh 300` guarda 44. El error de `set radio` siempre dice `bw 7-500` aunque el rango real sea `ZC_RADIO_BW_MIN/MAX_KHZ` del chip.
- Arreglo mínimo propuesto: validar el `int`/`long` antes de convertir, y construir el mensaje con los límites del chip.
- Cómo verificarlo: test de host con esos valores.

### PERS-12. `clock sync` con `sender_timestamp` 0xFFFFFFFF pone el reloj a 0
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream y Arduino MeshCore   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:604-605`; `time` en `:624-627`
- Causa: `sender_timestamp > curr` es cierto y `sender_timestamp + 1` desborda a 0. En `time`, `_atoi` desborda en silencio con más de 10 cifras.
- Escenario de fallo: un admin cuya app manda 0xFFFFFFFF deja el reloj en 1970, un retroceso que el invariante prohíbe. Requiere sesión de admin.
- Arreglo mínimo propuesto: exigir `sender_timestamp < UINT32_MAX`; en `time`, parsear con `strtoul` y comprobar el fin.
- Cómo verificarlo: test de host de las dos órdenes.

### PERS-13. El companion responde OK a ajustes que ni usa ni guarda
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: por comprobar   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp` (`handleSetCmd`), `zephcore/helpers/PrefsJson.cpp:236` (`CompanionJson`)
- Causa: `set password`, `guest.password`, `owner.info`, `flood.max*`, `advert.interval` y `flood.advert.interval` no filtran el rol companion, y `CompanionJson` no tiene esos campos. La CLI ya rechaza en companion `int.thresh`, `backoff.multiplier` y `loop.detect` (`:1760-1761`).
- Escenario de fallo: el usuario cambia la contraseña en un companion, recibe OK y tras reiniciar vuelve la anterior, sin que la contraseña se use en ningún sitio.
- Arreglo mínimo propuesto: `Error: not supported on companion` en esas ramas, igual que las ya filtradas.
- Cómo verificarlo: test de host con rol companion.

### PERS-14. `set buzzer` en room server dice que persiste, pero `prefs.json` no lo guarda
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: por comprobar (la rama existe en origin/dev, `CommonCLI.cpp:1476`)   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/CommonCLI.cpp:1536-1546`, `zephcore/helpers/PrefsJson.cpp:347` (`ServerJson`, sin `buzz_q`; solo `CompJson:185` lo tiene)
- Causa: la rama compila en todo lo que no es repetidor y escribe `buzzer_quiet`, pero el JSON de servidor no tiene la clave.
- Escenario de fallo: en un room server con UI, `set buzzer 0` silencia hasta el siguiente arranque. `docs/CLI_commands.md:549` dice "Applied live and persisted".
- Arreglo mínimo propuesto: `buzz_q` en `ServerJson` (es JSON, no toca el formato legado), o rechazar el comando fuera del companion y corregir la documentación.
- Cómo verificarlo: test de host de ida y vuelta de `serverPrefsToJson`.

### PERS-15. Valores NaN de radio no se rechazan al cargar las preferencias
- Severidad: baja
- Certeza: confirmado (seguido en código); el origen del NaN sería un fichero corrupto o editado
- Origen: upstream   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/PrefsJson.cpp:395-397`, `PrefsCodec.cpp:147-149` y `:373-375`, `zephcore/adapters/datastore/ZephyrDataStore.cpp:290-292`, `zephcore/app/RepeaterDataStore.cpp:121-123`
- Causa: el saneado compara `x < min || x > max`, que con NaN es falso; la CLI sí rechaza NaN (`CommonCLI.cpp:149`).
- Escenario de fallo: un `prefs.json` con `"freq": nan` (strtof lo acepta) llega al driver de radio como frecuencia.
- Arreglo mínimo propuesto: `!(x >= min && x <= max)` en el saneado común.
- Cómo verificarlo: test de host de carga con `nan`.

### PERS-16. `getBlobByKey` confía en la longitud del registro leído
- Severidad: baja
- Certeza: confirmado (seguido en código); requiere un registro corrupto
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`ZephyrDataStore.cpp:662-663`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/datastore/ZephyrDataStore.cpp:826-831`
- Causa: `memcpy(dest_buf, tmp.data, tmp.len)` con `tmp.len` hasta 255 y `data[166]`. LittleFS no lleva CRC en los bloques de datos.
- Escenario de fallo: un byte de longitud dañado lee 89 bytes de pila más allá de `tmp` y los escribe en el destino; con `rsp` ya corregido (PERS-1, 177 bytes) seguiría desbordando.
- Arreglo mínimo propuesto: tratar `tmp.len > MAX_ADVERT_PKT_LEN` como registro no válido.
- Cómo verificarlo: test de host con un registro de `len` 255.

### PERS-17. La respuesta de vecinos con prefijo puede escribir el NUL fuera de `temp`
- Severidad: baja
- Certeza: probable (requiere un `secs_ago` de 10 cifras)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no (`RepeaterMesh.cpp:1282`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/RepeaterMesh.cpp:1332-1342`
- Causa: el bucle corta con `dp - reply < 134`, pero no descuenta el prefijo `xx|` reflejado ni la longitud máxima de una línea.
- Escenario de fallo: un vecino oído con el reloj aún en 1970 y un reloj puesto después da un `secs_ago` de 10 cifras; la última línea termina en `temp[166]`, un byte fuera.
- Arreglo mínimo propuesto: `snprintf` contra el final real del buffer.
- Cómo verificarlo: test de host con 9 vecinos y `secs_ago` de 10 cifras.

### PERS-18. Un error al leer la identidad se trata como fichero incoherente y se genera una identidad nueva
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: por comprobar   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/datastore/IdentityFile.cpp:23-50`
- Causa: no se distingue un error de E/S de un contenido inválido: ambos renombran a `.bad` y regeneran.
- Escenario de fallo: un `-EIO` transitorio al arrancar cambia la clave pública del nodo; contactos y ACL de otros nodos dejan de reconocerlo.
- Arreglo mínimo propuesto: regenerar solo con contenido leído completo e inválido; ante error de E/S, no tocar el fichero.
- Cómo verificarlo: test de host con una lectura que devuelva `-EIO`.

### PERS-19. `rewriteMtKeys` confirma un fichero truncado si falla una lectura
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/datastore/ZephyrDataStore.cpp:640-656`; migración a `/ext` en `:119-166`
- Causa: un error de `open` o `read` se trata como fin de fichero y el `.tmp` parcial se renombra encima. Además `mtkeys` no está en la lista que se migra a `/ext`.
- Escenario de fallo: una lectura fallida durante el reescrito pierde las claves Meshtastic de los nodos posteriores.
- Arreglo mínimo propuesto: abortar sin renombrar ante cualquier error, y añadir `mtkeys` a la migración si debe vivir en `/ext`.
- Cómo verificarlo: test de host con un fallo de lectura a mitad.

### PERS-20. Con `prefs.json` corrupto se cargan ajustes legados antiguos o los de fábrica sin aviso, y `savePrefs` no informa de errores
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/datastore/ZephyrDataStore.cpp:342-360`, `zephcore/app/RepeaterDataStore.cpp:82-104`
- Causa: si el JSON no se parsea se cae al binario legado, si aún existe, o a defaults, y después se reescribe. `savePrefs` no devuelve error y la CLI responde OK.
- Escenario de fallo: un bloque dañado en `prefs.json` devuelve la radio a los parámetros de antes de la migración y el nodo deja de oír su malla.
- Arreglo mínimo propuesto: registrar el fallo de parseo y propagar el error de `savePrefs` a la respuesta de la CLI.
- Cómo verificarlo: test de host con `prefs.json` truncado y un `prefs` legado presente.

### PERS-21. La ayuda dice que `z.mt.hops` vale 7 por defecto; Kconfig dice 3
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/ZCommand.h:102`; `zephcore/Kconfig:1452-1456`, `NodePrefs.h:490-494`
- Causa: texto de ayuda desfasado. `docs/CLI_commands.md:255` y `docs/zephtacore/README.md:34` dicen 3.
- Arreglo mínimo propuesto: corregir el texto.
- Cómo verificarlo: `get z.mt.hops help`.

### PERS-22. Repetidor y observer retroceden el reloj por consenso de malla, contra el invariante de AGENTS.md y DESIGN §8.2
- Severidad: baja
- Certeza: confirmado (seguido en código y documentación)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/RepeaterMesh.h:170`, `zephcore/app/ObserverMesh.h:62` (`forward_only = false` por defecto), `zephcore/helpers/MeshTimeSync.cpp:333`; `docs/ARCHITECTURE.md:352-353`
- Causa: ARCHITECTURE declara "bidirectional" para esos dos roles, mientras `AGENTS.md` y DESIGN §8.2 dicen que el reloj solo avanza, salvo GPS y `clkreboot`.
- Escenario de fallo: un paso atrás de hasta 1 h silencia los adverts del repetidor ante sus pares durante ese tiempo; es una decisión documentada a medias, no un error de código.
- Arreglo mínimo propuesto: un ADR que recoja la excepción y la cite en §8.2, o pasar los dos roles a solo avance.
- Cómo verificarlo: revisión documental.

### PERS-23. Cada advert aceptado reescribe un registro de `adv_blobs`
- Severidad: mejora
- Certeza: probable (no medido)
- Origen: upstream y Arduino MeshCore   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/BaseChatMesh.cpp:207`, `zephcore/adapters/datastore/ZephyrDataStore.cpp:837-872`
- Causa: `putBlobByKey` reescribe el registro aunque el advert no haya cambiado; con el COW de LittleFS cada advert cuesta al menos una escritura de bloque.
- Escenario de fallo: en placas sin QSPI, `/lfs` de 128 KB y una malla con cientos de adverts al día acelera el desgaste.
- Arreglo mínimo propuesto: no escribir si el blob guardado es igual al nuevo.
- Cómo verificarlo: contar escrituras de bloque en un test de host con 1000 adverts repetidos.

## Descartados tras comprobarlo

- Wrap de temporizadores a los 49,7 días en repetidor y room server: los helpers `static` truncados de
  `RepeaterMesh.cpp:68-74` y `RoomServerMesh.cpp:60-66` quedan ocultos por `Dispatcher::millisHasNowPassed` y
  `futureMillis` (`include/mesh/Dispatcher.h:337-338`), que son seguros frente al wrap. Son código muerto;
  origin/dev los borró en `74a7e87e`.
- `gps diag` y `power` con límite 160: su salida máxima no lo alcanza.
- `saveContacts`: escribe a temporal y renombra; LittleFS no confirma un fichero tras un error de escritura.
- Claves y tokens de `prefs.json`: todos dentro de los límites de `ConfigSerializer` (clave 15, token 128, profundidad 8).
- `txdelay` y `rxdelay` fuera de rango: no se consumen en runtime.
- Los otros `rsp[128]` de `CompanionMesh.cpp:1489` y `:1507` recortan antes de copiar.

## Cobertura

- A fondo: `adapters/datastore/*` (prefs, contactos, canales, blobs, identidad, mtkeys, migración a `/ext`,
  formateo), `RepeaterDataStore.*`, `PrefsJson`, `PrefsCodec`, `ConfigSerializer`, `NodePrefs.h` (saneado),
  `CommonCLI.*` (comandos de reloj, `set`/`get`, límites de respuesta, roles), `adapters/clock/*`, `MeshTimeSync.*`
  (bootstrap y política de pasos), `helpers/compat/ZephyrFS.*`, `ClientACL` y `RegionMap` (load/save), callbacks SNTP,
  export de contactos del companion, montaje de LittleFS en Zephyr.
- Por encima: `ZCommand.h` y su ayuda, `ZInfo.h`, `AutoShutdownCli.h`, `tz_offset.h`, `MeshcoreJson.h`,
  `StatsFormatHelper.h`, `boot_prefs.h`, `IdentityStore.h`, `docs/CLI_commands.md` (rangos y defaults de los ajustes
  con hallazgo, lista de claves `z.` frente a la ayuda: coinciden).
- No revisado: `mt::handleCommand`, `ping::handleCommand`, `fw::handleCommand`, `rxstats`, `handleUplinkCommand`
  (permisos y límites de respuesta del uplink), el resto de comandos de `docs/CLI_commands.md` uno por uno.
- No ejecutado: ningún test de host ni compilación; nada probado en hardware. Ningún hallazgo figura en la memoria
  del proyecto.
