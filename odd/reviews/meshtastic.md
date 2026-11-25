# Revisión del área meshtastic (commit 2541fe5f)

Estado: terminado

Ya revisado: `MtWire`, `MtBridge`, `MtPki`, `MtDiscriminate`, `MtFirewall`, `MtKeyCache`, `MtPolicy`, `MtZeroHop`,
`MtConfig`, `MtRelayPort`, `RepeaterMeshtastic`, `VContact`, `CompanionMeshtastic`, las rutas de envío de canal y DM en
`CompanionMesh.cpp`, el alta de contactos de `BaseChatMesh`, `Dispatcher::recontend`, los ADR 0012/0013/0015/0016/0019 y
`docs/zephtacore/README.md`. Lo he contrastado con el firmware Meshtastic `develop` (`Router`, `ReliableRouter`,
`NextHopRouter`, `FloodingRouter`, `NodeDB`, `RoutingModule`, `RadioLibInterface`, `mesh.proto`), copiado en
`work-meshtastic/upstream/`.

Toda el área es código del fork: `origin/dev` no contiene nada de Meshtastic (`git log HEAD..origin/dev` sobre estas
rutas está vacío). Por eso la columna «¿arreglado en origin/dev?» vale n.a. en todos los hallazgos.

Pruebas de host propias: `work-meshtastic/harness/review_mt.cpp`. Incluye `tests/unit/meshtastic_tests.cpp` para reutilizar
sus ayudas y se compila contra una copia de `2541fe5f` sacada con `git archive` en `work-meshtastic/src/`. Se ejecutan con
`./review_mt --case REVIEW-MT-<x>`. Las cinco pasan, es decir, reproducen el fallo descrito. Los casos fijados
`UNIT-MT-008` y `UNIT-MT-056` también pasan con esa copia.

Resumen: 1 alta, 4 medias, 5 bajas y 1 mejora.

## Hallazgos

### MT-1. Un NodeInfo forjado renombra un contacto y cambia su rol aunque no traiga la clave fijada
- Severidad: alta
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMeshtastic.cpp:611`, también `zephcore/app/RepeaterMeshtastic.cpp:106`
- Causa: `MtSink::onNodeInfo` ejecuta `neighbours().setName`, `noteRole` y `mtUpsertContact` (que reescribe `c->name` en
  `:472-474`) antes de `mtStoreKey`, y la clave fijada no se comprueba en ningún punto de ese camino. El conflicto solo
  llega a un `LOG_WRN` (`:969-971`) cuando ya se ha aplicado todo. El contacto se busca por una clave derivada del número
  de nodo (`mtKeyFor`), así que basta con poner `from` igual al nodo de la víctima. Upstream descarta el NodeInfo entero si
  la clave anunciada difiere de la guardada o falta (`NodeDB.cpp:4065-4070`), y rechaza el que anuncia nuestra propia clave
  (`NodeDB.cpp:4040-4063`).
- Escenario de fallo: un companion con Meshtastic activo y el canal público (PSK `AQ==`, conocida por todos) tiene fijado
  el contacto `!a1b2c3d4` llamado «Ana». Un tercero emite un NodeInfo con `from=0xa1b2c3d4`, `long_name="Banco"` y otra
  clave o ninguna. La app muestra «Banco» en el contacto, en los vecinos y en los mensajes que lleguen después de ese
  nodo, sin que haya cambiado su clave. Con `z.mt.zerohop auto`, anunciar `role=ROUTER` para un vecino además mete ese
  nodo en la tabla de nodos fijos (`MtZeroHop.h:33`): a partir de ahí, lo que repite no gasta salto. El repetidor
  (`RepeaterMeshtastic.cpp:106-110`) aplica nombre y rol igual, y ni siquiera guarda claves.
- Arreglo mínimo propuesto: en `onNodeInfo`, si hay clave guardada para `from` y el NodeInfo no trae esa misma clave,
  salir sin tocar nombre, rol ni contacto, como hace upstream. Rechazar también la clave nula y la nuestra. En el repetidor
  el remedio coherente es no aceptar un cambio de rol de un nodo ya conocido sin clave (decisión de diseño a tomar).
- Cómo verificarlo: la regla (clave igual, distinta, ausente, nula, propia) es lógica pura y admite un test de host si se
  extrae a una función de `MtPolicy.h`, como `createsContact`. En hardware: dos nodos Meshtastic y el companion. Uno
  suplanta al otro con `--set-owner` y un `from` falseado (firmware modificado o meshtastic-python con radio en modo raw).

### MT-2. Un DM retransmitido no se vuelve a confirmar y el emisor lo da por fallido aunque llegó
- Severidad: media
- Certeza: confirmado (prueba de host `REVIEW-MT-2`)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no (el README habla del ACK repetido tras
  un *replay*, que es otro caso)
- Ubicación: `zephcore/helpers/meshtastic/MtBridge.cpp:270`
- Causa: un DM PKI cuyo `(from, id)` ya está en la tabla vuelve como `RX_DUPLICATE` antes de descifrarse. No llama a
  ningún método del sink, así que `onDirectText` y `mtSendAck` (`CompanionMeshtastic.cpp:710-712`) no se ejecutan. En
  upstream, un duplicado oído directamente de su origen (0 saltos), dirigido a nosotros, con `want_ack` y ya fuera de la
  cola de TX, recibe el ACK otra vez (`NextHopRouter.cpp:163-170`).
- Escenario de fallo: un nodo Meshtastic vecino envía un DM con `want_ack`. ZephCore lo entrega y su ACK se pierde en el
  aire. El emisor lo retransmite con el mismo id (hasta 5 intentos, `NextHopRouter.h:115`). ZephCore descarta todas las
  copias sin responder, y la app Meshtastic acaba marcando con `MAX_RETRANSMIT` un mensaje que sí llegó. La prueba de host
  muestra que la segunda copia devuelve `RX_DUPLICATE` y que el sink recibe una sola llamada.
- Arreglo mínimo propuesto: si la copia duplicada es un DM dirigido a nosotros con `want_ack` y `hopsAway() == 0`, abrirla
  de nuevo (para no confirmar copias forjadas) y pedir al sink que reenvíe el ACK con el mismo `request_id`, sin volver a
  entregar el texto. `UNIT-MT-056` sigue valiendo: la copia sigue siendo `RX_DUPLICATE` y no se entrega.
- Cómo verificarlo: un test de host en el que la segunda copia produzca una petición de ACK y ninguna entrega. En aire:
  DM desde un nodo Meshtastic con el ACK de ZephCore bloqueado (o con el emisor lejos) y comprobar en la app que el envío
  se confirma.

### MT-3. El alta de contactos por NodeInfo ignora el límite de saltos, y un mesh Meshtastic público llena la tabla compartida con MeshCore
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: en parte. El README (`docs/zephtacore/README.md:96`)
  dice que los nodos MT cuentan contra `MAX_CONTACTS`, y la memoria de la regla de contactos lo motivó. Que se salte
  `autoadd_max_hops` no figura en ningún sitio.
- Ubicación: `zephcore/app/CompanionMeshtastic.cpp:437`
- Causa: `mtUpsertContact` aplica `shouldAutoAddContactType` y luego `addContact`, pero nunca `getAutoAddMaxHops()`. El
  alta MeshCore sí lo aplica (`BaseChatMesh.cpp:183-189`). `Sink::onNodeInfo` ni siquiera recibe los saltos. Con los
  valores por defecto (`manual_add_contacts=0`, `autoadd_config=0` por el `memset` de `CompanionMesh.cpp:101`) se crea un
  contacto por cada NodeInfo con nombre y clave, venga de donde venga. Si `AUTO_ADD_OVERWRITE_OLDEST` está activo,
  `allocateContactSlot` (`BaseChatMesh.cpp:88-111`) expulsa el contacto no favorito con `lastmod` más antiguo. Los
  contactos MT renuevan su `lastmod` con cada NodeInfo o Position (`:479-480`), así que los primeros en caer son los de
  MeshCore.
- Escenario de fallo: un companion con Meshtastic activo en una ciudad con un mesh LongFast de unos 300 nodos (todos con
  firmware 2.5 o posterior anuncian clave). El usuario ha puesto `auto_max=2`, es decir, solo contactos directos. En unas
  horas la tabla (350, o 50 a 300 en placas ESP32) se llena de nodos a 3-7 saltos. Desde ese momento los adverts MeshCore
  nuevos ya no dan de alta contactos (`onContactsFull`), o, con overwrite, borran contactos MeshCore con su ruta y su
  secreto. Además, cada nodo nuevo hace una escritura en flash de `mtkeys`. Cualquiera con la PSK pública provoca lo mismo
  a propósito variando `from`.
- Arreglo mínimo propuesto: pasar `h.hopsAway()` a `onNodeInfo` (y a `onDirectText` y `onPosition` cuando crean contacto)
  y aplicar en `mtUpsertContact`, para los contactos nuevos, la misma condición `max_hops > 0 && hops >= max_hops` que usa
  MeshCore. Si no se cumple, `onDiscoveredContact` sin alta, como ya hace la rama de `shouldAutoAddContactType`.
- Cómo verificarlo: un test de host de la condición. En hardware: companion con `auto_max` bajo cerca de un mesh
  Meshtastic con varios saltos, y comprobar que `get contacts` no crece con nodos lejanos.

### MT-4. Un frame opaco sin autenticar con el `(from, id)` de otro bloquea el frame real, y 64 opacos vacían la tabla de duplicados
- Severidad: media
- Certeza: confirmado (pruebas de host `REVIEW-MT-4a` y `REVIEW-MT-4b`)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: en parte. El README (`:85`) acepta que un
  replay tras 64 frames se entregue otra vez, pero no que esos 64 frames puedan ser basura sin clave, ni el envenenamiento.
- Ubicación: `zephcore/helpers/meshtastic/MtBridge.cpp:315`
- Causa: con relay activo y `relay_opaque` (lo normal en un repetidor, `MtConfig.h:175`), un frame que ningún canal abre
  se guarda con `remember(h.from, h.id)` en la misma tabla de 64 entradas que deduplica el tráfico descifrado. Ese frame
  no está autenticado: su cabecera puede llevar cualquier `from` e `id`. Upstream lleva el relay opaco con una tabla
  propia, aislada de `PacketHistory` (`NextHopRouter.cpp:29-96`).
- Escenario de fallo:
  - Envenenamiento (`4a`): un atacante oye a A antes que nosotros y emite al momento un frame basura con la cabecera de A
    (mismo `from` e `id`, otro hash de canal). El relay ZephCore lo repite (`relay_len=36`). Cuando llega el frame real,
    lo trata como duplicado (`RX_DUPLICATE`, 0 textos entregados). En un companion con relay, el usuario no ve el
    mensaje. Si el frame real llega por un relay, ZephCore tampoco lo repite.
  - Vaciado (`4b`): 64 frames basura con ids distintos, que no necesitan ninguna clave, expulsan todas las entradas. Un
    texto capturado antes y reinyectado se entrega por segunda vez (`RX_HANDLED`, 2 textos). Un DM reinyectado se vuelve a
    confirmar.
- Arreglo mínimo propuesto: deduplicar los frames opacos en una tabla propia, como upstream, y no escribir en `_seen` nada
  que no se haya podido descifrar.
- Cómo verificarlo: las dos pruebas de host deberían fallar después del arreglo, y `UNIT-MT-056` debería seguir pasando.

### MT-5. Un slot `#mt-*` usa por MeshCore un secreto derivable de su nombre: entra texto de cualquiera y sale texto del usuario sin la PSK del canal
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: sí, en parte. La entrada es una consecuencia
  aceptada del ADR 0015 («its group messages land in that slot»). La salida desde la UI del dispositivo es un límite del
  README (`:89`), pero sin mencionar lo que supone para la confidencialidad. Sigue vigente.
- Ubicación: `zephcore/helpers/ui-joystick/joystick_ui_task.cpp:308`, `zephcore/helpers/BaseChatMesh.cpp:421`,
  `zephcore/app/CompanionMesh.cpp:1144`
- Causa: el slot `#mt-<name>` lleva como secreto MeshCore SHA256 de su nombre, que cualquiera puede calcular. Su PSK real
  vive en `prefs.json` `mt{}`. Lo que llega por MeshCore con ese secreto se muestra en el mismo slot que lo cifrado con la
  PSK. La UI de joystick envía con `sendGroupMessage` y no pasa por `mtSendChannelText`.
- Escenario de fallo: el usuario añade el canal privado «Familia» (PSK propia) con `z.mt.channel.add` y lo escribe desde
  el joystick. El texto sale por MeshCore cifrado con SHA256("#mt-familia"), legible por quien adivine el nombre. En
  sentido contrario, cualquier nodo MeshCore que añada `#mt-familia` escribe en ese slot como si fuera del canal privado.
- Arreglo mínimo propuesto: en la UI, enviar los slots que tienen entrada `mt{}` por el mismo camino que la app
  (`mtSendChannelText`), o rechazar el envío. Para la entrada, decidir en un ADR si un slot con PSK privada debe aceptar
  tráfico MeshCore.
- Cómo verificarlo: en hardware con pantalla de joystick, enviar a un slot `#mt-*` y comprobar con un segundo nodo
  MeshCore que no lo descifra.

### MT-6. El ACK con `want_ack` que manda un nodo Meshtastic no se responde, así que lo retransmite hasta 4 veces
- Severidad: baja
- Certeza: confirmado (seguido en código de ambos lados; no medido en el aire)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/meshtastic/MtBridge.cpp:420`
- Causa: nuestros DM llevan `want_ack` (`CompanionMeshtastic.cpp:889`). Para un DM de texto, upstream contesta con un ACK
  que a su vez lleva `want_ack` (`ReliableRouter.cpp:131-135`, `:310-337`) y lo mete en retransmisión con 5 intentos
  (`ReliableRouter.cpp:30-38`). Lo para un ACK de 0 saltos, que upstream manda a toda respuesta directa
  (`ReliableRouter.cpp:140-144`), o un ACK implícito al oír que alguien lo repite. ZephCore solo pasa el ACK a `onAck`
  (`:420-426`) y no contesta nada.
- Escenario de fallo: el companion y un nodo Meshtastic se oyen directamente, sin un tercero que repita. Por cada DM, el
  nodo Meshtastic transmite su ACK 5 veces en lugar de 1 y ocupa su cola y el canal. En un mesh con repetidores, el ACK
  implícito lo corta antes.
- Arreglo mínimo propuesto: si llega una respuesta (`request_id` o `reply_id` distinto de 0) con `want_ack`, dirigida a
  nosotros y con `hopsAway() == 0` o `next_hop` puesto, mandar un ACK con hop limit 0, como upstream.
- Cómo verificarlo: test de host de la decisión. En aire: un DM a un nodo Meshtastic vecino y contar en su log
  `Send retransmission` del ACK.

### MT-7. Un texto unicast cifrado con la clave del canal se muestra como mensaje público del canal
- Severidad: baja
- Certeza: confirmado (seguido en código; `UNIT-MT-008` fija que el bridge lo entrega con `to`)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMeshtastic.cpp:523`, origen en `zephcore/helpers/meshtastic/MtBridge.cpp:340`
- Causa: el bridge entrega a `onText` lo que va `to == kBroadcast || to == _self`, y el companion descarta `to` con
  `(void)to` y lo entrega como mensaje de canal. Upstream rechaza los DM «legacy» cifrados por canal (`Router.cpp:1063-1065`),
  salvo en modo radioaficionado.
- Escenario de fallo: un nodo con firmware anterior a 2.5, o en modo licensed, manda un DM a ZephCore. Aparece en el
  canal como si fuera público, sin ACK (el emisor lo da por fallido), y en el slot `Bots` dispara una respuesta pública
  del bot.
- Arreglo mínimo propuesto: en `CompanionMesh::MtSink::onText`, ignorar `to != mt::kBroadcast`, como upstream. El bridge
  y `UNIT-MT-008` no cambian.
- Cómo verificarlo: test de host del sink, o en aire con un nodo en modo licensed.

### MT-8. Los DM a un contacto Meshtastic de 152 a 160 bytes fallan con `ERR_BAD_STATE`
- Severidad: baja
- Certeza: confirmado (prueba de host `REVIEW-MT-6`)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: en parte (el README habla de unos 164
  caracteres, que vale para canal pero no para DM)
- Ubicación: `zephcore/helpers/meshtastic/MtBridge.h:21`, `zephcore/app/CompanionMeshtastic.cpp:886`,
  `zephcore/app/CompanionMesh.cpp:2084`
- Causa: `kMaxTxFrame = 184`. Menos 16 de cabecera, 12 de CCM y 5 de `Data`, quedan 151 bytes de texto, mientras que la
  app admite `MAX_TEXT_LEN = 160`. `buildDirectText` devuelve 0 y el comando responde con un error genérico.
- Escenario de fallo: un DM de 80 caracteres con acentos o emojis (más de 151 bytes en UTF-8) a un contacto Meshtastic
  no sale, y la app solo muestra un error.
- Arreglo mínimo propuesto: dimensionar el frame del DM al máximo que acepta `sendForeign` (255, con `ForeignTail`) o, si
  se prefiere quedarse en 184, devolver un error de tamaño. La primera opción iguala lo que ya se permite al repetir.
- Cómo verificarlo: la prueba de host (151 se construye, 152 no) y un DM de 160 bytes en aire.

### MT-9. Reintentar un DM después de haber enviado otro le da un id nuevo y el destino lo recibe dos veces
- Severidad: baja
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/app/CompanionMeshtastic.cpp:887`
- Causa: `_mt_last_direct` guarda solo el último DM enviado. Al enviar B se pierde el registro de A, y el reintento de A
  (`attempt > 0`) no lo reconoce y sale con un id nuevo y una entrada nueva en `_ack_table`.
- Escenario de fallo: el usuario envía A a Ana y B a Luis. La app reintenta A por falta de ACK. Ana lo recibe dos veces,
  porque Meshtastic deduplica por `(from, id)`.
- Arreglo mínimo propuesto: buscar el id del intento anterior en `_ack_table` por contacto y timestamp en lugar de en un
  único registro.
- Cómo verificarlo: test de host con la secuencia A, B, reintento de A (mismo id).

### MT-10. La re-contención de un frame Meshtastic puede adelantarlo, y Meshtastic nunca lo adelanta
- Severidad: baja
- Certeza: confirmado en código; efecto en el aire no medido
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no (ADR 0019 está pendiente de medir, pero
  da por hecho que se comporta como Meshtastic)
- Ubicación: `zephcore/src/Dispatcher.cpp:197`
- Causa: tras cada frame, un paquete foráneo pasa a `now + getContentionDelay(pkt)` aunque su turno estuviera más
  adelante. Upstream calcula `min(max(tx_after + add, now + add), now + 2·worst)` (`RadioLibInterface.cpp:732-733`):
  empuja el turno, nunca lo adelanta (salvo el tope). El ADR 0019 (`:13`, `:22`) describe esto como «restarts the delay ...
  as Meshtastic does».
- Escenario de fallo: un relay con SNR alto (ventana 2^8) tenía su turno a 1,5 s. Oye un frame corto y vuelve a sortear,
  esta vez 0,2 s, de modo que se adelanta a nodos con menos SNR que debían ir antes. Se pierde parte del orden por SNR en
  el que se basa la supresión de repeticiones.
- Arreglo mínimo propuesto: para los paquetes foráneos, aplicar la fórmula de upstream en `recontend`, o corregir el ADR
  si la diferencia es intencionada. No toca la forma de las ventanas ni la decisión de no cancelar por eco.
- Cómo verificarlo: test de host en `contention_tests` (un turno lejano nunca se adelanta tras un frame) y repetir el
  modelo de cinco nodos del ADR con las dos fórmulas.

### MT-11. Los ACK y las respuestas de NodeInfo salen con el hop limit configurado (7) en lugar de los saltos usados + 2
- Severidad: mejora
- Certeza: confirmado (seguido en código)
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/helpers/meshtastic/MtBridge.cpp:460`
- Causa: todos los frames propios usan `_hops`, que en `zephtacore.conf` vale 7. Upstream responde con
  `getHopLimitForResponse` (saltos que usó la petición + 2, con el tope de la configuración, `RoutingModule.cpp:64-81`).
- Escenario de fallo: el ACK a un vecino directo inunda hasta 7 saltos en un mesh público, cuando upstream lo limitaría
  a 2.
- Arreglo mínimo propuesto: pasar a `buildAck` y `buildNodeInfoTo` el hop limit calculado a partir de la petición.
- Cómo verificarlo: test de host del hop limit del ACK según los saltos de la petición.

## Descartado tras intentar refutarlo

- Falsos positivos de `decodeData` sobre tramas MeshCore (backlog B10, ya conocido). Con 200 000 buffers aleatorios por
  longitud (`REVIEW-MT-B10`), acepta un 0,007 % a 20 B, un 0,002 % a 40 B y ninguno a 170 B. Hay que multiplicarlo por
  1/256 de que `raw[13]` coincida con el hash de un canal: el orden de magnitud es 1e-7 por trama. No se reporta.
- Nonce PKI y CCM, CTR de canal, hash de canal, expansión de PSK, `relayByte`, números de campo protobuf (incluido
  `want_response` en el 3), `rebroadcastCw` y los retardos de cliente y router coinciden con upstream.
- La semilla de ids se sortea en cada arranque. `tryParsePacket` rechaza versión > 0, así que `0xFD` no llega del aire.
- Límites de buffers en `handleFrame`, `ForeignTail`, `encodeTelemetry`, `buildPosition`, CLI `z.mt.*` y base64: correctos.
- Salida por MeshCore de un slot `#mt-*` sin entrada (`CompanionMesh.cpp:2162-2171`): `mtSlotRewritten` impide que la
  app cree uno. Solo queda la UI de joystick (MT-5).
- Clave de sesión que conserva `aesEcb` en `MtRelayPort.cpp:25-46`: la clave privada está en la misma RAM, así que no
  hay ganancia práctica. No se reporta.

## Cobertura

- A fondo: decodificación de entrada (`MtWire`), `MtBridge::handleFrame` y sus ramas PKI, canal, opaco y relay, el sink
  del companion (NodeInfo, texto, DM, ACK, Position), alta de contactos y claves, envío de DM y de canal, dedupe, la regla
  de zero-hop por rol, y la comparación con upstream de ACK, re-ACK, legacy DM, fijación de clave y retardos.
- Por encima: `MtFirewall`, `MtConfig` y los comandos `z.mt.*` (límites y parseo), `VContact`, `RepeaterMeshtastic` (sin
  contactos ni PKI), `Dispatcher::recontend` y el ADR 0019, `docs/zephtacore/CAD.md`.
- Sin revisar o sin verificar:
  - La app Meshtastic por BLE: no está integrada en `2541fe5f` (ni `MtApp` ni `ZEPHTACORE_MT_APP` aparecen en el árbol),
    así que no se ha podido revisar.
  - Pila del hilo principal en la cadena `onRecvForeign → handleFrame → onNodeInfo → mtUpsertContact → putMtKey`: no
    medida.
  - RAM total del área: no medida.
  - Política XEdDSA STRICT de upstream (descarta frames sin firma): ZephCore no firma; impacto no evaluado en aire.
  - Ningún hallazgo se ha probado en hardware.
