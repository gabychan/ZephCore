# Revisión: roles (`zephcore/app/`, `src/main_*.cpp`, `server_main_common.*`, `mesh_events.h`)

Estado: terminado

Hallazgos: 10 (alta 1, media 4, baja 5) más ROLE-6, duplicado de PERS-3, que no cuenta.

Revisado:
- Pregunta abierta RADIO-4 (TX fallido antes de `outbound_expiry`): `Dispatcher::loop`, hilo `lora_tx_wait`, bucles de `server_main_common.cpp` y `main_companion.cpp`. Respuesta: ROLE-1 (confirmado, baja).
- Protocolo companion: todos los `handleCmd*` de `CompanionMesh.cpp` (longitudes, respuestas), cola offline, iterador de contactos, CLI remota, respuestas de contacto.
- Repetidor y room server: login, replay, `onPeerDataRecv`, `handleRequest`, peticiones anónimas, `handleCommand`, motor de push del room server, diff del fork (bot de ping, firewall, vecinos), plazos de mantenimiento.
- Observer (`main_observer.cpp`, `ObserverMesh.*`), KISS modem (`KissModem.cpp`, `main_kiss_modem.cpp`), `PowerPolicy.*`, orden de arranque y estado inicial del hardware en cada rol.
- Cruce con otros informes: PERS-1 (export `rsp[128]`), PERS-2 (respuesta CLI larga en servidores), PERS-3 (replay tras reinicio) y MESH-1 (KISS `EncryptData`) no se repiten aquí.

---

### ROLE-2. Un comando CLI remoto con `sender_timestamp = 0` recibe privilegios de consola local en el companion (clave privada, `erase`) y desborda el búfer de respuesta en la pila
- Severidad: alta
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore base `da3513aa`, `CompanionMesh.cpp:1028`)   ·   ¿arreglado en origin/dev?: no (`origin/dev` `CompanionMesh.cpp:1033`, misma línea)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMesh.cpp:1040-1052` y `:1097`; `zephcore/helpers/BaseChatMesh.cpp:284-293`; `zephcore/helpers/CommonCLI.h:158-161`; `zephcore/helpers/CommonCLI.cpp:689,1231`; `zephcore/src/main_companion.cpp:794-811`
- Causa: `handleCommand` distingue lo remoto de lo local por el valor del timestamp: `sender_timestamp != 0 && !isUpstreamCompanionCommand(command)` (`:1097`). Para `TXT_TYPE_CLI_COMMAND` recibido por radio, `BaseChatMesh::onPeerDataRecv` pasa el timestamp que viene dentro del mensaje cifrado, que elige el emisor, y `onCLICommandRecv` no lo valida. Con timestamp 0, el filtro no se aplica y todo lo que hay debajo (`companion_cli_exec`, `CommonCLI`) lo toma como consola local: `get prv.key` (`CommonCLI.cpp:1231`), `erase` (`:689`), `set prv.key`, `start ota`, `v.contact`, `autoshutdown`. Además `replyCap(0)` devuelve `CLI_REPLY_SIZE` (256), pero el búfer de respuesta de `onPeerDataRecv` es `temp[166]` con la respuesta en `temp[5]`: 161 bytes.
- Escenario de fallo: el usuario marca un contacto con el flag 0x10 (CLI remota) pensando que solo alcanza los comandos de upstream, como dice el comentario de `:1054-1057`. Ese contacto (o quien tenga su clave) envía un `TXT_MSG` con `flags = TXT_TYPE_CLI_COMMAND`, timestamp 0 y texto `get prv.key`. El companion responde por radio con su clave privada Ed25519 en hex (130 bytes, cabe). Con `erase` borra el sistema de ficheros y reinicia. Con `get z.mc.neighbors`, `help` o cualquier respuesta larga, el formateador escribe hasta 256 bytes en un búfer de 161 dentro de la pila del hilo principal: hasta ~95 bytes de corrupción de pila.
- Arreglo mínimo propuesto: que el origen remoto no dependa del valor del timestamp. Lo mínimo: en `onCLICommandRecv` descartar `sender_timestamp == 0` antes de llamar a `handleCommand` (un emisor real nunca manda 0; el repetidor ya lo excluye porque exige `sender_timestamp >= last_timestamp > 0`, `RepeaterMesh.cpp:223,941`). Mejor aún, que `handleCommand` reciba un `bool remote` explícito, que es lo que el filtro y `replyCap` quieren saber. Aparte, valorar protección de replay por contacto (no hay ninguna para `CLI_COMMAND` en el companion: un paquete capturado se puede reinyectar cuando salga de la tabla de vistos).
- Relación: PERS-2 describe el desborde de `temp[166]` en repetidor y room server por las ramas que usan `CLI_REPLY_SIZE`; aquí la causa es otra (el timestamp 0 elige la capacidad local y salta el filtro) y afecta al companion.
- Cómo verificarlo: test de host sobre `CompanionMesh::onCLICommandRecv` (o `BaseChatMesh::onPeerDataRecv`) con un contacto con flag 0x10, timestamp 0 y `get prv.key`: hoy la respuesta contiene la clave; con el arreglo debe estar vacía. Con ASan, el mismo test con `help` o `get z.mc.neighbors` y la tabla llena debe dar `stack-buffer-overflow` antes del arreglo.

### ROLE-3. Con la cola offline llena durante una sincronización, el mensaje que se descarta es la cabeza ya enviada y la confirmación siguiente borra otro que la app nunca recibió
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore base; upstream MeshCore no tiene peek/confirm: saca el mensaje al enviarlo y, si la cola está llena sin mensajes de canal, descarta el nuevo)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMesh.cpp:455-487` (expulsión), `:489-503` (peek/confirm), `:2582-2596` (`CMD_SYNC_NEXT_MESSAGE`)
- Causa: `CMD_SYNC_NEXT_MESSAGE` envía la cabeza sin sacarla (`peekOfflineMessage`) y deja `_sync_pending = true`; la siguiente petición llama a `confirmOfflineMessage()`, que avanza la cabeza. Si entre las dos peticiones llega un mensaje con la cola llena, `addToOfflineQueue` expulsa justo la cabeza: o bien es el mensaje de canal más antiguo (`i == 0`, y el desplazamiento de `:465-469` mete el segundo mensaje en la posición de la cabeza) o, sin mensajes de canal, avanza `_offline_queue_head` (`:477`). `_sync_pending` sigue a `true`, así que la siguiente confirmación borra el mensaje que ahora ocupa la cabeza, que nunca se ha enviado.
- Escenario de fallo: el teléfono pasa horas fuera de alcance y la cola llega a `OFFLINE_QUEUE_SIZE` (256 por defecto). Al reconectar, la app empieza a vaciarla: recibe el mensaje 1 (pendiente). Antes de que pida el siguiente llega por radio un mensaje nuevo, la cola sigue llena y se expulsa la cabeza (el 1, ya entregado). La petición siguiente confirma y borra el mensaje 2 sin enviarlo; la app recibe el 3. Cada mensaje que llegue con la cola llena mientras hay uno pendiente pierde otro en silencio. Un DM puede perderse aunque la política diga que se tiran antes los de canal.
- Arreglo mínimo propuesto: en `addToOfflineQueue`, cuando la víctima es la cabeza y `_sync_pending` está activo, poner `_sync_pending = false`: el mensaje ya enviado sale de la cola, que es lo mismo que confirmarlo. No hace falta tocar nada más.
- Cómo verificarlo: test de host (no hay ninguno para la cola offline) que llene la cola, haga un `CMD_SYNC_NEXT_MESSAGE`, añada un mensaje y haga otros dos `CMD_SYNC_NEXT_MESSAGE`: hoy el segundo mensaje no llega nunca a la app; con el arreglo llegan todos menos el expulsado, que ya se había entregado.

### ROLE-4. El límite de logins fallidos de repetidor y room server no frena nada: la contraseña se compara antes de mirarlo
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore `8d138c41`, 2026-05-28)   ·   ¿arreglado en origin/dev?: sí (`51d2e454` "login: enforce failed-login limit", añade `RateLimiter::blocked()`)   ·   ¿ya conocido?: no en las memorias del fork
- Ubicación: `zephcore/app/RepeaterMesh.cpp:195-220`; `zephcore/app/RoomServerMesh.cpp:405-429`
- Causa: `handleLoginReq` calcula `admin_match`/`guest_match` y, si alguno acierta, da el login. `login_fail_limiter.allow()` solo se consulta en la rama de fallo y solo cambia el texto del log. Con el límite agotado, la contraseña correcta sigue entrando y cada intento erróneo sigue costando solo el airtime del `ANON_REQ`.
- Escenario de fallo: un atacante prueba un diccionario contra un repetidor a la velocidad que le deja el canal (varios intentos por segundo en presets rápidos). El commit que añadió el límite prometía "no brute-force at line rate"; con el código actual el primer acierto, aunque llegue en mitad del bloqueo, da admin.
- Arreglo mínimo propuesto: traer `51d2e454` tal cual: antes de comparar, `if (login_fail_limiter.blocked(now)) return 0;`.
- Cómo verificarlo: test de host de `handleLoginReq`: cuatro contraseñas malas y luego la buena dentro de los 180 s; hoy devuelve 13 (login OK), con el arreglo 0.

### ROLE-5. Con la contraseña de admin vacía, cualquier login sin contraseña en un repetidor entra como admin
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: sí (`24762a20` "login: no empty admin password")   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/RepeaterMesh.cpp:187-207`; `zephcore/app/RoomServerMesh.cpp:405-418`; `zephcore/helpers/CommonCLI.cpp:672-675`
- Causa: la comparación rellena con ceros la contraseña recibida y la guardada. Si `_prefs.password` está vacía, un login vacío da `admin_match = true`, y `admin_match` se evalúa antes que el invitado. El CLI acepta `password ` sin argumento y guarda la cadena vacía.
- Escenario de fallo: el operador hace `password ` creyendo que así desactiva el admin por contraseña (o lo hace un admin remoto). Desde ese momento el login de invitado que mandan las apps (contraseña vacía) recibe `PERM_ACL_ADMIN` y acceso a todo el CLI remoto.
- Arreglo mínimo propuesto: traer `24762a20`: `if (_prefs.password[0] == 0) admin_match = false;` en los dos roles y rechazar `password ` vacío en `CommonCLI`.
- Cómo verificarlo: test de host con `password` vacío y login vacío: hoy permisos admin; con el arreglo, invitado.

### ROLE-7. El observer nunca ejecuta el mantenimiento de radio: un AGC atascado en SX126x lo deja sordo hasta reiniciar
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no (`origin/dev:zephcore/app/main_observer.cpp` tampoco llama a ningún mantenimiento)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/main_observer.cpp:472-496` (bucle), `zephcore/app/ObserverMesh.h:33` (hereda `Dispatcher::loop` sin `maintenanceLoop`); comparar con `zephcore/src/server_main_common.cpp:572-574` y `zephcore/src/main_kiss_modem.cpp:253-257`
- Causa: el bucle del observer solo llama a `observer_mesh.loop()` en `LORA_RX` y nunca a `maintenanceLoop()` ni a `triggerNoiseFloorCalibrate`/`cadMaintenance`/`radioMaintenance`, y no arma ningún despertar por plazo (solo el temporizador de estado de 300 s). La recuperación del AGC atascado de SX126x (`LoRaRadio::agcIdleMaintenance`, `LoRaRadio.cpp:1159-1184`, la única familia con `reset_agc`, `radio_ops_sx126x.cpp:27-29,60`) necesita además las muestras de ruido para contar `_agc_rssi_frozen`, que tampoco se toman. La recalibración por temperatura de LR11xx/LR20xx (`imageCalMaintenance`) tampoco corre.
- Escenario de fallo: observer en una Heltec V3 (SX1262) a la intemperie. El AGC se queda atascado (el fallo que motivó `agcIdleMaintenance`): no llega ninguna trama, no hay `LORA_RX`, el bucle solo se despierta cada 300 s para publicar "online" y nunca intenta el reset. El broker sigue viendo el observer vivo y sin paquetes hasta que alguien lo reinicia. En LR11xx/LR20xx la sensibilidad se degrada con la deriva térmica sin recalibrar.
- Arreglo mínimo propuesto: hacer en el observer lo mismo que el KISS modem (que tampoco transmite): tras cada pase, `triggerNoiseFloorCalibrate(0)`, `radioMaintenance()` y un one-shot a `lora_radio.msUntilNextMaintenance()`, o directamente `observer_mesh.maintenanceLoop()` más ese despertar, como `server_main_common.cpp`.
- Cómo verificarlo: en banco, observer SX1262 con `debug.conf`: comprobar que hoy no aparece nunca el log `silence:` ni `agc:` tras minutos sin tráfico y que con el arreglo sí; test de host del bucle no es práctico.

### ROLE-1. Un TX que el driver da por fallido antes de `outbound_expiry` deja la cola de salida parada hasta el siguiente despertar ajeno
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore base `da3513aa` ya tiene `!tx_active && millisHasNowPassed(outbound_expiry)` y la fórmula del Tx timeout del SX126x)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no (es la pregunta abierta de RADIO-4)
- Ubicación: `zephcore/src/Dispatcher.cpp:162-172` (vuelta temprana), `:254-264` (`msUntilNextMaintenance` no incluye `outbound_expiry`), `:543`; `zephcore/adapters/radio/LoRaRadio.cpp:188-213,233-259`; `zephcore/patches/zephyr/0003-lora-sx126x-native.patch:2146-2162`
- Causa: cuando el driver concluye el TX con error, el hilo de espera borra `_tx_active` sin publicar `_tx_complete` y llama a `_tx_done_cb`, que despierta el bucle con `MESH_EVENT_LORA_TX_DONE`. `Dispatcher::loop` ve `outbound != NULL`, `isSendComplete()` falso, `tx_active` falso, pero `outbound_expiry` aún no ha vencido, y hace `return` en `:171`. Esa vuelta temprana salta `checkRecv()`, `checkSend()` y `armNextScheduledTx()`, y nadie arma un despertar para `outbound_expiry`: `Dispatcher::msUntilNextMaintenance()` solo mira la radio y el vigilante de RX. El bucle de eventos se queda dormido hasta el próximo evento ajeno.
- Alcance real: con `_tx_active` a 0 y sin `_tx_complete`, el envío ya está perdido (el comentario de `:129-131` garantiza que una finalización correcta se publica antes de bajar `_tx_active`), así que esperar a `outbound_expiry` no aporta nada. El error temprano solo llega en SX126x: el Tx timeout del chip es `max(10000, 1,25·A + 500)` ms y `outbound_expiry` es `max(300, 1,5·A)`, así que el fallo llega antes del vencimiento cuando el airtime A supera ~6,7 s (SF12/BW125 con 255 B ≈ 9,3 s; SF11-12/BW62,5). LR11xx y LR20xx nunca señalan error: su fallo es el timeout del hilo de espera, `max(5000, 2·A + 1000)` ms, siempre posterior a `outbound_expiry`; ese caso sí se recoge bien.
- Escenario de fallo: repetidor SX1262 en SF12/BW125, paquete de 250 B (A ≈ 9 s). El chip no da TX_DONE (fallo de hardware real, p. ej. el caso que el parche documenta). A los ~11,8 s llega `-ETIMEDOUT`, el bucle se despierta y vuelve sin hacer nada; `outbound_expiry` vence a los 13,5 s. En un canal tranquilo el siguiente despertar es el muestreo de ruido (`CONFIG_ZEPHCORE_NOISE_FLOOR_INTERVAL_MS`, 15 s por defecto, hasta 120 s) o el tope `CONFIG_ZEPHCORE_MAINTENANCE_BACKSTOP_MS` (60 s); durante ese tiempo ni se transmite la cola ni se vacía el anillo RX (`checkRecv` también queda detrás del `return`). Con tráfico, la primera RX posterior al vencimiento lo desatasca. En companion el tick de housekeeping (5 s) acota la espera. No es un bloqueo permanente.
- Arreglo mínimo propuesto: en `Dispatcher.cpp:162` tratar `!tx_active` como fin del envío sin esperar al vencimiento (`else if (!tx_active)`), ya que sin `_tx_complete` el envío ya ha fallado. Así el mismo pase que despierta `TX_DONE` libera el paquete y sigue con `checkRecv`/`checkSend`. Alternativa si se quiere conservar `outbound_expiry`: que `Dispatcher::msUntilNextMaintenance()` devuelva también `outbound_expiry - now` mientras haya `outbound`. Lo primero es más simple y quita un estado imposible.
- Cómo verificarlo: test de host del Dispatcher con radio falsa que, tras `startSendRaw` correcto, baje `isTxActive()` sin `isSendComplete()` antes de `outbound_expiry`; tras un solo `loop()` el paquete debe liberarse (`logTxFail`) y un segundo paquete en cola debe salir sin avanzar el reloj hasta el vencimiento.

### ROLE-8. El companion inicializa `mesh_events` después de arrancar la radio y el GPS: los eventos de esa ventana se borran
- Severidad: baja
- Certeza: confirmado (seguido en código y en `kernel/events.c` de Zephyr)
- Origen: upstream (ZephCore desde `8d1823d0`, el primer commit)   ·   ¿arreglado en origin/dev?: no (`origin/dev:zephcore/src/main_companion.cpp:1194`, mismo orden)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/main_companion.cpp:124` (`static struct k_event mesh_events`), `:1003` (callback del GPS), `:1108-1112` (callbacks de radio y `companion_mesh.begin()`), `:1169` (`gps_enable(true)`), `:1205` (`k_event_init`); comparar con `zephcore/src/server_main_common.cpp:700,725`, donde el init va antes de `begin()`
- Causa: `lora_rx_callback`, `lora_tx_done_callback`, `tx_drain_work_fn` y el callback del GPS hacen `k_event_post(&mesh_events, ...)` desde que se registran (`:1003`, `:1108-1110`), y la radio recibe desde `begin()` (`:1112`). `k_event_init` llega 93 líneas después, tras la escritura condicional de prefs en flash (`:1188`), el chime y la configuración de CAD. En Zephyr, `k_event_post` sobre la estructura a cero no falla (la `wait_q` vacía no se recorre), pero deja el bit en `event->events`, y `z_impl_k_event_init` lo pone a 0 (`events.c`). El aviso se pierde; el dato sigue en el anillo RX o en `pending_gps_actions`.
- Escenario de fallo: un companion arranca con tráfico en el canal; llega una trama entre `begin()` y `:1205` (la escritura de prefs de `:1188`, cuando ocurre, alarga la ventana). Su `MESH_EVENT_LORA_RX` se borra y el paquete espera en el anillo hasta el primer tick de housekeeping (5 s) o la siguiente RX. Lo mismo para un `TX_DRAIN` de un paquete que `begin()` o `vcontactNotify` hayan encolado, y para un paso de `GPS_ACTION_REAPPLY` (que espera al siguiente evento del GPS). Acotado a segundos; no es un bloqueo.
- Arreglo mínimo propuesto: mover `k_event_init(&mesh_events)` al principio de `main()`, antes de `gps_set_event_callback` (`:1003`), como hace `server_main_common.cpp`. O declararlo con `K_EVENT_DEFINE(mesh_events)` y quitar el init.
- Cómo verificarlo: en banco, con `debug.conf`, enviar tramas al companion durante el arranque y comprobar el retraso entre el log de RX del driver y el de `Mesh`; con el cambio debe desaparecer. No hay test de host práctico (es orden de arranque).

### ROLE-9. El companion recorta a 120 bytes la respuesta de telemetría remota; upstream entrega la trama entera
- Severidad: baja
- Certeza: confirmado (seguido en código; comparado con MeshCore `examples/companion_radio/MyMesh.cpp:725-735`)
- Origen: upstream (ZephCore `8d1823d0`)   ·   ¿arreglado en origin/dev?: no (`origin/dev` `CompanionMesh.cpp:1368`, mismo `rsp[128]`)   ·   ¿ya conocido?: no (PERS-1 trata el desborde de `rsp[128]` en la exportación; su nota dice que estos dos recortan, que es correcto: aquí el problema es el recorte, no un desborde)
- Ubicación: `zephcore/app/CompanionMesh.cpp:1503-1518` (telemetría); el de estado (`:1485-1500`) no se ve afectado porque `RepeaterStats` cabe
- Causa: `PUSH_CODE_TELEMETRY_RESPONSE` se monta en `uint8_t rsp[128]` y `data_len` se recorta a `128 - 8 = 120` bytes de CayenneLPP. Upstream copia `len - 4` en `out_frame[MAX_FRAME_SIZE + 1]`, hasta 164 bytes. El nodo remoto puede mandar hasta `MAX_PACKET_PAYLOAD - 4` de LPP (`RepeaterMesh.cpp:1109`), y el propio companion ya usa `rsp[8 + MAX_PACKET_PAYLOAD - 4]` para su telemetría local (`:3016-3017`).
- Escenario de fallo: un nodo sensor con GPS y varios canales (p. ej. tres INA con tensión, corriente y potencia, más temperatura, humedad y presión) responde con 130-160 bytes de LPP. La app recibe el registro final cortado a mitad: el parser LPP lo descarta o falla, y se pierden los últimos sensores. Con firmware MeshCore el mismo nodo se ve completo.
- Arreglo mínimo propuesto: `uint8_t rsp[MAX_FRAME_SIZE]` en el bloque de telemetría (y por simetría en el de estado), como ya hace el de `PUSH_CODE_BINARY_RESPONSE` en `:1528`; el recorte a `sizeof(rsp) - i` se queda.
- Cómo verificarlo: test de host de `onContactResponse` con `_pending_telemetry` y una respuesta de 150 bytes: hoy la trama empujada mide 128; con el arreglo, 154.

### ROLE-10. `CMD_SET_TUNING_PARAMS` acepta cualquier factor de airtime; por encima de ~199 el límite de duty cycle desaparece
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore; MeshCore tampoco valida, pero allí un factor grande es más restrictivo, no menos)   ·   ¿arreglado en origin/dev?: sí (`74a7e87e` "core mesh audit fixes": rechaza `af > ZC_AIRTIME_FACTOR_MAX` con `ERR_ILLEGAL_ARG`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMesh.cpp:2693-2704` (sin validación), `:1691-1695` (`getDutyCyclePercent`), `zephcore/src/Dispatcher.cpp:379-381` (`duty_pct == 0` desactiva el gate); comparar con `zephcore/helpers/CommonCLI.cpp:1003-1008` (`set af` exige 0-9) y `NodePrefs.h:318` (al cargar se recorta a 0-9)
- Causa: el handler guarda `af = valor/1000` sin rango. `getDutyCyclePercent()` devuelve `(uint8_t)(100/(af+1) + 0,5)`, que es 0 para `af >= 199`, y `txBudgetAllowsSend` interpreta 0 como "sin límite". Además el valor vive en RAM hasta el reinicio, cuando `sanitizeNodePrefs` lo cambia a 9 (10 %).
- Escenario de fallo: el usuario pone en la app un factor de airtime alto (p. ej. 200) para ser muy conservador, como haría en MeshCore. En ZephCore el companion pasa a transmitir sin límite de duty cycle hasta el siguiente reinicio, y tras reiniciar queda en 10 % sin avisar. Con valores entre 10 y 198 se aplica de 1 a 9 % hasta reiniciar y luego vuelve a 10 %.
- Arreglo mínimo propuesto: traer el bloque de `74a7e87e` (rechazar `af` fuera de 0-9 antes de tocar prefs).
- Cómo verificarlo: test de host de `handleCmdSetTuningParams` con `af = 200000`: hoy responde OK y `getDutyCyclePercent()` da 0; con el arreglo, `ERR_ILLEGAL_ARG` y prefs sin cambios.

### ROLE-11. `CMD_ADD_UPDATE_CONTACT` puede bajar `last_advert_timestamp` y reabrir la puerta a adverts viejos repetidos
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (igual en MeshCore: `updateContactFromFrame` copia el timestamp de la app)   ·   ¿arreglado en origin/dev?: sí (`569215be` "companion: keep advert mark on app update")   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMesh.cpp:2414` y `:2438-2442`; la guarda de replay está en `zephcore/helpers/BaseChatMesh.cpp:150`
- Causa: el handler sustituye el contacto entero por lo que manda la app, incluido `last_advert_timestamp`. Ese campo es la guarda de replay de adverts (`timestamp <= from->last_advert_timestamp` se descarta). Si la copia de la app es anterior a la del nodo, la guarda baja.
- Escenario de fallo: el teléfono estuvo desconectado mientras el nodo oía un advert nuevo de un contacto (cambio de nombre o de posición). Al volver, el usuario edita el contacto (favorito, ruta, nombre) y la app manda su copia con el timestamp viejo. Cualquiera que tenga grabado un advert intermedio de ese contacto puede reemitirlo y el companion lo acepta: nombre, tipo y posición vuelven a valores antiguos.
- Arreglo mínimo propuesto: traer `569215be` (si el existente tiene un timestamp mayor, conservarlo).
- Cómo verificarlo: test de host: contacto con `last_advert_timestamp = 2000`, `CMD_ADD_UPDATE_CONTACT` con 1000 y luego un advert firmado con 1500: hoy se acepta; con el arreglo se descarta.

### ROLE-6. (Duplicado de PERS-3) Replay de comandos de admin tras reiniciar repetidor o room server
- Confirmado por mi lado siguiendo `ClientACL.h:21,54` y `RepeaterMesh.cpp:908,941`; ya está descrito como PERS-3 en `review/persistencia.md` (arreglado en parte en `origin/dev` por `353a91fd` + `b409998e`). No lo cuento en el total.

---

## Otras observaciones (no cuentan como hallazgo)

- Room server, CLI remota con `sender_timestamp = 0`: descartado. Tras un reinicio `last_timestamp` vale 0 y un 0 entra por `>=`, pero `is_retry` es verdadero y no se ejecuta nada (`RoomServerMesh.cpp:535-557`); en el repetidor pasa lo mismo. El problema de ROLE-2 es solo del companion.
- `CMD_SEND_TRACE_PATH` (`CompanionMesh.cpp:3241-3253`): si `Mesh::sendDirect` descarta el TRACE por no caber (`Mesh.cpp:837-839`), el companion responde igualmente `PACKET_SENT` con un timeout calculado sobre el paquete ya liberado. La lectura es inocua (el pool no se reutiliza en medio y es de un solo hilo), pero la app espera una respuesta que no llegará. Menor.
- Room server, `setperm <clave> 0` con `next_client_idx` en la última posición deja el round-robin apuntando una vez a la copia obsoleta del último cliente (`ClientACL::applyPermissions` no limpia la cola del array): como mucho un push duplicado. Igual en upstream.
- Observer: no llama a `oled_sleep()` ni a `ui_init()` como `server_main_common.cpp:705-709`. Hoy ningún `zephcore.yml` construye el observer en una placa con pantalla, así que no lo cuento; si se añade una, la regla de AGENTS.md ("display blanked") aplica.
- `oled_sleep()` escribe `0xAE` a 0x3D en `i2c0` sin identificar el dispositivo (`helpers/oled_power.c:25-38`); queda fuera de mi alcance (es de periféricos), lo anoto por si nadie lo ha cubierto.

## Cobertura

- A fondo: `CompanionMesh.cpp` (todos los comandos, cola offline, respuestas de contacto, CLI remota, trace), `main_companion.cpp` (bucle de eventos y arranque), `server_main_common.cpp`, `RepeaterMesh.cpp` (login, peticiones, `onPeerDataRecv`, `handleCommand`, mantenimiento), `RoomServerMesh.cpp` (login, posts, push, ACK, plazos), `KissModem.cpp`, `main_kiss_modem.cpp`, `main_observer.cpp`, `ObserverMesh.cpp`, `PowerPolicy.*`, `mesh_events.h`, `main_repeater.cpp`, `main_room_server.cpp`; y fuera del alcance lo necesario para seguir causas (`Dispatcher.cpp`, `LoRaRadio.cpp`, `BaseChatMesh.cpp`, `CommonCLI.*`, `ClientACL.*`, `ZephyrGPSManager.cpp`, `kernel/events.c` de Zephyr).
- Por encima: `CompanionMesh.h` (tamaños de búferes y de la cola offline), resto de `RepeaterMesh.cpp` (logs, fecha, `onRecvPacket`), `CompanionCLI.h`, `CompanionProtocol.h`, `CompanionSerial.h`.
- No revisado: `RepeaterDataStore.*` (lo cubre persistencia, PERS-*), los ficheros asignados a Meshtastic y a conectividad. Nada se ha compilado ni probado en hardware: todos los hallazgos están seguidos en código, ninguno reproducido.
