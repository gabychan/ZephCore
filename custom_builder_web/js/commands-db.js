/**
 * Zephtacore CLI Command Reference Database
 */
window.ZEPHTACORE_COMMANDS = [
  // --- Meshtastic Dual-Mesh Namespace (z.mt.*) ---
  {
    name: "z.mt.enable",
    category: "meshtastic",
    type: "get|set",
    params: "on|off|1|0",
    default: "on",
    desc: "Activa o desactiva la compatibilidad Meshtastic. En 'off', el radio solo escucha tramas nativas MeshCore.",
    example: "set z.mt.enable on",
    tags: ["meshtastic", "dual-mesh", "radio"]
  },
  {
    name: "z.mt.role",
    category: "meshtastic",
    type: "get|set",
    params: "client|client_base|router|router_late|repeater|client_mute",
    default: "client_base (repeater) / client_mute (companion)",
    desc: "Define el rol anunciado en el NodeInfo y la temporización de retransmisión. 'client_base' indica posición fija para Zero-Hop.",
    example: "set z.mt.role client_base",
    tags: ["meshtastic", "rol", "timing"]
  },
  {
    name: "z.mt.relay",
    category: "meshtastic",
    type: "get|set",
    params: "off|essential|all",
    default: "essential (repeater) / off (companion)",
    desc: "Política de retransmisión Meshtastic. 'essential': texto, DMs directos, ACKs, NodeInfo y traceroute.",
    example: "set z.mt.relay essential",
    tags: ["meshtastic", "relay", "repetidor"]
  },
  {
    name: "z.mt.relay.opaque",
    category: "meshtastic",
    type: "get|set",
    params: "on|off|1|0",
    default: "on",
    desc: "Repite tramas de canales cuya clave criptográfica desconocemos, protegiendo la privacidad de la comunidad.",
    example: "set z.mt.relay.opaque on",
    tags: ["meshtastic", "relay", "cifrado"]
  },
  {
    name: "z.mt.channel.add",
    category: "meshtastic",
    type: "set",
    params: "<nombre> [default|<clave_base64>]",
    default: "-",
    desc: "Añade un canal Meshtastic en el Companion. Aparece en la app oficial MeshCore como #mt-<nombre>.",
    example: "set z.mt.channel.add Catalunya",
    tags: ["meshtastic", "canales", "companion"]
  },
  {
    name: "z.mt.channel.remove",
    category: "meshtastic",
    type: "set",
    params: "<nombre>",
    default: "-",
    desc: "Elimina un canal Meshtastic y libera su ranura en la app.",
    example: "set z.mt.channel.remove Catalunya",
    tags: ["meshtastic", "canales", "companion"]
  },
  {
    name: "z.mt.channels",
    category: "meshtastic",
    type: "get",
    params: "",
    default: "-",
    desc: "Lista todos los canales Meshtastic actualmente configurados y su estado.",
    example: "get z.mt.channels",
    tags: ["meshtastic", "canales"]
  },
  {
    name: "z.mt.hops",
    category: "meshtastic",
    type: "get|set",
    params: "<1-7>",
    default: "7",
    desc: "Número de saltos (hop_limit) asignado a los mensajes originados en este nodo hacia Meshtastic.",
    example: "set z.mt.hops 7",
    tags: ["meshtastic", "saltos", "routing"]
  },
  {
    name: "z.mt.ping.enable",
    category: "meshtastic",
    type: "get|set",
    params: "on|off",
    default: "off",
    desc: "Activa el bot de ping en el canal 'Bots'. Responde con mediciones de saltos, último relay y SNR.",
    example: "set z.mt.ping.enable on",
    tags: ["meshtastic", "ping", "diagnostico"]
  },
  {
    name: "z.mt.stats",
    category: "meshtastic",
    type: "get",
    params: "",
    default: "-",
    desc: "Muestra estadísticas del puente Meshtastic: paquetes oídos, repetidos, colisiones y tasas de error.",
    example: "get z.mt.stats",
    tags: ["meshtastic", "metricas", "diagnostico"]
  },

  // --- MeshCore & Firewall Namespace (z.mc.*) ---
  {
    name: "z.mc.fw.drop",
    category: "meshcore",
    type: "get|set",
    params: "<payload_type>",
    default: "0",
    desc: "Firewall MeshCore: descarta selectivamente tipos de trama no deseados o flood ruidoso.",
    example: "set z.mc.fw.drop 4",
    tags: ["meshcore", "firewall", "seguridad"]
  },
  {
    name: "z.mc.ping.enable",
    category: "meshcore",
    type: "get|set",
    params: "on|off",
    default: "off",
    desc: "Activa el bot de respuesta automática en el canal MeshCore '#bot'.",
    example: "set z.mc.ping.enable on",
    tags: ["meshcore", "ping", "diagnostico"]
  },
  {
    name: "z.mc.hopstats",
    category: "meshcore",
    type: "get",
    params: "",
    default: "-",
    desc: "Distribución de paquetes recibidos clasificados según la cantidad de saltos requeridos.",
    example: "get z.mc.hopstats",
    tags: ["meshcore", "saltos", "metricas"]
  },
  {
    name: "z.mc.relays",
    category: "meshcore",
    type: "get",
    params: "",
    default: "-",
    desc: "Lista de repetidores más activos detectados en el radio de cobertura.",
    example: "get z.mc.relays",
    tags: ["meshcore", "vecinos", "topologia"]
  },

  // --- Diagnóstico General y Telemetría ---
  {
    name: "z.info",
    category: "diagnostico",
    type: "get",
    params: "",
    default: "-",
    desc: "Resumen de telemetría de 1 línea: versión de firmware, uptime, voltaje de batería, uso de flash, heap y contactos.",
    example: "get z.info",
    tags: ["diagnostico", "telemetria", "sistema"]
  },
  {
    name: "z.stats",
    category: "diagnostico",
    type: "get",
    params: "",
    default: "-",
    desc: "Métricas completas combinadas: RX, TX, errores, tiempos de aire y longitud de colas en ambas mallas.",
    example: "get z.stats",
    tags: ["diagnostico", "metricas", "radio"]
  },
  {
    name: "z.tx.stalls",
    category: "diagnostico",
    type: "get",
    params: "",
    default: "-",
    desc: "Contabiliza las ocasiones en las que la transmisión se vio bloqueada por actividad en canal (CAD) o colisión.",
    example: "get z.tx.stalls",
    tags: ["diagnostico", "cad", "rf"]
  },
  {
    name: "cad.stats",
    category: "diagnostico",
    type: "get",
    params: "",
    default: "-",
    desc: "Estadísticas del detector adaptativo de portadora LoRa (Channel Activity Detection).",
    example: "get cad.stats",
    tags: ["radio", "cad", "rf"]
  },
  {
    name: "batt",
    category: "sistema",
    type: "get",
    params: "",
    default: "-",
    desc: "Muestra el voltaje en milivoltios (mV) y la estimación porcentual de la batería LiPo/LiFePO4.",
    example: "get batt",
    tags: ["bateria", "energia", "hardware"]
  },
  {
    name: "reboot",
    category: "sistema",
    type: "set",
    params: "",
    default: "-",
    desc: "Ejecuta un reinicio seguro por software del nodo.",
    example: "set reboot",
    tags: ["sistema", "mantenimiento"]
  }
];
