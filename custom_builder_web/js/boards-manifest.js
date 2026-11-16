/**
 * Zephtacore Hardware & Boards Manifest
 */
window.ZEPHTACORE_BOARDS = [
  // --- nRF52840 Devices ---
  {
    id: "t1000_e",
    name: "Seeed SenseCAP Card T1000-E",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "LR1110",
    flashType: "uf2",
    recommendedRoles: ["companion"],
    desc: "Tarjeta ultra-fina con GPS, acelerómetro, botón SOS y transceptor Semtech LR1110 (palabra híbrida 0x1B).",
    defaultOffset: "0x00"
  },
  {
    id: "gat562_30s",
    name: "GAT-562 30dBm Power Repeater",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262 + PA",
    flashType: "uf2",
    recommendedRoles: ["repeater"],
    desc: "Placa de alta potencia (1 Watt / 30dBm) optimizada para repetidores solares y nodos de cumbre.",
    defaultOffset: "0x00"
  },
  {
    id: "heltec_t114",
    name: "Heltec Mesh Node T114",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["companion", "repeater"],
    desc: "Dispositivo de bajo consumo con pantalla TFT opcional y excelente receptor Semtech SX1262.",
    defaultOffset: "0x00"
  },
  {
    id: "xiao_nrf52840",
    name: "Seeed Studio XIAO nRF52840",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["companion", "repeater"],
    desc: "Factor de forma miniatura con cargador LiPo integrado; ideal para trackers y portables.",
    defaultOffset: "0x00"
  },
  {
    id: "rak4631",
    name: "RAK Wireless WisBlock RAK4631",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["repeater", "companion"],
    desc: "Ecosistema modular de bajo consumo líder en despliegues comunitarios de repetidores solares.",
    defaultOffset: "0x00"
  },
  {
    id: "promicro_sx1262",
    name: "ProMicro SuperMini SX1262",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["companion", "repeater"],
    desc: "Build artesanal DIY ultra-económica y ligera para nodos móviles o mochileros.",
    defaultOffset: "0x00"
  },
  {
    id: "wio_tracker_l1",
    name: "Seeed Wio Tracker L1",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["companion"],
    desc: "Tracker geográfico con GPS multi-constelación y sensorística avanzada integrada.",
    defaultOffset: "0x00"
  },
  {
    id: "lilygo_techo",
    name: "LilyGO T-Echo",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "SX1262",
    flashType: "uf2",
    recommendedRoles: ["companion"],
    desc: "Dispositivo portátil autónomo con pantalla E-Paper y receptor GNSS.",
    defaultOffset: "0x00"
  },
  {
    id: "meshtracker_x1",
    name: "SenseCAP MeshTracker X1",
    mcu: "nRF52840",
    arch: "nrf52",
    radio: "LR2021",
    flashType: "uf2",
    recommendedRoles: ["companion"],
    desc: "Primera placa Zephyr con radio Semtech LR2021 y GPS de doble banda L1+L5.",
    defaultOffset: "0x00"
  },

  // --- ESP32-S3 Devices ---
  {
    id: "heltec_wifi_lora32_v3/esp32s3/procpu",
    name: "Heltec WiFi LoRa 32 V3",
    mcu: "ESP32-S3",
    arch: "esp32",
    radio: "SX1262",
    flashType: "bin",
    recommendedRoles: ["companion", "repeater", "observer", "room_server"],
    desc: "Placa versátil con pantalla OLED 0.96\", conectividad WiFi, BLE y radio SX1262.",
    defaultOffset: "0x00"
  },
  {
    id: "heltec_wifi_lora32_v4/esp32s3/procpu",
    name: "Heltec WiFi LoRa 32 V4",
    mcu: "ESP32-S3",
    arch: "esp32",
    radio: "SX1262",
    flashType: "bin",
    recommendedRoles: ["companion", "repeater", "observer"],
    desc: "Revisión mejorada con circuitería RF optimizada y mayor eficiencia de recepción.",
    defaultOffset: "0x00"
  },
  {
    id: "heltec_wireless_tracker/esp32s3/procpu",
    name: "Heltec Wireless Tracker V1.1",
    mcu: "ESP32-S3",
    arch: "esp32",
    radio: "SX1262",
    flashType: "bin",
    recommendedRoles: ["companion"],
    desc: "Pantalla TFT a color 0.96\", módulo GNSS UC6580 y radio SX1262.",
    defaultOffset: "0x00"
  },
  {
    id: "lilygo_t3s3/esp32s3/procpu",
    name: "LilyGO T3-S3 LoRa",
    mcu: "ESP32-S3",
    arch: "esp32",
    radio: "SX1262",
    flashType: "bin",
    recommendedRoles: ["companion", "repeater", "observer"],
    desc: "Módulo ESP32-S3 con slot MicroSD, gestión de batería y transceptor LoRa SX1262.",
    defaultOffset: "0x00"
  }
];

window.ZEPHTACORE_ROLES = [
  {
    id: "companion",
    name: "Companion (Personal)",
    desc: "Para sincronizar con la app oficial de MeshCore vía Bluetooth o USB. Puente de canales #mt-* y chat.",
    defaultRelay: "off"
  },
  {
    id: "repeater",
    name: "Repeater (Repetidor de Red)",
    desc: "Reenvío autónomo para la comunidad con backoff reactivo por SNR, Zero-Hop y estadísticas remotas.",
    defaultRelay: "essential"
  },
  {
    id: "room_server",
    name: "Room Server (Servidor BBS)",
    desc: "Tablón de anuncios público y almacenamiento de mensajes offline para nodos de la malla.",
    defaultRelay: "off"
  },
  {
    id: "observer",
    name: "Observer (Gateway / MQTT)",
    desc: "Solo escucha y reenvía todos los paquetes escuchados por WiFi hacia un broker MQTT central.",
    defaultRelay: "off"
  },
  {
    id: "kiss_modem",
    name: "KISS Modem (TNC)",
    desc: "Modem LoRa transparente para control desde software host en PC/Raspberry Pi vía USB.",
    defaultRelay: "off"
  }
];
