/**
 * Zephtacore Web Flasher Core Engine - Direct USB Configuration Injection
 */
function initFlasherCore() {
  const dropzone = document.getElementById("flasherDropzone");
  const fileInput = document.getElementById("firmwareFileInput");
  const boardSelect = document.getElementById("flasherBoardSelect");
  const roleSelect = document.getElementById("flasherRoleSelect");
  const btnStartFlash = document.getElementById("btnStartFlash");
  const progressCard = document.getElementById("progressCard");
  const progressBar = document.getElementById("flashProgressBar");
  const progressPercent = document.getElementById("progressPercent");
  const flashStatusText = document.getElementById("flashStatusText");
  const flasherTabs = document.querySelectorAll(".flasher-tab-btn");
  const tabPanels = document.querySelectorAll(".flasher-tab-panel");

  // Preview elements
  const previewBoardTitle = document.getElementById("previewBoardTitle");
  const previewBoardDesc = document.getElementById("previewBoardDesc");
  const previewMcuTag = document.getElementById("previewMcuTag");
  const previewRadioTag = document.getElementById("previewRadioTag");
  const previewFlashTag = document.getElementById("previewFlashTag");

  // Pending config banner
  const pendingConfigBanner = document.getElementById("pendingConfigBanner");
  const cfgBadgeName = document.getElementById("cfgBadgeName");
  const cfgBadgeFreq = document.getElementById("cfgBadgeFreq");
  const cfgBadgeRole = document.getElementById("cfgBadgeRole");
  const cfgBadgeHops = document.getElementById("cfgBadgeHops");
  const cfgBadgeBoard = document.getElementById("cfgBadgeBoard");
  const btnUnifiedFlash = document.getElementById("btnUnifiedFlash");
  const btnInjectOnly = document.getElementById("btnInjectOnly");

  let activeFile = null;
  let activeBuffer = null;
  let pendingConfig = null;

  // Initialize Board and Role Selects
  if (boardSelect && window.ZEPHTACORE_BOARDS) {
    boardSelect.innerHTML = window.ZEPHTACORE_BOARDS.map(
      (b) => `<option value="${b.id}">${b.name} — [${b.mcu}]</option>`
    ).join("");
  }

  if (roleSelect && window.ZEPHTACORE_ROLES) {
    roleSelect.innerHTML = window.ZEPHTACORE_ROLES.map(
      (r) => `<option value="${r.id}">${r.name}</option>`
    ).join("");
  }

  // Check for configuration saved from Builder or URL parameter
  const urlParams = new URLSearchParams(window.location.search);
  const hasApplyParam = urlParams.get("apply_config") === "1";
  const savedCfgStr = localStorage.getItem("zephtacore_pending_config");

  if (savedCfgStr || hasApplyParam) {
    try {
      pendingConfig = savedCfgStr ? JSON.parse(savedCfgStr) : {
        name: "ZephtaNode-EU869",
        boardId: "t1000_e",
        role: "companion",
        freq: "869.618",
        sf: "7",
        bw: "62.5",
        cr: "2",
        txPower: "22",
        mtRelay: "essential",
        mtOpaque: true,
        mtHops: "7",
        adminPass: "ZephAdmin2026!"
      };
      if (pendingConfig && pendingConfigBanner) {
        pendingConfigBanner.style.display = "flex";
        if (cfgBadgeName) cfgBadgeName.textContent = `Nodo: ${pendingConfig.name}`;
        if (cfgBadgeFreq) cfgBadgeFreq.textContent = `${pendingConfig.freq} MHz (SF${pendingConfig.sf || 7}/BW${pendingConfig.bw || 62.5})`;
        if (cfgBadgeRole) cfgBadgeRole.textContent = `Rol: ${pendingConfig.role}`;
        if (cfgBadgeHops) cfgBadgeHops.textContent = `Saltos: ${pendingConfig.mtHops || 7}`;
        if (cfgBadgeBoard) {
          const b = window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS.find((x) => x.id === pendingConfig.boardId);
          cfgBadgeBoard.textContent = `Placa: ${b ? b.name : (pendingConfig.boardName || pendingConfig.boardId || "Auto")}`;
        }

        // Auto-select the board from builder
        if (boardSelect && pendingConfig.boardId) {
          boardSelect.value = pendingConfig.boardId;
        }
        if (roleSelect && pendingConfig.role) {
          roleSelect.value = pendingConfig.role;
        }
      }
    } catch (e) {
      console.warn("Error parsing pending config:", e);
    }
  }

  function syncBoardPreview() {
    if (!boardSelect || !window.ZEPHTACORE_BOARDS) return;
    const selectedId = boardSelect.value;
    const board = window.ZEPHTACORE_BOARDS.find((b) => b.id === selectedId) || window.ZEPHTACORE_BOARDS[0];
    if (!board) return;

    if (previewBoardTitle) previewBoardTitle.textContent = board.name;
    if (previewBoardDesc) previewBoardDesc.textContent = board.desc;
    if (previewMcuTag) previewMcuTag.textContent = board.mcu;
    if (previewRadioTag) previewRadioTag.textContent = board.radio;
    if (previewFlashTag) {
      if (board.flashType === "uf2") {
        previewFlashTag.textContent = "Modo UF2 (Arrastrar a Disco)";
        previewFlashTag.style.background = "rgba(139, 92, 246, 0.1)";
        previewFlashTag.style.color = "var(--brand-purple)";
        previewFlashTag.style.borderColor = "rgba(139, 92, 246, 0.3)";
      } else if (board.flashType === "hex") {
        previewFlashTag.textContent = "Modo SWD / JTAG (zephyr.hex)";
        previewFlashTag.style.background = "rgba(245, 158, 11, 0.1)";
        previewFlashTag.style.color = "#f59e0b";
        previewFlashTag.style.borderColor = "rgba(245, 158, 11, 0.3)";
      } else {
        previewFlashTag.textContent = `Modo Web Serial (Offset ${board.defaultOffset || "0x00"})`;
        previewFlashTag.style.background = "rgba(0, 242, 254, 0.1)";
        previewFlashTag.style.color = "var(--brand-cyan)";
        previewFlashTag.style.borderColor = "rgba(0, 242, 254, 0.3)";
      }
    }

    if (btnStartFlash) {
      if (board.flashType === "uf2") {
        btnStartFlash.textContent = "📥 Descargar Firmware Oficial UF2";
      } else if (board.flashType === "hex") {
        btnStartFlash.textContent = "📥 Descargar Firmware HEX (SWD)";
      } else {
        btnStartFlash.textContent = "⚡ Flashear e Inyectar Ajustes por USB";
      }
    }
  }

  if (boardSelect) {
    boardSelect.addEventListener("change", syncBoardPreview);
    syncBoardPreview();
  }

  // Tabs Switcher
  flasherTabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      flasherTabs.forEach((t) => t.classList.remove("active"));
      tabPanels.forEach((p) => (p.style.display = "none"));
      tab.classList.add("active");
      const target = document.getElementById(tab.getAttribute("data-target"));
      if (target) target.style.display = "block";
    });
  });

  // Coding rate: the builder stores 1-4 (4/5-4/8), the firmware takes 5-8.
  function firmwareCodingRate(cr) {
    const n = parseInt(cr, 10);
    return n <= 4 ? n + 4 : n;
  }

  function buildConfigCommands(cfg) {
    const cr = firmwareCodingRate(cfg.cr);
    const isObserver = cfg.role === "observer";

    if (isObserver) {
      return [
        `set name ${cfg.name}`,
        `set freq ${cfg.freq}`,
        `set sf ${cfg.sf}`,
        `set bw ${cfg.bw}`,
        `set cr ${cr}`,
        cfg.lat ? `set lat ${cfg.lat}` : null,
        cfg.lon ? `set lon ${cfg.lon}` : null,
        `reboot`
      ].filter(Boolean);
    }

    return [
      `set name ${cfg.name}`,
      `password ${cfg.adminPass}`,
      `set radio ${cfg.freq},${cfg.bw},${cfg.sf},${cr}`,
      `set tx ${cfg.txPower}`,
      `set z.mt.enable on`,
      `set z.mt.relay ${cfg.mtRelay}`,
      `set z.mt.relay.opaque ${cfg.mtOpaque ? "on" : "off"}`,
      `set z.mt.hops ${cfg.mtHops}`,
      cfg.pingBot ? `set z.mt.ping.enable on /ping` : null,
      cfg.lat ? `set lat ${cfg.lat}` : null,
      cfg.lon ? `set lon ${cfg.lon}` : null,
      `reboot`
    ].filter(Boolean);
  }

  // Sends the settings over the serial port. Returns true only when every command
  // was written; the node's replies are not read, they show in the terminal.
  async function injectConfigSequence(cfg) {
    if (!window.ZephtacoreSerial.isConnected()) {
      if (flashStatusText) flashStatusText.textContent = "Sin conexión serie: no se ha enviado ningún ajuste.";
      return false;
    }

    if (flashStatusText) flashStatusText.textContent = "Enviando configuración por puerto serie...";

    for (const cmd of buildConfigCommands(cfg)) {
      if (!(await window.ZephtacoreSerial.sendCommand(cmd))) {
        if (flashStatusText) flashStatusText.textContent = `Se interrumpió el envío en "${cmd}". Revisa el terminal.`;
        return false;
      }
      await new Promise((r) => setTimeout(r, 250));
    }

    alert(
      `Configuración enviada.\n\n` +
      `Nombre: ${cfg.name}\n` +
      `Frecuencia LoRa: ${cfg.freq} MHz (SF${cfg.sf} / BW${cfg.bw})\n\n` +
      `Se ha enviado el comando reboot. Comprueba las respuestas del nodo en el terminal.`
    );
    return true;
  }

  // Button to inject / start flash
  if (btnUnifiedFlash) {
    btnUnifiedFlash.addEventListener("click", () => runUnifiedFlash(pendingConfig));
  }
  if (btnInjectOnly) {
    btnInjectOnly.addEventListener("click", () => runInjectOnly(pendingConfig));
  }

  // Dropzone drag-and-drop
  if (dropzone && fileInput) {
    dropzone.addEventListener("click", () => fileInput.click());

    dropzone.addEventListener("dragover", (e) => {
      e.preventDefault();
      dropzone.classList.add("dragover");
    });

    dropzone.addEventListener("dragleave", () => {
      dropzone.classList.remove("dragover");
    });

    dropzone.addEventListener("drop", (e) => {
      e.preventDefault();
      dropzone.classList.remove("dragover");
      if (e.dataTransfer.files.length > 0) {
        handleFileSelected(e.dataTransfer.files[0]);
      }
    });

    fileInput.addEventListener("change", (e) => {
      if (e.target.files.length > 0) {
        handleFileSelected(e.target.files[0]);
      }
    });
  }

  function handleFileSelected(file) {
    activeFile = file;

    // Check if user uploaded a prefs.json backup
    if (file.name.endsWith(".json")) {
      const reader = new FileReader();
      reader.onload = (e) => {
        try {
          const imported = JSON.parse(e.target.result);
          if (imported) {
            pendingConfig = {
              name: imported.name || "ZephtaNode",
              adminPass: imported.password || "Secr3tAdmin!",
              freq: (imported.radio && imported.radio.freq) || "869.618",
              sf: (imported.radio && imported.radio.sf) || "7",
              bw: (imported.radio && imported.radio.bw) || "62.5",
              cr: (imported.radio && imported.radio.cr) || "2",
              txPower: (imported.radio && imported.radio.tx_power) || "22",
              mtRelay: (imported.mt && imported.mt.relay) || "essential",
              mtOpaque: imported.mt ? imported.mt.opaque : true,
              mtHops: (imported.mt && imported.mt.hops) || "7",
              pingBot: imported.mt ? imported.mt.ping : false
            };
            if (pendingConfigBanner) {
              pendingConfigBanner.style.display = "flex";
              if (cfgBadgeName) cfgBadgeName.textContent = `Respaldo: ${pendingConfig.name}`;
              if (cfgBadgeFreq) cfgBadgeFreq.textContent = `${pendingConfig.freq} MHz`;
            }
            alert(`¡Respaldo "${file.name}" cargado con éxito! Pulsa "Enviar ajustes por USB" para restaurar tu nodo.`);
          }
        } catch (err) {
          alert(`Error al leer el archivo JSON: ${err.message}`);
        }
      };
      reader.readAsText(file);
      return;
    }

    // Binary file
    const reader = new FileReader();
    reader.onload = (e) => {
      activeBuffer = e.target.result;
      if (flashStatusText) {
        flashStatusText.textContent = `Archivo listo: ${file.name} (${Math.round(file.size / 1024)} KB)`;
      }
      if (progressCard) progressCard.style.display = "block";
      if (progressBar) progressBar.style.width = "100%";
      if (progressPercent) progressPercent.textContent = "Cargado";
    };
    reader.readAsArrayBuffer(file);
  }

  // Resolve real firmware binary URL from official catalog (xejarque fork with fallback)
  async function resolveFirmwareUrl(board, role) {
    const roleKey = (role || "companion").toLowerCase();
    const primaryCatalog = "https://cdn.jsdelivr.net/gh/xejarque/ZephCore@firmware-dist/catalog.json";
    const fallbackCatalog = "https://cdn.jsdelivr.net/gh/liquidraver/ZephCore@firmware-dist/catalog.json";

    for (const catUrl of [primaryCatalog, fallbackCatalog]) {
      try {
        const resp = await fetch(catUrl + "?_t=" + Date.now());
        if (!resp.ok) continue;
        const catalog = await resp.json();
        const devices = catalog.device || [];
        const dev = devices.find((d) => {
          const dn = (d.name || "").toLowerCase();
          const bn = (board.name || "").toLowerCase();
          const bid = (board.id || "").toLowerCase().split("/")[0];
          return dn.includes(bn) || bn.includes(dn) || (d.firmware && JSON.stringify(d.firmware).toLowerCase().includes(bid));
        });

        if (dev && dev.firmware) {
          const targetRole = roleKey === "repeater" ? "repeater" : "companion";
          const fw = dev.firmware.find((f) => (f.role || "").toLowerCase().startsWith(targetRole)) || dev.firmware[0];
          if (fw && fw.version) {
            const verKey = Object.keys(fw.version)[0];
            const files = (fw.version[verKey] && fw.version[verKey].files) || [];
            if (board.flashType === "uf2") {
              const fileObj = files.find((f) => f.name.endsWith(".uf2"));
              if (fileObj && fileObj.url) return fileObj.url;
            } else if (board.flashType === "hex") {
              const fileObj = files.find((f) => f.name.endsWith(".hex"));
              if (fileObj && fileObj.url) return fileObj.url;
            } else {
              const fileObj = files.find((f) => f.name.includes("merged") || f.name.endsWith(".bin"));
              if (fileObj && fileObj.url) return fileObj.url;
            }
          }
        }
      } catch (err) {
        console.warn("Catalog fetch check error:", err);
      }
    }
    return null;
  }

  function setProgress(percent, text) {
    if (flashStatusText && text) flashStatusText.textContent = text;
    if (progressBar) progressBar.style.width = `${percent}%`;
    if (progressPercent) progressPercent.textContent = `${percent}%`;
  }

  // Downloads the catalog binary for the board and role. Throws when there is none.
  async function downloadFirmware(board, role) {
    const url = await resolveFirmwareUrl(board, role);
    if (!url) throw new Error(`No hay firmware publicado para ${board.name} (${role}).`);

    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`La descarga de ${url.split("/").pop()} falló (HTTP ${resp.status}).`);

    const name = url.split("/").pop();
    const a = document.createElement("a");
    a.href = URL.createObjectURL(await resp.blob());
    a.download = name;
    a.click();
    return name;
  }

  function afterDownloadHint(board, fileName) {
    const next = 'Cuando la placa arranque con el firmware nuevo, pulsa "Enviar ajustes por USB".';
    if (board.flashType === "uf2") return `Descargado "${fileName}". Cópialo al disco UF2 de la placa. ${next}`;
    if (board.flashType === "hex") return `Descargado "${fileName}". Prográmalo con west flash o una sonda SWD. ${next}`;
    return `Descargado "${fileName}". Esta web no graba ESP32: grábalo en 0x0 con esptool. ${next}`;
  }

  // Step 1: download the firmware for the board and role.
  async function runUnifiedFlash(cfg) {
    const configToUse = cfg || pendingConfig;
    const boardId = (configToUse && configToUse.boardId) || (boardSelect ? boardSelect.value : "t1000_e");
    const board = (window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS.find((b) => b.id === boardId)) || (window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS[0]);
    if (!board) return;
    const targetRole = (configToUse && configToUse.role) || "companion";

    if (progressCard) {
      progressCard.style.display = "block";
      progressCard.scrollIntoView({ behavior: "smooth" });
    }

    try {
      setProgress(20, `Consultando catálogo de firmware para ${board.name}...`);
      const fileName = await downloadFirmware(board, targetRole);
      setProgress(60, afterDownloadHint(board, fileName));
    } catch (err) {
      console.error("Firmware download error:", err);
      setProgress(0, `Error: ${err.message}`);
    }
  }

  // Step 2: the board runs the new firmware, send the settings.
  async function runInjectOnly(cfg) {
    const configToUse = cfg || pendingConfig;
    if (!configToUse) return;
    if (!("serial" in navigator)) {
      alert("Web Serial no está soportado en este navegador. Por favor usa Google Chrome o Microsoft Edge en tu ordenador.");
      return;
    }

    if (progressCard) progressCard.style.display = "block";
    if (!(await window.ZephtacoreSerial.connect())) return;
    if (await injectConfigSequence(configToUse)) {
      setProgress(100, "Ajustes enviados. El nodo se reinicia.");
    }
  }

  // Flasher Trigger Action
  if (btnStartFlash) {
    btnStartFlash.addEventListener("click", () => {
      if (progressCard) progressCard.style.display = "block";
      if (!activeFile) {
        setProgress(0, "Selecciona primero un archivo de firmware.");
        return;
      }
      setProgress(0, `Esta web no graba archivos locales. Copia "${activeFile.name}" al disco UF2 de la placa o grábalo con esptool (ESP32, en 0x0), y después usa "Enviar ajustes por USB".`);
    });
  }
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initFlasherCore);
} else {
  initFlasherCore();
}
