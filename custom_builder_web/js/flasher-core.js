/**
 * Zephtacore Web Flasher Core Engine - Direct USB Configuration Injection
 */
function initFlasherCore() {
  const { buildConfigCommands, catalogFiles, findCatalogFile, isErrorReply } = window.ZephtacoreLogic;
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
  const previewBuildTag = document.getElementById("previewBuildTag");

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

  // build.sh publishes only these two roles; the others are built with west.
  const PUBLISHED_ROLES = ["companion", "repeater"];

  if (roleSelect && window.ZEPHTACORE_ROLES) {
    roleSelect.innerHTML = window.ZEPHTACORE_ROLES.filter((r) => PUBLISHED_ROLES.includes(r.id)).map(
      (r) => `<option value="${r.id}">${r.name}</option>`
    ).join("");
  }

  // Settings come from the Builder (same tab, sessionStorage) and are dropped once sent.
  const PENDING_KEY = "zephtacore_pending_config";
  const savedCfgStr = sessionStorage.getItem(PENDING_KEY);

  function showPendingConfig() {
    if (!pendingConfig || !pendingConfigBanner) return;
    pendingConfigBanner.style.display = "flex";
    if (cfgBadgeName) cfgBadgeName.textContent = `Nodo: ${pendingConfig.name}`;
    if (cfgBadgeFreq) cfgBadgeFreq.textContent = `${pendingConfig.freq} MHz (SF${pendingConfig.sf || 7}/BW${pendingConfig.bw || 62.5})`;
    if (cfgBadgeRole) cfgBadgeRole.textContent = `Rol: ${pendingConfig.role || "companion"}`;
    if (cfgBadgeHops) cfgBadgeHops.textContent = `Saltos: ${pendingConfig.mtHops || 7}`;
    if (cfgBadgeBoard) {
      const b = window.ZEPHTACORE_BOARDS && window.ZEPHTACORE_BOARDS.find((x) => x.id === pendingConfig.boardId);
      cfgBadgeBoard.textContent = `Placa: ${b ? b.name : (pendingConfig.boardName || pendingConfig.boardId || "Auto")}`;
    }
  }

  if (savedCfgStr) {
    try {
      pendingConfig = JSON.parse(savedCfgStr);
      showPendingConfig();

      // Preselect what the Builder chose; the selects stay the source of truth.
      if (boardSelect && pendingConfig.boardId) boardSelect.value = pendingConfig.boardId;
      if (roleSelect && pendingConfig.role) roleSelect.value = pendingConfig.role;
    } catch (e) {
      console.warn("Error parsing pending config:", e);
      sessionStorage.removeItem(PENDING_KEY);
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
        previewFlashTag.textContent = "Grabado por USB (esptool-js, 0x0)";
        previewFlashTag.style.background = "rgba(0, 242, 254, 0.1)";
        previewFlashTag.style.color = "var(--brand-cyan)";
        previewFlashTag.style.borderColor = "rgba(0, 242, 254, 0.3)";
      }
    }
  }

  if (boardSelect) {
    boardSelect.addEventListener("change", () => {
      syncBoardPreview();
      refreshBuildAvailability();
    });
    syncBoardPreview();
  }
  if (roleSelect) roleSelect.addEventListener("change", refreshBuildAvailability);

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

  // Sends the settings over the serial port and stops at the first command the node
  // refuses, does not answer or cannot be sent. Returns true when all were accepted.
  function fail(cmd, reason) {
    if (flashStatusText) flashStatusText.textContent = `Envío interrumpido en "${cmd}": ${reason}. Revisa el terminal.`;
    return false;
  }

  async function injectConfigSequence(cfg) {
    if (!window.ZephtacoreSerial.isConnected()) {
      if (flashStatusText) flashStatusText.textContent = "Sin conexión serie: no se ha enviado ningún ajuste.";
      return false;
    }

    if (flashStatusText) flashStatusText.textContent = "Enviando configuración por puerto serie...";

    const serial = window.ZephtacoreSerial;
    for (const cmd of buildConfigCommands(cfg)) {
      // reboot drops the port as it answers, so it is sent without waiting.
      if (cmd === "reboot") {
        if (!(await serial.sendCommand(cmd))) return fail(cmd, "no se pudo enviar");
        continue;
      }
      const reply = await serial.sendCommandAndWait(cmd);
      if (!reply.sent) return fail(cmd, "no se pudo enviar");
      if (reply.timedOut) return fail(cmd, "el nodo no respondió");
      if (isErrorReply(reply.text)) return fail(cmd, `el nodo respondió: ${reply.text}`);
    }

    sessionStorage.removeItem(PENDING_KEY);
    alert(
      `Configuración enviada.\n\n` +
      `Nombre: ${cfg.name}\n` +
      `Frecuencia LoRa: ${cfg.freq} MHz (SF${cfg.sf} / BW${cfg.bw})\n\n` +
      `El nodo aceptó todos los comandos y se reinicia.`
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
              adminPass: imported.password || "",
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

  const CATALOG_URLS = [
    "https://cdn.jsdelivr.net/gh/xejarque/ZephCore@firmware-dist/catalog.json",
    "https://cdn.jsdelivr.net/gh/liquidraver/ZephCore@firmware-dist/catalog.json"
  ];
  // Files of the catalogs that load, fetched once per page.
  let catalogFilesPromise = null;
  function loadCatalogFiles() {
    if (!catalogFilesPromise) {
      catalogFilesPromise = Promise.all(
        CATALOG_URLS.map(async (catUrl) => {
          try {
            const resp = await fetch(`${catUrl}?_t=${Date.now()}`);
            return resp.ok ? catalogFiles(await resp.json()) : null;
          } catch (err) {
            console.warn("Catalog fetch error:", catUrl, err);
            return null;
          }
        })
      ).then((lists) => (lists.some(Boolean) ? lists.flat().filter(Boolean) : null));
    }
    return catalogFilesPromise;
  }

  async function resolveFirmwareUrl(board, role) {
    if (!PUBLISHED_ROLES.includes(role)) {
      throw new Error(`El rol "${role}" no se publica como binario: compílalo con west desde el Builder.`);
    }
    const files = await loadCatalogFiles();
    const file = files && findCatalogFile(files, board, role);
    return file ? file.url : null;
  }

  // Tells whether the catalog has a Zephtacore build for the selected board and role.
  async function refreshBuildAvailability() {
    if (!previewBuildTag) return;
    const board = selectedBoard();
    const role = selectedRole();
    if (!board) return;

    previewBuildTag.textContent = "Consultando catálogo...";
    const files = await loadCatalogFiles();
    if (board !== selectedBoard() || role !== selectedRole()) return;

    const available = !!files && PUBLISHED_ROLES.includes(role) && !!findCatalogFile(files, board, role);
    previewBuildTag.textContent = !files
      ? "Catálogo no disponible"
      : available ? "Build Zephtacore publicado" : "Sin build Zephtacore: compílalo con west";
    previewBuildTag.style.color = available ? "var(--brand-emerald)" : "var(--text-muted)";
    if (btnUnifiedFlash) {
      btnUnifiedFlash.disabled = !available;
      btnUnifiedFlash.textContent = board.flashType === "bin" ? "⚡ Grabar firmware por USB" : "⬇️ Descargar firmware";
    }
  }

  function setProgress(percent, text) {
    if (flashStatusText && text) flashStatusText.textContent = text;
    if (progressBar) progressBar.style.width = `${percent}%`;
    if (progressPercent) progressPercent.textContent = `${percent}%`;
  }

  // Fetches the catalog binary for the board and role. Throws when there is none.
  async function fetchFirmware(board, role) {
    const url = await resolveFirmwareUrl(board, role);
    if (!url) throw new Error(`No hay build Zephtacore publicado para ${board.name} (${role}). Compílalo con west desde el Builder.`);

    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`La descarga de ${url.split("/").pop()} falló (HTTP ${resp.status}).`);
    return { name: url.split("/").pop(), bytes: new Uint8Array(await resp.arrayBuffer()) };
  }

  async function downloadFirmware(board, role) {
    const { name, bytes } = await fetchFirmware(board, role);
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([bytes], { type: "application/octet-stream" }));
    a.download = name;
    a.click();
    return name;
  }

  // ESP32: writes the merged image at 0x0 with esptool-js. The chip must answer and be the
  // board's own family before anything is written.
  async function flashEsp32(board, role) {
    if (!("serial" in navigator)) {
      alert("Web Serial no está soportado en este navegador. Por favor usa Google Chrome o Microsoft Edge en tu ordenador.");
      return;
    }
    const serial = window.ZephtacoreSerial;
    let transport = null;
    try {
      // requestPort needs a recent user gesture: ask for it before the download.
      setProgress(5, "Elige el puerto de la placa (si no aparece, mantén BOOT y pulsa RESET).");
      const port = await navigator.serial.requestPort();

      setProgress(10, `Descargando el firmware de ${board.name}...`);
      const { name, bytes } = await fetchFirmware(board, role);
      const { ESPLoader, Transport } = window.ZephtacoreEsptool || (await import("../vendor/esptool-js.bundle.js"));
      transport = new Transport(port, false);
      const terminal = {
        clean() {},
        write: (text) => serial.log(text, "info"),
        writeLine: (text) => serial.log(`${text}\n`, "info")
      };
      const loader = new ESPLoader({ transport, baudrate: 460800, romBaudrate: 115200, terminal });

      setProgress(15, "Conectando con el chip...");
      const chipDescription = await loader.main();
      if (loader.chip.CHIP_NAME.toUpperCase() !== board.mcu.toUpperCase()) {
        throw new Error(`El chip detectado (${chipDescription}) no es un ${board.mcu}. No se ha escrito nada.`);
      }

      setProgress(20, `Grabando ${name} en 0x0...`);
      await loader.writeFlash({
        fileArray: [{ data: bytes, address: 0 }],
        flashSize: "keep",
        flashMode: "keep",
        flashFreq: "keep",
        eraseAll: false,
        compress: true,
        reportProgress: (_file, written, total) => setProgress(20 + Math.round((written / total) * 75), `Grabando ${name}...`)
      });

      await loader.after("hard_reset");
      setProgress(100, 'Firmware grabado y placa reiniciada. Cuando arranque, pulsa "Enviar ajustes por USB".');
    } catch (err) {
      console.error("ESP32 flash error:", err);
      setProgress(0, `Error: ${err.message}`);
    } finally {
      if (transport) await transport.disconnect().catch(() => {});
    }
  }

  function afterDownloadHint(board, fileName) {
    const next = 'Cuando la placa arranque con el firmware nuevo, pulsa "Enviar ajustes por USB".';
    if (board.flashType === "uf2") return `Descargado "${fileName}". Cópialo al disco UF2 de la placa. ${next}`;
    if (board.flashType === "hex") return `Descargado "${fileName}". Prográmalo con west flash o una sonda SWD. ${next}`;
    return `Descargado "${fileName}". ${next}`;
  }

  function selectedBoard() {
    const boards = window.ZEPHTACORE_BOARDS || [];
    return boards.find((b) => b.id === (boardSelect && boardSelect.value)) || boards[0];
  }

  function selectedRole() {
    return (roleSelect && roleSelect.value) || "companion";
  }

  // Step 1: download the firmware for the board and role.
  async function runUnifiedFlash(cfg) {
    const board = selectedBoard();
    if (!board) return;
    const targetRole = selectedRole();

    if (progressCard) {
      progressCard.style.display = "block";
      progressCard.scrollIntoView({ behavior: "smooth" });
    }

    if (board.flashType === "bin") {
      await flashEsp32(board, targetRole);
      return;
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
    const base = cfg || pendingConfig;
    if (!base) return;
    const configToUse = { ...base, role: selectedRole() };
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
      setProgress(0, `Esta web graba solo el firmware del catálogo, no archivos locales. Copia "${activeFile.name}" al disco UF2 de la placa o grábalo con esptool (ESP32, en 0x0), y después usa "Enviar ajustes por USB".`);
    });
  }

  refreshBuildAvailability();
}

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initFlasherCore);
} else {
  initFlasherCore();
}
