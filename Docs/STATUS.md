# CommonAI implementation status

## Current foundation milestone

The single Runtime module now provides a consumer-facing agent component/profile API with a private Athena backend. Public headers expose no Athena types. The backend uses the installed vendor scorer and action sequence, including a safe cancellation gate for ordinary replacements and fallback.

Implemented: source-owned hierarchical tag contributions; artist-facing montage tag windows; typed actor/component interaction and optional movement contracts; timed/signaled targets; execution-scoped external completion; safe deferred Stop/Profile replacement; emergency teardown and reentrancy guards; and one agent diagnostic snapshot. Original Work profile and curve assets are included under Content/Profiles.

The profile projects its authored availability curve into transient vendor definitions per agent. Specialized activity identities can reuse the source curve without changing its row name. Shared assets are not modified by runtime execution.

## Current verified evidence

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
### Current milestone

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

### Next narrow task

Create and independently reload a neutral Athena preset and Curve Table with the Work task and an interruptible Wait fallback, then verify the activity in an authorized runtime host when tests are in scope again. Keep any consumer movement-authority adapter in that consumer.


