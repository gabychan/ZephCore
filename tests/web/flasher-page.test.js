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
async function loadPage({ fetchOk = true, board = "t1000_e" } = {}) {
  const els = {};
  const el = (id) =>
    (els[id] ||= { id, textContent: "", style: {}, value: "", disabled: false, innerHTML: "", addEventListener() {}, classList: { add() {}, remove() {} } });
  const doc = {
    readyState: "complete",
    getElementById: el,
    querySelectorAll: () => [],
    addEventListener() {},
    createElement: () => ({})
  };
  const win = {
    ZephtacoreLogic: L,
    ZEPHTACORE_BOARDS: [
      { id: "t1000_e", name: "T1000-E", mcu: "nRF52840", radio: "LR1110", flashType: "uf2", desc: "" },
      { id: "rak4631", name: "RAK4631", mcu: "nRF52840", radio: "SX1262", flashType: "uf2", desc: "" }
    ],
    ZEPHTACORE_ROLES: [{ id: "companion", name: "Companion" }, { id: "repeater", name: "Repeater" }, { id: "observer", name: "Observer" }],
    location: { search: "" }
  };
  const store = { getItem: () => null, setItem() {}, removeItem() {} };
  const fetchFn = async () => ({ ok: fetchOk, json: async () => catalog });
  const src = fs.readFileSync(path.join(__dirname, "../../custom_builder_web/js/flasher-core.js"), "utf8");
  els.flasherBoardSelect = { ...el("flasherBoardSelect"), value: board };
  els.flasherRoleSelect = { ...el("flasherRoleSelect"), value: "companion" };
  new Function("window", "document", "sessionStorage", "navigator", "fetch", "URLSearchParams", src)(win, doc, store, {}, fetchFn, URLSearchParams);
  await new Promise((r) => setTimeout(r, 20));
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
