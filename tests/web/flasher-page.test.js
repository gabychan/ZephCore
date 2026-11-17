const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const L = require("../../custom_builder_web/js/flasher-logic.js");

const CDN = "https://cdn.jsdelivr.net/gh/xejarque/ZephCore@firmware-dist/";
const catalog = {
  device: [{ firmware: [{ role: "companionUsb", version: { "1.18.2": { files: [
    { name: "t1000_e-companion-zephtacore-abc1234.uf2", url: CDN + "t1000_e-companion-zephtacore-abc1234.uf2" }
  ] } } }] }]
};

// Runs initFlasherCore against a minimal DOM and a fake catalog fetch.
async function loadPage({ fetchOk = true, board = "t1000_e", esptool = null, boards = null, install = "keep" } = {}) {
  const els = {};
  const downloads = [];
  const dialog = { shown: 0 };
  const el = (id) =>
    (els[id] ||= {
      id, textContent: "", style: {}, value: "", disabled: false, innerHTML: "",
      listeners: {}, scrollIntoView() {}, addEventListener(type, fn) { this.listeners[type] = fn; }, classList: { add() {}, remove() {} }
    });
  const doc = {
    readyState: "complete",
    getElementById: el,
    querySelectorAll: () => [],
    addEventListener() {},
    createElement: () => {
      const a = { click() { downloads.push(a.download); } };
      return a;
    }
  };
  const win = {
    ZephtacoreLogic: L,
    ZephtacoreSerial: { log() {} },
    ZephtacoreEsptool: esptool,
    ZEPHTACORE_BOARDS: boards || [
      { id: "t1000_e", name: "T1000-E", mcu: "nRF52840", radio: "LR1110", flashType: "uf2", desc: "" },
      { id: "rak4631", name: "RAK4631", mcu: "nRF52840", radio: "SX1262", flashType: "uf2", desc: "" }
    ],
    ZEPHTACORE_ROLES: [{ id: "companion", name: "Companion" }, { id: "repeater", name: "Repeater" }, { id: "observer", name: "Observer" }],
    location: { search: "" }
  };
  const store = { getItem: () => null, setItem() {}, removeItem() {} };
  const fetchFn = async (url) =>
    String(url).includes("catalog.json")
      ? { ok: fetchOk, json: async () => catalog }
      : { ok: true, arrayBuffer: async () => new Uint8Array([1, 2, 3, 4]).buffer };
  const src = fs.readFileSync(path.join(__dirname, "../../custom_builder_web/js/flasher-core.js"), "utf8");
  const choice = { keep: "btnInstallKeep", clean: "btnInstallClean", cancel: "btnInstallCancel" }[install];
  Object.assign(el("installModeDialog"), {
    showModal() { dialog.shown++; els[choice].listeners.click(); },
    close() {}
  });
  els.flasherBoardSelect = { ...el("flasherBoardSelect"), value: board };
  els.flasherRoleSelect = { ...el("flasherRoleSelect"), value: "companion" };
  const navigatorStub = { serial: { requestPort: async () => ({ fake: "port" }) } };
  new Function("window", "document", "sessionStorage", "navigator", "fetch", "URLSearchParams", src)(win, doc, store, navigatorStub, fetchFn, URLSearchParams);
  await new Promise((r) => setTimeout(r, 20));
  els.downloads = downloads;
  els.dialog = dialog;
  return els;
}

test("the page starts and offers only the roles the catalog publishes", async () => {
  const els = await loadPage();
  assert.ok(!els.flasherRoleSelect.innerHTML.includes("observer"));
  assert.ok(els.flasherRoleSelect.innerHTML.includes("companion") && els.flasherRoleSelect.innerHTML.includes("repeater"));
});

test("a board with a Zephtacore build is marked and its download button stays enabled", async () => {
  const els = await loadPage();
  assert.equal(els.previewBuildTag.textContent, "Build Zephtacore publicado");
  assert.equal(els.btnUnifiedFlash.disabled, false);
});

test("an unreachable catalog is reported and blocks the download", async () => {
  const els = await loadPage({ fetchOk: false });
  assert.equal(els.previewBuildTag.textContent, "Catálogo no disponible");
  assert.equal(els.btnUnifiedFlash.disabled, true);
});

test("a board without a Zephtacore build says so and blocks the download", async () => {
  const els = await loadPage({ board: "rak4631" });
  assert.match(els.previewBuildTag.textContent, /^Sin build Zephtacore/);
  assert.equal(els.btnUnifiedFlash.disabled, true);
});

const ESP_CDN_FILE = "heltec_v4-esp32s3-procpu-companion-zephtacore-abc1234-merged.bin";
const espCatalog = () => {
  catalog.device[0].firmware[0].version["1.18.2"].files.push({ name: ESP_CDN_FILE, url: CDN + ESP_CDN_FILE });
};
const espBoards = [{ id: "heltec_v4/esp32s3/procpu", name: "Heltec V4", mcu: "ESP32-S3", radio: "SX1262", flashType: "bin", desc: "" }];

function fakeEsptool(chipName) {
  const calls = { writeFlash: [], after: [], disconnected: 0 };
  class Transport { constructor(port) { this.port = port; } async disconnect() { calls.disconnected++; } }
  class ESPLoader {
    constructor(opts) { this.opts = opts; this.chip = { CHIP_NAME: chipName }; }
    async main() { return `${chipName} (QFN56) (revision v0.2)`; }
    async writeFlash(o) { calls.writeFlash.push(o); o.reportProgress(0, 50, 100); }
    async after(mode) { calls.after.push(mode); }
  }
  return { calls, module: { ESPLoader, Transport } };
}

test("ESP32: the merged image is written at 0x0 unchanged, then the board is reset", async () => {
  espCatalog();
  const esp = fakeEsptool("ESP32-S3");
  const els = await loadPage({ boards: espBoards, board: "heltec_v4/esp32s3/procpu", esptool: esp.module });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(esp.calls.writeFlash.length, 1);
  const w = esp.calls.writeFlash[0];
  assert.equal(w.fileArray[0].address, 0);
  assert.deepEqual([...w.fileArray[0].data], [1, 2, 3, 4]);
  assert.deepEqual([w.flashSize, w.flashMode, w.flashFreq, w.eraseAll], ["keep", "keep", "keep", false]);
  assert.deepEqual(esp.calls.after, ["hard_reset"]);
  assert.equal(esp.calls.disconnected, 1);
  assert.match(els.flashStatusText.textContent, /Firmware grabado/);
});

test("ESP32: a chip of another family stops the flash before anything is written", async () => {
  espCatalog();
  const esp = fakeEsptool("ESP32-C6");
  const els = await loadPage({ boards: espBoards, board: "heltec_v4/esp32s3/procpu", esptool: esp.module });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(esp.calls.writeFlash.length, 0);
  assert.deepEqual(esp.calls.after, []);
  assert.equal(esp.calls.disconnected, 1);
  assert.match(els.flashStatusText.textContent, /no es un ESP32-S3.*No se ha escrito nada/);
});

test("ESP32: keeping the data does not erase the flash", async () => {
  espCatalog();
  const esp = fakeEsptool("ESP32-S3");
  const els = await loadPage({ boards: espBoards, board: "heltec_v4/esp32s3/procpu", esptool: esp.module, install: "keep" });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(els.dialog.shown, 1);
  assert.equal(esp.calls.writeFlash[0].eraseAll, false);
});

test("ESP32: a clean install erases the whole flash while writing", async () => {
  espCatalog();
  const esp = fakeEsptool("ESP32-S3");
  const els = await loadPage({ boards: espBoards, board: "heltec_v4/esp32s3/procpu", esptool: esp.module, install: "clean" });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(esp.calls.writeFlash[0].eraseAll, true);
  assert.deepEqual(esp.calls.after, ["hard_reset"]);
});

test("cancelling the install question touches nothing", async () => {
  espCatalog();
  const esp = fakeEsptool("ESP32-S3");
  const els = await loadPage({ boards: espBoards, board: "heltec_v4/esp32s3/procpu", esptool: esp.module, install: "cancel" });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(esp.calls.writeFlash.length, 0);
  assert.equal(esp.calls.disconnected, 0);
});

test("UF2: without a formatter in the catalog the question is not asked and the firmware downloads", async () => {
  const els = await loadPage({ install: "clean" });
  await els.btnUnifiedFlash.listeners.click();
  assert.equal(els.dialog.shown, 0);
  assert.deepEqual(els.downloads, ["t1000_e-companion-zephtacore-abc1234.uf2"]);
});

test("UF2: a clean install hands out the formatter first and the firmware after the board is formatted", async () => {
  catalog.device[0].firmware[0].erase = CDN + "SoftDevice_v7_formatter.zip";
  try {
    const els = await loadPage({ install: "clean" });
    await els.btnUnifiedFlash.listeners.click();
    assert.equal(els.dialog.shown, 1);
    assert.deepEqual(els.downloads, ["SoftDevice_v7_formatter.uf2"]);
    assert.equal(els.btnAfterFormat.style.display, "");
    await els.btnAfterFormat.listeners.click();
    assert.deepEqual(els.downloads, ["SoftDevice_v7_formatter.uf2", "t1000_e-companion-zephtacore-abc1234.uf2"]);
    assert.equal(els.btnAfterFormat.style.display, "none");
  } finally {
    delete catalog.device[0].firmware[0].erase;
  }
});

test("UF2: keeping the data downloads only the firmware even when a formatter exists", async () => {
  catalog.device[0].firmware[0].erase = CDN + "SoftDevice_v7_formatter.zip";
  try {
    const els = await loadPage({ install: "keep" });
    await els.btnUnifiedFlash.listeners.click();
    assert.deepEqual(els.downloads, ["t1000_e-companion-zephtacore-abc1234.uf2"]);
  } finally {
    delete catalog.device[0].firmware[0].erase;
  }
});
