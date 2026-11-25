# Revisión: conectividad (`adapters/ble`, `usb`, `wifi`, `mqtt`, `ota`, `transport`, `observer_creds`, `CompanionWifi`, `RepeaterUplink`, `SECURITY.md`)

Estado: terminado

Resumen: 11 hallazgos. Alta 2 (CONN-1, CONN-2), media 5 (CONN-3 a CONN-7), baja 3 (CONN-8 a CONN-10), mejora 1 (CONN-11).
Ningún fichero del alcance cambia en el fork (`git diff da3513aa HEAD` vacío) ni en `origin/dev` (`git log HEAD..origin/dev` vacío):
todo es código heredado de ZephCore, y ninguno de los hallazgos está arreglado en `origin/dev`. El fork solo es origen de CONN-11.
Relación con otros informes: CONN-2 es la misma clase de fallo que PERS-2 en otro fichero (el arreglo de PERS-2 no lo cubre);
CONN-6 se suma a PERS-6 (SNTP sin autenticar) en el mismo escenario de red hostil; ROLE-2 (CLI remota del companion con timestamp 0)
también expone `get wifi.pwd`, que está en la lista blanca remota (`CompanionMesh.cpp:1064`), y no se repite aquí.

---

### CONN-1. El PIN BLE es siempre 123456 y fijo, también en placas con pantalla donde MeshCore usa uno aleatorio por sesión
- Severidad: alta
- Certeza: confirmado (seguido en código; comparado con `~/git/MeshCore` `examples/companion_radio/MyMesh.cpp:951-962`)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/ble/ZephyrBLE.cpp:116,536-545,966-974`; `zephcore/src/main_companion.cpp:1031-1036`; `zephcore/Kconfig:387-392`
- Causa: `ble_passkey` arranca en `CONFIG_ZEPHCORE_BLE_PASSKEY` (123456) y `auth_app_passkey` lo devuelve en cada emparejamiento. Con `prefs.ble_pin == 0` (valor de fábrica) nadie lo cambia. `auth_passkey_display` no hace nada, así que la pantalla nunca muestra un PIN. MeshCore, con `ble_pin == 0` y pantalla, genera un PIN aleatorio por sesión (`rng.nextInt(100000, 999999)`) y lo muestra; ZephCore no.
  Además, aunque el usuario fije otro PIN, sigue siendo estático. En Passkey Entry de Secure Connections el respondedor descarta en la ronda *i* si el bit *i* del PIN que supuso el iniciador es erróneo, así que un iniciador malicioso aprende un bit por intento fallido y recupera el PIN en 20 intentos como máximo. Zephyr no aplica la espera de "Repeated Attempts" (Core Spec Vol 3 Part H §2.3.6; no hay nada en `smp.c`) y `pairing_failed` solo desconecta.
- Escenario de fallo: un companion recién flasheado (o con `ble_pin` en 0) anuncia `MeshCore-<nombre>`. Cualquiera a alcance BLE empareja con 123456 (MITM cumplido, nivel L3/L4) y tiene el protocolo companion completo: lee la cola offline y los contactos, envía en nombre del nodo, cambia radio y canales, y `CMD_EXPORT_PRIVATE_KEY` (`CompanionMesh.cpp:2781-2788`, sin compuerta) le da la clave privada Ed25519. Con el vínculo creado, vuelve cuando quiera sin PIN. En una placa con pantalla, MeshCore habría exigido el PIN aleatorio que solo ve quien tiene el aparato delante. En nRF52, el mismo vínculo permite escribir 0x01 en el punto de control DFU (`ble_dfu.cpp:54-67`, solo pide `WRITE_AUTHEN`) y el bootloader Adafruit queda en modo OTA, que acepta imágenes sin firma: el compromiso se vuelve persistente.
- Arreglo mínimo propuesto: hacer lo que hace MeshCore. Con `ble_pin == 0` y pantalla (capacidad: el nodo de display en devicetree, la misma condición que ya usa la UI), `auth_app_passkey` devuelve `BT_PASSKEY_RAND`; Zephyr genera un PIN aleatorio por emparejamiento y llama a `passkey_display`, que lo pasa a la UI. Sin pantalla se queda 123456, como en MeshCore (documentado en `README.md:57`). Aparte, y con ADR, valorar la espera creciente tras fallos que pide la especificación para el PIN fijo.
- Cómo verificarlo: en una placa con pantalla y `ble_pin` 0, emparejar dos veces: el PIN pedido cambia y aparece en pantalla; 123456 falla. En una sin pantalla, 123456 sigue funcionando.

### CONN-2. `handleUplinkCommand` escribe hasta 256 bytes en el búfer remoto de 161: un `get/set uplink.<clave larga>` de admin desborda la pila
- Severidad: alta (requiere sesión de admin; solo en repetidores compilados con uplink)
- Certeza: confirmado (seguido en código)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no (el fichero no cambia en `origin/dev`)   ·   ¿ya conocido?: no. Es la misma clase de fallo que PERS-2, en otro sitio: el arreglo de PERS-2 (dos ramas de `CommonCLI.cpp`) no lo cubre
- Ubicación: `zephcore/app/RepeaterUplink.cpp:38-58` (todas las ramas `get`, en especial `:58`) y `:104`; llamador `zephcore/app/RepeaterMesh.cpp:965-971` (`temp[166]`, `reply = &temp[5]`) y `:1472-1487` (se llama antes que nada, tras quitar el prefijo `XX|`)
- Causa: todas las respuestas usan `snprintf(reply, CLI_REPLY_SIZE, ...)` (256), pero en remoto `reply` apunta a `temp[5]`, con 161 bytes útiles, y aún 3 menos si el comando llevaba el prefijo `XX|`. `CommonCLI::replyCap(sender_timestamp)` ya calcula el límite correcto (`CommonCLI.h:158-161`), pero `handleUplinkCommand` no recibe ni el timestamp ni el tamaño.
- Escenario de fallo: un admin remoto (o quien tenga la contraseña, `password` por defecto según PERS-2) envía `get uplink.` seguido de ~160 caracteres. La respuesta `unknown config: uplink.<clave>` ocupa ~184 bytes desde `temp[5]`: unos 23 bytes, con contenido elegido por el emisor, más allá de `temp` en la pila del hilo principal. `set uplink.<clave de ~155> x` hace lo mismo por `:104`. Las ramas `get` legítimas caben (`mqtt.host` como mucho 2 + 127).
- Arreglo mínimo propuesto: pasar a `handleUplinkCommand` la capacidad real (`_cli.replyCap(sender_timestamp)`, que ya descuenta el prefijo) y usarla en todos sus `snprintf`; acotar también la clave con `%.*s` en `:58` como en `:104`.
- Cómo verificarlo: test de host con ASan que llame a `RepeaterMesh::handleCommand` con `sender_timestamp != 0`, un `reply` de 161 bytes y `get uplink.` + 160 caracteres: hoy `stack-buffer-overflow`; con el arreglo, respuesta truncada.

### CONN-3. Se acepta emparejamiento Just Works: un dispositivo sin teclado se vincula, retiene la única conexión y expulsa los vínculos de los teléfonos legítimos
- Severidad: media
- Certeza: confirmado (seguido en código del host Zephyr; no probado en aire)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/ble/ZephyrBLE.cpp:181-202,407-414`; `zephcore/boards/common/zephcore_common.conf:104-113`; Zephyr `subsys/bluetooth/host/smp.c:2961-2981,3277-3285`, `conn.c:2923`
- Causa: el servicio NUS exige `*_AUTHEN` (L3), pero nada impide un emparejamiento que solo llega a L2. Una conexión nueva tiene `required_sec_level = L1` (`conn.c:2923`) y ZephCore, por ADR 0004, no lo sube; `remote_sec_level_reachable` devuelve 0 para L1, así que un iniciador con IO `NoInputNoOutput` (o `DisplayOnly`/`DisplayYesNo`, frente a nuestro `DisplayOnly`) empareja por Just Works. `CONFIG_BT_SMP_ENFORCE_MITM` solo pone el bit MITM en nuestra respuesta; no rechaza. Con `BT_BONDING_REQUIRED` el atacante pide vínculo y se guarda. Después, `security_changed` con L2 cancela el temporizador de 15 s (`:407-411`), cuyo comentario dice que existe para liberar la única ranura de una conexión que no sirve.
- Escenario de fallo: (a) un equipo a alcance con IO "sin entrada ni salida" conecta, empareja Just Works y se queda conectado: el anuncio se para (`BT_MAX_CONN=1`) y el teléfono del dueño no encuentra el nodo mientras dure. (b) Repite con 5 direcciones aleatorias: `CONFIG_BT_MAX_PAIRED=5` con `BT_KEYS_OVERWRITE_OLDEST` expulsa los vínculos del dueño; su teléfono falla al cifrar y tiene que olvidar el dispositivo y volver a emparejar. Cada emparejamiento escribe en `/lfs/settings`, sin límite. (c) Mientras ese enlace L2 sigue abierto, `zephcore_ble_is_active()` es verdadero (`ZephyrBLE.cpp:943-946`, solo mira `ble_tx_ready`), así que `MultiSerialInterface` le escribe cada trama; el CCC exige AUTHEN y nunca se activa, la cola BLE (24) se llena y no se vacía, `isWriteBusy()` queda en verdadero (`MultiSerialInterface.h:143-158`) y el volcado de contactos de una app conectada por USB o TCP se para (`main_companion.cpp:374`; `CompanionMesh.cpp:586-587,605-606` reintenta el paso y reenvía la misma trama al transporte que sí funciona).
- Arreglo mínimo propuesto: rechazar en el emparejamiento lo que no puede dar una clave autenticada, que es lo único que el servicio acepta. `CONFIG_BT_SMP_APP_PAIRING_ACCEPT=y` y un `pairing_accept` en `auth_cb` que devuelva `BT_SECURITY_ERR_AUTH_REQUIREMENT` salvo que `feat->io_capability` sea `BT_IO_KEYBOARD_ONLY` o `BT_IO_KEYBOARD_DISPLAY` (con nuestro `DisplayOnly`, son los únicos que llevan a Passkey Entry). No es una Security Request, así que respeta ADR 0004. Y, por coherencia con los permisos, que el temporizador y `ble_tx_ready` usen L3 en vez de L2.
- Cómo verificarlo: con `bluetoothctl` (agente `NoInputNoOutput`) o nRF Connect con IO "none", emparejar: hoy termina vinculado y conectado más de 15 s; con el arreglo, "Pairing Failed: Authentication Requirements". Comprobar que iOS, Android y Windows (Legacy) siguen emparejando con PIN.

### CONN-4. WiFi OTA del repetidor: AP abierta y subida sin autenticar que no se cierra nunca sola
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: igual que upstream MeshCore (`src/helpers/ESP32Board.cpp:14-40`: `softAP("MeshCore-OTA", NULL)`, sin plazo)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/ota/wifi_ota.c:374-380` (AP `WIFI_SECURITY_TYPE_NONE`), `:201-286` (`/upload` sin autenticación), `:444-497` (sin temporizador); `zephcore/helpers/CommonCLI.cpp:535-546`
- Causa: `start ota` levanta una AP abierta y un servidor HTTP cuyo `POST /upload` escribe el cuerpo en slot1 y llama a `boot_request_upgrade(BOOT_UPGRADE_PERMANENT)`. MCUboot solo comprueba el hash (`BOOT_SIGNATURE_TYPE_NONE`, ADR 0001/0011), así que cualquier imagen bien formada se instala. A diferencia de BLE OTA, que ZephCore acota a 600 s y a un vínculo autenticado (ADR 0011), aquí no hay ni ventana ni credencial: la AP queda abierta hasta `stop ota` o un reinicio.
- Escenario de fallo: un admin lanza `start ota` por la malla en un repetidor de un tejado y no llega a actualizar (o se olvida de `stop ota`). Días después, cualquiera que pase con un portátil se une a "ZephCore-OTA" y sube su propio firmware; el repetidor arranca con él de forma permanente (overwrite-only, sin vuelta atrás). Mientras tanto la AP abierta impide el light sleep (`zc_pm_block_sleep`).
- Arreglo mínimo propuesto: aplicar la misma regla que ADR 0011 ya fijó para BLE OTA: una ventana con plazo (`CONFIG_ZEPHCORE_BLE_OTA_WINDOW_S` o equivalente, prorrogada mientras hay subida) que llame a `wifi_ota_stop()`. Si se quiere control de acceso, una AP WPA2 con clave derivada o configurable, con ADR; la firma de imágenes en MCUboot es la defensa de fondo y también es una decisión de ADR.
- Cómo verificarlo: `start ota` en una Heltec V3 repetidor, esperar el plazo y comprobar que la AP desaparece y `get` del estado dice cerrada; antes del plazo la subida sigue funcionando.

### CONN-5. El transporte TCP da el protocolo companion completo, clave privada incluida, a cualquiera de la misma red, y nada lo advierte
- Severidad: media
- Certeza: confirmado (seguido en código)
- Origen: igual que upstream MeshCore (`SerialWifiInterface` tampoco autentica)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/transport/TcpCompanionTransport.c:199-229` (escucha en `INADDR_ANY:5000`), `:279-324`; `zephcore/app/CompanionMesh.cpp:2781-2788`; `docs/LINUX_NATIVE.md:294-320`
- Causa: el transporte TCP no tiene ningún equivalente al emparejamiento BLE: cualquier trama que llegue al puerto se entrega a `CompanionMesh` como si viniera del dueño. El listener se abre en todas las interfaces en cuanto hay IP.
- Escenario de fallo: el companion se une a la WiFi de un bar, una oficina o un evento. Otro equipo de la red conecta al puerto 5000 y manda `CMD_EXPORT_PRIVATE_KEY` (recibe la identidad Ed25519), `CMD_SET_DEVICE_PIN`, cambia canales, lee la cola offline o envía mensajes en nombre del nodo. `README.md`, ADR 0009 y `docs/ARCHITECTURE.md` no dicen en ningún sitio que la WiFi companion solo es segura en una red de confianza.
  En Linux nativo es peor: con sockets del host el mismo listener queda en todas las interfaces de la máquina, y `docs/LINUX_NATIVE.md:294-303,316-320` recomienda además publicar la consola CLI del nodo (que en local no tiene contraseña y responde a `get prv.key` o `erase`) con `socat ... TCP-LISTEN:6000` "from anywhere on the network", también como servicio de systemd.
- Arreglo mínimo propuesto: como es el comportamiento de upstream y cambiarlo rompería la app, lo mínimo es documentarlo (README, ADR 0009, `LINUX_NATIVE.md`, nota de versión): "TCP sin autenticación; úsalo solo en redes de confianza", y que la receta de `socat` escuche en `127.0.0.1` (acceso remoto por SSH) en vez de en todas las interfaces. Si se quiere cerrar de verdad, es una decisión con ADR (por ejemplo, exigir que la primera trama tras conectar sea el PIN BLE, con soporte en la app).
- Cómo verificarlo: con la placa en una WiFi de pruebas, conectar desde otro equipo con un cliente companion TCP (meshcore-cli por TCP) sin ningún paso previo y comprobar que responde a comandos de lectura.

### CONN-6. MQTT sobre TLS sin verificar el certificado: usuario y contraseña del broker salen hacia cualquiera que se haga pasar por él
- Severidad: media
- Certeza: confirmado (seguido en código; documentado en `docs/ARCHITECTURE.md:997` como decisión)
- Origen: upstream (ZephCore; Arduino MeshCore no tiene observer ni uplink MQTT)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/mqtt/ZephyrMQTTPublisher.c:237-247` (`TLS_PEER_VERIFY_NONE`, sin `sec_tag`), `:215-224` (credenciales en el CONNECT); `zephcore/adapters/mqtt/uplink_creds.h:21` (TLS por defecto); resolución por DNS de la red local en `:155-177`
- Causa: el cliente acepta cualquier certificado. El comentario lo justifica con las renovaciones de Let's Encrypt, pero esas cambian el certificado hoja, no la raíz: verificar contra la CA (no fijar la hoja) no se rompe al renovar. Sin verificación, el TLS solo protege contra quien escucha, no contra quien intercepta.
- Escenario de fallo: un repetidor con uplink o un observer conectado a una WiFi que controla otro (abierta, de evento, o con DNS envenenado). El atacante resuelve el host del broker a su IP, presenta un certificado autofirmado, recibe el CONNECT con `mqtt_user`/`mqtt_password` en claro dentro del túnel y puede publicar en el broker con la identidad del nodo, o falsear el estado "online". El mismo DNS da además la hora por SNTP (PERS-6).
- Arreglo mínimo propuesto: `TLS_PEER_VERIFY_REQUIRED` con la raíz del broker por defecto (ISRG Root X1 para Let's Encrypt) registrada con `tls_credential_add` en un `sec_tag`, y `hostname` para comprobar el nombre (ya se pasa para SNI). Si la flash o la RAM no lo permiten en alguna placa, que sea una opción explícita y documentada, no el comportamiento por defecto. Es un cambio de decisión: ADR.
- Cómo verificarlo: en una placa con uplink, apuntar `mqtt.host` a un `mosquitto` con certificado autofirmado: hoy conecta y entrega la contraseña (visible en el log del broker); con el arreglo, el handshake falla.

### CONN-7. TCP companion: el primer cliente se queda la plaza para siempre; una conexión medio abierta deja fuera a la app
- Severidad: media
- Certeza: confirmado (seguido en código; no reproducido en placa)
- Origen: upstream (ZephCore; Arduino MeshCore hace lo contrario)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/transport/TcpCompanionTransport.c:260-266,279-285`; `zephcore/boards/common/wifi_companion.conf` (sin `CONFIG_NET_TCP_KEEPALIVE`)
- Causa: con `client_fd >= 0` toda conexión nueva se cierra ("Second client rejected"). El cliente actual solo se libera si `zsock_recv` devuelve error o EOF, o si un envío falla. No hay keepalive TCP, ni plazo de inactividad en el `recv`, que bloquea sin límite. Upstream (`src/helpers/esp32/SerialWifiInterface.cpp:56-66`) hace lo contrario: un cliente nuevo sustituye al anterior.
- Escenario de fallo: el teléfono pierde la WiFi sin FIN (sale de cobertura, cambia de red, el sistema suspende la app). El nodo conserva la conexión muerta. Al volver, la app conecta y el nodo la rechaza. Solo se recupera cuando el nodo intenta enviar algo y la retransmisión TCP agota sus reintentos (minutos con tráfico de malla; nunca en una malla tranquila sin pushes). Lo mismo, a propósito: cualquier equipo de la LAN abre una conexión al puerto 5000 y no hace nada; la app del dueño queda fuera indefinidamente.
- Arreglo mínimo propuesto: hacer lo que hace upstream: al aceptar con un cliente activo, cerrar el anterior (`zsock_shutdown` del viejo bajo `sock_mu`, que despierta su `recv`) y servir al nuevo. Requiere que el bucle de RX deje de ser el mismo hilo que el `accept` (hoy el `accept` no vuelve a correr hasta que el cliente se va): un `zsock_poll` sobre `listen_fd` y `client_fd` en el mismo hilo basta, sin hilo nuevo.
- Cómo verificarlo: en una placa WiFi companion, conectar con `nc <ip> 5000` y dejarlo abierto; conectar la app: hoy la rechaza, con el arreglo la app entra y `nc` recibe el cierre. Segundo caso: conectar la app y cortar la WiFi del teléfono sin cerrar la app; reconectar en menos de un minuto.

### CONN-8. USB companion sin contrapresión: más de 4 tramas seguidas del host se pierden
- Severidad: baja
- Certeza: probable (camino seguido en código; que una app concreta encadene más de 4 comandos por USB no está comprobado)
- Origen: upstream (ZephCore; en Arduino los bytes esperan en el búfer CDC, que frena al host)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/usb/ZephyrCompanionUSB.cpp:31,148-154,351-357`
- Causa: el callback de la UART copia todo lo que llega a un anillo de 512 B (`ring_buf_put` descarta en silencio lo que no cabe) y `usb_rx_work_fn`, en la workqueue del sistema (cooperativa), parsea el anillo entero de una vez y mete cada trama en `usb_recv_queue`, de profundidad 4, con `K_NO_WAIT`. El hilo principal no corre mientras dura ese trabajo, así que la quinta trama de una ráfaga se descarta ("recv queue full"). BLE tiene 24 por exactamente este motivo (`Kconfig:394-402`: "at 12 the recv queue overran and dropped requests, stalling sync") y TCP espera con `K_MSEC(250)` y frena el socket.
- Escenario de fallo: un cliente por USB que encadena peticiones sin esperar respuesta (como hace la app con `CMD_GET_CHANNEL` según ese comentario; un paquete USB FS de 64 B lleva 12 de esas tramas de 5 B) pierde de la quinta en adelante y se queda esperando respuestas que no llegan.
- Arreglo mínimo propuesto: dar contrapresión en vez de tirar: cuando `usb_recv_queue` está llena, dejar de consumir el anillo (los bytes esperan) y reanudar desde `zephcore_usb_companion_recv` al sacar una trama; cuando el anillo se llena, `uart_irq_rx_disable` hasta que haya hueco, para que la clase CDC deje de aceptar paquetes del host. Así no hace falta subir la profundidad. El propio repo ya lo hace así en `adapters/transport/ZephyrUartStream.cpp` (`_rx_paused`, `uart_irq_rx_disable` con el anillo lleno, reactivado en `read()`).
- Cómo verificarlo: test en placa: escribir de golpe por el puerto 10 tramas `CMD_GET_CHANNEL` (índices 0-9) y contar respuestas: hoy llegan 4; con el arreglo, 10.

### CONN-9. TCP companion: el hilo principal espera al mutex del socket mientras la workqueue lo retiene en un envío bloqueante
- Severidad: baja
- Certeza: probable (seguido en código; la duración depende de cuándo `zsock_send` agota `SO_SNDTIMEO`, no medido)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/transport/TcpCompanionTransport.c:144-167` (envío bajo `sock_mu`), `:69-77` (`tcp_link_up` toma `sock_mu` con `K_FOREVER`), `:371-377,409-412,419-432` (llamadas desde el hilo principal)
- Causa: `tx_drain_work_fn` corre en la workqueue del sistema y llama a `sock_send_all` con `sock_mu` tomado; cada `zsock_send` puede bloquear hasta `TX_SEND_TIMEOUT_SEC` (2 s) si el cliente no lee. El hilo principal pasa por `tcp_link_up()` en cada `writeFrame`, `isConnected` y `txIdle` de `MultiSerialInterface`, y espera ese mismo mutex sin plazo. Durante ese tiempo también se para todo lo demás de la workqueue del sistema (drenaje BLE, parser USB, UI).
- Escenario de fallo: el teléfono suspende la app con el socket abierto (iOS lo hace) o un equipo de la LAN conecta y no lee. Cuando se llena la ventana TCP, el siguiente push bloquea la workqueue y el hilo principal hasta 2 s antes de cerrar el cliente; el cliente malicioso reconecta y lo repite con cada push. El bucle principal (malla, radio, reintentos) se retrasa esos segundos.
- Arreglo mínimo propuesto: que la comprobación de enlace no dependa del mutex que cubre la E/S: `client_fd` como `atomic_t` leído sin lock en `tcp_link_up()`; el cierre sigue bajo `sock_mu`. Lo de fondo (no bloquear la workqueue del sistema con E/S de red) sería enviar desde el hilo del listener o en una workqueue propia.
- Cómo verificarlo: cliente TCP que conecta y nunca lee, con tráfico de malla que genere pushes; medir con el log o con un GPIO el tiempo que el bucle principal tarda en procesar la siguiente trama LoRa.

### CONN-10. Ningún build de CI compila el observer ni el uplink del repetidor
- Severidad: baja
- Certeza: confirmado (búsqueda en `.github/workflows/build.yml`, todos los `zephcore.yml` y `custom_builder_web/`)
- Origen: upstream (ZephCore)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/boards/common/repeater_uplink.conf`, `zephcore/boards/common/observer.conf`; `.github/workflows/build.yml:172-180,350-351`; ningún `release.roles` incluye `observer` (solo `boards/example_board/zephcore.yml:38`, en un comentario)
- Causa: `ZephyrMQTTPublisher.c`, `observer_creds.cpp`, `RepeaterUplink.cpp` y `main_observer.cpp` solo se compilan con un fragmento pasado a mano. `AGENTS.md` pide que un rol o fragmento que ninguna fila de `zephcore.yml` construye esté en el paso "compile-only roles"; aquí no está.
- Escenario de fallo: un cambio en una interfaz común (por ejemplo `CommonCLI`, `MeshcoreJson`, la API de net de Zephyr en un salto de versión) rompe la compilación del observer o del uplink y nadie lo ve hasta que un usuario lo intenta. CONN-2 y CONN-6 viven en ese código.
- Arreglo mínimo propuesto: añadir al job ESP32 de `build.yml` dos pasos compile-only: `heltec_wifi_lora32_v3` con `observer.conf` y con `repeater.conf;repeater_uplink.conf`.
- Cómo verificarlo: que esos dos pasos pasen en CI.

### CONN-11. `SECURITY.md` del fork manda los informes al mantenedor de upstream y no cubre la capa Meshtastic
- Severidad: mejora
- Certeza: confirmado (lectura del fichero; idéntico a `da3513aa`)
- Origen: fork (fichero heredado sin adaptar)   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `SECURITY.md:7-10,23,27-33`
- Causa: el fichero es el de upstream: enlaza el aviso privado de `liquidraver/ZephCore`, da el correo de su mantenedor, dice que los arreglos van a `dev` (el fork publica desde `master`) y su alcance no menciona el puente Meshtastic, que solo existe en el fork.
- Escenario de fallo: quien encuentre un fallo en `CompanionMeshtastic.cpp` o en el CLI `z.*` lo reporta, como pide el fichero, a un mantenedor que no tiene ese código; el dueño del fork no se entera.
- Arreglo mínimo propuesto: adaptar contacto, rama soportada y alcance al fork (incluida la capa Meshtastic, y qué se reporta upstream y qué aquí).
- Cómo verificarlo: lectura.

---

## Alcance de la revisión

Revisado a fondo (leído entero, con llamadores y llamados):
- `adapters/ble/ZephyrBLE.cpp`, `ble_gatt_layout.cpp`, `ble_dfu.cpp`, `ble_internal.h`; configuración BT de `boards/common/zephcore_common.conf` y `esp32_common.conf`; ADR 0004 (sin Security Request, `USE_IDENTITY`: se cumplen) y 0011; camino del respondedor en `subsys/bluetooth/host/smp.c` de Zephyr (`remote_sec_level_reachable`, `smp_pairing_req`, `display_passkey`). Comparado con `examples/companion_radio/MyMesh.cpp` de MeshCore.
- `adapters/transport/TcpCompanionTransport.c`, `frame_txq.*`, `companion_framing.h`, `CompanionInterfaces.h`, `ZephyrUartStream.cpp`; `helpers/MultiSerialInterface.h` (agregación de `isConnected`/`isWriteBusy`/`writeFrame`). Comparado con `SerialWifiInterface.cpp` de MeshCore.
- `adapters/usb/ZephyrCompanionUSB.cpp`, `ZephyrUSBCDC.cpp` (parser V3, CLI de texto, DTR, toque de 1200 baudios).
- `adapters/ota/wifi_ota.c`, `ble_ota.c`, `ble_ota.conf`, `ota_page.html`; `start/stop ota` en `CommonCLI.cpp:520-578`. Comparado con `ESP32Board::startOTAUpdate` de MeshCore.
- `adapters/wifi/ZephyrWiFiStation.c`, `adapters/mqtt/ZephyrMQTTPublisher.c`, `uplink_creds.h`, `app/observer_creds.*`, `app/RepeaterUplink.cpp` (incluido `handleUplinkCommand` y su llamador remoto `RepeaterMesh.cpp:935-990,1459-1487`), `app/CompanionWifi.*`, `SECURITY.md`.

Comprobado y descartado:
- Longitudes de trama en BLE, TCP y USB: las tres rechazan `len == 0` y `len > MAX_FRAME_SIZE` antes de copiar; los destinos son de `MAX_FRAME_SIZE`.
- Escrituras preparadas (offset ignorado en `secure_nus_rx_write`): `CONFIG_BT_ATT_PREPARE_COUNT` queda en 0, Zephyr las rechaza.
- `current_conn` entre el hilo de BT y la workqueue: los dos son cooperativos en un solo núcleo; `disconnected` y `tx_drain_work_fn` no se interrumpen entre sí.
- MQTT: el payload (hasta 1 KB) no pasa por `tx_buf` de 512 B (Zephyr lo envía como segundo iovec); el observer no se suscribe.
- `observer_creds_load` no termina las cadenas en NUL, pero el escritor (`StrHelper::strncpy`) siempre lo hace; solo un fichero corrupto llegaría ahí. `wifi_pwd[64]` cabe en las respuestas.
- BLE OTA: la subida solo se acepta dentro de la ventana y con vínculo autenticado; el servicio SMP se da de baja al cerrar.

RAM de cada transporte (de las definiciones, no medido): BLE ~8,9 KB (2 colas de 24 x 178 B, más retry y overflow); TCP ~5,9 KB (2 colas de 8 x 178 B y pila de 3 KB); USB ~3,4 KB (anillos de 512 B y 2 KB, cola de 4); MQTT ~23 KB (pila de 12 KB, cola de 8 x 1028 B, `s_stage` y `s_drain` de 1 KB, búferes de 768 B); BLE OTA ~7,5 KB según ADR 0011.

Solo por encima o no verificado:
- WiFi OTA y uplink en la misma placa: `wifi_ap_start` usa `net_if_get_default()` (la interfaz de la estación), cambia la pasarela a 192.168.100.1 y no la restaura en `wifi_ap_stop`, y la estación reintenta conectar cada 5-10 s mientras la AP está activa. Sospecho que se pisan (OTA imposible o uplink roto hasta reiniciar), pero depende del driver WiFi de ESP32 en Zephyr y no lo he seguido hasta el final. La combinación no la compila ningún CI (CONN-10).
- Bloqueo de la workqueue del sistema hasta 8 s por `sntp_simple` dentro del callback de DHCP (`ZephyrWiFiStation.c:112-118`): leído, sin evaluar el impacto en otros manejadores de `net_mgmt`.
- Reintento de WiFi a intervalo fijo de 5/10 s sin backoff (`ZephyrWiFiStation.c:155-167`): coste de energía con la AP caída, no medido.
- Bucle MQTT con `poll` de 100 ms para drenar la cola (`ZephyrMQTTPublisher.c:285-346`): despierta la CPU 10 veces por segundo; no es el hilo principal, así que no viola AGENTS.md, pero cuesta energía en los builds con light sleep.
- Bootloader Adafruit en modo OTA (nRF52) tras el DFU sin botón: acepta imágenes sin firma; es el comportamiento de upstream y del bootloader, no del código revisado.
- No revisado: `helpers/MultiSerialInterface.h` más allá de esas tres funciones, `custom_builder_web/js/flasher-serial.js` (cambio del fork), los parches de Zephyr del stack USB/WiFi (`zephcore/patches/zephyr/0017-0019`), la pila de MCUmgr y MCUboot.
- Nada se ha compilado ni probado en hardware: todas las pruebas propuestas están por hacer.
