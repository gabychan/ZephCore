# Zephtacore: análisis inicial

Estado: borrador de partida; el MVP descrito aquí ya está implementado, ver [README.md](README.md). Fecha: 2026-11-09. Base: ZephCore `da3513a`. Referencia: `meshtacore-firmware` (fork de Meshtastic 2.8.1).

## Decisiones tomadas

| Tema | Decisión |
| --- | --- |
| Alcance | Puente completo: la radio oye y repite Meshtastic, y lo expone al cliente. |
| Cliente | App MeshCore (protocolo companion de ZephCore) mostrando canales Meshtastic traducidos. |
| Repo | Repo/fork separado. Este worktree es el punto de partida; las reglas de `AGENTS.md` no aplican al fork. |

## 1. Qué es meshtacore

Meshtastic como base, con MeshCore añadido sobre la misma radio SX126x.

| Función | Cómo lo hace |
| --- | --- |
| Recepción híbrida | Sync word por registro: `0x14B4` (SX126x), `0x1B` (LR11x0). Medido solo SX1262 a SX1262 en banco. Sin medida de pérdida. |
| Clasificación en RX | Un frame que parece MeshCore se prueba primero con los canales Meshtastic propios. Si descifra, es Meshtastic. Veto si los bytes 0..3 o 4..7 coinciden con nuestro NodeNum. |
| TX | Sync word por frame: `0x12` (MeshCore) o `0x2B` (Meshtastic). Preámbulo largo para MeshCore (32 símbolos con SF8 o menos, 16 con más). |
| Repetidor | Flood con dedupe independiente del path, delay adaptativo, cola de prioridad, LBT, direct, TRACE, MULTIPART ACK. |
| Puente al cliente | Canales `MC:<nombre>` con PSK de grupo, mensajes directos con ECDH, contactos aprendidos de ADVERTs, `/stats`. |
| Identidad | Ed25519 persistida, ADVERTs firmados (zero-hop cada 2 min, flood cada 47 h). |

Solo está verificado en ThinkNode M1. Pendiente: pérdida de paquetes del sync híbrido, otras placas, otras regiones y presets.

## 2. Zephtacore es el caso inverso

| | Meshtacore | Zephtacore |
| --- | --- | --- |
| Base | Meshtastic | ZephCore (MeshCore en Zephyr) |
| Se añade | MeshCore | Meshtastic |
| Trabajo grande | Reimplementar MeshCore | Implementar Meshtastic |

### Reutilización de ZephCore

| Componente | Reutilización |
| --- | --- |
| Motor MeshCore (`Mesh`, `Dispatcher`, `Packet`, `Identity`, `Utils`) | Tal cual. |
| Cripto (Ed25519, ECDH, AES) | Tal cual, con AES-CTR de Meshtastic añadido. |
| `ContentionTracker`, `CadController`, `StaticPoolPacketManager` | Tal cual. Meshtacore ya los copió de ZephCore. |
| `LoRaRadio` y familias (`radio_ops_*`) | Adaptar: sync word híbrido (sección 4). |
| Companion BLE (contactos, canales, mensajes) | Base del cliente. Hay que traducir Meshtastic a canales y contactos MeshCore. |
| Roles (repeater, room server, observer, KISS) | El repetidor es el primer destino. |
| Boards y build (west/Zephyr, sysbuild, `zephcore.yml`) | Tal cual. Hay que verificar flash y RAM por placa. |

### Hay que construir

| Pieza | Detalle |
| --- | --- |
| Frame Meshtastic | Cabecera de 16 bytes (dest, sender, packet id, flags, canal hash, next hop, relay node) más payload. |
| Cifrado | AES-CTR con PSK de canal (nonce de packet id, sender y contador). Clave por defecto `AQ==` ampliada. |
| Protobuf | `Data`, `User`, `Position` con nanopb, sin heap en la ruta de paquetes. |
| NodeDB mínimo | NodeNum, nombre y último oído, para el veto de clasificación y las traducciones. |
| Relay Meshtastic | Flood con `hop_limit`, dedupe por (sender, id) y rebroadcast. Sin descifrar. |
| Traducción | Meshtastic a canales y contactos MeshCore, y a la inversa. |

## 3. Riesgos y conflictos con AGENTS.md

El fork separado se libra de las reglas, pero estas son las restricciones que habrá que decidir de forma explícita:

| Invariante de ZephCore | Efecto en zephtacore |
| --- | --- |
| Byte-compat con MeshCore en el aire y en companion | Se mantiene: es el producto. |
| Sin heap en la ruta de paquetes | Protobuf obliga a nanopb con buffers estáticos. |
| Companion limitado por RAM | La NodeDB y el relay Meshtastic compiten por RAM con la tabla de contactos. Medir antes de decidir tamaños. |
| `prefs.json` es el único almacén de ajustes | Los ajustes Meshtastic (canales, PSK, relay on/off) deben entrar ahí. |
| Gate por capacidad, no por nombre de placa | Mantener el criterio: Kconfig o devicetree. |

Riesgos técnicos:

1. **Pérdida de paquetes del sync híbrido**: no está medida ni en meshtacore. Es el mayor riesgo de viabilidad. El plan B de meshtacore es una segunda SX1262.
2. **Colisión de clasificación**: un unicast Meshtastic entre terceros puede parecer MeshCore. Meshtacore deja una ventana pequeña pero real.
3. **Medición en otros chips**: LR1110, LR2021 y SX127x no tienen sync híbrido (SX127x tiene sync de 8 bits, LR2021 sin soporte).

## 4. Spike: sync word en la radio de ZephCore

Resultado: viable, con un parche nuevo en el driver.

Hallazgos:

- `LoRaRadio::buildModemConfig()` fija `cfg.public_network = false` (`zephcore/adapters/radio/LoRaRadio.cpp:311`). La API LoRa de Zephyr solo ofrece este booleano, que se traduce a dos valores: privado `0x1424` o público `0x3444` (`SX126X_LORA_SYNC_WORD_PRIVATE` / `_PUBLIC` en `zephcore/patches/zephyr/0003-lora-sx126x-native.patch`). `0x3444` es LoRaWAN, no Meshtastic.
- Meshtastic usa `0x2B`, que en registro es `0x24B4`. Ninguno de los dos valores de `public_network` sirve, así que hace falta una vía para escribir el registro crudo.
- El driver ya escribe `SX126X_REG_LORA_SYNC_WORD_MSB/LSB` (`0x0740`/`0x0741`) en `sx126x_set_sync_word()`. Hay que exponerlo: `sx126x_ext.h` es el sitio natural, junto a `sx126x_set_rx_boost` y similares, con una entrada nueva en `LoRaRadioOps` (`set_sync_word_reg`).
- `LoRaRadio::configure()` tiene una vía rápida solo de dirección (`onlyDirectionDiffers`, `LoRaRadio.cpp:408`) que no llama a `hwConfigure`. El sync word por frame debe escribirse en `configureTx()` y `configureRx()`, no en `lora_config()`. Si no, esa vía rápida lo omitiría.
- El preámbulo ya depende de SF (`preambleLengthForSF`). Meshtastic usa 16 símbolos fijos, así que para TX Meshtastic hay que sobreescribirlo por frame.

Valores a probar, tomados de meshtacore:

| Uso | Registro SX126x |
| --- | --- |
| RX híbrido | `0x14B4` |
| TX MeshCore | `0x1424` |
| TX Meshtastic | `0x24B4` |

Para LR11x0 el RX híbrido es el sync word de 8 bits `0x1B`.

Sin verificar: nada de esto se ha probado en hardware desde este worktree. Los valores salen de la documentación de meshtacore.

## 5. Fases propuestas

| Fase | Objetivo | Criterio de salida |
| --- | --- | --- |
| 0 | Fork a repo propio, renombrado, CI de tests de host | Build y tests verdes. |
| 1 | Sync híbrido en `LoRaRadio` y parche del driver | Recibe ambas redes en una SX1262. Medir pérdida frente a build normal. |
| 2 | Parser de frames Meshtastic y clasificación RX | Tests de host con vectores reales de Meshtastic. |
| 3 | Relay Meshtastic (flood, dedupe, hop_limit) | Repite sin descifrar. TX con sync correcto. |
| 4 | Descifrado de canales y NodeDB mínimo | Texto de canal legible. |
| 5 | Puente al cliente MeshCore | Canales Meshtastic visibles en la app. |
| 6 | Más placas y medidas de flash y RAM | Una placa nRF52 y una ESP32 más. |

La fase 1 decide si el proyecto es viable con una sola radio. Conviene hacerla antes de invertir en las demás.

## 6. Preguntas abiertas

- ¿Hardware de banco para la fase 1 (placa SX1262 de ZephCore y un nodo Meshtastic)?
- ¿Qué parte de Meshtastic debe verse en la app MeshCore: solo texto de canal, o también nodos y posiciones?
- ¿Licencia? ZephCore es MIT. Un código derivado de Meshtastic (GPL-3.0) cambia la licencia del conjunto. Si se reimplementa desde la especificación, no. Hay que decidirlo antes de copiar nada.
