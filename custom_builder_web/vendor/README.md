# Vendored libraries

Third-party code served as is, with its license next to it. Nothing here is edited.

| File | What | Version | Source | License |
|------|------|---------|--------|---------|
| `leaflet.js`, `leaflet.css` | Leaflet, the map of the builder | 1.9.4 (from the file header) | https://leafletjs.com | BSD-2-Clause, `leaflet.LICENSE` |
| `esptool-js.bundle.js` | ESP32 flashing from the browser (`ESPLoader`, `Transport`), loaded by `js/flasher-core.js` | not stated in the file | https://github.com/espressif/esptool-js | Apache-2.0, `esptool-js.LICENSE` |

The `esptool-js` bundle does not record its version. To pin it, replace the file with the `bundle.js` of a
named release of the `esptool-js` npm package and write that version in the table.
