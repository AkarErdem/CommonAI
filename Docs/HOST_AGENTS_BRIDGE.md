<!-- COMMONAI:HOST-BRIDGE:BEGIN -->
## CommonAI integration

For NPC/AI planning, design, implementation or review anywhere in this project, explicitly read `Plugins/CommonAI/AGENTS.md` and `Plugins/CommonAI/Docs/LAYER_PLACEMENT_POLICY.md` before choosing the layer. Do not assume child instructions were automatically loaded. For setup or cross-repository work, also read `Plugins/CommonAI/Docs/HOST_INTEGRATION.md`.

For new NPC behavior, put reusable selection inputs and policy, routines, needs, sequencing, timing, retries, reservations, cancellation and cleanup in CommonAI by default, even for the first consumer; AthenaAI keeps its utility scorer. Keep this project's code to thin bindings for authoritative state, concrete world operations, game-specific rules/effects, presentation and content. If a game adapter begins owning general NPC behavior, move that mechanism into CommonAI.

When shared behavior needs a game system, add or reuse the smallest typed CommonAI contract required by the concrete behavior and implement it here through the existing authority. Do not add speculative public interfaces or replace the production pipeline just to minimize game code. State the intended placement before coding; ask before a materially ambiguous placement or public-API choice, and record any justified exception.

Maintain `Game -> CommonAI -> AthenaAI` ownership/dependency direction. Do not modify the vendor or introduce CommonAI references back to this project. Keep existing controller and execution authority. Public vendor inheritance can expose transitive Athena types.

For Athena-related work, use `Plugins/CommonAI/Docs/AthenaAI_Manual/Athena_AI_Manual.md` as the behavioral source of truth and verify native APIs against the installed vendor source. Ask about material conflicts rather than silently overriding the manual.

Preserve this project's existing instructions. Keep its names, private design, logs and asset paths out of CommonAI code, assets, docs, commits and reports. Read this project's applicable guidance before host edits; keep shared rules scoped to the plugin. If required guidance is missing or a material conflict cannot be resolved from the user's instruction, ask about the affected work.
<!-- COMMONAI:HOST-BRIDGE:END -->
