# Vendored libraries

Third-party code served as is, with its license next to it. Nothing here is edited.

| File | What | Version | Source | License |
|------|------|---------|--------|---------|
| `leaflet.js`, `leaflet.css` | Leaflet, the map of the builder | 1.9.4 (from the file header) | https://leafletjs.com | BSD-2-Clause, `leaflet.LICENSE` |
| `esptool-js.bundle.js` | ESP32 flashing from the browser (`ESPLoader`, `Transport`), loaded by `js/flasher-core.js` | not stated in the file | https://github.com/espressif/esptool-js | Apache-2.0, `esptool-js.LICENSE` |
| `fflate.js` | Unzips the DFU packages of nRF52 builds, loaded by `js/flasher-nrf.mjs` | not stated in the file | https://github.com/101arrowz/fflate | MIT, `fflate.LICENSE` |

The `esptool-js` and `fflate` bundles do not record their version. To pin one, replace the file with the build of a named release
of its npm package and write that version in the table.
