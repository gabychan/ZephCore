/**
 * Zephtacore Interactive Docs Hub & Command Generator
 */
document.addEventListener("DOMContentLoaded", () => {
  const searchInput = document.getElementById("docsSearchInput");
  const filterPills = document.querySelectorAll(".filter-pill");
  const commandsList = document.getElementById("commandsList");
  const countBadge = document.getElementById("commandsCount");

  // Command Builder Elements
  const builderAction = document.getElementById("cbAction");
  const builderCommand = document.getElementById("cbCommand");
  const builderParams = document.getElementById("cbParams");
  const builderOutput = document.getElementById("cbOutput");
  const btnCopyBuilder = document.getElementById("btnCopyBuilder");

  // Command Builder Explainer Elements
  const cbInfoBox = document.getElementById("cbInfoBox");
  const cbInfoName = document.getElementById("cbInfoName");
  const cbInfoCatBadge = document.getElementById("cbInfoCatBadge");
  const cbInfoTypeBadge = document.getElementById("cbInfoTypeBadge");
  const cbInfoDefault = document.getElementById("cbInfoDefault");
  const cbInfoDesc = document.getElementById("cbInfoDesc");
  const cbInfoParams = document.getElementById("cbInfoParams");
  const cbInfoTags = document.getElementById("cbInfoTags");

  const categoriesMap = {
    meshtastic: "📡 Meshtastic Dual-Mesh (z.mt.*)",
    meshcore: "⚡ MeshCore Nativo (z.mc.*)",
    diagnostico: "📊 Diagnóstico & Telemetría",
    sistema: "⚙️ Sistema & Hardware"
  };

  const catBadgeLabels = {
    meshtastic: "Meshtastic (z.mt.*)",
    meshcore: "MeshCore (z.mc.*)",
    diagnostico: "Diagnóstico & Telemetría",
    sistema: "Sistema & Hardware"
  };

  let currentCategory = "all";

  function renderCommands() {
    if (!commandsList || !window.ZEPHTACORE_COMMANDS) return;

    const term = searchInput ? searchInput.value.toLowerCase().trim() : "";
    commandsList.innerHTML = "";
    let matches = 0;

    window.ZEPHTACORE_COMMANDS.forEach((cmd) => {
      // Category filter
      if (currentCategory !== "all" && cmd.category !== currentCategory) {
        return;
      }

      // Search term filter
      if (term) {
        const inName = cmd.name.toLowerCase().includes(term);
        const inDesc = cmd.desc.toLowerCase().includes(term);
        const inTags = cmd.tags.some((t) => t.toLowerCase().includes(term));
        if (!inName && !inDesc && !inTags) return;
      }

      matches++;
      const card = document.createElement("div");
      card.className = "command-card";
      card.innerHTML = `
        <div class="command-header">
          <span class="command-title">${cmd.name}</span>
          <div style="display:flex; gap:0.4rem; align-items:center;">
            <span class="command-tag">${cmd.type}</span>
            <span class="command-tag" style="color:var(--color-primary);">${cmd.category}</span>
          </div>
        </div>
        <div class="command-desc">${cmd.desc}</div>
        ${cmd.params ? `<div style="font-size:0.8rem; color:var(--text-dim); margin-bottom:0.5rem;"><strong>Parámetros:</strong> <code>${cmd.params}</code> (def: ${cmd.default})</div>` : ""}
        <div class="command-example">
          <code>${cmd.example}</code>
          <button class="btn-copy-cmd" data-code="${cmd.example}">Copiar</button>
        </div>
      `;
      commandsList.appendChild(card);
    });

    if (countBadge) countBadge.textContent = `${matches} comandos`;

    // Setup Copy Buttons
    document.querySelectorAll(".btn-copy-cmd").forEach((btn) => {
      btn.addEventListener("click", () => {
        const text = btn.getAttribute("data-code");
        if (text) {
          navigator.clipboard.writeText(text);
          const orig = btn.textContent;
          btn.textContent = "¡Copiado!";
          setTimeout(() => (btn.textContent = orig), 1500);
        }
      });
    });
  }

  // Populate Interactive Command Builder
  function setupCommandBuilder() {
    if (!builderCommand || !window.ZEPHTACORE_COMMANDS) return;

    // Group commands by category in <optgroup>
    const groups = {};
    window.ZEPHTACORE_COMMANDS.forEach((c) => {
      const cat = c.category || "sistema";
      if (!groups[cat]) groups[cat] = [];
      groups[cat].push(c);
    });

    let html = "";
    Object.keys(groups).forEach((cat) => {
      const label = categoriesMap[cat] || cat;
      html += `<optgroup label="${label}">`;
      groups[cat].forEach((c) => {
        const brief = c.desc.split(".")[0];
        html += `<option value="${c.name}">${c.name} — ${brief}</option>`;
      });
      html += `</optgroup>`;
    });

    builderCommand.innerHTML = html;

    function updateBuilder() {
      const cmdName = builderCommand.value;
      const cmd = window.ZEPHTACORE_COMMANDS.find((c) => c.name === cmdName) || window.ZEPHTACORE_COMMANDS[0];
      if (!cmd) return;

      const typeStr = cmd.type || "get";
      const paramsHint = cmd.params || "";

      // Sync action dropdown
      if (builderAction) {
        if (!typeStr.includes("set")) {
          builderAction.value = "get";
          builderAction.disabled = true;
        } else if (!typeStr.includes("get")) {
          builderAction.value = "set";
          builderAction.disabled = true;
        } else {
          builderAction.disabled = false;
        }
      }

      // Sync parameter input
      const action = builderAction ? builderAction.value : "get";
      if (builderParams) {
        if (action === "get") {
          builderParams.value = "";
          builderParams.placeholder = "(Sin parámetros para get)";
          builderParams.disabled = true;
        } else {
          builderParams.disabled = false;
          builderParams.placeholder = paramsHint ? `Ej: ${paramsHint}` : "Valor...";
        }
      }

      // Generate output line
      let line = `${action} ${cmd.name}`;
      if (action === "set" && builderParams && builderParams.value.trim()) {
        line += ` ${builderParams.value.trim()}`;
      }

      if (builderOutput) {
        builderOutput.textContent = line;
      }

      // Update Contextual Command Info Box
      if (cbInfoName) cbInfoName.textContent = cmd.name;
      if (cbInfoDesc) cbInfoDesc.textContent = cmd.desc;
      if (cbInfoCatBadge) {
        cbInfoCatBadge.textContent = catBadgeLabels[cmd.category] || cmd.category;
      }
      if (cbInfoTypeBadge) {
        cbInfoTypeBadge.textContent = cmd.type.toUpperCase();
      }
      if (cbInfoDefault) {
        cbInfoDefault.textContent = cmd.default && cmd.default !== "-" ? `Fábrica: ${cmd.default}` : "";
      }
      if (cbInfoParams) {
        if (cmd.params) {
          cbInfoParams.innerHTML = `<strong>Valores aceptados:</strong> <code>${cmd.params}</code>`;
          cbInfoParams.style.display = "inline-block";
        } else {
          cbInfoParams.innerHTML = "";
          cbInfoParams.style.display = "none";
        }
      }
      if (cbInfoTags) {
        if (cmd.tags && cmd.tags.length > 0) {
          cbInfoTags.innerHTML = cmd.tags.map((t) => `<span class="cb-info-tag-item">#${t}</span>`).join("");
        } else {
          cbInfoTags.innerHTML = "";
        }
      }
    }

    if (builderAction) builderAction.addEventListener("change", updateBuilder);
    if (builderCommand) builderCommand.addEventListener("change", updateBuilder);
    if (builderParams) builderParams.addEventListener("input", updateBuilder);

    if (btnCopyBuilder && builderOutput) {
      btnCopyBuilder.addEventListener("click", () => {
        navigator.clipboard.writeText(builderOutput.textContent);
        const orig = btnCopyBuilder.textContent;
        btnCopyBuilder.textContent = "¡Copiado!";
        setTimeout(() => (btnCopyBuilder.textContent = orig), 1500);
      });
    }

    updateBuilder();
  }

  // Event Listeners
  if (searchInput) {
    searchInput.addEventListener("input", renderCommands);
  }

  filterPills.forEach((pill) => {
    pill.addEventListener("click", () => {
      filterPills.forEach((p) => p.classList.remove("active"));
      pill.classList.add("active");
      currentCategory = pill.getAttribute("data-category") || "all";
      renderCommands();
    });
  });

  setupCommandBuilder();
  renderCommands();
});
