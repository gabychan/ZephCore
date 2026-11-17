const test = require("node:test");
const assert = require("node:assert/strict");
const L = require("../../custom_builder_web/js/flasher-logic.js");

const CDN = "https://cdn.jsdelivr.net/gh/xejarque/ZephCore@firmware-dist/";
const file = (name, url = CDN + name) => ({ name, url });
const catalog = (versions) => ({ device: [{ firmware: [{ role: "x", version: versions }] }] });

test("coding rate: builder values 1-4 become 5-8, firmware values pass through", () => {
  assert.equal(L.firmwareCodingRate("1"), 5);
  assert.equal(L.firmwareCodingRate(2), 6);
  assert.equal(L.firmwareCodingRate("4"), 8);
  assert.equal(L.firmwareCodingRate("5"), 5);
  assert.equal(L.firmwareCodingRate("8"), 8);
});

const cfg = {
  name: "Node", adminPass: "pw", freq: "869.618", bw: "62.5", sf: "7", cr: "2", txPower: "22",
  mtRelay: "essential", mtOpaque: true, mtHops: "7", pingBot: false, lat: "", lon: ""
};

test("repeater and companion commands follow docs/CLI_commands.md", () => {
  assert.deepEqual(L.buildConfigCommands({ ...cfg, role: "repeater" }), [
    "set name Node", "password pw", "set radio 869.618,62.5,7,6", "set tx 22",
    "set z.mt.enable on", "set z.mt.relay essential", "set z.mt.relay.opaque on", "set z.mt.hops 7",
    "reboot"
  ]);
});

test("no password is sent when none is set; ping bot carries its command; position only when set", () => {
  const cmds = L.buildConfigCommands({ ...cfg, adminPass: "", pingBot: true, lat: "41.4", lon: "2.1", mtOpaque: false });
  assert.ok(!cmds.some((c) => c.startsWith("password")));
  assert.ok(cmds.includes("set z.mt.ping.enable on /ping"));
  assert.ok(cmds.includes("set z.mt.relay.opaque off"));
  assert.ok(cmds.includes("set lat 41.4") && cmds.includes("set lon 2.1"));
  assert.equal(cmds.at(-1), "reboot");
});

test("observer gets its own commands: no password, tx or z.mt", () => {
  assert.deepEqual(L.buildConfigCommands({ ...cfg, role: "observer" }), [
    "set name Node", "set freq 869.618", "set sf 7", "set bw 62.5", "set cr 6", "reboot"
  ]);
});

test("catalogFiles takes only the newest version of each firmware", () => {
  const files = L.catalogFiles(catalog({
    "1.9.0": { files: [file("old")] },
    "1.18.2": { files: [file("new")] },
    "1.10.0": { files: [file("mid")] }
  }));
  assert.deepEqual(files.map((f) => f.name), ["new"]);
});

test("findCatalogFile matches the exact target, role and zephtacore variant", () => {
  const files = [
    file("t1000_e-companion-abc1234.uf2"),
    file("t1000_e-companion-zephtacore-abc1234.uf2"),
    file("t1000_e-repeater-zephtacore-abc1234.uf2"),
    file("heltec_v4-esp32s3-procpu-companion-zephtacore-def5678-merged.bin"),
    file("heltec_v4-esp32s3-procpu-companion-zephtacore-def5678-update.bin")
  ];
  const uf2 = { id: "t1000_e", flashType: "uf2" };
  assert.equal(L.findCatalogFile(files, uf2, "companion").name, "t1000_e-companion-zephtacore-abc1234.uf2");
  assert.equal(L.findCatalogFile(files, uf2, "repeater").name, "t1000_e-repeater-zephtacore-abc1234.uf2");
  assert.equal(
    L.findCatalogFile(files, { id: "heltec_v4/esp32s3/procpu", flashType: "bin" }, "companion").name,
    "heltec_v4-esp32s3-procpu-companion-zephtacore-def5678-merged.bin"
  );
});

test("findCatalogFile finds nothing for a board without a zephtacore build or the wrong format", () => {
  const files = [file("t1000_e-companion-abc1234.uf2"), file("t1000_e-companion-zephtacore-abc1234.uf2")];
  assert.equal(L.findCatalogFile(files, { id: "rak4631", flashType: "uf2" }, "companion"), null);
  assert.equal(L.findCatalogFile(files, { id: "t1000_e", flashType: "hex" }, "companion"), null);
  assert.equal(L.findCatalogFile(files, { id: "t1000_e", flashType: "uf2" }, "repeater"), null);
});

test("findCatalogFile does not treat a board id as a regex or a prefix", () => {
  const files = [file("t1000_e-companion-zephtacore-abc1234.uf2")];
  assert.equal(L.findCatalogFile(files, { id: "t1000.e", flashType: "uf2" }, "companion"), null);
  assert.equal(L.findCatalogFile(files, { id: "t1000", flashType: "uf2" }, "companion"), null);
});

test("findCatalogFile refuses a URL outside the ZephCore firmware-dist CDN", () => {
  const name = "t1000_e-companion-zephtacore-abc1234.uf2";
  const board = { id: "t1000_e", flashType: "uf2" };
  assert.equal(L.findCatalogFile([file(name, "https://evil.example/" + name)], board, "companion"), null);
  assert.equal(L.findCatalogFile([file(name, "http://cdn.jsdelivr.net/gh/xejarque/ZephCore@firmware-dist/" + name)], board, "companion"), null);
  assert.equal(L.findCatalogFile([file(name, "https://cdn.jsdelivr.net/gh/other/ZephCore@firmware-dist/" + name)], board, "companion"), null);
});

test("kconfigString escapes quotes and backslashes; shellQuote survives a single quote", () => {
  assert.equal(L.kconfigString('a"b\\c'), '"a\\"b\\\\c"');
  assert.equal(L.shellQuote("it's"), "'it'\\''s'");
});
