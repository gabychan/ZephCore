# Balance de la revisión: qué entró y qué no

Una fila por hallazgo de los nueve informes de `odd/reviews/` (revisión de `2541fe5f`, 24-11-2026). Generado por
`odd/review-ledger.py` desde los informes y las tablas de `odd/tasks/review-fixes.md`; el script falla si un hallazgo
no está asignado a una tarea, así que nada puede quedar fuera del balance. Las únicas fuentes son los nueve informes
(y su `BRIEF.md`): comprobado que ninguna otra sesión del directorio original escribió artefactos de revisión.

- Hallazgos: **100** fichas = **99** hallazgos distintos: `ROLE-6` está marcado en su
  propio informe como duplicado de `PERS-3` y por eso comparte tarea.
- Por severidad: crítica 0 · alta 10 · media 36 ·
  baja 45 · mejora 8.
- Por estado: hecho 69 · pendiente 0 ·
  bloqueado (decisión humana) 31.

`dev` = ¿lo arregla ya `origin/dev`? `sí` significa que el arreglo upstream existía y se portó o se portará;
`no` significa que el arreglo es nuestro.

| Hallazgo | Sev. | Certeza | dev | Tarea | Estado | Motivo y commit |
| --- | --- | --- | --- | --- | --- | --- |
| RADIO-1 | alta | confirmado en código | no | T29 | **hecho** | abort del TX y redondeo del SNR en el parche `0003`, regenerado y verificado sin el fallback · `3ad30ab4` |
| RADIO-2 | alta | sospecha | no | T25 | **bloqueado** | ola C: hace falta datasheet o hardware para el CAD_LBT, y rediseñar el driver para identificar el chip (RADIO-11) |
| RADIO-3 | media | probable | no | T25 | **bloqueado** | ola C: hace falta datasheet o hardware para el CAD_LBT, y rediseñar el driver para identificar el chip (RADIO-11) |
| RADIO-4 | media | confirmado en código | no | T13 | **hecho** | veredicto del driver, TCXO, recalibración diferida y atribución de RSSI/SNR; compilado en LR2021, LR1110 y SX126x · `37c81a2f` |
| RADIO-5 | media | confirmado en código | no | T13 | **hecho** | veredicto del driver, TCXO, recalibración diferida y atribución de RSSI/SNR; compilado en LR2021, LR1110 y SX126x · `37c81a2f` |
| RADIO-6 | media | confirmado en código | por | T13 | **hecho** | veredicto del driver, TCXO, recalibración diferida y atribución de RSSI/SNR; compilado en LR2021, LR1110 y SX126x · `37c81a2f` |
| RADIO-7 | media | confirmado en código | sí | T6 | **hecho** | portado de upstream `6e458e24`/`5e73610f` solo en el veredicto del CAD; verificado por lectura del driver · `a34a9d8c` |
| RADIO-8 | baja | confirmado en código | no | T13 | **hecho** | veredicto del driver, TCXO, recalibración diferida y atribución de RSSI/SNR; compilado en LR2021, LR1110 y SX126x · `37c81a2f` |
| RADIO-9 | baja | confirmado en código | n.a | T14 | **hecho** | despertar del TX pendiente, sorteo solo con canal ocupado y paridad de `is_receiving`; compilado en LR1110, LR2021 y room server · `3e66ddb1` |
| RADIO-10 | baja | confirmado en código | no | T14 | **hecho** | despertar del TX pendiente, sorteo solo con canal ocupado y paridad de `is_receiving`; compilado en LR1110, LR2021 y room server · `3e66ddb1` |
| RADIO-11 | baja | confirmado en código | no | T25 | **bloqueado** | ola C: hace falta datasheet o hardware para el CAD_LBT, y rediseñar el driver para identificar el chip (RADIO-11) |
| RADIO-12 | baja | confirmado en código | no | T29 | **hecho** | abort del TX y redondeo del SNR en el parche `0003`, regenerado y verificado sin el fallback · `3ad30ab4` |
| RADIO-13 | mejora | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| CONN-1 | alta | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-2 | alta | confirmado | no | T9 | **hecho** | acotado a la capacidad real del búfer remoto; compilado en companion, repeater con uplink y room server · `3bf17801` |
| CONN-3 | media | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-4 | media | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-5 | media | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-6 | media | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-7 | media | confirmado | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-8 | baja | probable | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-9 | baja | probable | no | T20 | **bloqueado** | ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta |
| CONN-10 | baja | confirmado | no | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| CONN-11 | mejora | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| ROLE-1 | baja | confirmado | no | T14 | **hecho** | despertar del TX pendiente, sorteo solo con canal ocupado y paridad de `is_receiving`; compilado en LR1110, LR2021 y room server · `3e66ddb1` |
| ROLE-2 | alta | confirmado | no | T8 | **hecho** | verificado leyendo el camino remoto; el arnés no compila `app/` (QA-3) · `9abebd88` |
| ROLE-3 | media | confirmado | no | T15 | **hecho** | política de descarte de upstream, mantenimiento en el observer, orden de eventos y telemetría entera; compilado en companion y observer ESP32 · `aa726644` |
| ROLE-4 | media | confirmado | sí | T3 | **hecho** | portado de upstream `51d2e454` y `24762a20` · `d5e029a2, b45bbf0b` |
| ROLE-5 | media | confirmado | sí | T3 | **hecho** | portado de upstream `51d2e454` y `24762a20` · `d5e029a2, b45bbf0b` |
| ROLE-6 |  | — | — | T4 | **hecho** | portado de upstream `353a91fd` y `b409998e` · `c83f9c40, a265edfe` |
| ROLE-7 | media | confirmado | no | T15 | **hecho** | política de descarte de upstream, mantenimiento en el observer, orden de eventos y telemetría entera; compilado en companion y observer ESP32 · `aa726644` |
| ROLE-8 | baja | confirmado | no | T15 | **hecho** | política de descarte de upstream, mantenimiento en el observer, orden de eventos y telemetría entera; compilado en companion y observer ESP32 · `aa726644` |
| ROLE-9 | baja | confirmado | no | T15 | **hecho** | política de descarte de upstream, mantenimiento en el observer, orden de eventos y telemetría entera; compilado en companion y observer ESP32 · `aa726644` |
| ROLE-10 | baja | confirmado | sí | T7 | **hecho** | portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito · `a972bf41` |
| ROLE-11 | baja | confirmado | sí | T2 | **hecho** | portado de upstream `569215be` · `c1e6c021` |
| PERS-1 | alta | confirmado | no | T9 | **hecho** | acotado a la capacidad real del búfer remoto; compilado en companion, repeater con uplink y room server · `3bf17801` |
| PERS-2 | alta | confirmado | no | T9 | **hecho** | acotado a la capacidad real del búfer remoto; compilado en companion, repeater con uplink y room server · `3bf17801` |
| PERS-3 | alta | confirmado | sí | T4 | **hecho** | portado de upstream `353a91fd` y `b409998e` · `c83f9c40, a265edfe` |
| PERS-4 | media | confirmado | no | T11 | **hecho** | durabilidad y confianza en lo leído; la mitad del almacén de PERS-20 incluida · `97276ae1` |
| PERS-5 | media | confirmado | no | T23 | **bloqueado** | ola C: decide qué significa «el reloj solo avanza» cuando la fuente es SNTP o el consenso de malla |
| PERS-6 | media | confirmado | no | T23 | **bloqueado** | ola C: decide qué significa «el reloj solo avanza» cuando la fuente es SNTP o el consenso de malla |
| PERS-7 | media | confirmado | no | T23 | **bloqueado** | ola C: decide qué significa «el reloj solo avanza» cuando la fuente es SNTP o el consenso de malla |
| PERS-8 | media | probable | no | T24 | **bloqueado** | ola C: decide qué hacer ante un fallo de montaje (PERS-8) y si se paga el coste de `adv_blobs` (PERS-23) |
| PERS-9 | baja | confirmado | no | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-10 | baja | confirmado | no | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-11 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-12 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-13 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-14 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-15 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-16 | baja | confirmado | no | T11 | **hecho** | durabilidad y confianza en lo leído; la mitad del almacén de PERS-20 incluida · `97276ae1` |
| PERS-17 | baja | probable | no | T9 | **hecho** | acotado a la capacidad real del búfer remoto; compilado en companion, repeater con uplink y room server · `3bf17801` |
| PERS-18 | baja | confirmado | por | T11 | **hecho** | durabilidad y confianza en lo leído; la mitad del almacén de PERS-20 incluida · `97276ae1` |
| PERS-19 | baja | confirmado | n.a | T11 | **hecho** | durabilidad y confianza en lo leído; la mitad del almacén de PERS-20 incluida · `97276ae1` |
| PERS-20 | baja | confirmado | por | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-21 | baja | confirmado | n.a | T12 | **hecho** | validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos · `a40d9d61` |
| PERS-22 | baja | confirmado | no | T23 | **bloqueado** | ola C: decide qué significa «el reloj solo avanza» cuando la fuente es SNTP o el consenso de malla |
| PERS-23 | mejora | probable | no | T24 | **bloqueado** | ola C: decide qué hacer ante un fallo de montaje (PERS-8) y si se paga el coste de `adv_blobs` (PERS-23) |
| MESH-1 | media | confirmado | sí | T1 | **hecho** | portado de upstream `dcafa339`; verificado leyendo el camino (Utils no está en el arnés) y suite verde · `9fa79f33` |
| MESH-2 | media | confirmado | sí | T7 | **hecho** | portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito · `a972bf41` |
| MESH-3 | baja | confirmado | sí | T7 | **hecho** | portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito · `a972bf41` |
| MESH-4 | media | confirmado | no | T10 | **hecho** | cotas y estado de la malla; 4 casos de host nuevos, compilado en companion y room server · `429894aa` |
| MESH-5 | media | confirmado | n.a | T21 | **bloqueado** | ola C: decide política de malla (fail-closed de la semilla de identidad, exención admin del duty cycle) y el futuro de `z.mc.fw.gap` |
| MESH-6 | media | confirmado en código; el fallo de `sys_csrand_get` en el primer arranque no se ha observado | no | T21 | **bloqueado** | ola C: decide política de malla (fail-closed de la semilla de identidad, exención admin del duty cycle) y el futuro de `z.mc.fw.gap` |
| MESH-7 | baja | confirmado | n.a | T10 | **hecho** | cotas y estado de la malla; 4 casos de host nuevos, compilado en companion y room server · `429894aa` |
| MESH-8 | baja | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| MESH-9 | baja | confirmado | no | T10 | **hecho** | cotas y estado de la malla; 4 casos de host nuevos, compilado en companion y room server · `429894aa` |
| MESH-10 | media | confirmado | no | T21 | **bloqueado** | ola C: decide política de malla (fail-closed de la semilla de identidad, exención admin del duty cycle) y el futuro de `z.mc.fw.gap` |
| MESH-11 | baja | confirmado | sí | T7 | **hecho** | portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito · `a972bf41` |
| MESH-12 | baja | confirmado | sí | T7 | **hecho** | portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito · `a972bf41` |
| MESH-13 | baja | confirmado | no | T10 | **hecho** | cotas y estado de la malla; 4 casos de host nuevos, compilado en companion y room server · `429894aa` |
| MT-1 | alta | confirmado | n.a | T22 | **bloqueado** | ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic |
| MT-2 | media | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| MT-3 | media | confirmado | n.a | T22 | **bloqueado** | ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic |
| MT-4 | media | confirmado | n.a | T22 | **bloqueado** | ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic |
| MT-5 | media | confirmado | n.a | T22 | **bloqueado** | ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic |
| MT-6 | baja | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| MT-7 | baja | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| MT-8 | baja | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| MT-9 | baja | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| MT-10 | baja | confirmado en código; efecto en el aire no medido | n.a | T22 | **bloqueado** | ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic |
| MT-11 | mejora | confirmado | n.a | T16 | **hecho** | seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore · `0d9d9d8b` |
| BOARD-1 | baja | confirmado | n.a | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| BOARD-2 | media | confirmado | no | T18 | **hecho** | empaquetado que falla en voz alta, fichero de Zephyr que pasa a parche, joystick gateado por devicetree · `98c72a43` |
| BOARD-3 | baja | confirmado | no | T18 | **hecho** | empaquetado que falla en voz alta, fichero de Zephyr que pasa a parche, joystick gateado por devicetree · `98c72a43` |
| BOARD-4 | mejora | confirmado | no | T30 | **hecho** | solo el `HAPTICS_DRV2605` redundante: la retirada del tamaño de flash se revirtió al comprobar que el MMU del C6 depende de él · `ce1d9275` |
| BOARD-5 | mejora | confirmado | no | T18 | **hecho** | empaquetado que falla en voz alta, fichero de Zephyr que pasa a parche, joystick gateado por devicetree · `98c72a43` |
| BOARD-6 | alta | probable | sí | T5 | **hecho** | portado de upstream `eb84ba25`, `9b820396`, `4dbd6fca`; no compilado aquí (ver nota de compilación) · `0eb8d6ae, e9c4eed2, 3ea78b95` |
| BOARD-7 | media | probable | sí | T5 | **hecho** | portado de upstream `eb84ba25`, `9b820396`, `4dbd6fca`; no compilado aquí (ver nota de compilación) · `0eb8d6ae, e9c4eed2, 3ea78b95` |
| BOARD-8 | media | confirmado en código | sí | T5 | **hecho** | portado de upstream `eb84ba25`, `9b820396`, `4dbd6fca`; no compilado aquí (ver nota de compilación) · `0eb8d6ae, e9c4eed2, 3ea78b95` |
| QA-1 | baja | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| QA-2 | media | confirmado | no | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| QA-3 | media | confirmado | no | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| QA-4 | media | confirmado | n.a | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| QA-5 | baja | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| QA-6 | media | confirmado | n.a | T26 | **bloqueado** | ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B |
| QA-7 | mejora | confirmado | n.a | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| QA-8 | mejora | confirmado | no | T17 | **hecho** | cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY · `882cac64` |
| HW-1 | baja | confirmado | no | T19 | **hecho** | panel y sensores alcanzados por su nodo del devicetree, sin escribir a un chip sin identificar · `9d433583` |
| HW-2 | baja | confirmado | no | T19 | **hecho** | panel y sensores alcanzados por su nodo del devicetree, sin escribir a un chip sin identificar · `9d433583` |

## Qué no entró, y por qué

- **Ola C (decisión humana).** 31 fichas esperan una decisión de producto, seguridad o alcance: BLE/USB/TCP/MQTT
  (T20), política de malla y fail-closed (T21), cuánto se acepta de la capa Meshtastic (T22), semántica del reloj
  (T23), datos ante fallo de montaje (T24), datasheet o rediseño del driver (T25) y alcance de CI, release y arnés
  (T26). No se tocaron a propósito: elegir por mi cuenta sería decidir producto o compatibilidad sin el dueño.
- **Ola B (pendiente).** 53 fichas con causa demostrada y arreglo inequívoco. Están aceptadas y con tarea asignada;
  lo que falta es el trabajo, no el criterio.

## Límites de verificación (afectan a lo que valen los «hecho»)

- El arnés de host compila la lógica de malla, radio, paquetes y preferencias, pero **no** `zephcore/app/` ni
  `CommonCLI.cpp` (QA-3, T26). Los arreglos de T1 y T8 no tienen test de host posible hoy: se verificaron leyendo el
  camino completo y con la suite verde.
- ASan/UBSan solo existe en Linux; aquí no hay sanitizer.
- Los datos de compilación y qué se compiló de verdad están en el `## Log` de `odd/tasks/review-fixes.md`, no en este
  balance: un hallazgo marcado `hecho` no afirma verificación en hardware.
