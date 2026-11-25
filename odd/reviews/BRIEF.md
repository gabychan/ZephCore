# Briefing común: revisión completa del fork ZephCore

## Contexto

- Repo (checkout compartido): `/Users/xavier.ejarque/.t3/worktrees/ZephCore/t3-ab151ae2-90b71ff1ccc504cc`
- Commit revisado: `2541fe5f` (= `fork/master`). Firmware Zephyr compatible con MeshCore en el aire, con una capa
  Meshtastic añadida ("Zephtacore").
- Upstream: remoto `origin`. Base común con `origin/dev`: `da3513aa`. El fork lleva 214 commits propios
  (213 ficheros, +27358/-383). `origin/dev` lleva 32 commits que el fork no tiene.
- Antes de revisar, lee `AGENTS.md` y las secciones de `docs/DESIGN.md`, `docs/ARCHITECTURE.md` y `docs/adr/` de tu área.
  Los invariantes de `AGENTS.md` son criterio de revisión: una violación es un hallazgo.
- Trabajo ya conocido, en `/Users/xavier.ejarque/.claude/projects/-Users-xavier-ejarque-git-ZephCore/memory/`
  (empieza por `MEMORY.md`, y lee `zephtacore-improvement-backlog.md` y los de tu área). Pueden estar desfasados:
  compruébalos contra el código. Si un hallazgo ya figura ahí, márcalo `ya conocido` y di si sigue vigente.

## Reglas estrictas

- **Solo lectura.** No modifiques, crees ni borres nada dentro del repo. Nada de commit, push, checkout, stash,
  reset, ni comentarios o issues en GitHub. No flashees hardware.
- Si necesitas compilar o ejecutar algo para demostrar un fallo, hazlo en un directorio propio bajo
  `/private/tmp/claude-503/-Users-xavier-ejarque--t3-worktrees-ZephCore-t3-ab151ae2-90b71ff1ccc504cc/42672751-9bf9-4e3d-b434-8bd61fc43ba5/scratchpad/review/work-<área>/`
  (por ejemplo extrayendo con `git archive HEAD <ruta> | tar -x -C <dir>`).
- No lances subagentes. Haz el trabajo tú.
- Lee el código de verdad, incluidos los llamadores y los llamados. No informes de nada que no hayas seguido
  hasta el final.

## Qué buscar

Bugs reales primero: corrupción de memoria, desbordes, enteros, off-by-one, condiciones de carrera (ISR/workqueue/hilo
principal), bloqueos, fugas, estados imposibles, errores no tratados, wraparound de contadores y tiempo, endianness,
incompatibilidades de bytes con MeshCore o Meshtastic, seguridad (entrada remota no confiable, autenticación, cripto,
replay), consumo y arranque, regresiones del fork frente a upstream.

Después: mejoras con valor concreto (rendimiento, RAM/flash, fiabilidad, simplificación real, código muerto,
duplicación, huecos de tests, documentación que contradice el código).

Compara con upstream cuando ayude: `git diff da3513aa HEAD -- <ruta>`, `git log HEAD..origin/dev -- <ruta>`,
`git show origin/dev:<ruta>`. Indica si `origin/dev` ya arregla el problema. Da prioridad al código nuevo del fork,
sin ignorar el heredado.

## Criterio de evidencia

Cada hallazgo necesita una causa demostrada en el código y un escenario de fallo concreto (entradas, estado, resultado).
Antes de reportar, intenta refutarlo: busca la guarda en el llamador, el invariante que lo impide, el test que lo cubre.
Si no resiste, descártalo. No rellenes: pocos hallazgos sólidos valen más que muchos dudosos. Las propuestas siguen
`AGENTS.md`: causa raíz, sin parches sintomáticos, el cambio mínimo, común a todas las placas, gating por capacidad.

## Formato del informe

Escribe el informe completo, en español, en
`/private/tmp/claude-503/-Users-xavier-ejarque--t3-worktrees-ZephCore-t3-ab151ae2-90b71ff1ccc504cc/42672751-9bf9-4e3d-b434-8bd61fc43ba5/scratchpad/review/<área>.md`.

Para cada hallazgo, ordenados de más a menos grave:

```
### <ÁREA>-<n>. <título en una frase>
- Severidad: crítica | alta | media | baja | mejora
- Certeza: confirmado (demostrado o seguido en código) | probable | sospecha
- Origen: fork | upstream | ambos   ·   ¿arreglado en origin/dev?: sí/no/n.a.   ·   ¿ya conocido?: sí/no
- Ubicación: `ruta:línea`
- Causa: ...
- Escenario de fallo: ...
- Arreglo mínimo propuesto: ...
- Cómo verificarlo: test de host, prueba en hardware concreta, etc.
```

Termina el informe con: qué partes revisaste a fondo, cuáles solo por encima y cuáles no revisaste.

## Escritura incremental (obligatoria)

Una ronda anterior se cortó por límite de uso y perdió todo lo no escrito. Crea el informe en cuanto tengas el primer
hallazgo confirmado y añade cada hallazgo nuevo al momento, sin esperar al final (el orden por severidad se arregla al
terminar). Mantén al principio del informe una línea `Estado: en curso | terminado` y una lista corta de lo ya revisado.
Si tu directorio `work-<área>/` ya tiene material de la ronda anterior, reutilízalo en lugar de volver a generarlo.
Sé económico con el contexto: lee rangos concretos en vez de ficheros enteros cuando baste, y no repitas búsquedas.

## Respuesta final

Tu mensaje final debe ser breve (máximo 40 líneas): ruta del informe, número de hallazgos por severidad, y los
hallazgos críticos y altos con ID, `ruta:línea`, certeza y una frase. Di también lo que no pudiste revisar o verificar.
