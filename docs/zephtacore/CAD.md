# CAD, colisiones y tiempos de retransmisión: Meshtastic, MeshCore y ZephCore

Explicación para quien se pregunta por qué una red LoRa pierde mensajes en zonas congestionadas, qué es el CAD y si el de
Meshtastic es "mejor" que el de MeshCore. Basado en la lectura del código de los tres proyectos (MeshCore upstream a
2026-08-24, Meshtastic upstream a 2026-10-23, meshtacore y ZephCore/Zephtacore a 2026-11-09). Nada de lo que sigue se ha
medido en el aire: es lo que hace el código.

## 1. El problema: dos nodos que hablan a la vez

LoRa es un canal compartido. Si dos nodos que un tercero oye transmiten a la vez, ese tercero normalmente pierde los dos
paquetes (salvo que uno sea mucho más fuerte que el otro: efecto captura). En una malla esto ocurre sobre todo con los
**floods**: un mensaje llega a la vez a varios repetidores y todos quieren repetirlo.

Hay dos herramientas para evitarlo, y conviene no mezclarlas:

1. **Escuchar antes de hablar (LBT, *listen before talk*)**: justo antes de transmitir, mirar si alguien ya está
   transmitiendo. Aquí entra el CAD.
2. **Esperar un tiempo aleatorio antes de repetir (ventana de contención)**: para que los repetidores que recibieron el
   mismo paquete no salten todos en el mismo milisegundo. El LBT no sirve de nada si todos miran el canal a la vez, lo
   ven libre y transmiten juntos.

La mayoría de las pérdidas en zonas densas vienen de que la segunda herramienta está mal ajustada, no de la primera.

## 2. Qué es el CAD

**CAD** (*Channel Activity Detection*) es una función del propio chip de radio (SX126x, LR11xx, LR20xx). El chip escucha
unos pocos símbolos y correla buscando la forma de un **preámbulo LoRa**. No mide energía: detecta LoRa, incluso por
debajo del nivel de ruido, que es justo donde LoRa es capaz de recibir.

En el SX126x se lanza con dos comandos SPI:

- `SetCadParams` (`0x88`): cuántos símbolos escuchar (`cadSymbolNum`), el umbral de detección (`cadDetPeak`, `cadDetMin`)
  y qué hacer al terminar (`cadExitMode`: volver a standby o pasar directamente a recibir).
- `SetCAD` (`0xC5`): ejecutarlo. El chip responde con la interrupción `CAD_DONE` y, si vio algo, `CAD_DETECTED`.

**El detector es el mismo hardware en todos los firmwares.** Lo que cambia es con qué parámetros se lanza y cómo encaja el
firmware esa operación en su manejo de la radio.

### El umbral (`cadDetPeak`)

Es un umbral de correlación, no de dBm. Un valor alto hace que el CAD solo reaccione a señales fuertes (y transmita encima
de las débiles); uno bajo hace que vea más cosas, incluidos falsos positivos, y retrase la transmisión sin motivo. El valor
correcto depende del sitio: del ruido local y de lo lejos que estén los vecinos.

## 3. Cómo lo hace cada firmware

| | MeshCore upstream | Meshtastic upstream | ZephCore / Zephtacore |
|---|---|---|---|
| Detector por defecto | Umbral de **RSSI** (energía) sobre el ruido de fondo (`int.thresh`) | CAD de hardware | CAD de hardware |
| CAD de hardware | Opcional (`_cad_enabled`), apagado por defecto | Siempre | Siempre, dentro del driver |
| Símbolos de CAD | — | 2 (`NUM_SYM_CAD`, AN1200.48) | 4 |
| Umbral `cadDetPeak` | Por defecto de RadioLib | Por defecto de RadioLib (fijo) | Tabla de Semtech según SF y ancho de banda, más un ajuste que se calibra solo en cada sitio |
| Al detectar | — | Pasa directamente a RX (`CAD_GOTO_RX`) | El driver vuelve a poner RX |
| Bloqueo al ver un preámbulo o una cabecera | Sí (`isReceivingPacket`) | Sí | Sí, con gracia según el SF, y RSSI como respaldo |

(meshtacore, el fork de Meshtastic con MeshCore, usa CAD de 4 símbolos.)

### Lo que la comunidad llama "CAD por software" de MeshCore

Es el umbral de RSSI: si la energía en el canal supera el ruido de fondo más `int.thresh`, considera el canal ocupado. Es
robusto (no saca la radio de recepción), pero es peor detector: no ve un preámbulo LoRa débil, que es justo el caso en que
transmitir encima rompe la recepción de otro nodo, y puede ver como "ocupado" ruido que no es LoRa.

Así que la intuición de que "lo de hardware es mejor" es correcta en este caso. ZephCore usa el de hardware.

## 4. Si el CAD de hardware es mejor, ¿por qué MeshCore no lo usa?

No es por el sistema operativo. MeshCore y Meshtastic usan los dos el framework Arduino (con FreeRTOS debajo en ESP32 y
nRF52) y la misma librería de radio, RadioLib. El problema es cómo se encaja el CAD en la máquina de estados de la radio.

Para hacer un CAD el firmware tiene que:

1. sacar el chip de recepción (standby);
2. cambiar qué interrupciones salen por el pin DIO1;
3. lanzar el CAD y esperar `CAD_DONE`;
4. o bien transmitir de inmediato (canal libre), o bien volver a configurar las interrupciones de recepción y relanzar RX.

Si cualquiera de esos pasos se cruza con otro evento (llega un paquete, salta otra interrupción, otra tarea usa el bus
SPI) y el software se queda con una idea equivocada del estado del chip, la radio se queda **sorda**: no está en RX, o
lo está con las interrupciones mal configuradas. Desde fuera parece que "se cuelga". No falla el hardware, falla la
coordinación.

- **MeshCore** usa un único flag de interrupción para todo: el fin del CAD y el fin de una recepción levantan el mismo
  flag, que hay que borrar a mano tras el CAD para no leer un paquete que no existe. `scanChannel()` de RadioLib es
  bloqueante y deja el chip fuera de RX. Su diseño es simple a propósito, y el CAD es la operación que más estados
  atraviesa. Los mantenedores han preferido el umbral de RSSI, que no puede dejar la radio sorda.
- **Meshtastic** ha pulido esa máquina de estados durante años: notificaciones separadas por evento, máscara de
  interrupciones propia del CAD, `CAD_GOTO_RX`, y un bloqueo para que el CAD y la transmisión sean una sola secuencia.
  Aun así tiene red de seguridad: si una transmisión no termina en 60 s, registra un fallo de hardware y reinicia el
  nodo.
- **ZephCore** metió el CAD **dentro del driver** (Zephyr), como parte de la propia operación de envío: transición
  atómica del estado del driver de RX a TX, CAD, y si está ocupado, vuelta a RX en el mismo driver. La aplicación nunca
  ve un estado intermedio. También tiene una recuperación acotada: si lleva 4 s sin poder transmitir, reinicia la
  recepción.

En resumen: el CAD de hardware es mejor detector, y que se cuelgue o no depende de quién controla el estado de la radio y
de si el CAD es una operación atómica.

## 5. La otra mitad: cuánto espera cada nodo antes de repetir

Aquí es donde se ganan o se pierden la mayoría de los paquetes en zonas densas.

### MeshCore upstream: `txdelay` fijo

El repetidor espera un tiempo aleatorio entre 0 y `5 × airtime × tx_delay_factor`. El factor es un ajuste fijo por nodo.
Con muchos repetidores que oyen el mismo paquete, la ventana se queda corta y varios eligen casi el mismo instante. Si
cada operador lo ajusta a su manera, peor.

### Meshtastic: ventana en *slots* ponderada por SNR

Un *slot* es lo que tarda un nodo en darse cuenta de que otro empezó a transmitir: la duración del CAD, más propagación,
más el cambio de RX a TX, más el procesado (`2.5 × Tsímbolo + 7.6 ms` en sub-GHz).

Al repetir, el tamaño de la ventana depende del SNR con que se recibió el paquete (de −20 dB a +10 dB → de 2³ a 2⁸
slots). **Con SNR bajo la ventana es pequeña**: el nodo más lejano del emisor repite antes, que es el que más terreno
nuevo cubre. Los nodos cliente esperan además `2 × 8` slots fijos para dejar paso primero a los routers.

### ZephCore: ventana adaptativa y espera extra por duplicados

- **Ventana adaptativa**: el nodo cuenta cuántos duplicados oye de los paquetes que repite y mantiene una media móvil.
  Con pocos vecinos la ventana se estrecha (menos latencia); con muchos se ensancha sola. Topes: 6 airtimes y 2 s.
- **Espera extra por duplicado**: si mientras un paquete está en cola oye a otro nodo repetirlo, retrasa el suyo
  `backoff_multiplier × airtime` cada vez, hasta un tope de `min(2 s, 12 airtimes)`. **No cancela.**

### Por qué no cancelar al oír el eco

Meshtastic cancela la repetición propia si oye a otro repetir el mismo paquete. Eso supone que, si yo oigo el eco, mis
vecinos también lo han oído. En una topología en cadena o en estrella (un nodo que solo me oye a mí), la suposición falla
y el mensaje se pierde sin que nadie se entere. Se ahorra airtime a cambio de fiabilidad, y en LoRa perder un mensaje es
peor que gastar unos milisegundos de aire.

La espera extra de ZephCore consigue lo importante (apartarse del duplicado que acaba de sonar para no pisarlo) sin
renunciar a repetir. meshtacore lo resolvió de otra forma: la cancelación se puede activar o desactivar.

## 6. Qué usa Zephtacore

| Pieza | Elección | Motivo |
|---|---|---|
| CAD, bloqueo de RX, recuperación | Los de ZephCore | CAD de hardware siempre activo, umbral adaptativo, operación atómica en el driver |
| Repetición de floods MeshCore | La de ZephCore | Ventana adaptativa y espera extra por duplicados |
| Repetición de floods Meshtastic | Ventana en slots ponderada por SNR, como Meshtastic, más la espera extra por duplicados de ZephCore, sin cancelar | Coordinarse con los repetidores Meshtastic de alrededor sin perder mensajes en topologías débiles |
| Frames Meshtastic propios (NodeInfo, texto, respuestas del ping bot) | Entre 0 y 7 slots de espera antes del CAD (la ventana mínima de Meshtastic, 2³) | Si varios nodos contestan al mismo mensaje, sin espera todos llegan al CAD en el mismo instante y lo ven libre |
| Preámbulo de los frames Meshtastic | 16 símbolos, el de Meshtastic, en SX126x, LR11xx y LR20xx | Con SF≤8 MeshCore usa 32: enviar los frames Meshtastic con 32 solo gastaba aire |

## 7. Cómo comprobarlo en una zona congestionada

Comparar antes y después de un cambio, en el mismo sitio y durante el mismo tiempo:

- `get cad.stats`: tasa de "ocupado" y el umbral con que trabaja el CAD.
- Contadores de duplicados y de paquetes repetidos.
- Desde otro nodo: cuántos de los mensajes enviados llegan.

Una sola medida en un sitio no generaliza: el resultado depende de la densidad, del preset y de la topología.
