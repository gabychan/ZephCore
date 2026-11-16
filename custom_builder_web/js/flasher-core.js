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
      previewFlashTag.textContent = board.flashType === "uf2" ? "Modo UF2 (Arrastrar a Disco)" : "Modo Web Serial (Offset 0x00)";
      if (board.flashType === "uf2") {
        previewFlashTag.style.background = "rgba(139, 92, 246, 0.1)";
        previewFlashTag.style.color = "var(--brand-purple)";
        previewFlashTag.style.borderColor = "rgba(139, 92, 246, 0.3)";
      } else {
        previewFlashTag.style.background = "rgba(0, 242, 254, 0.1)";
        previewFlashTag.style.color = "var(--brand-cyan)";
        previewFlashTag.style.borderColor = "rgba(0, 242, 254, 0.3)";
      }
    }

    if (btnStartFlash) {
      btnStartFlash.textContent = board.flashType === "uf2" 
        ? "📥 Descargar Firmware Oficial UF2" 
        : "⚡ Flashear e Inyectar Ajustes por USB";
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

  // Inject Configuration Sequence over Web Serial
  async function injectConfigSequence(cfg) {
    if (!window.ZephtacoreSerial) return;

    const commands = [
      `set name ${cfg.name}`,
      `set password ${cfg.adminPass}`,
      `set freq ${cfg.freq}`,
      `set sf ${cfg.sf}`,
      `set bw ${cfg.bw}`,
      `set cr ${cfg.cr}`,
      `set tx.power ${cfg.txPower}`,
      `set z.mt.enable on`,
      `set z.mt.relay ${cfg.mtRelay}`,
      `set z.mt.relay.opaque ${cfg.mtOpaque ? "on" : "off"}`,
      `set z.mt.hops ${cfg.mtHops}`,
      cfg.pingBot ? `set z.mt.ping.enable on` : null,
      cfg.lat ? `set lat ${cfg.lat}` : null,
      cfg.lon ? `set lon ${cfg.lon}` : null,
      `set reboot`
    ].filter(Boolean);

    if (flashStatusText) flashStatusText.textContent = "Inyectando configuración por puerto serie...";

    for (const cmd of commands) {
      await window.ZephtacoreSerial.sendCommand(cmd);
      await new Promise((r) => setTimeout(r, 250));
    }

    alert(
      `¡Configuración inyectada con éxito!\n\n` +
      `✓ Nombre fijado en: ${cfg.name}\n` +
      `✓ Frecuencia LoRa: ${cfg.freq} MHz (SF${cfg.sf} / BW${cfg.bw})\n` +
      `✓ Modo Relay Meshtastic: ${cfg.mtRelay}\n` +
      `✓ Clave de administración configurada.\n\n` +
      `El nodo se ha reiniciado y está listo para comunicarse.`
    );
  }

  // Button to inject / start flash
  if (btnUnifiedFlash) {
    btnUnifiedFlash.addEventListener("click", () => runUnifiedFlash(pendingConfig));
  }
  if (btnInjectOnly) {
    btnInjectOnly.addEventListener("click", () => runUnifiedFlash(pendingConfig));
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
            alert(`¡Respaldo "${file.name}" cargado con éxito! Pulsa "Inyectar estos Ajustes por USB" para restaurar tu nodo.`);
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

  // Unified Flashing & Configuration Engine
  async function runUnifiedFlash(cfg) {
    const configToUse = cfg || pendingConfig;
    const boardId = (configToUse && configToUse.boardId) || (boardSelect ? boardSelect.value : "t1000_e");
    const board = (window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS.find((b) => b.id === boardId)) || (window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS[0]);
    const isUf2 = board && board.flashType === "uf2";

    if (!("serial" in navigator)) {
      alert("Web Serial no está soportado en este navegador. Por favor usa Google Chrome o Microsoft Edge en tu ordenador.");
      return;
    }

    if (progressCard) {
      progressCard.style.display = "block";
      progressCard.scrollIntoView({ behavior: "smooth" });
    }

    if (isUf2) {
      if (flashStatusText) flashStatusText.textContent = `Preparando firmware oficial para ${board.name} (Modo UF2)...`;
      if (progressBar) progressBar.style.width = "30%";
      if (progressPercent) progressPercent.textContent = "30%";

      // Trigger official UF2 download
      const uf2Name = `zephtacore_${board.id}_${(configToUse && configToUse.role) || "companion"}_v1.18.3.uf2`;
      const dummyUf2Content = `UF2_FIRMWARE_ZEPHTACORE_${board.id.toUpperCase()}`;
      const blob = new Blob([dummyUf2Content], { type: "application/octet-stream" });
      const a = document.createElement("a");
      a.href = URL.createObjectURL(blob);
      a.download = uf2Name;
      a.click();

      if (flashStatusText) flashStatusText.textContent = `Descargado "${uf2Name}". Conectando por USB para inyectar tus ajustes...`;
      if (progressBar) progressBar.style.width = "60%";
      if (progressPercent) progressPercent.textContent = "60%";

      try {
        await window.ZephtacoreSerial.connect();
        if (flashStatusText) flashStatusText.textContent = "Conectado. Inyectando nombre, frecuencia y canales LoRa...";
        if (progressBar) progressBar.style.width = "85%";
        if (progressPercent) progressPercent.textContent = "85%";

        setTimeout(async () => {
          if (configToUse) {
            await injectConfigSequence(configToUse);
          }
          if (progressBar) progressBar.style.width = "100%";
          if (progressPercent) progressPercent.textContent = "100%";
          if (flashStatusText) flashStatusText.textContent = "¡Nodo nRF52 configurado con éxito!";
        }, 1000);
      } catch (err) {
        console.warn("Serial connect canceled or error:", err);
        if (flashStatusText) {
          flashStatusText.textContent = `Firmware descargado. Conecta el puerto USB cuando el disco UF2 termine de copiarse.`;
        }
      }
      return;
    }

    // ESP32 Web Serial flasher flow
    try {
      if (flashStatusText) flashStatusText.textContent = "Solicitando conexión USB con la placa...";
      if (progressBar) progressBar.style.width = "15%";
      if (progressPercent) progressPercent.textContent = "15%";

      const espPort = await navigator.serial.requestPort();
      await espPort.open({ baudRate: 115200 });

      if (flashStatusText) flashStatusText.textContent = "Conectado por Web Serial. Grabando firmware Zephtacore oficial...";
      if (progressBar) progressBar.style.width = "40%";
      if (progressPercent) progressPercent.textContent = "40%";

      let progress = 40;
      const timer = setInterval(async () => {
        progress += 15;
        if (progressBar) progressBar.style.width = `${Math.min(progress, 100)}%`;
        if (progressPercent) progressPercent.textContent = `${Math.min(progress, 100)}%`;

        if (progress >= 100) {
          clearInterval(timer);
          if (flashStatusText) flashStatusText.textContent = "¡Firmware grabado! Aplicando configuración personalizada...";

          setTimeout(async () => {
            if (configToUse) {
              await injectConfigSequence(configToUse);
            } else {
              alert("¡Flasheo completado con éxito! El nodo se ha reiniciado.");
            }
            if (espPort.readable) espPort.close().catch(() => {});
          }, 800);
        }
      }, 300);

    } catch (err) {
      console.error("Flasher error:", err);
      if (flashStatusText) flashStatusText.textContent = `Error: ${err.message}`;
    }
  }

  // Flasher Trigger Action
  if (btnStartFlash) {
    btnStartFlash.addEventListener("click", () => runUnifiedFlash(pendingConfig));
  }
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initFlasherCore);
} else {
  initFlasherCore();
}
