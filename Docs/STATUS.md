# CommonAI implementation status

## Consumer-owned daily-life authoring

Consumers own occupation profiles and tag registrations. Each activity declares its exact identity and four scoring input tags. CommonAI retains the reusable timing, needs, selection, movement, claims and completion lifecycle. Site-driven effects can report their own result and acknowledge safe cancellation. Slot loss detaches the effect using its original request. Closing a routine clears the projected profile's exact destination bindings.

The UE 5.8 Win64 Development Editor build and six focused checks passed in an independent minimal consumer. Coverage included separate consumer activities, schedule-based selection, target and cooldown isolation, safe cancellation and replay, stale completion, target loss, saved progress, and the shared Smart Object definition. The consumer integration also passed its focused daily-life cycle and persistence tests.

## Earlier reusable daily-life extraction

The reusable daily-life implementation now includes default binding to an engine Character/AIController, owned path-following requests, bounded controller handoff retries and shared interruption/cleanup. Native navigation and controller hooks keep existing execution authorities available through thin consumer adapters. The optional shared clock supplies accumulated hours; an external context can continue supplying its own time and interruption facts. Athena remains the utility selector.

Persistence uses explicit weak resident registration, stable identities, retained unloaded records and optional clock snapshots. Resident-only snapshots leave external time authority untouched. Records and loaded identities are checked before restore writes; callback-driven lifecycle changes return failure and may leave partially applied state. No automatic disk saves or legacy compatibility branch are introduced.

At this checkpoint, CommonAI centralized daily-life tags under `CommonAI.*` and shipped seven occupation profiles alongside `SO_DailyLifeSingleSlot` under `/CommonAI/DailyLife`. The consumer-owned authoring milestone above moved occupation tags and profiles into the game. Neutral fixtures and acceptance checks belong to the CommonAITests Editor module.

CommonAITests is restricted to Editor targets, including Play In Editor and editor commandlets. Development and Shipping game targets do not compile or load it. Consumers that reference its fixtures must also guard that dependency with `Target.bBuildEditor`. The development settlement is a PIE fixture, not a packaged gameplay spawner.

The follow-up Editor build and focused PIE settlement/debug startup check passed. A resolved Development Game target graph includes CommonAI and excludes CommonAITests. This boundary check does not claim a new packaged-game validation run.

The UE 5.8 Win64 Development Editor build and focused consumer integration checks passed, including a rendered 60-resident day, controller handoff, needs/interruption, persistence and real Smart Object ownership. The eight shared assets independently reloaded with current hashes and no outgoing consumer references. Temporary migration redirects and old tag registrations were removed.

A fresh minimal consumer containing only an empty game module, CommonAI, CommonAITests and the separately installed AthenaAI dependency compiled successfully. Its three exact checks passed: `CommonAI.DailyLife.PortableLifecycle`, `CommonAI.DailyLife.PersistenceBoundaries` and `CommonAI.DailyLife.SharedContent`. These exercise unmodified engine character/controller binding, real slot occupancy and cleanup, logical progress, disk save/load, unload/re-registration, external-context persistence boundaries and all eight authored shared assets. The 124 source/content/resource/descriptor files matched the working copies by SHA-256; startup diagnostics were clean. The stationary portable fixture does not separately establish navigation travel in a new host; consumer integration covers real travel. No new Shipping/package, multiplayer or Full-suite acceptance is claimed. Raw evidence remains outside the plugin. Changes remain uncommitted.

## Earlier daily-life milestone

Authored daily-life profiles/components, a consumer time/interruption context, serializable needs/progress, a Smart Object single-slot adapter and `CommonAI.Debug.All` canvas labels were implemented using the existing Athena-backed profile and action sequence. The UE 5.8 Editor build and focused runtime checks passed in the authorized consumer at that checkpoint. Coverage included needs/routines, per-run commitment, stable state restoration, real claim contention/disable/destruction/preemption, bounded alternative acquisition, and truthful debug controls. A rendered population completed a full simulation day without activity animations; debug-on/off labels were visually checked. The clock at that checkpoint was supplied by the consumer. Full-suite, Shipping/package and separate portable-host release acceptance were not run for that milestone. Raw consumer evidence stays outside the plugin.

## General foundation

One Runtime module exposes CommonAI/engine-only profiles, agent APIs, execution contracts and diagnostics. Athena-derived task, action, service and scoring adapters remain private. Vendor source and content are unchanged. The consumer keeps its existing controller and concrete world operations.

Implemented: general task/action definitions, numeric considerations and per-task curves, evaluator composition, conditional services, interval/input/manual scoring, exact-tag commands with typed payloads, source-owned pauses/locks, safe profile transitions, task policy controls, start/progress/end events, optional Behavior Tree execution, typed external execution bindings, and a native inspector. Existing tags, montage windows, reservations, movement and safe cancellation use the same lifecycle.

The saved Work profile uses the current general schema; obsolete Work-only fields and runtime migration branches are absent. DA_GeneralWorker and CT_GeneralWorker provide a second authored example with competing station-work/standby tasks, external preference, distance evaluation and an observation service. Setup and tuning instructions are in README.md.

## Earlier general-foundation verification and coverage

- Unreal 5.8 / AthenaAI 2.6 / Win64 Development Editor builds succeeded.
- The focused General/Foundation batch at that checkpoint passed 19 of 20 checks initially. The existing FailurePaths check exposed a fallback-identity duplication bug; reflecting the private flag fixed it, and that check then passed. No assertions were removed or weakened. The 20 existing checks are covered by those two runs.
- Five vendor characterization checks passed in the preceding grouped run. Priority, resistance, momentum, locking, normalization, service timing and transition differences inform the private adapters. The owner-approved floating count-factor correction is documented in D026; single-consideration behavior is unchanged.
- Review fixes for reservation cancellation reentry, stale evaluator updates, disabled services and atomic batches passed their focused checks. BT latent-abort completion uses the engine's public tick scheduling and passed the safe-stop check.
- Saved Work migration and current-schema resave passed guarded Preview/Apply, backup and independent reload. The new example profile and curves independently reloaded without errors, load warnings or truncation. Registry dependencies contain shared plugin/script references only.
- Runtime architecture and CommonAI literal asset-path checks passed. Public headers contain no vendor types; AthenaAI is a private module dependency. These checks do not establish exhaustive native binary-payload privacy.
- An independent minimal consumer compiled its Editor target using CommonAI and ordinary engine dependencies. The Game target also built, cooked and packaged. The packaged rendered run passed service/evaluator-driven Standby-to-Work selection and delayed cancellation with busy-state retention and cleanup. Its inspector screenshot was visually checked for readable scores, inputs, service state and controls. Source/content inventory hashes matched the working inputs.

| Feature family | Evidence at that checkpoint |
|---|---|
| Profiles, IDs, fallback, curves and action occurrences | Definition/curve authoring checks; saved Work and general example inspections |
| Numeric ranges, validity, batches and producer ownership | Evaluation, selection, manual evaluation, atomic inputs and stale evaluator checks |
| Task policies and native scoring | Vendor characterization plus normal selection/command checks |
| Sequences, interaction, movement, tags and montage windows | Original six foundation checks plus execution/binding/reentrant-reservation checks |
| Services, conditions and evaluator composition | Service/evaluation and atomic update checks |
| Commands, pause, payload and profile/pawn lifecycle | Command ownership, binding payload, selection and teardown checks |
| Behavior Tree and Blackboard ownership | Safe-stop fixture, including latent abort |
| External ability integration | Typed execution contract and neutral binding fixture; concrete GAS integration excluded |
| Inspector data and controls | Existing InspectorControls check and visually inspected packaged screenshot |
| Visual Logger, Gameplay Debugger and Insights | Native integration compiled; interactive capture sessions not exercised |

## Limits and decisions

Reusable daily-life needs, routines, binding, movement ownership, clock and persistence are implemented. Concrete farming outputs, trade, healing, combat, inventory effects and character presentation remain consumer responsibilities through the typed contracts. Gameplay tags identify concepts; continuous consideration values remain numeric. Multiplayer and a concrete GAS adapter remain excluded by design. The latest refactor still requires the validation stated above.

Cancellation waits for the effect owner's safe acknowledgement or natural completion. The watchdog reports overdue cancellation without forcing success or releasing active effects. Natural-completion profile changes can wait indefinitely on an infinite task. Emergency owner/world loss uses explicit failure cleanup. Montage tag windows are montage-only; movement adapters must relinquish their own request when cancelled.

Visual Logger/Insights capture sessions, aim selection and world-panel presentation have no separate interactive acceptance run. Their implementation/compile evidence must not be described as exhaustive live verification. Native authoring, snapshots and extension contracts do not prove every possible downstream Blueprint or ability integration.

The host-wide Plan/Affected runner stopped at its bounded scope gate and retains pre-existing failures; no Full run was performed. Focused results are not a full-host green claim. Raw host-specific logs and reports stay outside this plugin. No commit, publication or vendor modification was performed by this task.

## Historical Work/Wait foundation checkpoint

The single Runtime module now provides a consumer-facing agent component/profile API with a private Athena backend. Public headers expose no Athena types. The backend uses the installed vendor scorer and action sequence, including a safe cancellation gate for ordinary replacements and fallback.

Implemented: source-owned hierarchical tag contributions; artist-facing montage tag windows; typed actor/component interaction and optional movement contracts; timed/signaled targets; execution-scoped external completion; safe deferred Stop/Profile replacement; emergency teardown and reentrancy guards; and one agent diagnostic snapshot. Original Work profile and curve assets are included under Content/Profiles.

The profile projects its authored availability curve into transient vendor definitions per agent. Specialized activity identities can reuse the source curve without changing its row name. Shared assets are not modified by runtime execution.

## Verified evidence at the Work/Wait checkpoint

- Unreal 5.8 / AthenaAI 2.6 / Win64 Development Editor build succeeded.
- Six focused automation checks passed: tag ownership, interaction ownership, normal scoring and drain, teardown/reentry, real montage windows, and failure paths.
- Coverage includes overlapping exact-tag contributions, two-agent reservation exclusion, stale completion, safe natural cancellation, stop/profile/pawn transitions, target/pawn destruction, navigation failure, fallback, montage overlap/replay/interruption and possession transfer.
- A fresh independent asset inspection verified the saved Work profile and curve, exact package identities/fingerprints, complete non-truncated exports and no load errors/warnings. Their dependency exports contain only CommonAI/script references.
- Runtime architecture and literal runtime asset-path checks passed. Source scans found no consumer identifier or reverse content reference. These checks do not claim exhaustive native binary-payload privacy analysis.
- A separate minimal consumer with only CommonAI and its Athena prerequisite compiled its Editor target. Source inventory/hash comparison confirmed the tested plugin copy matches the working runtime source.
- The same independent consumer compiled its Game target, cooked and packaged successfully. Its packaged null-RHI smoke run loaded the saved profile, selected Work through ordinary scoring, retained busy state during cancellation, reached Cancelled at natural completion, released the state, and exited successfully. The source copies matched the working inputs by inventory and SHA-256.

## Limits

This is the verified Work/Wait foundation, not a universal behavior library or production consumer integration. Montage tag windows are montage-only. Optional movement adapters must synchronously relinquish their own movement request when cancelled. Signaled interactions must acknowledge a safe end or complete naturally; a diagnostic watchdog does not force-release their effects.

The host-wide change-aware runner exceeded its bounded scope and retained previously known failures. Focused CommonAI results are not a full-host validation claim. Raw host-specific commands, logs and reports remain outside this plugin. No commit, publication or vendor modification was performed.

## Historical first-slice checkpoint

The following records describe the previous checkpoint and are retained as history, not current limitations.
### Milestone at that checkpoint

The local first native slice is implemented in the `CommonAI` Runtime module: an execution context and opportunity provider, exclusive reservation sites with timed and signaled completion, reusable Move and Wait, and a Work sequence with Find, Reserve, Move, Perform and Release actions. No production controller or movement pipeline was replaced.

The ownership policy now directs future reusable NPC behavior into CommonAI and limits consumer code to thin authoritative-state and concrete-effect bindings. This policy update adds no speculative runtime API.

Completed bootstrap and first-slice task prompts were removed. The read-only audit prompt and the lasting architecture, source audit, acceptance gates and host bridge remain available.

The neutral authored Athena preset and Curve Table are deferred to the next narrow task by scope decision. Until they exist, normal utility scoring cannot select Work from packaged content. No runtime execution result is claimed.

### Verified evidence

- The manual and 86 media files are present and currently indexed in the CommonAI repository. There is no directory-specific ignore rule. This observed state is not authorization to change tracking, commit or distribute them.
- The local AthenaAI 2.6 manual and its media are present; installed AthenaAI 2.6 source and Unreal 5.8 native declarations were audited in `ATHENA_SOURCE_AUDIT_RESULTS.md`.
- The final CommonAI Runtime sources compiled and linked in the current host's Unreal 5.8 Win64 Development Editor and non-editor Game targets. Project files were regenerated after the final C++ file changes.
- The CommonAI descriptor, Build.cs and native module entry point agree on `CommonAI`; dependencies point toward AthenaAI. A text scan found no consumer class, `/Game/` path or runtime load in shared source. This does not inspect serialized asset references.
- At an earlier implementation checkpoint, an ignore rule was added and 87 manual/media files were removed from the nested index without deleting the working copies. A later change restored the tracked index state. Neither step rewrote history; future tracking changes require a specific owner decision.
- The host instruction bridge has one valid marked block. No unrelated host instruction text was replaced.

### Validation limits

- No CommonAI runtime tests remain in this change or were run, by scope decision. Normal completion, navigation failure, target loss, replacement, disable, repeated execution, stale callbacks and two-agent isolation remain unverified runtime behavior.
- The host's change-aware Plan selected 250 tests and 38 fixture jobs, beyond its affected-test budget, and reported 13 existing known failures. No test or full integration run is claimed.
- No authored preset, Curve Table, packaged-content check or independent minimal-host portability check has run.
- The host source-control agent was unavailable during this session, so parent tracking ownership could not be confirmed through its status/diff commands. No parent source-control strategy was changed.

### Next step recorded at that checkpoint

Create and independently reload a neutral Athena preset and Curve Table with the Work task and an interruptible Wait fallback, then verify the activity in an authorized runtime host when tests are in scope again. Keep any consumer movement-authority adapter in that consumer.


