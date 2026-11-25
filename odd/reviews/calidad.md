# Revisión de calidad (QA): tests, CI, web, scripts, licencias y documentación

Estado: terminado
Commit: `2541fe5f`. Revisado: resultados de tests, `.github/workflows/*` (pins verificados contra los tags), ejecuciones reales de CI del fork, web builder/flasher (sinks HTML, catálogo, SHA-256, comandos), `tests/` (arnés, catálogo, cobertura), README/LICENSE/ADR/CLI docs frente al código, `build.sh`/`board_manifest.py`/`gen_provider_catalog.py`.

## Resultado de los tests

- `python3 tests/run.py run --profile quick` en la copia extraída (`review/work-calidad/src`, macOS): 307 PASS (297 casos C++ + 10 del arnés), 0 FAIL, 0 ERROR, 0 BLOCKED (`work-calidad/quick.log`). Sin `--sanitize`; el CI de Linux lo pasa con ASan/UBSan en `2541fe5f` (run "host regression tests", success). Nada falló por ser macOS.
- `node --test tests/web/*.test.*` (node v26.13.0): 57 pass, 0 fail (`work-calidad/web.log`). Los stack traces del log son errores esperados que imprimen los propios tests (SHA-256 que no coincide, etc.).

## Hallazgos

Resumen: 4 media (QA-2, QA-6, QA-4, QA-3), 2 baja (QA-1, QA-5), 2 mejora (QA-7, QA-8). Ninguna crítica ni alta. Los IDs se mantienen; el orden es por severidad.

### QA-2. La release del fork sale como `1.18.2-zephcore` con las notas de upstream; las notas de Zephtacore (`1.18.3`) no se publican nunca
- Severidad: media
- Certeza: confirmado (release publicada comprobada con `gh release view`)
- Origen: fork · ¿arreglado en origin/dev?: no (upstream sube a `1.18.3-zephcore` con sus propias notas del mismo nombre) · ¿ya conocido?: en parte (la memoria anota el conflicto add/add de `RELEASE_NOTES_1.18.3`, y cree que el push publica "release 1.18.3"; no lo que de verdad se publica)
- Ubicación: `zephcore/CMakeLists.txt:36`, `releasenotes/RELEASE_NOTES_1.18.3-zephcore.md`, `.github/workflows/build.yml:433-449`
- Causa: `create-release` toma la etiqueta de `ZEPHCORE_FIRMWARE_VERSION` (`1.18.2-zephcore`, sin cambiar desde la base `da3513aa`) y usa `releasenotes/RELEASE_NOTES_<etiqueta>.md` como cuerpo. El fork escribió todas sus notas en `RELEASE_NOTES_1.18.3-zephcore.md` pero nunca subió la versión.
- Escenario de fallo: `fork/master` = `2541fe5f` está publicado como release no-prerelease `Firmware 1.18.2-zephcore` (2026-11-23T22:12Z) con el texto "Firmware updates over Bluetooth for ESP32 companions…", que no menciona Meshtastic ni los cambios de comportamiento del fork (auto-apagado por defecto, relay, etc.). El nodo informa `1.18.2-zephcore` a la app y en `ver`, la misma cadena que el firmware de upstream sin capa Meshtastic, así que un informe de fallo no permite saber qué firmware corre. Además upstream ya tiene su `1.18.3-zephcore` con otro contenido: subir el fork a `1.18.3` sin más repetiría la colisión.
- Arreglo mínimo propuesto: dar al fork una versión propia que no choque con upstream (p. ej. sufijo de variante en `ZEPHCORE_FIRMWARE_VERSION`, decisión que merece ADR porque toca la cadena que ven las apps) y renombrar sus notas a esa versión; mientras tanto, no publicar desde `master` del fork con `prerelease: "false"`.
- Cómo verificarlo: tras el cambio, `gh release view <tag> -R xejarque/ZephCore` muestra las notas de Zephtacore y `ver` en el nodo da una cadena distinta de la de upstream.

### QA-6. El flasher web sirve un firmware anterior a la release: Pages despliega tras builds cancelados y el despliegue tras el build bueno falló sin que nada lo repita
- Severidad: media
- Certeza: confirmado (efecto y disparo por build cancelado); la causa del fallo de `deploy-pages` es sospecha
- Origen: fork · ¿arreglado en origin/dev?: n.a. (`pages.yml` es del fork) · ¿ya conocido?: no
- Ubicación: `.github/workflows/pages.yml:3-7` (sin condición sobre `workflow_run.conclusion`), `:66-73`
- Causa: (1) `on: workflow_run: types: [completed]` se dispara también cuando "build firmware" termina cancelado o fallido, y el job no mira `github.event.workflow_run.conclusion`. (2) En las cuatro últimas ejecuciones por `workflow_run`, dos (45271891704 y 45277566168) fallaron en `deploy-pages` con `No artifacts named "github-pages" were found`, 0,3 s después de que `upload-pages-artifact` dijera haber subido el artefacto (54,7 MB). Parece un retraso de la API de artefactos, no lo he podido confirmar. Nada repite el despliegue.
- Escenario de fallo: el 2026-11-23, el build de `5b8245d9` se canceló al llegar `2541fe5f`; su `workflow_run` desplegó Pages a las 21:54 con la release que había entonces (build `ea66ec6`). El build de `2541fe5f` sustituyó la release `1.18.2-zephcore` a las 22:12, y su despliegue de las 22:13 falló. Hoy `https://xejarque.github.io/ZephCore/firmware/catalog.json` lista 197 ficheros, todos `…-ea66ec6…`, mientras la release tiene `…-2541fe5…`: el flasher instala un firmware sin el último cambio (`help` de `z.`), con la misma versión `1.18.2-zephcore` (ver QA-2), y no hay forma de notarlo desde la página.
- Arreglo mínimo propuesto: `if: github.event_name != 'workflow_run' || github.event.workflow_run.conclusion == 'success'` en el job. Para el fallo de `deploy-pages`, buscar antes la causa (versión de `deploy-pages`/`upload-pages-artifact`, incidencias conocidas). Un reintento a ciegas sería un parche según AGENTS.md.
- Cómo verificarlo: tras un push que cancele un build, no debe aparecer ejecución de Pages por `workflow_run`; tras cada release, comparar el sufijo de hash de `catalog.json` en Pages con los assets de la release.

### QA-4. El CI del fork ya no compila ningún build sin Meshtastic ni seis placas (Linux, ESP32-C6, STM32WL)
- Severidad: media
- Certeza: confirmado (seguido en `build.yml`, `build.sh`, `board_manifest.matrix` y los 49 `zephcore.yml`); no compilé esos builds para ver si hoy rompen
- Origen: fork · ¿arreglado en origin/dev?: n.a. (upstream sigue compilando las variantes planas y Linux) · ¿ya conocido?: no (CONN-10 trata observer y uplink, que tampoco compila upstream)
- Ubicación: `zephcore/scripts/board_manifest.py:178-207` (`published_variants` dentro de `matrix`); `.github/workflows/build.yml:166-174` (compile-only), job `build-linux` borrado en `54617540`; `build.sh`
- Causa: `3c4a2ff9` hizo que `matrix()` devuelva solo la variante `zephtacore` de cada placa, y `54617540` quitó `build-linux` porque ya no producía nada. Con eso: (a) ningún paso compila companion, repetidor o room server sin `zephtacore.conf` (los únicos builds sin él son los dos `kiss_modem.conf` de compile-only), así que el lado `CONFIG_ZEPHTACORE_MESHTASTIC=n` de las 65 guardas en 19 ficheros no se compila nunca; (b) `lilygo_tlora_c6`, `xiao_esp32c6`, `lora_e5_mini` y los tres presets de `linux_native` no tienen variante `zephtacore`, `matrix()` les da cero filas y `build.sh stm32wl` termina sin compilar nada: RISC-V C6, STM32WL y native Linux quedan sin compilar en CI; (c) room server no lo compila nadie (esto ya pasaba en upstream).
- Escenario de fallo: un cambio del fork en código común, por ejemplo un símbolo usado fuera de un `#ifdef CONFIG_ZEPHTACORE_MESHTASTIC` pero definido solo dentro, o un aumento de RAM que no cabe en el STM32WL de 64 KB, deja roto el build plano o esas placas; el CI sigue verde y el fallo aparece al traer el fork a upstream o cuando alguien compila ZephCore sin Meshtastic desde el fork. `AGENTS.md` pide "fix it for every board" y que un rol o fragmento que ninguna fila construye esté en compile-only.
- Arreglo mínimo propuesto: añadir a compile-only, sin publicar nada: `rak4631` companion y repetidor sin `zephtacore.conf`, un repetidor `lora_e5_mini` (cabe aunque el companion no), un `xiao_esp32c6` y un preset `linux_native` (`build.sh linux` sin publicar, o un `west build -b native_sim` del preset); y `room_server.conf` en una placa.
- Cómo verificarlo: los pasos nuevos pasan en CI; un `#error` temporal en una rama `#ifndef CONFIG_ZEPHTACORE_MESHTASTIC` hace fallar el CI, que hoy sigue verde.

### QA-3. La batería de host no llega a los manejadores de comandos ni a la capa de aplicación, que es donde están casi todos los hallazgos altos; el código nuevo del fork en `app/` no tiene tests
- Severidad: media
- Certeza: confirmado (lista de fuentes de `tests/CMakeLists.txt` frente a los ficheros de producción)
- Origen: ambos (el arnés es de upstream; el código Meshtastic de `app/` es del fork) · ¿arreglado en origin/dev?: no (el `tests/CMakeLists.txt` de `origin/dev` tampoco compila `CommonCLI`, `*Mesh.cpp` de `app/`, `Utils.cpp`, `ClientACL` ni `RegionMap`) · ¿ya conocido?: no
- Ubicación: `tests/CMakeLists.txt:6-59`; `tests/support/identity_utils_boundary.cpp:36-52`
- Causa: el ejecutable `zephcore_unit` compila Dispatcher, Mesh, Packet, BaseChatMesh, PrefsJson/Codec, KissModem y `helpers/meshtastic/Mt*.cpp`, más cabeceras de lógica pura. No compila (líneas): `app/CompanionMesh.cpp` (3630), `helpers/CommonCLI.cpp` (2238), `app/RepeaterMesh.cpp` (1770), `app/RoomServerMesh.cpp` (1282), `app/CompanionMeshtastic.cpp` (1065, fork), `app/ObserverMesh.cpp` (696), `helpers/RegionMap.cpp` (382), `src/Utils.cpp` (316), `app/MtRelayPort.cpp` (296, fork), `app/RepeaterUplink.cpp` (209), `app/RepeaterMeshtastic.cpp` (192, fork), `helpers/ClientACL.cpp` (153). Además `Utils::encryptThenMAC`, `MACThenDecrypt`, `sha256`, `constantTimeEqual` están sustituidos por funciones que lanzan excepción, así que ningún test ejercita el cifrado de MeshCore de verdad. AGENTS.md pide tests de host para "logic that runs without hardware"; las tres unidades del fork en `app/` deciden contactos, saltos y respuestas sin ninguno.
- Escenario de fallo: no es hipotético: de los hallazgos altos de las otras áreas, CONN-2 (`RepeaterUplink`/`RepeaterMesh`), PERS-1 y ROLE-2 (`CompanionMesh`), PERS-2 (`CommonCLI`), PERS-3 (`ClientACL`) y MT-1 (`CompanionMeshtastic`/`RepeaterMeshtastic`) están en ficheros que ningún test compila; con ASan en CI, un test que mandara un comando de 170 caracteres a un `reply` de 161 bytes los habría parado. El contrato "una respuesta cabe en 161 bytes" solo se comprueba para la ayuda de `z.` (`UNIT-ZCMD-010`).
- Tests que faltan (propuestos por otras áreas, sin test hoy), según el arnés que necesitan:
  - Se pueden añadir ya con el fixture actual de Dispatcher/MtBridge/Contention/MeshTimeSync/PrefsJson/fw: MESH-2, MESH-7, MESH-10, MESH-12, ROLE-1, RADIO-4, RADIO-9 (`dispatcher_tests`); MESH-11, MT-10 (`contention_tests`); MT-2, MT-4, MT-6, MT-8, MT-9, MT-11 (`meshtastic_tests`); MESH-5 (`fw_tests`); PERS-7 (`UNIT-SYNC-006`); PERS-14, PERS-15 (`prefs_json_tests`); MESH-13, MESH-3 (Mesh ya compilado).
  - Necesitan un arnés nuevo para `CommonCLI` + `RepeaterMesh`/`RoomServerMesh`/`CompanionMesh` con un `reply` de 161 bytes bajo ASan: CONN-2, PERS-2, PERS-9..13, PERS-17, ROLE-2, ROLE-4, ROLE-5, ROLE-9, ROLE-10, ROLE-11, PERS-1, ROLE-3.
  - Necesitan compilar `Utils.cpp`, `ClientACL.cpp`, `RegionMap.cpp` o un FS simulado: MESH-1, MESH-4, PERS-3, PERS-4, PERS-8, PERS-16, PERS-18, PERS-19, PERS-20.
  - Necesitan sacar la regla de `app/CompanionMeshtastic.cpp` a una cabecera pura (como ya se hizo con `mt::createsContact`): MT-1, MT-3, MT-7.
- Arreglo mínimo propuesto: no un refactor general. Primero un único test de `CommonCLI::handleCommand` con `reply` de 161 bytes y entradas al límite, compilando `CommonCLI.cpp` con stubs de `DataStore`/`Board` (cubre la clase de error de CONN-2, PERS-2, ROLE-2); después cada arreglo de las otras áreas llega con su test, como pide AGENTS.md.
- Cómo verificarlo: el test nuevo falla con ASan en `2541fe5f` y pasa con los arreglos de PERS-2/CONN-2.

### QA-1. `flasher-logic.js` y `flasher-core.js` quedan fuera del sellado de caché de Pages por un `?v=9` escrito a mano
- Severidad: baja
- Certeza: confirmado
- Origen: fork · ¿arreglado en origin/dev?: n.a. · ¿ya conocido?: no
- Ubicación: `custom_builder_web/flasher.html:18`, `custom_builder_web/flasher.html:20`, `custom_builder_web/builder.html:19`; `.github/workflows/pages.yml:57-60`
- Causa: `efd61e80` quitó los `?v=N` manuales y añadió el sed que sella cada `src`/`href` local con el commit. Su regex exige `\.(js|css)"` justo tras el nombre (`[^"?]+`), así que una referencia que ya lleva `?` no se toca. `98a59551` (posterior, mismo día) volvió a escribir `?v=9` en `flasher-logic.js` y `flasher-core.js`. Esas dos son las únicas que no se sellan.
- Escenario de fallo: un despliegue cambia `flasher-core.js` y `flasher-serial.js` a la vez. El navegador (o la caché de GitHub Pages, `max-age=600`) sirve `flasher-core.js?v=9` viejo con `flasher-serial.js?v=<sha nuevo>`; durante la vida de la caché el flasher corre con dos versiones mezcladas. Nadie sube el `9` porque el commit del sellado dice que ya no hace falta.
- Arreglo mínimo propuesto: quitar los tres `?v=9`; el sed ya los sella.
- Cómo verificarlo: tras el cambio, `grep -rn '?v=' custom_builder_web --include=*.html` vacío; en el artefacto de Pages los cinco scripts llevan `?v=<sha>`.

### QA-5. El README del fork no menciona Zephtacore y su tabla de placas publicadas no es la del fork
- Severidad: baja
- Certeza: confirmado
- Origen: fork · ¿arreglado en origin/dev?: n.a. · ¿ya conocido?: no
- Ubicación: `README.md:100-110`; generador `zephcore/scripts/board_manifest.py:219-226`
- Causa: `summary()` cuenta como publicada toda placa con `release`, pero desde `3c4a2ff9` el fork solo publica las que tienen variante `zephtacore` (`published_variants`). El README no tiene ninguna mención de Zephtacore ni de Meshtastic (`grep -ci` da 0), aunque es la portada del repo del fork y de sus releases.
- Escenario de fallo: el README dice 43 placas con firmware publicado (STM32WL 1, Linux 3); la release del fork lleva 37 y ninguna de STM32WL, ESP32-C6 ni Linux. Quien llega al repo no sabe que el firmware habla Meshtastic ni que está en `docs/zephtacore/README.md`.
- Arreglo mínimo propuesto: que `summary()` cuente `bool(published_variants(b))` y regenerar con `board_manifest.py docs`; un párrafo al principio del README que diga qué es Zephtacore y enlace `docs/zephtacore/README.md`.
- Cómo verificarlo: `python zephcore/scripts/board_manifest.py docs` y comparar la tabla con `ls` de los assets de la release.

### QA-7. El flasher comprueba el SHA-256 del firmware pero no el del formateador, que también graba
- Severidad: mejora
- Certeza: confirmado
- Origen: fork · ¿arreglado en origin/dev?: n.a. · ¿ya conocido?: no
- Ubicación: `custom_builder_web/js/flasher-core.js:498` y `:543` (`fetchBytes(url)` sin hash); `gen_provider_catalog.py:299-302` (`erase` es una URL suelta, sin `sha256`)
- Causa: `98a59551` añadió `sha256` a cada fichero del catálogo, pero el formateador llega como la cadena `erase` de la opción y `fetchBytes` se llama sin hash; `sha256Matches` da por buena una descarga sin hash esperado.
- Escenario de fallo: en la instalación limpia nRF52 el formateador se graba por DFU (`flash(formatter, …)`) o se descarga como UF2 para copiarlo a mano sin la comprobación que la página promete para el firmware ("descarga dañada o alterada. No se ha escrito nada"). Por DFU el CRC del paquete en el bootloader limita el daño; por UF2 no hay más control que el de `fetch`. Riesgo bajo, pero la garantía documentada en `docs/PROVIDER_CATALOG.md` ("each file … carries sha256") no cubre este fichero.
- Arreglo mínimo propuesto: que `gen_provider_catalog.py` publique también el SHA-256 de los cuatro ficheros del formateador (por ejemplo un objeto `erase_sha256` junto a `erase`, fuera de la especificación como el resto) y que `flasher-core.js` lo pase a `fetchBytes`.
- Cómo verificarlo: caso en `tests/web/flasher-page.test.js` como el de ESP32: un formateador con hash distinto no se graba.

### QA-8. `LICENSE` dice listar el código de Zephyr con Apache-2.0 y omite `seeed_lr2021_evk/support/openocd.cfg`
- Severidad: mejora
- Certeza: confirmado
- Origen: upstream (fichero de `26751c76`), la lista es del fork (`5b8245d9`) · ¿arreglado en origin/dev?: no · ¿ya conocido?: no
- Ubicación: `LICENSE:33-38`; `zephcore/boards/nrf54l/seeed_lr2021_evk/support/openocd.cfg:1` (`SPDX-License-Identifier: Apache-2.0`, traído de Zephyr main)
- Causa: la entrada de Zephyr en `LICENSE` enumera los ficheros Apache-2.0 del árbol (los `.patch` y `fake_entropy_native_sim.c`); `git grep 'SPDX-License-Identifier: Apache-2.0'` da además este `openocd.cfg`. Es el único fichero de terceros con SPDX que falta; el resto (Monocypher, Semtech LR20xx y LR1110, vendor web) sí está.
- Escenario de fallo: el aviso que el commit presenta como completo no lo es; quien redistribuya el árbol bajo "MIT salvo lo listado" incluye un fichero Apache-2.0 sin su aviso.
- Arreglo mínimo propuesto: añadir esa ruta a la entrada de Zephyr en `LICENSE`.
- Cómo verificarlo: `git grep -l 'SPDX-License-Identifier: Apache-2.0' -- zephcore` y comparar con `LICENSE`.

## Revisado sin hallazgo (o ya cubierto por otra área)

- CI: todas las acciones están fijadas por SHA y cada SHA coincide con el tag del comentario (comprobado con `git ls-remote`). Permisos mínimos por job (`contents: read` por defecto; `contents: write` solo en `create-release`/`publish-catalog`; `pages`/`id-token` solo en Pages). Sin `pull_request_target` ni interpolación de datos de terceros en `run:` (solo `github.repository`, la versión de `CMakeLists.txt` y salidas propias). `build.yml` no corre en PR, así que el `workflow_run` de Pages no lo puede disparar un fork. Observer, uplink: CONN-10.
- Web: los `innerHTML` pintan datos estáticos del repo (`boards-manifest.js`, `commands-db.js`) o números (`toFixed`); lo que viene de fuera (ipwho.is, mensajes de error) pasa por `escapeHtml`/`textContent`. Contraseñas con `crypto.getRandomValues` sin sesgo; `kconfigString`/`shellQuote` correctos y probados. URLs de firmware y formateador limitadas a los dos CDN del fork y de upstream; ESP32 comprueba el chip antes de escribir. Sin CSP ni SRI, pero todo es local (vendor) salvo el catálogo de upstream.
- Tests: 297 casos C++ + 10 del arnés pasan; `run.py` exige que el catálogo y el ejecutable coincidan, y cada `source` del catálogo contiene su ID (comprobado). Revisé los tests con una sola aserción y el fuzz `UNIT-MT-066` (usa bloques de heap del tamaño exacto, así ASan ve lecturas fuera): no encontré tests que no prueben lo que dicen. `UNIT-ZCMD-010` comprueba de verdad que la ayuda cabe en 161 bytes.
- Docs: las 44 claves `z.` de `ZCommand.h` están en `docs/CLI_commands.md` y en `commands-db.js` con los mismos valores por defecto (salvo `z.autoshutdown`, que falta en `commands-db.js`). ADR: falta 0018 (RADIO-13); 0010 falta también en upstream. `z.mt.hops` 7 frente a Kconfig 3: PERS-21 (`zephtacore.conf` pone 7, así que la ayuda acierta en los builds publicados).
- Licencias: el código Meshtastic del fork lleva MIT y cita funciones de Meshtastic (GPL-3.0) como referencia de fórmulas (`MtBridge.h:24-43`); no comparé el texto con el original, así que no puedo afirmar ni descartar que haya copia literal.

## Alcance

- A fondo: `.github/workflows/*` y sus ejecuciones recientes en `xejarque/ZephCore`, `pages.yml` con el sitio publicado, `custom_builder_web/js/{flasher-core,flasher-logic,builder,docs}.js`, `tests/CMakeLists.txt`, `tests/run.py` (validación del catálogo), `zephcore/scripts/board_manifest.py` (`matrix`, `summary`), diff del fork en `gen_provider_catalog.py`, `LICENSE`, `README.md`, tabla `z.` frente a docs.
- Por encima: `flasher-nrf.mjs`, `flasher-serial.js`, `nrf-dfu-*.mjs` (solo flujo y validación de entradas), tests C++ del fork (muestreo), `docs/zephtacore/README.md`, `docs/PROVIDER_CATALOG.md`, release notes 1.18.3, `.devcontainer/`, `.claude/agents/` (sin riesgo; llevan rutas de la máquina del autor), `zephcore/tools/lr1110_updater` (sin cambios del fork salvo un comentario).
- No revisado: `formatter/` y `LR1110_updater/` binarios (upstream), `vendor/` más allá de versiones (esptool-js y fflate no declaran versión, ya dicho en `vendor/README.md`), `docs/ARCHITECTURE.md` y `docs/DESIGN.md` párrafo a párrafo, `docs/REGRESSION_TEST_PLAN.md` (su cifra de 113 casos de `tests/README.md` está desfasada, 297 hoy). No ejecuté los tests con `--sanitize` en macOS (el CI de Linux los pasa en `2541fe5f`) ni compilé los builds de QA-4.
