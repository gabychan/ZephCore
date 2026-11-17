/**
 * Zephtacore Universal Navigation Drawer & Quick Menu
 */
(function () {
  const MENU_HTML = `
  <div id="zephtacoreNavDrawer" class="nav-drawer-backdrop" style="display: none;">
    <div class="nav-drawer-panel">
      <div class="nav-drawer-header">
        <div style="display: flex; align-items: center; gap: 0.65rem;">
          <img src="{{ROOT}}logo.svg" alt="Zephtacore" width="26" height="26" />
          <span style="font-weight: 700; font-size: 1.1rem; color: var(--text-primary);">Explorar Zephtacore</span>
        </div>
        <button id="btnCloseNavDrawer" class="btn-drawer-close" aria-label="Cerrar menú">✕</button>
      </div>

      <div class="nav-drawer-body">
        <!-- Section 1 -->
        <div class="nav-drawer-group">
          <div class="nav-drawer-group-title">Herramientas Principales</div>
          <a href="{{ROOT}}index.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">🏠</span>
            <div>
              <div class="nav-drawer-link-title">Inicio / Landing</div>
              <div class="nav-drawer-link-desc">Visión general, arquitectura híbrida y comparativa.</div>
            </div>
          </a>
          <a href="{{ROOT}}builder.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">🚀</span>
            <div>
              <div class="nav-drawer-link-title">Firmware Builder & Calculador</div>
              <div class="nav-drawer-link-desc">Configura hardware, roles y parámetros de radio.</div>
            </div>
          </a>
          <a href="{{ROOT}}flasher.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">⚡</span>
            <div>
              <div class="nav-drawer-link-title">Web Flasher & Consola Serial</div>
              <div class="nav-drawer-link-desc">Grabación USB directa y terminal interactivo a 115200.</div>
            </div>
          </a>
          <a href="{{ROOT}}docs/index.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">📖</span>
            <div>
              <div class="nav-drawer-link-title">Centro de Documentación & CLI</div>
              <div class="nav-drawer-link-desc">Referencia de más de 20 comandos y generador visual.</div>
            </div>
          </a>
        </div>

        <!-- Section 2 -->
        <div class="nav-drawer-group">
          <div class="nav-drawer-group-title">Guías y Despliegues</div>
          <a href="{{ROOT}}docs/guia-inicio.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">🎯</span>
            <div>
              <div class="nav-drawer-link-title">Guía de Primeros Pasos</div>
              <div class="nav-drawer-link-desc">Flasheo inicial y emparejamiento con la app móvil MeshCore.</div>
            </div>
          </a>
          <a href="{{ROOT}}docs/repetidor-solar.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">☀️</span>
            <div>
              <div class="nav-drawer-link-title">Repetidor Solar de Montaña</div>
              <div class="nav-drawer-link-desc">Parámetros Zero-Hop, políticas de relay y bajo consumo.</div>
            </div>
          </a>
          <a href="{{ROOT}}docs/arquitectura-dual.html" class="nav-drawer-link">
            <span class="nav-drawer-icon">📡</span>
            <div>
              <div class="nav-drawer-link-title">Arquitectura Dual-Mesh (0x14B4)</div>
              <div class="nav-drawer-link-desc">Demodulación en paralelo sobre transceptores Semtech.</div>
            </div>
          </a>
        </div>

        <!-- Section 3 -->
        <div class="nav-drawer-group">
          <div class="nav-drawer-group-title">Herramientas Interactivas</div>
          <a href="{{ROOT}}docs/index.html#constructor" class="nav-drawer-link">
            <span class="nav-drawer-icon">🛠️</span>
            <div>
              <div class="nav-drawer-link-title">Constructor Visual de Comandos</div>
              <div class="nav-drawer-link-desc">Genera comandos z.mt.* y z.mc.* en 1 clic.</div>
            </div>
          </a>
          <a href="{{ROOT}}flasher.html#terminal" class="nav-drawer-link">
            <span class="nav-drawer-icon">💻</span>
            <div>
              <div class="nav-drawer-link-title">Terminal Serie Web USB</div>
              <div class="nav-drawer-link-desc">Consola interactiva con comandos de telemetría.</div>
            </div>
          </a>
        </div>

        <!-- Section 4 -->
        <div class="nav-drawer-group">
          <div class="nav-drawer-group-title">Comunidad & Código</div>
          <a href="https://github.com/xejarque/ZephCore" target="_blank" rel="noopener" class="nav-drawer-link">
            <span class="nav-drawer-icon">🐙</span>
            <div>
              <div class="nav-drawer-link-title">Repositorio GitHub Zephtacore</div>
              <div class="nav-drawer-link-desc">Código fuente C++17, pull requests e incidencias.</div>
            </div>
          </a>
        </div>
      </div>
    </div>
  </div>`;

  function initNavDrawer() {
    // Detect if we are in /docs/ or root
    const isDocs = window.location.pathname.includes("/docs/");
    const rootPath = isDocs ? "../" : "./";
    const renderedHtml = MENU_HTML.replace(/\{\{ROOT\}\}/g, rootPath);

    document.body.insertAdjacentHTML("beforeend", renderedHtml);

    const drawer = document.getElementById("zephtacoreNavDrawer");
    const btnClose = document.getElementById("btnCloseNavDrawer");

    function openDrawer() {
      if (drawer) {
        drawer.style.display = "flex";
        setTimeout(() => drawer.classList.add("open"), 10);
      }
    }

    function closeDrawer() {
      if (drawer) {
        drawer.classList.remove("open");
        setTimeout(() => (drawer.style.display = "none"), 200);
      }
    }

    // Attach to menu buttons
    document.querySelectorAll(".btn-open-menu, #btnOpenNavDrawer").forEach((b) => {
      b.addEventListener("click", openDrawer);
    });

    if (btnClose) btnClose.addEventListener("click", closeDrawer);

    if (drawer) {
      drawer.addEventListener("click", (e) => {
        if (e.target === drawer) closeDrawer();
      });
    }

    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") closeDrawer();
    });
  }

  document.addEventListener("DOMContentLoaded", initNavDrawer);
})();
