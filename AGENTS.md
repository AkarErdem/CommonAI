# CommonAI contributor instructions

## Scope and entry points

These instructions govern the CommonAI plugin and its neutral shared outputs. They supplement the authorized host project's instructions; they are never a replacement for a host AGENTS.md. Preserve all existing host rules and edits outside an explicitly managed bridge block.

For NPC/AI work, read `Docs/LAYER_PLACEMENT_POLICY.md` before choosing where code or data belongs. For host integration or instruction setup, also read `Docs/HOST_INTEGRATION.md`. Read the relevant architecture, audit, asset-policy and roadmap sections for the task; do not require the user to reread the whole pack.

CommonAI can be a nested Git repository at `Plugins/CommonAI`. Automatic instruction discovery may stop at that repository boundary or omit child directories when starting in the host. Explicitly read the applicable host guidance before inspecting/modifying an authorized host, and explicitly read this file when starting from the host. Check applicable AGENTS.override.md files and narrower instructions. Never delete or weaken existing guidance to resolve a conflict; reconcile it with the current user instruction and ask only about a material unresolved conflict.

## Primary reference: the local manual

The canonical behavioral source of truth for AthenaAI is `Docs/AthenaAI_Manual/Athena_AI_Manual.md`, relative to this plugin. Keep its `media/` directory beside it. Read the relevant manual sections for every Athena-related design, implementation or review. Read the file explicitly even though Git ignores it; absence from an ordinary rg file listing is not evidence that it is missing.

The manual governs intended vendor behavior and authoring conventions. Installed vendor source establishes exact C++ names, exports, signatures, construction hooks and executable behavior. Verify both before implementing an extension. When they materially disagree, record the discrepancy and ask before choosing a conflicting behavior; do not silently override the manual, invent an API, or edit the manual to fit a workaround. Missing evidence blocks only the affected work. Shared audit summaries cite the manual's relative path and section, and vendor-relative source paths/symbols; never copy the manual or vendor implementation bodies into tracked shared files.

## Dependency direction and module identity

The ownership direction is `Game -> CommonAI -> AthenaAI`, with ordinary Unreal Engine dependencies at each level. The current runtime module is `CommonAI`: `CommonAI.uplugin`, `Source/CommonAI/CommonAI.Build.cs`, the ModuleRules class and module entry point must agree. Keep public export macros consistent with `COMMONAI_API`. Add `CommonAIEditor` only for concrete editor tooling, with no runtime dependency on it.

Use AthenaAI as the separately installed vendor implementation. Do not modify it. Useful native Component, Preset, Task, Sequence, Action, Service and Evaluator subclasses belong in CommonAI when supported. A Data Manager subclass must be instantiated through a verified supported construction path; otherwise use a supported composition alternative and disclose the limitation. Do not invent hooks or use multiple UObject inheritance to combine task families.

CommonAI must not depend on any consumer module, concrete consumer class, /Game asset, soft path, identifier-based branch or consumer-specific configuration. Define small interfaces/context records in CommonAI and implement consumer bindings in the game. Game integration should use CommonAI's API rather than introducing a new direct vendor coupling; an unavoidable exception requires an explicit decision. Public Athena-derived headers naturally expose a transitive vendor dependency, which is expected.

## Prefer shared mechanisms; preserve user choice

Put reusable NPC mechanisms in CommonAI by default, even when first requested by one consumer. This includes routines, scheduling, needs logic, activity sequences, reusable role behavior, reservations, interruption, cleanup and neutral presets/curves. Put concrete world bindings, game rules, presentation and game-owned content in the consumer. Split mixed features into a reusable mechanism plus narrow consumer adapters/data; do not move the whole feature to the game just because it uses a game service.

Before implementation, briefly state the proposed placement and reason. When placement or a new public contract is materially ambiguous, ask one focused question with a recommendation and tradeoff before implementing that disputed part. Do not silently choose a layer, create both versions, or add an unnecessary generic framework. Continue independent clear work. Record accepted reusable decisions in `Docs/DECISIONS.md`; keep concrete consumer decisions and identifiers only in the host. The detailed decision procedure and examples are in `Docs/LAYER_PLACEMENT_POLICY.md`.

## Context and distribution boundaries

Use only CommonAI, the explicitly authorized current host and its installed dependency. Do not retrieve unrelated workspaces, project histories or account context. Keep shared names, comments, examples, asset metadata, documents, commit messages and reports neutral. Never put private consumer names in a shared denylist. Raw host logs, screenshots, absolute paths and private integration notes remain outside CommonAI. Instruction files are guidance, not an access-control sandbox.

The local manual and media may stay under `Docs/AthenaAI_Manual/`; that entire directory is untracked local reference material. Do not delete it, commit it or include it in a release. Keep vendor source/assets/binaries separately installed. Check the Git index as well as ignore rules; ignoring a path does not remove already tracked content or old history. Never rewrite history or publish as an implicit cleanup step.

## Implementation and verification

Use C++ for behavior and lifecycle; use original assets for configuration, presets and curves. Preserve the host's controller and execution authority. Keep immutable definitions separate from per-agent/per-run state. Give async work an execution identity, reject stale callbacks, reset reused instances, and release only owned resources on all termination paths. Distinguish actual activity success from vendor task termination. Finite actions must finish; fallback must remain interruptible without a finish/rescore loop. Consult verified vendor facilities before adding a competing scorer or planner.

Before completing a change, inspect C++/Build.cs/descriptor dependencies and hard/soft asset references for the intended direction. Validate meaningful lifecycle risks and builds relevant to the change. Verify portability in an independent minimal host before release; it is not a prerequisite to start inside the authorized host. State unrun checks honestly. Do not claim text scans prove binary asset safety.

`Prompts/00_START_HERE.txt` explicitly authorizes bootstrap, source audit and the bounded first implementation slice. `Prompts/01_AUDIT_AND_PLAN.txt` used alone is read-only. `Prompts/02_IMPLEMENT_FIRST_SLICE.txt` authorizes its implementation scope. Do not add another approval pause for routine choices already authorized by the selected task. Ask when placement, evidence or scope is materially unresolved. Maintain `Docs/STATUS.md` and neutral audit evidence; do not overwrite existing progress with templates. Never commit, push, publish or advance to later roadmap phases without the relevant user request.
