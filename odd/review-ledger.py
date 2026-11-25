#!/usr/bin/env python3
"""Build odd/review-ledger.md from the frozen review reports.

Reads odd/reviews/<area>.md (the nine reports plus BRIEF) and the task tables of
odd/tasks/review-fixes.md, then writes one ledger row per finding: what each
finding is, where it was filed, which task carries it, its state and why.
Fails loudly if a finding is not carried by a task, so nothing can be dropped.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVIEWS = ROOT / "odd" / "reviews"
DOC = ROOT / "odd" / "tasks" / "review-fixes.md"
OUT = ROOT / "odd" / "review-ledger.md"

AREAS = ["radio", "conectividad", "roles", "persistencia", "mesh",
         "meshtastic", "placas", "calidad", "perifericos"]

ID_RE = re.compile(r"\b(MESH|ROLE|PERS|RADIO|CONN|MT|BOARD|QA|HW)-(\d+)\b")

# task -> (state, reason, commit)
TASKS = {
    "T1": ("hecho", "portado de upstream `dcafa339`; verificado leyendo el camino (Utils no está en el arnés) y suite verde", "9fa79f33"),
    "T2": ("hecho", "portado de upstream `569215be`", "c1e6c021"),
    "T3": ("hecho", "portado de upstream `51d2e454` y `24762a20`", "d5e029a2, b45bbf0b"),
    "T4": ("hecho", "portado de upstream `353a91fd` y `b409998e`", "c83f9c40, a265edfe"),
    "T5": ("hecho", "portado de upstream `eb84ba25`, `9b820396`, `4dbd6fca`; no compilado aquí (ver nota de compilación)", "0eb8d6ae, e9c4eed2, 3ea78b95"),
    "T6": ("hecho", "portado de upstream `6e458e24`/`5e73610f` solo en el veredicto del CAD; verificado por lectura del driver", "a34a9d8c"),
    "T7": ("hecho", "portado de upstream `74a7e87e` con 5 casos de host nuevos (RED observado); MESH-2 diverge a propósito", "a972bf41"),
    "T8": ("hecho", "verificado leyendo el camino remoto; el arnés no compila `app/` (QA-3)", "9abebd88"),
    "T9": ("hecho", "acotado a la capacidad real del búfer remoto; compilado en companion, repeater con uplink y room server", "3bf17801"),
    "T10": ("hecho", "cotas y estado de la malla; 4 casos de host nuevos, compilado en companion y room server", "429894aa"),
    "T11": ("hecho", "durabilidad y confianza en lo leído; la mitad del almacén de PERS-20 incluida", "97276ae1"),
    "T12": ("hecho", "validación de ajustes, respuesta honesta al fallo de escritura y ayuda corregida; 3 casos de host nuevos", "a40d9d61"),
    "T13": ("hecho", "veredicto del driver, TCXO, recalibración diferida y atribución de RSSI/SNR; compilado en LR2021, LR1110 y SX126x", "37c81a2f"),
    "T14": ("hecho", "despertar del TX pendiente, sorteo solo con canal ocupado y paridad de `is_receiving`; compilado en LR1110, LR2021 y room server", "3e66ddb1"),
    "T15": ("hecho", "política de descarte de upstream, mantenimiento en el observer, orden de eventos y telemetría entera; compilado en companion y observer ESP32", "aa726644"),
    "T16": ("hecho", "seis bugs mecánicos de la capa Meshtastic, con 4 casos de host; compilado en companion y repeater Zephtacore", "0d9d9d8b"),
    "T17": ("hecho", "cercas de fork, ADR 0018, sellado de Pages, README, digest del formateador, LICENSE y SECURITY", "882cac64"),
    "T18": ("hecho", "empaquetado que falla en voz alta, fichero de Zephyr que pasa a parche, joystick gateado por devicetree", "98c72a43"),
    "T19": ("hecho", "panel y sensores alcanzados por su nodo del devicetree, sin escribir a un chip sin identificar", "9d433583"),
    "T30": ("hecho", "solo el `HAPTICS_DRV2605` redundante: la retirada del tamaño de flash se revirtió al comprobar que el MMU del C6 depende de él", "ce1d9275"),
    "T29": ("hecho", "abort del TX y redondeo del SNR en el parche `0003`, regenerado y verificado sin el fallback", "3ad30ab4"),
    "T20": ("bloqueado", "ola C: decide política de seguridad y compatibilidad de BLE, USB, TCP y MQTT; hay que elegir entre seguir a MeshCore y cerrar la puerta", ""),
    "T21": ("bloqueado", "ola C: decide política de malla (fail-closed de la semilla de identidad, exención admin del duty cycle) y el futuro de `z.mc.fw.gap`", ""),
    "T22": ("bloqueado", "ola C: decide cuánto se acepta de la capa Meshtastic; cada arreglo cambia compatibilidad con nodos Meshtastic", ""),
    "T23": ("bloqueado", "ola C: decide qué significa «el reloj solo avanza» cuando la fuente es SNTP o el consenso de malla", ""),
    "T24": ("bloqueado", "ola C: decide qué hacer ante un fallo de montaje (PERS-8) y si se paga el coste de `adv_blobs` (PERS-23)", ""),
    "T25": ("bloqueado", "ola C: hace falta datasheet o hardware para el CAD_LBT, y rediseñar el driver para identificar el chip (RADIO-11)", ""),
    "T26": ("bloqueado", "ola C: decide el alcance de CI, release y arnés de tests, que es lo que hoy limita la verificación de toda la ola B", ""),
}


def parse_findings():
    findings = []
    for area in AREAS:
        text = (REVIEWS / f"{area}.md").read_text(encoding="utf-8")
        cur = None
        for line in text.splitlines():
            m = re.match(r"^### ([A-Z]+-\d+)\.\s*(.+)$", line)
            if m:
                cur = {"id": m.group(1), "title": m.group(2).strip(), "area": area,
                       "severity": "", "certainty": "", "origin": "", "dev_fix": ""}
                findings.append(cur)
                continue
            if cur is None:
                continue
            if line.startswith("- Severidad:"):
                cur["severity"] = line.split(":", 1)[1].strip().split(" ")[0].split("(")[0].strip()
            elif line.startswith("- Certeza:"):
                cur["certainty"] = line.split(":", 1)[1].strip().split("(")[0].split(",")[0].strip()
            elif line.startswith("- Origen:"):
                cur["origin"] = line.split(":", 1)[1].strip()
                m2 = re.search(r"¿arreglado en origin/dev\?:\s*([^\s·]+)", cur["origin"])
                cur["dev_fix"] = m2.group(1).rstrip(",.") if m2 else ""
    return findings


def parse_tasks():
    """finding id -> task id, from the two wave tables of the feature document."""
    text = DOC.read_text(encoding="utf-8")
    tasks_section = text.split("## Tasks", 1)[1].split("## Log", 1)[0]
    mapping = {}
    for line in tasks_section.splitlines():
        if not line.startswith("| T"):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        task = cells[0]
        if task not in TASKS:
            continue
        for fid in ID_RE.findall(cells[1] + " " + cells[1].replace(" ", "")):
            pass
        for m in re.finditer(r"\b((?:MESH|ROLE|PERS|RADIO|CONN|MT|BOARD|QA|HW)-\d+)\b", line):
            fid = m.group(1)
            if fid in mapping:
                print(f"ERROR: {fid} appears in {mapping[fid]} and {task}", file=sys.stderr)
                sys.exit(1)
            mapping[fid] = task
    return mapping


def main():
    findings = parse_findings()
    mapping = parse_tasks()
    unmapped = [f["id"] for f in findings if f["id"] not in mapping]
    if unmapped:
        print(f"ERROR: findings with no task: {', '.join(unmapped)}", file=sys.stderr)
        sys.exit(1)

    order = {a: i for i, a in enumerate(AREAS)}
    findings.sort(key=lambda f: (order[f["area"]], int(f["id"].split("-")[1])))

    rows = []
    by_state = {}
    for f in findings:
        task = mapping[f["id"]]
        state, reason, commit = TASKS[task]
        by_state[state] = by_state.get(state, 0) + 1
        extra = f" · `{commit}`" if commit else ""
        rows.append(
            f"| {f['id']} | {f['severity']} | {f['certainty'] or '—'} | {f['dev_fix'] or '—'} | "
            f"{task} | **{state}** | {reason}{extra} |"
        )

    def count(sev):
        return sum(1 for f in findings if f["severity"] == sev)

    header = f"""# Balance de la revisión: qué entró y qué no

Una fila por hallazgo de los nueve informes de `odd/reviews/` (revisión de `2541fe5f`, 24-11-2026). Generado por
`odd/review-ledger.py` desde los informes y las tablas de `odd/tasks/review-fixes.md`; el script falla si un hallazgo
no está asignado a una tarea, así que nada puede quedar fuera del balance. Las únicas fuentes son los nueve informes
(y su `BRIEF.md`): comprobado que ninguna otra sesión del directorio original escribió artefactos de revisión.

- Hallazgos: **{len(findings)}** fichas = **{len(findings) - 1}** hallazgos distintos: `ROLE-6` está marcado en su
  propio informe como duplicado de `PERS-3` y por eso comparte tarea.
- Por severidad: crítica {count('crítica')} · alta {count('alta')} · media {count('media')} ·
  baja {count('baja')} · mejora {count('mejora')}.
- Por estado: hecho {by_state.get('hecho', 0)} · pendiente {by_state.get('pendiente', 0)} ·
  bloqueado (decisión humana) {by_state.get('bloqueado', 0)}.

`dev` = ¿lo arregla ya `origin/dev`? `sí` significa que el arreglo upstream existía y se portó o se portará;
`no` significa que el arreglo es nuestro.

| Hallazgo | Sev. | Certeza | dev | Tarea | Estado | Motivo y commit |
| --- | --- | --- | --- | --- | --- | --- |
"""
    footer = """
## Qué no entró, y por qué

- **Ola C (decisión humana).** 31 fichas esperan una decisión de producto, seguridad o alcance: BLE/USB/TCP/MQTT
  (T20), política de malla y fail-closed (T21), cuánto se acepta de la capa Meshtastic (T22), semántica del reloj
  (T23), datos ante fallo de montaje (T24), datasheet o rediseño del driver (T25) y alcance de CI, release y arnés
  (T26). No se tocaron a propósito: elegir por mi cuenta sería decidir producto o compatibilidad sin el dueño.
- **Ola B (pendiente).** 53 fichas con causa demostrada y arreglo inequívoco. Están aceptadas y con tarea asignada;
  lo que falta es el trabajo, no el criterio.

## Límites de verificación (afectan a lo que valen los «hecho»)

- El arnés de host compila la lógica de malla, radio, paquetes y preferencias, pero **no** `zephcore/app/` ni
  `CommonCLI.cpp` (QA-3, T26). Los arreglos de T1 y T8 no tienen test de host posible hoy: se verificaron leyendo el
  camino completo y con la suite verde.
- ASan/UBSan solo existe en Linux; aquí no hay sanitizer.
- Los datos de compilación y qué se compiló de verdad están en el `## Log` de `odd/tasks/review-fixes.md`, no en este
  balance: un hallazgo marcado `hecho` no afirma verificación en hardware.
"""
    OUT.write_text(header + "\n".join(rows) + "\n" + footer, encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}: {len(findings)} findings, states {by_state}")


if __name__ == "__main__":
    main()
