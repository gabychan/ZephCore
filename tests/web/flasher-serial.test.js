const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const L = require("../../custom_builder_web/js/flasher-logic.js");

// Loads flasher-serial.js against a fake port whose node answers via reply(cmd).
async function connectedSerial(reply) {
  const sent = [];
  let rx;
  const readable = new ReadableStream({ start: (c) => (rx = c) });
  const writable = new WritableStream({
    write(chunk) {
      const line = new TextDecoder().decode(chunk);
      sent.push(line);
      const answer = reply(line.trim());
      if (answer !== null) setTimeout(() => rx.enqueue(new TextEncoder().encode(answer)), 5);
    }
  });
  const port = { open: async () => {}, readable, writable, close: async () => {} };
  const doc = { getElementById: () => null, querySelectorAll: () => [], addEventListener: () => {} };
  const nav = { serial: { requestPort: async () => port, addEventListener: () => {} } };
  const src = fs.readFileSync(path.join(__dirname, "../../custom_builder_web/js/flasher-serial.js"), "utf8");
  const win = {};
  new Function("window", "document", "navigator", src)(win, doc, nav);
  assert.equal(await win.ZephtacoreSerial.connect(), true);
  return { serial: win.ZephtacoreSerial, sent };
}

test("an error reply is recognised", () => {
  assert.equal(L.isErrorReply("Error: unsupported"), true);
  assert.equal(L.isErrorReply("> Error: bad key"), true);
  assert.equal(L.isErrorReply("ERR - repeat now deprecated"), true);
  assert.equal(L.isErrorReply("OK"), false);
  assert.equal(L.isErrorReply("> z.mt.relay: essential"), false);
  assert.equal(L.isErrorReply(""), false);
});

test("sendCommandAndWait returns what the node answers", async () => {
  const { serial, sent } = await connectedSerial(() => "OK\n");
  const r = await serial.sendCommandAndWait("set tx 22", { timeoutMs: 1000, quietMs: 30 });
  assert.deepEqual(sent, ["set tx 22\r\n"]);
  assert.equal(r.sent, true);
  assert.equal(r.timedOut, false);
  assert.equal(r.text, "OK");
});

test("sendCommandAndWait reports an error reply and a silent node", async () => {
  const { serial } = await connectedSerial((cmd) => (cmd === "set tx 99" ? "Error: out of range\n" : null));
  const bad = await serial.sendCommandAndWait("set tx 99", { timeoutMs: 1000, quietMs: 30 });
  assert.equal(L.isErrorReply(bad.text), true);
  const silent = await serial.sendCommandAndWait("set name X", { timeoutMs: 80, quietMs: 30 });
  assert.equal(silent.timedOut, true);
});

test("sendCommandAndWait without a connection sends nothing", async () => {
  const doc = { getElementById: () => null, querySelectorAll: () => [], addEventListener: () => {} };
  const src = fs.readFileSync(path.join(__dirname, "../../custom_builder_web/js/flasher-serial.js"), "utf8");
  const win = {};
  new Function("window", "document", "navigator", src)(win, doc, {});
  const r = await win.ZephtacoreSerial.sendCommandAndWait("reboot", { timeoutMs: 50 });
  assert.equal(r.sent, false);
});
