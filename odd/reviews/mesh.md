# Revisión: mesh (commit 2541fe5f)

Estado: terminado (tres revisores: MESH-1..8, MESH-9..10, MESH-11..13)

Ya revisado: `Dispatcher.cpp` (RX, liberación de paquetes, `sendForeign`), `Mesh.cpp` (`onRecvPacket`, creación de
datagramas), `Packet.cpp`, `StaticPoolPacketManager.cpp`, `Identity.cpp`, `Utils.cpp` (cifrado), `adapters/rng/`,
`McFirewall.h` y su uso en `RepeaterMesh`, `NeighbourList.h`, `PingBot.h`, `BaseChatMesh.cpp` (recepción),
`AdvertDataHelpers`, `RegionMap`, `TransportKeyStore`, `ClientACL` (gestión), `RateLimiter`, `RoutingPolicy`,
`OffgridPolicy`, `UTF8Helpers`, `TxtDataHelpers`, cercas `// ZEPHCORE:`; `BaseChatMesh.cpp` (envío, contactos,
conexiones, canales, `importContact`), `ClientACL` (carga, `putClient`, permisos), `ContactInfo`, `ChannelDetails`,
`Mesh.cpp` (`routeRecvPacket`, multipart, `create*`, `send*`), `Utils.cpp` (resto), `ContentionTracker`,
`ForeignTail.h`, `TxStall.h`, `recontend`/`armNextScheduledTx`, uso de monocypher, y los arreglos de `origin/dev`
`74a7e87e` frente al fork.

"Upstream" es ZephCore `origin/dev`; donde el código viene de Arduino MeshCore se dice.

Orden por severidad: media (MESH-4, 1, 10, 6, 5, 2), baja (MESH-9, 7, 13, 3, 12, 11, 8).

## Hallazgos

### MESH-4. `region load` con más de 32 regiones escribe fuera de `regions[]`
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (Arduino MeshCore y ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/RegionMap.cpp:161-163`; llamadores `zephcore/app/RepeaterMesh.cpp:1684`,
  `zephcore/app/RoomServerMesh.cpp:1170`
- Causa: `putRegion` solo mira la capacidad cuando `id == 0`. `handleRegionLoadLine` pasa el id antiguo
  (`old ? old->id : 0`) para cada región que ya existía, así que esas filas se añaden sin límite a
  `temp_map.regions[MAX_REGION_ENTRIES]`.
- Escenario de fallo: mapa con 20 regiones; el admin pega un `region load` de 35 filas, 15 nuevas y las 20
  antiguas al final. Las nuevas llenan hasta 32 por la rama con límite; las antiguas restantes escriben
  `regions[32]` (que es `wildcard`, el padre de `load_stack[0]`), `regions[33]` y `regions[34]`, ya fuera de
  `RegionMap`, sobre los miembros de `RepeaterMesh` que siguen a `temp_map`. Al cerrar, `region_map = temp_map`
  copia `num_regions = 35` y `findMatch` recorre entradas fuera del array en cada flood. Lo puede hacer un admin
  por la consola o por CLI remoto.
- Arreglo mínimo propuesto: `if (num_regions >= MAX_REGION_ENTRIES) return NULL;` sin condición sobre `id`,
  con cerca `// ZEPHCORE:` (el fichero es portado).
- Cómo verificarlo: test de host de `RegionMap`: 32 `putRegion(nombre, 0)` y luego `putRegion("x", 0, 7)` debe
  dar `NULL` y dejar `getCount() == 32`. No hay ningún test de `RegionMap` en `tests/`.

### MESH-1. `Utils::encrypt` no acota la longitud: el KISS `EncryptData` desborda la pila
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: sí (`dcafa339`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Utils.cpp:121-141`; llamador `zephcore/app/KissModem.cpp:530-541`
- Causa: `encrypt` copia `src_len` bytes en `tmp[MAX_PACKET_PAYLOAD + CIPHER_BLOCK_SIZE]` (200 B) sin mirar
  `src_len`. Los llamadores de la malla acotan antes (`createDatagram`, `createGroupDatagram`...), pero
  `KissModem::handleEncryptData` pasa `len - 32` bytes de una trama KISS de hasta `KISS_MAX_FRAME_SIZE` (512).
- Escenario de fallo: el host KISS manda `SetHardware EncryptData` con clave + 400 bytes de texto: `encrypt`
  escribe ~400 bytes en un buffer de 200 de la pila del hilo del módem; corrupción de pila o fallo.
- Arreglo mínimo propuesto: el de `origin/dev`: `if (src_len > MAX_PACKET_PAYLOAD) return 0;` al principio de
  `Utils::encrypt` (y la nota en `docs/kiss_modem_protocol.md`).
- Cómo verificarlo: test de host que llame a `Utils::encrypt` con 300 bytes bajo ASan; o enviar la trama por
  serie a un rol `kiss_modem`.

### MESH-10. Con el presupuesto de duty cycle agotado, un paquete "admin" pendiente abre la puerta a todos los demás, y cuenta también el tráfico ajeno que se repite
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore, `a7d07390`; Arduino MeshCore no tiene exención)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:379-399` (`txBudgetAllowsSend`), `:118-122` (`isAdminPacket`), `:495-496`
  (`getNextOutbound` elige por prioridad entre todos los vencidos); `zephcore/src/StaticPoolPacketManager.cpp:42-55`
- Causa: la exención se decide por cola, no por paquete: si hay **algún** REQ/RESPONSE/ANON_REQ/CONTROL vencido,
  `txBudgetAllowsSend` devuelve verdadero y `checkSend` transmite el paquete vencido de mayor prioridad, que puede
  ser cualquier otro (un DM directo o un flood a 1 salto tiene prioridad 0-1; un REQ por flood a 3 saltos, 3). Además
  `isAdminPacket` mira solo el tipo, así que cuenta igual un REQ o un ANON_REQ ajeno que el nodo solo repite, no solo
  la gestión de este nodo, que es lo que justifica el comentario. Lo transmitido así tampoco se descuenta después:
  `tx_budget_ms` se satura en 0 (`:144-148`).
- Escenario de fallo: repetidor con el 10 % por defecto (`airtime_factor` 9) y el presupuesto agotado en hora punta;
  tiene en cola varios floods de texto ya vencidos. Llega un ANON_REQ de login de otro usuario hacia un room server
  (o cualquiera manda un `DISCOVER_REQ`, que se responde con CONTROL). Mientras ese paquete espere, cada pasada de
  `checkSend` saca primero los floods de prioridad mejor, sin presupuesto; luego sale el ANON_REQ. Con tráfico de
  logins, peticiones de estado o telemetría de otros, el límite configurado deja de acotar nada. Un atacante sin
  claves lo provoca a voluntad mandando REQ por flood con hashes cualesquiera: los repetidores no los pueden descifrar
  y los repiten exentos.
- Arreglo mínimo propuesto: con el presupuesto por debajo del umbral, transmitir solo el paquete admin vencido
  (sacarlo por índice en lugar de `getNextOutbound`), no abrir la cola entera. Restringir la exención a lo que el
  nodo origina (p. ej. `getPathHashCount() == 0` en flood, o una marca puesta al crear la respuesta) cambia una
  decisión documentada en `docs/ARCHITECTURE.md:261,266`: necesita decidirlo el mantenedor y actualizar el texto.
- Cómo verificarlo: test de host en `tests/unit/dispatcher_tests.cpp`: presupuesto a 0, en cola un TXT directo
  (prioridad 0) y un REQ por flood (prioridad 3), ambos vencidos; hoy `checkSend` transmite el TXT primero.

### MESH-6. La semilla de identidad ignora el error de `sys_csrand_get`: sin "beat", la clave sale del ID público
- Severidad: media (consecuencia grave, ocurrencia improbable)
- Certeza: confirmado en código; el fallo de `sys_csrand_get` en el primer arranque no se ha observado
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/rng/ZephyrRNG.cpp:316` y `:367` (`rc1`, `rc5` solo se imprimen);
  `:86-94` (el beat solo existe en ESP32 y con `SYS_CLOCK_HW_CYCLES_PER_SEC < 1 MHz`); `:403-410`
- Causa: `ZephyrRNG::random` trata un fallo de `sys_csrand_get` como fatal (`cryptoPanicReboot`, `:31-41`),
  pero `mixIdentitySeed` sigue adelante con lo que haya en el pool. En STM32WL (SysTick a la frecuencia de CPU)
  y nRF54L (GRTC a 1 MHz) no hay beat, así que si las dos llamadas fallan el pool solo contiene el ID de
  hardware, que el propio código llama público (`:330-331`). La salida no es todo ceros ni todo 0xFF, así que
  la comprobación final no la detecta.
- Escenario de fallo: un STM32WL cuyo driver de entropía no está listo o devuelve error en el primer arranque
  (por ejemplo, un error de semilla del RNG): `generateFirstBootIdentity` deriva la clave Ed25519 de
  `SHA-256(ID del chip)`, la guarda y la usa para siempre. Quien lea el ID por serie o del log de arranque puede
  calcular la clave privada del nodo.
- Arreglo mínimo propuesto: tratar `rc1 != 0 || rc5 != 0` igual que `random()`: `cryptoPanicReboot` cuando no
  haya una fuente de timing sana (`!HAVE_TWO_CLOCK_BEAT` o `!health_ok`); o, más simple y coherente, siempre
  que falle una de las dos lecturas.
- Cómo verificarlo: en `tools/rng_selftest`, un hook de prueba que fuerce `rc != 0` y compruebe el reinicio;
  revisar en el log de arranque de un STM32WL la línea `stage1 csrand : rc=`.

### MESH-5. `z.mc.fw.gap`: repetir un advert viejo y válido de un nodo silencia sus adverts nuevos
- Severidad: media (solo con `z.mc.fw.gap` > 0; por defecto 0)
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/RepeaterMesh.cpp:872-877` (`advertKept` en `onAdvertRecv`),
  `:658-664` (`advertInGap`); `zephcore/helpers/McFirewall.h:171-210`
- Causa: el hueco se abre con la hora local de recepción de cualquier advert con firma válida. La firma no
  prueba frescura: un advert capturado hace horas sigue verificando, y `RepeaterMesh::onAdvertRecv` llama a
  `advertKept` sin mirar su timestamp. El comentario de `McFirewall.h:189-190` ("solo uno que pasó la firma, o
  uno falso podría silenciar al nodo real") da por cerrado justo este caso.
- Escenario de fallo: el repetidor tiene `fw.gap 60`. Un atacante guardó varios adverts antiguos de V (cada uno
  tiene otro hash). Reemite uno cada 59 minutos, siempre uno que ya salió de la tabla de vistos (160 entradas):
  Mesh lo verifica, `advertKept` abre el hueco de V y el advert real siguiente de V cae en `advertInGap`
  (`:661`), no se procesa ni se repite. V deja de anunciarse a través de ese repetidor mientras dure el ataque.
  La copia vieja sí se repite. Basta un solo advert si el atacante conoce el periodo de V y lo reemite justo
  antes.
- Arreglo mínimo propuesto: medir el hueco en el reloj del emisor: guardar en `AdvertSeen` el timestamp del
  advert que lo abrió, no abrirlo con un timestamp que no sea mayor que el guardado, y en `advertInGap` tirar
  solo si el nuevo está a menos de `gap` del guardado. El lado Meshtastic (`MtFirewall.h:74-81`) usa el mismo
  hueco con NodeInfo sin firma; allí la suplantación es inherente al protocolo y basta con documentarlo.
- Cómo verificarlo: test de host de `fw::Firewall`: `advertKept(k, ts=100)`, luego un advert de `k` con
  ts=100 (repetición) no debe renovar el hueco y uno con ts=100+gap+1 debe pasar.

### MESH-2. Tras 24,9 días sin un TX que encuentre la radio ocupada, se desactiva la recuperación de radio atascada
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: ambos (el fork reestructuró `checkSend`; el patrón viene de upstream)   ·   ¿arreglado en origin/dev?: sí, con otra estructura (`74a7e87e`, `tx_hold`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:458-485` (gate en `:460`), `next_tx_time` solo se escribe en `:61` y `:482`
- Causa: `next_tx_time` solo se actualiza cuando un paquete vencido encuentra la radio recibiendo o no lista.
  Si eso no ocurre en 2^31 ms, `millisHasNowPassed(next_tx_time)` (`:619-620`) pasa a dar falso y lo sigue dando
  otros 24,9 días: el sello viejo parece futuro.
- Escenario de fallo: repetidor tranquilo, 25 días sin que un TX coincida con una recepción. Después la radio
  se queda atascada con `isReceiving()` verdadero (el caso para el que existe `recoverRxState()`). Cada pasada
  de `checkSend` sale en `:460-462` antes de anotar `cad_busy_start`, así que el `CAD timeout` de `:468` nunca
  llega y el nodo no transmite hasta que la radio se desatasque sola o pasen otros 24,9 días. Además pide un
  despertar a ~24,9 días (`:461`). Con recepciones normales el efecto es solo retraso.
- Arreglo mínimo propuesto: que el plazo deje de contar en cuanto se alcanza, como hace `origin/dev`: una
  bandera `tx_hold` que se pone al escribir `next_tx_time` en `:482` y se borra cuando `now` lo alcanza; el
  gate de `:460` comprueba `tx_hold && !millisHasNowPassed(next_tx_time)`.
- Cómo verificarlo: test de host en `tests/unit/dispatcher_tests.cpp` con reloj falso: arrancar, avanzar
  2^31 + 1000 ms, encolar un paquete con `isReceiving()` fijo a verdadero y comprobar que tras
  `getCADFailMaxDuration()` se llama a `recoverRxState()`.

### MESH-9. Dos `CMD_IMPORT_CONTACT` en la misma ráfaga pierden el primero y una ranura del pool para siempre
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore; Arduino MeshCore no lo sufre porque lee una trama por `loop()`)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/BaseChatMesh.cpp:632-645` (`_pendingLoopback = pkt` sin mirar si ya hay uno), `:1053-1057`
  (solo `loop()` lo consume); `zephcore/src/main_companion.cpp:344-355` (procesa todas las tramas pendientes en un bucle)
- Causa: `importContact` pide un paquete al pool y lo deja en `_pendingLoopback`, que se procesa y libera en el
  siguiente `BaseChatMesh::loop()`. `process_companion_rx` atiende en un `while` todas las tramas que haya en la
  cola del transporte; mientras la respuesta OK se escriba sin bloquear (`CompanionSerial::writeFrame`), lee la
  siguiente sin pasar por `loop()`. Un segundo import sobrescribe el puntero.
- Escenario de fallo: la app (o un script que no espera la respuesta) manda dos `CMD_IMPORT_CONTACT` seguidos por
  BLE. Los dos reciben OK; solo el segundo advert se procesa. El paquete del primero no se libera nunca: el pool
  (32 ranuras, `StaticPoolPacketManager.cpp:10`) pierde una por cada vez. Con importaciones masivas el nodo se queda
  sin paquetes para recibir y transmitir hasta reiniciar.
- Arreglo mínimo propuesto: en `importContact`, rechazar (o procesar antes) si `_pendingLoopback != NULL`; lo más
  simple es devolver `false` en ese caso, y la app recibe `ERR_ILLEGAL_ARG` en lugar de un OK falso. Alternativa
  equivalente: que `process_companion_rx` salga del bucle tras un import.
- Cómo verificarlo: test de host sobre `BaseChatMesh`: dos `importContact` seguidos y un `loop()`;
  `getFreeCount()` del pool debe volver al valor inicial.

### MESH-7. `releasePacket` decide si devuelve una ranura `ForeignTail` mirando campos que pueden ser restos de otro uso
- Severidad: baja
- Certeza: confirmado (seguido en código); el disparo necesita una orden malformada de la app
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no (la ronda de radio descartó otro
  ángulo: que un paquete recibido con `0xFD` se tome por foráneo)
- Ubicación: `zephcore/src/Dispatcher.cpp:561-566`, `:550-558` (`obtainNewPacket` no limpia `header` ni
  `transport_codes`), `:270-274` (`tryParsePacket` escribe `header` antes de rechazar la versión);
  disparo en `zephcore/app/CompanionMesh.cpp:3612-3620`
- Causa: un paquete foráneo se reconoce solo por `header == 0xFD` y guarda su ranura en `transport_codes[1]`.
  Un paquete del pool conserva `transport_codes` de su uso anterior; un TRANSPORT_FLOOD recibido deja
  `[0] = código de región` (≠ 0) y `[1] = 0`, que es lo que manda MeshCore. Si luego alguien escribe `0xFD`
  en `header` y libera con `releasePacket`, se devuelve la ranura 0 aunque sea de otro paquete.
- Escenario de fallo: con Meshtastic y una trama larga F1 (> 184 B) esperando en la ranura 0, la app envía
  `CMD_SEND_RAW_PACKET` con el byte de cabecera `0xFD` y el slot que le toca llevó antes un TRANSPORT_FLOOD
  recibido (`[1]` suele ser 0 en MeshCore): `tryParsePacket` pone `header = 0xFD` y falla por versión,
  `releasePacket` libera la ranura 0. La siguiente trama larga F2 la toma y sobrescribe la cola de F1, que sale
  al aire con bytes de F2; al liberarse F1 se libera también la ranura de F2.
- Arreglo mínimo propuesto: que `StaticPoolPacketManager::allocNew` deje `header = 0` y
  `transport_codes[0] = 0` junto al `tx_window = 0` que ya pone (cerca `// ZEPHCORE:`). Así solo `sendForeign`
  puede dejar un paquete con cola.
- Cómo verificarlo: test de host en `dispatcher_tests.cpp`: recibir un TRANSPORT_FLOOD con códigos {X, 0},
  liberar, encolar una trama foránea de 200 B, pedir el mismo slot con `obtainNewPacket`, `tryParsePacket` de
  `{0xFD, 0x00}` y `releasePacket`: `ForeignTail::freeCount()` no debe subir.

### MESH-13. Un flood que llega con 63 saltos de 1 byte se repite con `path_len` 0x40: tamaño de hash 2 y 0 saltos
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (Arduino MeshCore, mismo código en `~/git/MeshCore` 0679dbef, y ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Mesh.cpp:467-473` (`routeRecvPacket`); `zephcore/include/mesh/Packet.h:95`
  (`setPathHashCount`)
- Causa: la guarda solo mira bytes: `(n + 1) * size <= MAX_PATH_SIZE`. Con hashes de 1 byte y `n = 63` da
  64 <= 64, se añade el hash en `path[63]` y `setPathHashCount(64)` hace `path_len = (path_len & ~63) | 64`: el bit 6
  cae en el campo de tamaño. El contador de saltos tiene 6 bits (máximo 63).
- Escenario de fallo: un repetidor con los valores por defecto (`flood_max` 64, `flood_max_unscoped` 64) recibe un
  flood de texto con `path_len` 63. `isFloodHopLimitExceeded` lo deja pasar (63 < 64) y sale al aire con
  `path_len` 0x40 y sin camino. Los nodos que no lo habían visto lo toman como un flood oído en directo con hashes
  de 2 bytes: se salta el límite de saltos y la detección de bucles (el contador vuelve a 0), y el destinatario
  aprende un camino vacío y falso hacia el emisor. Un atacante puede inyectar floods con cualquier camino de todos
  modos, así que el impacto es de corrección más que de seguridad.
- Arreglo mínimo propuesto: añadir `n < 63` a la condición de `routeRecvPacket` (cerca `// ZEPHCORE:`); es lo
  mismo que limitar a 63 saltos, que el formato no puede expresar de otro modo. Compatible en el aire.
- Cómo verificarlo: test de host: recibir un GRP_TXT por flood con `path_len` 63 en un mesh con
  `allowPacketForward` verdadero y comprobar que no se encola nada (hoy se encola con `path_len` 0x40).

### MESH-3. TRACE lee la siguiente entrada del camino aunque no quepa entera en el payload
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: upstream (Arduino MeshCore)   ·   ¿arreglado en origin/dev?: sí (`74a7e87e`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Mesh.cpp:162-165`
- Causa: solo se comprueba `offset >= len`; con `path_sz` 3 (entradas de 8 bytes) `isHashMatch` lee
  `1 << path_sz` bytes desde `payload[i + offset]`, hasta 7 más allá de `payload_len` (como `offset` es múltiplo
  de 8 y `len <= 175`, a lo sumo 1 byte fuera del array, sobre `_snr`).
- Escenario de fallo: un TRACE directo con `flags & 3 == 3`, `payload_len` 178 (`len` 169) y `path_len` 21
  (`offset` 168): se compara la clave propia contra 1 byte del paquete y 7 de restos de un uso anterior del slot.
  La decisión de reenviar depende de basura (solo lectura, sin fuga al aire).
- Arreglo mínimo propuesto: el de `origin/dev`: `else if (offset + (1 << path_sz) > len) { }` antes del
  `isHashMatch`, con cerca `// ZEPHCORE:`.
- Cómo verificarlo: test de host con ese TRACE bajo ASan.

### MESH-12. El reabastecimiento del presupuesto de duty cycle desborda tras ~5 días sin paquete vencido
- Severidad: baja
- Certeza: confirmado (seguido en código); escenario poco frecuente
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: sí (`74a7e87e`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:80-101` (`updateTxBudget`); llamado solo en `:142` (fin de TX) y `:450`
  (hay un paquete vencido)
- Causa: `refill = elapsed * duty_pct / 100` en 32 bits. `updateTxBudget` solo corre cuando hay algo que transmitir,
  así que `elapsed` cubre todo el tiempo sin tráfico propio: con 10 % desborda a partir de 2^32/10 ms (4,97 días),
  con 100 % a partir de 11,9 h. Además, el resto de la división se pierde en cada llamada.
- Escenario de fallo: un companion con el presupuesto gastado (ráfaga de mensajes) se queda 5 días sin enviar
  nada. El primer mensaje calcula un `refill` truncado módulo 2^32 (puede ser casi 0): el presupuesto sigue por
  debajo del umbral y el mensaje espera hasta ~6 min (umbral / 10 %) aunque la ventana de una hora ya se rellenó
  entera.
- Arreglo mínimo propuesto: el de `origin/dev`: si `elapsed >= duty_cycle_window_ms`, presupuesto al máximo; si no,
  calcular en 64 bits y llevar el resto (`tx_budget_frac`).
- Cómo verificarlo: test de host en `dispatcher_tests.cpp`: gastar el presupuesto, avanzar el reloj 5 días,
  encolar un paquete y comprobar que sale sin espera.

### MESH-11. La caída del EMA de contención se queda por debajo de 8 y el nodo despierta cada 5 s para siempre
- Severidad: baja (consumo)
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: sí (`74a7e87e`)   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/ContentionTracker.cpp:176` (paso de caída), `:198-214` (`msUntilNextTick`);
  `zephcore/src/Mesh.cpp:24-27`
- Causa: cada paso resta `_ema_x256 >> EMA_SHIFT` (1/8). Con `_ema_x256` entre 1 y 7 el paso es 0: el valor no
  baja más y nunca llega a 0. Mientras `_ema_x256 > 0`, `msUntilNextTick` pide el siguiente paso a
  `DECAY_PERIOD_MS` (5 s), y `Mesh::msUntilNextMaintenance` lo toma como plazo del bucle.
- Escenario de fallo: un repetidor o companion repite un flood y oye algún duplicado (el EMA sube), y luego el
  canal se queda tranquilo. Tras `STALE_MS` el EMA baja hasta un valor de 1 a 7 y se queda ahí: el hilo de malla
  despierta cada 5 s durante todo el tiempo que siga encendido, en lugar de solo para la medida de ruido (15 s).
  Triplica los despertares de mantenimiento de un nodo en reposo.
- Arreglo mínimo propuesto: el de `origin/dev`: `uint32_t step = _ema_x256 >> EMA_SHIFT; _ema_x256 -= step ? step : 1;`.
- Cómo verificarlo: test de host en `tests/unit/contention_tests.cpp` (el de `origin/dev`): EMA a 7, avanzar el
  reloj en pasos de 5 s; debe llegar a 0 y `msUntilNextTick` debe devolver `MAINTENANCE_IDLE`.

### MESH-8. Cambios del fork en ficheros portados sin cerca `// ZEPHCORE:`
- Severidad: baja
- Certeza: confirmado
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:407` y `:530` (`getContentionDelay(outbound)` sustituye a la línea
  de upstream `getCADFailRetryDelay()`), `:459`, `:479` (`tx_contended`), `:313`, `:324`, `:356` (`heard`);
  `zephcore/include/mesh/Mesh.h:51-53` (`_flood_is_reply`, `originFloodDelay`); `Dispatcher.h:232`, `:348-349`
- Causa: AGENTS.md exige que todo cambio en un fichero portado vaya cercado. Las funciones nuevas sí llevan
  `// ZEPHCORE: see Mesh.h`, pero estas líneas cambian el comportamiento de funciones de upstream sin marca, y
  el diff contra upstream no las distingue de código original.
- Escenario de fallo: en el próximo merge con `origin/dev` (que reescribió `checkSend` y `onLbtRefused`) el
  cambio de `getCADFailRetryDelay` a `getContentionDelay` se pierde sin conflicto visible.
- Arreglo mínimo propuesto: añadir `// ZEPHCORE:` a esas líneas.
- Cómo verificarlo: `git diff da3513aa HEAD -- <fichero>` y comprobar que cada hunk tiene la marca.

## Cobertura

Revisado a fondo: `Dispatcher.cpp` (RX, TX, presupuesto de duty cycle, re-contención, liberación, `sendForeign`),
`Mesh.cpp` (`onRecvPacket`, TRACE, `routeRecvPacket`, multipart, creación y envío de paquetes), `Packet.cpp`,
`StaticPoolPacketManager.cpp`, `ForeignTail.h`, `Utils.cpp`, `Identity.cpp` y su uso de monocypher, `adapters/rng/`,
`BaseChatMesh.cpp` (recepción, envío, contactos, conexiones, canales), `ClientACL`, `RegionMap`, `McFirewall.h` y su
uso en `RepeaterMesh`, `RateLimiter`, `AdvertDataHelpers`, `ContentionTracker.cpp`.

Revisado por encima: `NeighbourList.h`, `PingBot.h`, `TransportKeyStore`, `RoutingPolicy`, `OffgridPolicy`,
`UTF8Helpers`, `TxtDataHelpers`, `ContactInfo`, `ChannelDetails`, `TxStall.h`, interacción de `recontend` con la
espera de LBT (diseño de ADR 0019, sin hallazgo).

No revisado: el código de monocypher en sí (`zephcore/lib/monocypher`, se da por bueno el upstream); el envío y la
política de colas de Meshtastic sobre `Dispatcher` (está en `meshtastic.md`); los llamadores de roles salvo donde un
hallazgo lo necesitaba. Nada se ha ejecutado: ningún hallazgo tiene test de host ni prueba en hardware hecha en esta
revisión; las verificaciones propuestas están por hacer.

Descartado tras comprobarlo: desborde en `sendAnonReq`/`sendRequest` (el marco companion limita a 176 B);
`createAck` sin cota (todos los llamadores pasan `payload_len <= 184`); `notifyTxQueued` suprimiendo un despertar
(el callback usa `k_work_reschedule` y se conserva el plazo más temprano); `searchContactsByPrefix` con prefijo largo
(no tiene llamadores); desalojo de un admin en `putClient` (solo con las 32 entradas admin); `shareContactZeroHop`
sin comprobar `readFrom` (solo con un blob corrupto, ya cubierto por PERS-16).
