/**
 * Flasher and builder logic that needs no DOM: command generation, catalog file
 * matching and quoting. Loaded by the pages and by node --test.
 */
(function (root) {
  const FIRMWARE_URL_PREFIX = /^https:\/\/cdn\.jsdelivr\.net\/gh\/(xejarque|liquidraver)\/ZephCore@firmware-dist\//;
  const FILE_SUFFIX = { uf2: "\\.uf2", hex: "\\.hex", bin: "-merged\\.bin" };

  // Coding rate: the builder stores 1-4 (4/5-4/8), the firmware takes 5-8.
  function firmwareCodingRate(cr) {
    const n = parseInt(cr, 10);
    return n <= 4 ? n + 4 : n;
  }

  function buildConfigCommands(cfg) {
    const cr = firmwareCodingRate(cfg.cr);

    if (cfg.role === "observer") {
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
      cfg.adminPass ? `password ${cfg.adminPass}` : null,
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

  function newestVersion(versions) {
    return Object.keys(versions || {}).sort((x, y) => x.localeCompare(y, undefined, { numeric: true })).pop();
  }

  // Every file of the newest version of every firmware option, whatever the device tile.
  function catalogFiles(catalog) {
    const out = [];
    for (const dev of catalog.device || []) {
      for (const fw of dev.firmware || []) {
        const ver = newestVersion(fw.version);
        if (ver) out.push(...(fw.version[ver].files || []));
      }
    }
    return out;
  }

  // build.sh names files <target with / as ->>-<role>-<variant>-<git hash><suffix>.
  // The flasher injects z.* settings, so it only offers the zephtacore variant.
  function findCatalogFile(files, board, role) {
    const stem = board.id.replace(/\//g, "-").replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const pat = new RegExp(`^${stem}-${role}-zephtacore-[0-9a-f]{7,40}${FILE_SUFFIX[board.flashType] || FILE_SUFFIX.bin}$`);
    return files.find((f) => pat.test(f.name || "") && FIRMWARE_URL_PREFIX.test(f.url || "")) || null;
  }

  function kconfigString(value) {
    return `"${String(value).replace(/\\/g, "\\\\").replace(/"/g, '\\"')}"`;
  }

  function shellQuote(value) {
    return `'${String(value).replace(/'/g, "'\\''")}'`;
  }

  // The CLI answers a refused command with "Error: ..." or "ERR ..." (observer), maybe after "> ".
  function isErrorReply(text) {
    return /^[>\s]*(Error|ERR)\b/im.test(text || "");
  }

  const api = { isErrorReply, firmwareCodingRate, buildConfigCommands, catalogFiles, findCatalogFile, kconfigString, shellQuote };
  if (typeof module !== "undefined" && module.exports) module.exports = api;
  else root.ZephtacoreLogic = api;
})(typeof window !== "undefined" ? window : globalThis);
