/**
 * Zephtacore Web Serial & Interactive Terminal Controller
 */
window.ZephtacoreSerial = (function () {
  let port = null;
  let reader = null;
  let writer = null;
  let readableStreamClosed = null;
  let writableStreamClosed = null;
  let isConnected = false;
  let commandHistory = [];
  let historyIdx = -1;

  const terminalBody = () => document.getElementById("terminalBody");
  const terminalInput = () => document.getElementById("terminalInput");
  const connectBtn = () => document.getElementById("btnSerialConnect");
  const statusBadge = () => document.getElementById("serialStatusBadge");

  function logToTerminal(text, type = "info") {
    const el = terminalBody();
    if (!el) return;
    const span = document.createElement("span");
    span.className = `term-line term-${type}`;
    span.textContent = text;
    el.appendChild(span);
    el.scrollTop = el.scrollHeight;
  }

  async function connect() {
    if (!("serial" in navigator)) {
      alert("Tu navegador no soporta Web Serial API. Por favor usa Google Chrome, Microsoft Edge o Opera en escritorio.");
      return false;
    }

    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });

      isConnected = true;
      updateUIState(true);
      logToTerminal("\n[✓] Conectado exitosamente al puerto serie a 115200 baudios.\n", "success");
      logToTerminal("Escribe un comando o pulsa un botón de diagnóstico rápido abajo.\n", "dim");

      // Setup writer
      const textEncoder = new TextEncoderStream();
      writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
      writer = textEncoder.writable.getWriter();

      // Setup reader
      readLoop();
      return true;
    } catch (err) {
      console.error("Serial connection error:", err);
      logToTerminal(`[!] Error al abrir puerto: ${err.message}\n`, "error");
      isConnected = false;
      port = null;
      updateUIState(false);
      return false;
    }
  }

  async function disconnect() {
    if (!port) return;
    try {
      if (reader) {
        await reader.cancel();
        await readableStreamClosed.catch(() => {});
        reader = null;
      }
      if (writer) {
        await writer.close();
        await writableStreamClosed;
        writer = null;
      }
      await port.close();
      port = null;
      isConnected = false;
      updateUIState(false);
      logToTerminal("\n[-] Puerto serie desconectado.\n", "info");
    } catch (err) {
      console.error("Disconnect error:", err);
    }
  }

  async function readLoop() {
    const textDecoder = new TextDecoderStream();
    readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
    reader = textDecoder.readable.getReader();

    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          logToTerminal(value, "rx");
        }
      }
    } catch (err) {
      if (isConnected) {
        logToTerminal(`[!] Error de lectura: ${err.message}\n`, "error");
      }
    } finally {
      reader.releaseLock();
    }
  }

  async function sendCommand(cmd) {
    if (!writer || !isConnected) {
      logToTerminal("[!] No hay conexión serie activa. Pulsa 'Conectar Puerto Serie'.\n", "error");
      return false;
    }
    const cleanCmd = cmd.trim();
    if (!cleanCmd) return false;

    logToTerminal(`> ${cleanCmd}\n`, "tx");
    await writer.write(`${cleanCmd}\r\n`);

    if (commandHistory[commandHistory.length - 1] !== cleanCmd) {
      commandHistory.push(cleanCmd);
    }
    historyIdx = commandHistory.length;
    return true;
  }

  function updateUIState(connected) {
    const btn = connectBtn();
    const badge = statusBadge();
    if (btn) {
      btn.textContent = connected ? "🔌 Desconectar" : "⚡ Conectar Puerto Serie";
      btn.className = connected ? "btn btn-outline btn-sm" : "btn btn-primary btn-sm";
    }
    if (badge) {
      badge.textContent = connected ? "ONLINE (115200)" : "OFFLINE";
      badge.style.color = connected ? "#10b981" : "#94a3b8";
    }
  }

  function setup() {
    const btn = connectBtn();
    if (btn) {
      btn.addEventListener("click", () => {
        if (isConnected) disconnect();
        else connect();
      });
    }

    const input = terminalInput();
    if (input) {
      input.addEventListener("keydown", (e) => {
        if (e.key === "Enter") {
          sendCommand(input.value);
          input.value = "";
        } else if (e.key === "ArrowUp") {
          if (historyIdx > 0) {
            historyIdx--;
            input.value = commandHistory[historyIdx] || "";
          }
        } else if (e.key === "ArrowDown") {
          if (historyIdx < commandHistory.length - 1) {
            historyIdx++;
            input.value = commandHistory[historyIdx] || "";
          } else {
            historyIdx = commandHistory.length;
            input.value = "";
          }
        }
      });
    }

    // Diagnostic Shortcuts
    document.querySelectorAll(".btn-command-shortcut").forEach((b) => {
      b.addEventListener("click", () => {
        const cmd = b.getAttribute("data-cmd");
        if (cmd) sendCommand(cmd);
      });
    });
  }

  return {
    setup,
    sendCommand,
    connect,
    disconnect,
    isConnected: () => isConnected
  };
})();

document.addEventListener("DOMContentLoaded", () => {
  window.ZephtacoreSerial.setup();
});
