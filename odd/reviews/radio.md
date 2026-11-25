# Revisión del área radio (commit 2541fe5f)

Estado: terminado

Revisado: ver la sección final. 2 altas, 5 medias, 5 bajas, 1 mejora.

Nota sobre rutas: `sx126x.c:N` es la línea en el árbol Zephyr con los parches aplicados (`scratchpad/review/work-radio/zephyr/drivers/lora/native/sx126x/sx126x.c`). El fuente real es `zephcore/patches/zephyr/0003-lora-sx126x-native.patch`. `lr11xx_lora.c` y `lr20xx_lora.c` están en `zephcore/patches/zephyr-new/drivers/lora/<chip>/`.

## Hallazgos

### RADIO-1. La recuperación del timeout de TX no devuelve el SX126x a RX
- Severidad: alta
- Certeza: confirmado en código (la pérdida de TX_DONE que lo dispara no se ha observado en hardware)
- Origen: upstream (adaptador y parche `0003`, ambos en la base `da3513aa`)   ·   ¿arreglado en origin/dev?: no (la espera vence y llama a `startReceive()` igual)   ·   ¿ya conocido?: sí, como sospecha en la revisión de drivers; sigue vigente
- Ubicación: `zephcore/adapters/radio/LoRaRadio.cpp:138-153,216-231,704-707`; `sx126x.c:1930-1934,2104-2116,2284-2288,3494-3497`
- Causa: la espera del adaptador vence a `max(5000, 2·air+1000)` ms y llama a `startReceive()`. En SX126x, la entrada a RX es un CAS desde IDLE; con el estado en TX falla sin tocar el chip. El adaptador deja `_in_recv_mode=0` y `_tx_active=0`, y nadie saca al driver del estado TX.
- Escenario de fallo: se pierde TX_DONE (flanco de DIO1 perdido, o el TX abortado por la carrera de RADIO-3). El estado queda en TX: cada `send_async` devuelve `-EBUSY`, el Dispatcher lo trata como canal ocupado (`Dispatcher.cpp:406-438`) y cada 60 s relaja y persiste el umbral de CAD. El nodo no vuelve a transmitir ni recibir hasta reiniciar. Si el que vence es el timeout del chip (10 s), el driver vuelve a dormir y nadie reinicia RX hasta el siguiente TX.
- Arreglo mínimo propuesto: que el timeout del adaptador pase por una operación del driver que aborte el TX en curso (standby, estado a IDLE) antes de `startReceive()`, igual que hacen LR11xx y LR20xx, cuyo `recv_async` sale de TX con `start_rx` (`lr20xx_lora.c:2403-2414`).
- Cómo verificarlo: en hardware, enmascarar TX_DONE con el depurador durante un TX y comprobar que el nodo vuelve a recibir tras la espera.

### RADIO-2. El CAD_LBT de LR20xx deja la trama en la FIFO de TX cuando el canal está ocupado
- Severidad: alta
- Certeza: sospecha (el código no vacía la FIFO; falta confirmar en el datasheet o en hardware que `SetStandby` no lo haga)
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/patches/zephyr-new/drivers/lora/lr20xx/lr20xx_lora.c:2132,2164-2177,2304`
- Causa: el camino CAD_LBT escribe el payload en la FIFO de TX antes del CAD. Si el CAD da ocupado no hay TX y la trama se queda. Ningún punto del driver llama a `lr20xx_radio_fifo_clear_tx` (solo existe `fifo_clear_rx`), y el siguiente intento vuelve a escribir detrás. `cfg.cad.mode` es siempre LBT (`LoRaRadio.cpp:379`), así que este camino se usa en toda trama con timeout de TX de hasta 524 ms.
- Escenario de fallo: si la FIFO conserva los bytes, tras un CAD ocupado el siguiente TX emite la trama anterior, o una mezcla si las longitudes difieren, con la longitud de la nueva. Varios CAD ocupados seguidos acumulan bytes hasta desbordar los 256 de la FIFO.
- Arreglo mínimo propuesto: llamar a `lr20xx_radio_fifo_clear_tx` antes de cada `fifo_write_tx` en los dos caminos de TX.
- Cómo verificarlo: en un LR2021, forzar CAD ocupado y leer `GetTxFifoLevel` antes del siguiente `WriteTxFifo`: debe ser 0. Después, capturar en el aire la trama siguiente a un CAD ocupado.

### RADIO-3. El manejador RX_DONE del SX126x rearma RX sin mirar el estado ni tomar el lock
- Severidad: media
- Certeza: probable (carrera seguida en código, no observada)
- Origen: upstream (parche `0003`)   ·   ¿arreglado en origin/dev?: no (misma estructura y mismo comentario)   ·   ¿ya conocido?: no
- Ubicación: `sx126x.c:1368-1452` (llamada a `sx126x_restart_rx` en `:1452`), `sx126x.c:1099-1201`, `sx126x.c:3249-3251`
- Causa: `sx126x_handle_irq_rx_done` hace todo su SPI sin `data->lock` y llama a `sx126x_restart_rx` (ClearIrq ALL más SetRx) sin comprobar que el estado siga en RX. El comentario de `:3249` dice que "the RX_DONE and timeout handlers both gate on state", y no es así. El hilo de la cola DIO1 cede el procesador en cada transferencia SPI con interrupciones, así que el hilo principal puede pasar de RX a TX (CAS en `:1930-1934`) y empezar el CAD mientras el manejador sigue a medias.
- Escenario de fallo: llega un paquete justo cuando vence un TX programado. `sx126x_is_receiving()` ya da falso (el latch se borra al entrar en el manejador, `:1381`). El hilo principal arranca el CAD y espera el semáforo. El manejador toma el lock, rearma RX y borra CAD_DONE: el CAD vence con `-ETIMEDOUT`, el TX se rechaza y cuenta como rechazo LBT (RADIO-4). Si el SetRx del manejador cae después del SetTx, el TX se aborta sin TX_DONE y el driver queda en TX para siempre (RADIO-1).
- Arreglo mínimo propuesto: mantener `data->lock` en todo el manejador, como hacen LR11xx y LR20xx con `spi_mutex`, y rearmar solo si el estado sigue en RX.
- Cómo verificarlo: en hardware, programar un TX periódico mientras otro nodo emite tramas seguidas, contar los CAD con `-ETIMEDOUT` y comprobar que desaparecen con el arreglo.

### RADIO-4. Cualquier error de `startSendRaw` se trata como canal ocupado
- Severidad: media
- Certeza: confirmado en código
- Origen: upstream (la base ya llama a `onLbtRefused` ante cualquier fallo)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:538-541,406-438`
- Causa: el fallo de arranque del TX llega a `onLbtRefused` sin distinguir `-EBUSY` por LBT de un error permanente.
- Escenario de fallo: un error de driver persistente (`-EINVAL`, `-EIO`) se reintenta sin fin y además relaja y persiste el umbral de CAD, degradando la escucha previa al TX después de reiniciar.
- Arreglo mínimo propuesto: propagar el motivo y reservar `onLbtRefused` al rechazo real por canal ocupado.
- Cómo verificarlo: test de host del Dispatcher con una radio falsa que devuelva error distinto de ocupado; el umbral no debe cambiar.

### RADIO-5. Tras un reset de recuperación, LR20xx vuelve a programar el TCXO aunque ya se descartó
- Severidad: media
- Certeza: confirmado en código
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/patches/zephyr-new/drivers/lora/lr20xx/lr20xx_lora.c:680-685` (la guarda correcta está en `:3317`)
- Causa: `lr20xx_hw_init` marca `tcxo_disabled` y sigue en XTAL si el oscilador no arranca con TCXO. `lr20xx_hardware_reset` comprueba solo `tcxo_voltage_mv > 0` y vuelve a llamar a `set_tcxo_mode`.
- Escenario de fallo: una placa declara TCXO pero lleva cristal; el arranque cae a XTAL y funciona. Al primer reset de recuperación (DIO1 atascado o standby fallido) el chip vuelve a modo TCXO, falla `HF_XOSC_START` y, según el propio comentario de `:3315`, rechaza los comandos siguientes. La recuperación deja la radio sorda hasta reiniciar.
- Arreglo mínimo propuesto: añadir `&& !data->tcxo_disabled` a la condición de `:680`.
- Cómo verificarlo: en una placa con cristal y `tcxo-voltage` declarado, forzar `lr20xx_hardware_reset` y comprobar `get errors` y la recepción posterior.

### RADIO-6. La recalibración por temperatura se pierde si el driver la difiere
- Severidad: media
- Certeza: confirmado en código
- Origen: fork   ·   ¿arreglado en origin/dev?: por comprobar   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/radio/LoRaRadio.cpp:1244-1249`; `lr11xx_lora.c:1723-1731`; `lr20xx_lora.c:1119-1128`
- Causa: `imageCalMaintenance` actualiza `_image_cal_last_temp_c` tras `hwRecalibrate()`, pero `lr11xx_recalibrate` y `lr20xx_recalibrate` vuelven sin calibrar, y sin informar, si el mutex está ocupado o hay una recepción en curso.
- Escenario de fallo: la temperatura cambia 5 °C mientras llega un paquete. El driver difiere, el adaptador guarda la temperatura nueva como referencia y no reintenta hasta otro delta de 5 °C. El receptor sigue con la calibración de imagen antigua.
- Arreglo mínimo propuesto: que `recalibrate` devuelva si se ejecutó y que el adaptador solo actualice la referencia en ese caso.
- Cómo verificarlo: test de host del mantenimiento con un `hwRecalibrate` falso que difiere; la referencia no debe moverse.

### RADIO-7. LR11xx y LR20xx transmiten sin veredicto cuando el CAD falla
- Severidad: media
- Certeza: confirmado en código
- Origen: upstream   ·   ¿arreglado en origin/dev?: sí, en LR11xx y LR20xx ("CAD failed, restoring RX")   ·   ¿ya conocido?: sí; sigue vigente en el fork
- Ubicación: `lr11xx_lora.c:1040-1043`; `lr20xx_lora.c:2258-2261`
- Causa: un retorno negativo de CAD distinto de `-ENOSYS` se registra y el TX continúa. SX126x, en cambio, restaura RX y no transmite.
- Escenario de fallo: con el CAD fallando por timeout de IRQ, el nodo transmite sin escuchar y puede pisar una trama en curso.
- Arreglo mínimo propuesto: traer el cambio de origin/dev: restaurar RX y devolver el error, como SX126x (`sx126x.c:1969-1977`).
- Cómo verificarlo: forzar el fallo de CAD en una prueba de driver y comprobar que no se emite SetTx.

### RADIO-8. RSSI y SNR de un paquete pueden atribuirse a otro
- Severidad: baja
- Certeza: confirmado en código
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `zephcore/adapters/radio/LoRaRadio.cpp:333-334,729-730`; `zephcore/src/Dispatcher.cpp:326-339`
- Causa: el anillo ya guarda RSSI y SNR por paquete y `recvRaw` los copia a `_last_*`, pero `rxCallbackStatic` también escribe `_last_rssi` y `_last_snr` desde el hilo del driver al encolar. Entre `recvRaw` y las lecturas de `Dispatcher.cpp:326-339`, un paquete nuevo sobrescribe los valores del que se está procesando.
- Escenario de fallo: llegan dos paquetes seguidos; el primero se procesa con los valores del segundo, lo que falsea `packetScore` y el retardo de retransmisión.
- Arreglo mínimo propuesto: borrar las dos escrituras de `LoRaRadio.cpp:333-334`; `recvRaw` ya publica los valores del paquete que entrega.
- Cómo verificarlo: test de host que encole dos paquetes con métricas distintas antes de drenar.

### RADIO-9. El BUSY de la fase de sueño del ciclo de RX del SX126x cuenta como canal ocupado y fuerza otro sorteo
- Severidad: baja
- Certeza: confirmado en código
- Origen: fork (la re-contención de ADR 0019)   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: no
- Ubicación: `zephcore/src/Dispatcher.cpp:456-493`; `zephcore/adapters/radio/LoRaRadio.cpp:855-860`; `radio_ops_sx126x.cpp:56`
- Causa: `isRadioReady()` es `!BUSY`, y en SX126x BUSY está alto durante el sueño del ciclo de RX. El Dispatcher trata `!is_radio_ready` igual que `isReceiving()`: marca `tx_contended`, espera 200 ms y luego llama a `recontend()`, que vuelve a sortear el retardo de todos los paquetes vencidos. ADR 0019 pide ese sorteo para un canal ocupado, no para un chip dormido.
- Escenario de fallo: con `set rxduty on` (por defecto `n`, solo sin Meshtastic), a SF7/BW62.5 el chip duerme cerca del 40 % del ciclo. Casi la mitad de los TX esperan 200 ms más un sorteo completo de contención sin tráfico en el aire, y los reenvíos llegan tarde frente a los vecinos.
- Arreglo mínimo propuesto: marcar `tx_contended` solo cuando `is_receiving` es verdadero; el chip no listo conserva el reintento sin nuevo sorteo.
- Cómo verificarlo: test de host del Dispatcher con una radio falsa que dé `isRadioReady()==false` una vez: el horario del paquete no debe cambiar.

### RADIO-10. En LR11xx y LR20xx, `is_receiving` da falso cuando el mutex SPI está ocupado
- Severidad: baja
- Certeza: confirmado en código
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `lr20xx_lora.c:2647-2649`; `lr11xx_lora.c:1515-1517`
- Causa: la puerta de TX intenta el mutex con `K_NO_WAIT` y, si no lo consigue, contesta "no recibe". Quien tiene el mutex suele ser el manejador DIO1, que justo está atendiendo un HEADER_VALID o un RX_DONE. SX126x contesta lo contrario en ese caso.
- Escenario de fallo: el Dispatcher consulta mientras el manejador sella un HEADER_VALID: recibe falso, entra en `send_async`, y el SetStandby del CAD corta el paquete que estaba llegando.
- Arreglo mínimo propuesto: devolver `true` si el mutex está ocupado; el Dispatcher reintenta en 200 ms.
- Cómo verificarlo: test de driver que bloquee `spi_mutex` y compruebe que `is_receiving` da verdadero.

### RADIO-11. Ningún driver LoRa identifica el chip antes de escribirle
- Severidad: baja
- Certeza: confirmado en código
- Origen: upstream   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: sí (C12 de la revisión de drivers); sigue vigente
- Ubicación: `lr20xx_lora.c:3278-3300`; `lr11xx_lora.c:1986-1990`; SX126x no lee versión ni registro de identificación (`sx126x.c`)
- Causa: LR20xx y LR11xx dan por encontrado el chip con cualquier `get_version` que vuelva sin error, sin validar tipo ni versión (LR20xx solo registra un aviso); SX126x no identifica nada. Es contrario al invariante de `AGENTS.md` "Never write to a peripheral that was not cleanly identified first".
- Escenario de fallo: una placa con variantes de radio sobre los mismos pines (o una imagen de otra variante) recibe la secuencia completa de otro chip: TCXO, conmutadores de RF, PA y TX, sin ningún error visible.
- Arreglo mínimo propuesto: validar el tipo de chip devuelto por `get_version` (LR11xx `ver.type`, LR20xx la familia) y fallar `lora_config` si no coincide; en SX126x, comprobar un registro de valor conocido tras el reset.
- Cómo verificarlo: arrancar una imagen LR20xx en una placa SX1262 y comprobar que `lora_config` falla sin escribir.

### RADIO-12. El SNR del SX126x se redondea hacia abajo a dB enteros
- Severidad: baja
- Certeza: confirmado en código
- Origen: upstream (driver nativo)   ·   ¿arreglado en origin/dev?: no   ·   ¿ya conocido?: no
- Ubicación: `sx126x.c:707-708`; `zephcore/src/Dispatcher.cpp:338`
- Causa: `((int8_t)buf[1]) >> 2` es división por 4 con suelo. El valor se multiplica luego por 4 para `_snr`, que llega al SNR de TRACE y a `packetScore`. LR20xx redondea con `(raw + 2) >> 2` (`lr20xx_lora.c:1644`). Arduino MeshCore entrega 0,25 dB.
- Escenario de fallo: un enlace a 5,75 dB se registra como 5 dB: sesgo medio de -0,375 dB y pérdida de la resolución de 0,25 dB en los saltos de TRACE.
- Arreglo mínimo propuesto: redondear como LR20xx. Recuperar la resolución completa exige cambiar la API `int8_t` de Zephyr, fuera de este alcance.
- Cómo verificarlo: test de host de la conversión con valores negativos y positivos.

### RADIO-13. La decisión de rechazar cabeceras sin CRC no tiene ADR en el árbol
- Severidad: mejora
- Certeza: confirmado
- Origen: fork   ·   ¿arreglado en origin/dev?: n.a.   ·   ¿ya conocido?: sí (la memoria cita "ADR 0018", que no existe en `docs/adr/`)
- Ubicación: `docs/adr/` (no hay 0018); el código está en `sx126x.c:1354-1366,1413`, `lr11xx_lora.c:536-547,876-880` y `lr20xx_lora.c:1600-1610,1935-1941`
- Causa: el comportamiento existe en los tres drivers LoRa del parche, pero la decisión no está escrita. Además la lectura del estado falla en abierto (`return true` o `false` según el driver) y SX127x/loramac no aplica la comprobación, cosa que ningún documento recoge.
- Escenario de fallo: ninguno en el aire; un revisor no puede saber si la falta en SX127x es deliberada.
- Arreglo mínimo propuesto: añadir el ADR con el alcance real (qué drivers, qué pasa si falla la lectura).
- Cómo verificarlo: n.a.

## Alcance

Revisado a fondo:
- `zephcore/adapters/radio/`: `LoRaRadio.cpp/.h`, `LoRaRadioOps.h`, los cuatro `radio_ops_*.cpp`, `radio_common.h`, `radio_tuning.h`, `CadController.*`, `NoiseFloorEstimator.h`, `zc_lora_timing.h`.
- `zephcore/src/Dispatcher.cpp` en todo lo que toca a la radio (RX, `checkSend`, LBT, presupuesto, `recontend`), `ContentionTracker.*`, `StaticPoolPacketManager.cpp`, `ForeignTail.h`.
- Parche `0003` (SX126x nativo: `sx126x.c`, `sx126x.h`, `sx126x_hal.c`, `sx126x_hal_common.c`), salvo el bloque de retención.
- `lr11xx_lora.c` y `lr20xx_lora.c` completos; parche `0004` y `modules/loramac-node/0001-sx1276-bw62k5.patch` (mapa de BW y LDRO a 62,5 kHz, correctos).

Revisado por encima:
- `lr11xx_hal_zephyr.c` (desde la línea 60) y `lr20xx_hal_zephyr.c` (solo la configuración de interrupciones), SDK LR20xx (solo `radio_fifo.h` y `get_packet_status`).
- `Mesh.cpp` (solo `onRecvPacket` y la retransmisión), `tests/unit/dispatcher_tests.cpp` (solo los casos foreign y de re-contención).

No revisado:
- `sx126x_hal_stm32wl.c`, el bloque de retención de SX126x (`sx126x.c:720-840`), `lr11xx_cad_peak.h`, `sx126x_ext.h`, parche `0001` (solo build).
- El bucle de eventos de los roles (despertares de TX, RX y mantenimiento), por lo que no he podido confirmar ni descartar que un TX fallido antes de `outbound_expiry` se quede sin despertar.

Descartado tras verificarlo:
- Fuga de ranuras `ForeignTail` por la expulsión de `queueOutbound`: la cola y el pool tienen 32 entradas, así que la cola nunca está llena mientras exista un paquete libre y esa rama es inalcanzable.
- Paquete recibido con cabecera `0xFD` tomado por foráneo: `tryParsePacket` rechaza cualquier versión mayor que 0 (`Dispatcher.cpp:270-273`).
- IRQ perdidas en las salidas tempranas del manejador DIO1 de LR11xx/LR20xx: un flanco durante el manejador vuelve a encolar el trabajo.
- Desbordamiento de `elapsed * duty_pct` en `Dispatcher.cpp:92`: solo tras días sin actualizar, con el presupuesto ya lleno.
- Fórmula de airtime (incluidos SF5/6 y LDRO), sync word y preámbulo por trama tras CAD ocupado y TX en SX126x, LR11xx y LR20xx, `applyPendingRxWord`, y el ciclo de RX con preámbulo de 16 símbolos (ya documentado en Kconfig).

Conocidos en la memoria y no repetidos como hallazgos: híbrido asimétrico sin ADR, `getActiveSyncWord()` fijo en 0x12, `recv_errors` que mezcla causas, `_tx_foreign` no atómico (benigno), ciclo de RX nunca activo con Meshtastic y LR2021 sin hardware de prueba.

Nada se ha probado en hardware. RADIO-2 se resuelve con una lectura de `GetTxFifoLevel` en un LR2021.
