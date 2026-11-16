/**
 * Zephtacore Web Builder Engine - User-Centric Flow
 */
document.addEventListener("DOMContentLoaded", () => {
  // Hardware & Role Elements
  const boardsContainer = document.getElementById("boardsGrid");
  const rolesContainer = document.getElementById("rolesGrid");
  const boardFilterInput = document.getElementById("boardSearch");

  // Radio Elements
  const sfSlider = document.getElementById("loraSf");
  const sfValDisplay = document.getElementById("sfVal");
  const bwSelect = document.getElementById("loraBw");
  const crSelect = document.getElementById("loraCr");
  const freqInput = document.getElementById("loraFreq");
  const txPowerInput = document.getElementById("txPower");

  // Dual-Mesh & Relay Elements
  const mtRelaySelect = document.getElementById("mtRelay");
  const mtOpaqueCheck = document.getElementById("mtOpaque");
  const mtHopsInput = document.getElementById("mtHops");
  const pingBotCheck = document.getElementById("pingBot");
  const firewallCheck = document.getElementById("firewallEnable");

  // Custom Identity Elements
  const nodeNameInput = document.getElementById("nodeNameInput");
  const btnRandomName = document.getElementById("btnRandomName");
  const adminPassInput = document.getElementById("adminPassInput");
  const btnRandomPass = document.getElementById("btnRandomPass");
  const guestPassInput = document.getElementById("guestPassInput");
  const nodeLatInput = document.getElementById("nodeLatInput");
  const nodeLonInput = document.getElementById("nodeLonInput");
  const nodeAltInput = document.getElementById("nodeAltInput");
  const gpsModeSelect = document.getElementById("gpsMode");
  const btnGeolocate = document.getElementById("btnGeolocate");
  const btnClearLocation = document.getElementById("btnClearLocation");
  const mapStatus = document.getElementById("mapStatus");

  // Primary Action Button
  const btnFlashAndApply = document.getElementById("btnFlashAndApply");
  const btnDownloadPrefs = document.getElementById("btnDownloadPrefs");

  // Dev Cloud & West Elements
  const ghRepoInput = document.getElementById("ghRepoInput");
  const ghBranchInput = document.getElementById("ghBranchInput");
  const ghTokenInput = document.getElementById("ghTokenInput");
  const btnDispatchAction = document.getElementById("btnDispatchAction");
  const dispatchStatusBox = document.getElementById("dispatchStatusBox");
  const westCommandPre = document.getElementById("westCommand");
  const btnCopyWest = document.getElementById("btnCopyWest");
  const btnDownloadConf = document.getElementById("btnDownloadConf");

  // Airtime Gauge Elements
  const gaugeFill = document.getElementById("gaugeFill");
  const airtimeMsVal = document.getElementById("airtimeMsVal");
  const bitrateVal = document.getElementById("bitrateVal");
  const sensitivityVal = document.getElementById("sensitivityVal");
  const airtimeStatus = document.getElementById("airtimeStatus");

  let selectedBoard = window.ZEPHTACORE_BOARDS[0];
  let selectedRole = window.ZEPHTACORE_ROLES[0];

  function getRandomHex(len = 4) {
    const chars = "0123456789ABCDEF";
    let str = "";
    for (let i = 0; i < len; i++) str += chars[Math.floor(Math.random() * chars.length)];
    return str;
  }

  function generateSecurePass(len = 10) {
    const chars = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789!@#$%&*";
    let pass = "";
    for (let i = 0; i < len; i++) pass += chars[Math.floor(Math.random() * chars.length)];
    return pass;
  }

  // Render Boards
  function renderBoards(filter = "") {
    if (!boardsContainer) return;
    boardsContainer.innerHTML = "";
    const term = filter.toLowerCase().trim();

    window.ZEPHTACORE_BOARDS.forEach((b) => {
      const match = !term || 
        b.name.toLowerCase().includes(term) || 
        b.id.toLowerCase().includes(term) || 
        b.mcu.toLowerCase().includes(term) || 
        b.radio.toLowerCase().includes(term) || 
        (b.desc && b.desc.toLowerCase().includes(term));
      if (!match) return;

      const card = document.createElement("div");
      card.className = `select-card ${selectedBoard.id === b.id ? "selected" : ""}`;
      const flashBadge = b.flashType === "uf2" 
        ? `<span class="select-card-tag" style="background: rgba(139, 92, 246, 0.12); border-color: rgba(139, 92, 246, 0.3); color: var(--brand-purple);">UF2</span>`
        : (b.flashType === "hex" 
          ? `<span class="select-card-tag" style="background: rgba(245, 158, 11, 0.12); border-color: rgba(245, 158, 11, 0.3); color: #f59e0b;">SWD/HEX</span>`
          : `<span class="select-card-tag" style="background: rgba(0, 242, 254, 0.12); border-color: rgba(0, 242, 254, 0.3); color: var(--brand-cyan);">WebSerial</span>`);

      card.innerHTML = `
        <div class="select-card-name">${b.name}</div>
        <div class="select-card-desc">${b.desc}</div>
        <div style="display: flex; gap: 0.4rem; flex-wrap: wrap; margin-top: 0.4rem;">
          <span class="select-card-tag">${b.mcu}</span>
          <span class="select-card-tag" style="background: rgba(16, 185, 129, 0.1); border-color: rgba(16, 185, 129, 0.25); color: var(--brand-emerald);">${b.radio}</span>
          ${flashBadge}
        </div>
      `;
      card.addEventListener("click", () => {
        selectedBoard = b;
        renderBoards(boardFilterInput ? boardFilterInput.value : "");
        saveCurrentConfig();
        updateOutputs();
      });
      boardsContainer.appendChild(card);
    });
  }

  // Render Roles
  function renderRoles() {
    if (!rolesContainer) return;
    rolesContainer.innerHTML = "";
    window.ZEPHTACORE_ROLES.forEach((r) => {
      const card = document.createElement("div");
      card.className = `select-card ${selectedRole.id === r.id ? "selected" : ""}`;
      card.innerHTML = `
        <div class="select-card-name">${r.name}</div>
        <div class="select-card-desc">${r.desc}</div>
        <span class="select-card-tag">Rol: ${r.id}</span>
      `;
      card.addEventListener("click", () => {
        selectedRole = r;
        if (mtRelaySelect) {
          mtRelaySelect.value = r.defaultRelay;
        }
        renderRoles();
        saveCurrentConfig();
        updateOutputs();
      });
      rolesContainer.appendChild(card);
    });
  }

  // Calculate Airtime
  function updateAirtime() {
    if (!sfSlider || !bwSelect || !crSelect || !window.LoRaCalc) return;
    const sf = parseInt(sfSlider.value, 10);
    const bw = parseFloat(bwSelect.value);
    const cr = parseInt(crSelect.value, 10);

    if (sfValDisplay) sfValDisplay.textContent = `SF${sf}`;

    const res = window.LoRaCalc.calculateAirtime({
      sf: sf,
      bw: bw,
      cr: cr,
      payloadBytes: 45,
      preamble: 16
    });

    if (airtimeMsVal) airtimeMsVal.textContent = `${res.airtimeMs} ms`;
    if (bitrateVal) bitrateVal.textContent = `${res.bitRateBps} bps`;
    if (sensitivityVal) sensitivityVal.textContent = `${res.sensitivityDbm} dBm`;

    const pct = Math.min(Math.max((res.airtimeMs / 1800) * 100, 5), 100);
    if (gaugeFill) {
      gaugeFill.style.width = `${pct}%`;
      if (res.airtimeMs < 250) {
        gaugeFill.style.background = "var(--brand-emerald)";
        if (airtimeStatus) {
          airtimeStatus.textContent = "⚡ Excelente (Ultra rápido)";
          airtimeStatus.style.color = "var(--brand-emerald)";
        }
      } else if (res.airtimeMs < 750) {
        gaugeFill.style.background = "var(--brand-cyan)";
        if (airtimeStatus) {
          airtimeStatus.textContent = "✓ Óptimo para Mesh Dual";
          airtimeStatus.style.color = "var(--brand-cyan)";
        }
      } else {
        gaugeFill.style.background = "var(--brand-amber)";
        if (airtimeStatus) {
          airtimeStatus.textContent = "⚠️ Lento (Mayor ocupación de canal)";
          airtimeStatus.style.color = "var(--brand-amber)";
        }
      }
    }
  }

  // Build the complete configuration bundle
  function getCurrentConfig() {
    return {
      boardId: selectedBoard.id,
      boardName: selectedBoard.name,
      mcu: selectedBoard.mcu,
      flashType: selectedBoard.flashType,
      role: selectedRole.id,
      roleName: selectedRole.name,
      name: nodeNameInput && nodeNameInput.value.trim() ? nodeNameInput.value.trim() : `Zephta-${getRandomHex(4)}`,
      adminPass: adminPassInput ? adminPassInput.value.trim() : "Secr3tAdmin!",
      guestPass: guestPassInput ? guestPassInput.value.trim() : "",
      lat: nodeLatInput ? nodeLatInput.value.trim() : "",
      lon: nodeLonInput ? nodeLonInput.value.trim() : "",
      alt: nodeAltInput ? nodeAltInput.value.trim() : "0",
      gpsMode: gpsModeSelect ? gpsModeSelect.value : "ENABLED",
      freq: freqInput ? freqInput.value : "869.618",
      bw: bwSelect ? bwSelect.value : "62.5",
      sf: sfSlider ? sfSlider.value : "7",
      cr: crSelect ? crSelect.value : "2",
      txPower: txPowerInput ? txPowerInput.value : "22",
      mtRelay: mtRelaySelect ? mtRelaySelect.value : "essential",
      mtOpaque: mtOpaqueCheck ? mtOpaqueCheck.checked : true,
      mtHops: mtHopsInput ? mtHopsInput.value : "7",
      pingBot: pingBotCheck ? pingBotCheck.checked : false,
      firewall: firewallCheck ? firewallCheck.checked : true,
      timestamp: new Date().toISOString()
    };
  }

  function saveCurrentConfig() {
    const cfg = getCurrentConfig();
    localStorage.setItem("zephtacore_pending_config", JSON.stringify(cfg));
  }

  function updateOutputs() {
    updateAirtime();
    saveCurrentConfig();

    const roleName = selectedRole.id;
    const boardId = selectedBoard.id;
    const nodeName = nodeNameInput ? nodeNameInput.value.trim() : "ZephtaNode";
    const adminPass = adminPassInput ? adminPassInput.value.trim() : "password";

    const cmd = `west build -b ${boardId} zephcore --pristine -- \\
  -DEXTRA_CONF_FILE="boards/common/zephtacore.conf;boards/common/${roleName}.conf;custom_node.conf" \\
  -DCONFIG_ZEPHCORE_DEFAULT_NODE_NAME='\\"${nodeName}\\"' \\
  -DCONFIG_ZEPHCORE_ADMIN_PASSWORD='\\"${adminPass}\\"'`;

    if (westCommandPre) {
      westCommandPre.textContent = cmd;
    }
  }

  // 1-Click Flash & Configure Action
  if (btnFlashAndApply) {
    btnFlashAndApply.addEventListener("click", () => {
      saveCurrentConfig();
      window.location.href = `flasher.html?apply_config=1`;
    });
  }

  // Download Prefs.json Backup
  if (btnDownloadPrefs) {
    btnDownloadPrefs.addEventListener("click", () => {
      const cfg = getCurrentConfig();
      const prefs = {
        name: cfg.name,
        password: cfg.adminPass,
        guest_password: cfg.guestPass,
        lat: cfg.lat ? parseFloat(cfg.lat) : 0.0,
        lon: cfg.lon ? parseFloat(cfg.lon) : 0.0,
        alt: cfg.alt ? parseInt(cfg.alt, 10) : 0,
        gps_mode: cfg.gpsMode,
        mt: {
          enable: true,
          role: cfg.role === "repeater" ? "client_base" : "client_mute",
          relay: cfg.mtRelay,
          opaque: cfg.mtOpaque,
          hops: parseInt(cfg.mtHops, 10),
          ping: cfg.pingBot
        },
        radio: {
          freq: parseFloat(cfg.freq),
          bw: parseFloat(cfg.bw),
          sf: parseInt(cfg.sf, 10),
          cr: parseInt(cfg.cr, 10),
          tx_power: parseInt(cfg.txPower, 10)
        }
      };
      const blob = new Blob([JSON.stringify(prefs, null, 2)], { type: "application/json" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = `zephtacore_${cfg.name.replace(/\s+/g, "_")}_prefs.json`;
      a.click();
    });
  }

  // Copy West Command
  if (btnCopyWest) {
    btnCopyWest.addEventListener("click", () => {
      if (westCommandPre) {
        navigator.clipboard.writeText(westCommandPre.textContent);
        const orig = btnCopyWest.textContent;
        btnCopyWest.textContent = "¡Copiado!";
        setTimeout(() => (btnCopyWest.textContent = orig), 2000);
      }
    });
  }

  // Download Kconfig
  if (btnDownloadConf) {
    btnDownloadConf.addEventListener("click", () => {
      const cfg = getCurrentConfig();
      const freqHz = Math.round(parseFloat(cfg.freq) * 1000000);
      const conf = `# Zephtacore Custom Kconfig
CONFIG_ZEPHTACORE_MESHTASTIC=y
CONFIG_ZEPHTACORE_MT_HOP_LIMIT=${cfg.mtHops}
CONFIG_ZEPHCORE_DEFAULT_FREQ_HZ=${freqHz}
CONFIG_ZEPHCORE_DEFAULT_BANDWIDTH_KHZ=${cfg.bw}
CONFIG_ZEPHCORE_DEFAULT_SPREADING_FACTOR=${cfg.sf}
CONFIG_ZEPHCORE_DEFAULT_CODING_RATE=${cfg.cr}
CONFIG_ZEPHCORE_DEFAULT_TX_POWER_DBM=${cfg.txPower}
CONFIG_ZEPHCORE_DEFAULT_NODE_NAME="${cfg.name}"
CONFIG_ZEPHCORE_ADMIN_PASSWORD="${cfg.adminPass}"
${cfg.guestPass ? `CONFIG_ZEPHCORE_GUEST_PASSWORD="${cfg.guestPass}"\n` : ""}${cfg.lat ? `CONFIG_ZEPHCORE_DEFAULT_NODE_LAT="${cfg.lat}"\n` : ""}${cfg.lon ? `CONFIG_ZEPHCORE_DEFAULT_NODE_LON="${cfg.lon}"\n` : ""}${cfg.alt ? `CONFIG_ZEPHCORE_DEFAULT_NODE_ALT=${cfg.alt}\n` : ""}CONFIG_ZEPHCORE_GPS_MODE="${cfg.gpsMode}"\n${cfg.firewall ? "CONFIG_ZEPHTACORE_FIREWALL=y\n" : ""}${cfg.pingBot ? "CONFIG_ZEPHTACORE_PING_BOT=y\n" : ""}`;

      const blob = new Blob([conf], { type: "text/plain" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = "custom_node.conf";
      a.click();
    });
  }

  function rollRandomName() {
    if (nodeNameInput) {
      nodeNameInput.value = `Zephta-${getRandomHex(4)}`;
    }
  }

  // Generate a random unique node name on initial page load if empty or generic
  if (nodeNameInput && (!nodeNameInput.value || nodeNameInput.value === "ZephtaNode")) {
    rollRandomName();
  }

  // Buttons for random generator
  if (btnRandomName) {
    btnRandomName.addEventListener("click", () => {
      rollRandomName();
      updateOutputs();
    });
  }

  if (btnRandomPass && adminPassInput) {
    btnRandomPass.addEventListener("click", () => {
      adminPassInput.value = generateSecurePass(12);
      updateOutputs();
    });
  }

  // Listeners
  if (boardFilterInput) {
    boardFilterInput.addEventListener("input", (e) => renderBoards(e.target.value));
  }

  [
    sfSlider, bwSelect, crSelect, freqInput, txPowerInput,
    mtRelaySelect, mtOpaqueCheck, mtHopsInput, pingBotCheck, firewallCheck,
    nodeNameInput, adminPassInput, guestPassInput, nodeLatInput, nodeLonInput,
    nodeAltInput, gpsModeSelect
  ].forEach((el) => {
    if (el) {
      el.addEventListener("input", updateOutputs);
      el.addEventListener("change", updateOutputs);
    }
  });

  // --- Interactive Leaflet Map & Geolocation ---
  let map = null;
  let mapMarker = null;
  const DEFAULT_MAP_CENTER = [40.4168, -3.7038]; // Madrid / Iberian center

  function initLocationMap() {
    const mapContainer = document.getElementById("locationMap");
    if (!window.L || !mapContainer) return;

    // Check if initial coordinates already exist in inputs
    const initLat = parseFloat(nodeLatInput ? nodeLatInput.value : "");
    const initLon = parseFloat(nodeLonInput ? nodeLonInput.value : "");
    const hasInitialCoords = !isNaN(initLat) && !isNaN(initLon);
    const startCenter = hasInitialCoords ? [initLat, initLon] : DEFAULT_MAP_CENTER;

    map = L.map("locationMap", {
      zoomControl: true,
      attributionControl: true
    }).setView(startCenter, hasInitialCoords ? 14 : 6);

    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
      maxZoom: 19,
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noopener">OpenStreetMap</a>'
    }).addTo(map);

    const cyberPin = L.divIcon({
      className: "zephta-map-pin",
      html: '<div class="pin-marker-pulse"></div><div class="pin-marker-core"></div>',
      iconSize: [24, 24],
      iconAnchor: [12, 12]
    });

    if (hasInitialCoords) {
      mapMarker = L.marker(startCenter, { icon: cyberPin, draggable: true }).addTo(map);
      attachMarkerEvents(mapMarker);
      if (mapStatus) {
        mapStatus.innerHTML = `📍 Coordenadas fijas: <strong>${initLat.toFixed(6)}, ${initLon.toFixed(6)}</strong>`;
      }
    }

    map.on("click", (e) => {
      setCoordsFromMap(e.latlng.lat, e.latlng.lng, cyberPin);
    });

    setTimeout(() => { if (map) map.invalidateSize(); }, 50);
    setTimeout(() => { if (map) map.invalidateSize(); }, 250);
    setTimeout(() => { if (map) map.invalidateSize(); }, 600);
    window.addEventListener("resize", () => { if (map) map.invalidateSize(); });
  }

  function attachMarkerEvents(marker) {
    marker.on("dragend", (e) => {
      const pos = e.target.getLatLng();
      if (nodeLatInput) nodeLatInput.value = pos.lat.toFixed(6);
      if (nodeLonInput) nodeLonInput.value = pos.lng.toFixed(6);
      if (mapStatus) {
        mapStatus.innerHTML = `📍 Coordenadas fijadas: <strong>${pos.lat.toFixed(6)}, ${pos.lng.toFixed(6)}</strong>`;
      }
      updateOutputs();
    });
  }

  function setCoordsFromMap(lat, lon, icon) {
    if (nodeLatInput) nodeLatInput.value = lat.toFixed(6);
    if (nodeLonInput) nodeLonInput.value = lon.toFixed(6);

    if (!mapMarker && map) {
      const cyberPin = icon || L.divIcon({
        className: "zephta-map-pin",
        html: '<div class="pin-marker-pulse"></div><div class="pin-marker-core"></div>',
        iconSize: [24, 24],
        iconAnchor: [12, 12]
      });
      mapMarker = L.marker([lat, lon], { icon: cyberPin, draggable: true }).addTo(map);
      attachMarkerEvents(mapMarker);
    } else if (mapMarker) {
      mapMarker.setLatLng([lat, lon]);
    }

    if (mapStatus) {
      mapStatus.innerHTML = `📍 Coordenadas seleccionadas: <strong>${lat.toFixed(6)}, ${lon.toFixed(6)}</strong>`;
    }
    updateOutputs();
  }

  function syncMapFromInputs() {
    if (!nodeLatInput || !nodeLonInput) return;
    const lat = parseFloat(nodeLatInput.value);
    const lon = parseFloat(nodeLonInput.value);
    if (!isNaN(lat) && !isNaN(lon) && map) {
      setCoordsFromMap(lat, lon);
      map.setView([lat, lon], 14);
    }
  }

  if (nodeLatInput) nodeLatInput.addEventListener("change", syncMapFromInputs);
  if (nodeLonInput) nodeLonInput.addEventListener("change", syncMapFromInputs);

  // Fallback Geolocation via IP
  async function fetchIpLocation() {
    if (mapStatus) mapStatus.innerHTML = "🌐 Consultando ubicación aproximada por red/IP...";
    try {
      const res = await fetch("https://ipwho.is/");
      const data = await res.json();
      if (data && data.success && data.latitude && data.longitude) {
        const lat = data.latitude;
        const lon = data.longitude;
        setCoordsFromMap(lat, lon);
        if (map) map.setView([lat, lon], 13);
        const cityStr = [data.city, data.region].filter(Boolean).join(", ");
        if (mapStatus) {
          mapStatus.innerHTML = `📍 Ubicación por red (${cityStr || "Aproximada"}): <strong>${lat.toFixed(6)}, ${lon.toFixed(6)}</strong> <span style="color: var(--brand-cyan);">(Ajustable en el mapa)</span>`;
        }
      } else {
        throw new Error(data.message || "Sin respuesta");
      }
    } catch (err) {
      if (mapStatus) {
        mapStatus.innerHTML = `⚠️ No se pudo obtener la ubicación automática (${err.message}). Haz clic directamente en el mapa para marcar tu posición.`;
      }
    }
  }

  // Geolocation Button
  if (btnGeolocate) {
    btnGeolocate.addEventListener("click", () => {
      if (mapStatus) mapStatus.innerHTML = "🛰️ Obteniendo ubicación GPS del dispositivo...";
      if (!navigator.geolocation) {
        fetchIpLocation();
        return;
      }

      navigator.geolocation.getCurrentPosition(
        (pos) => {
          const lat = pos.coords.latitude;
          const lon = pos.coords.longitude;
          setCoordsFromMap(lat, lon);
          if (map) map.setView([lat, lon], 15);
          if (mapStatus) {
            mapStatus.innerHTML = `✅ Ubicación detectada vía GPS/WiFi: <strong>${lat.toFixed(6)}, ${lon.toFixed(6)}</strong>`;
          }
        },
        (err) => {
          console.warn("Geolocation browser error, falling back to IP:", err.message);
          fetchIpLocation();
        },
        { enableHighAccuracy: true, timeout: 8000 }
      );
    });
  }

  // Clear Location Button
  if (btnClearLocation) {
    btnClearLocation.addEventListener("click", () => {
      if (nodeLatInput) nodeLatInput.value = "";
      if (nodeLonInput) nodeLonInput.value = "";
      if (nodeAltInput) nodeAltInput.value = "0";
      if (map && mapMarker) {
        map.removeLayer(mapMarker);
        mapMarker = null;
      }
      if (mapStatus) {
        mapStatus.innerHTML = "Haz clic o arrastra el marcador para fijar la posición exacta del nodo.";
      }
      updateOutputs();
    });
  }

  // Initial render
  renderBoards();
  renderRoles();
  initLocationMap();
  updateOutputs();
});
