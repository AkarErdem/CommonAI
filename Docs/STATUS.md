# CommonAI implementation status

## Current milestone

The local first native slice is implemented in the `CommonAI` Runtime module: an execution context and opportunity provider, exclusive reservation sites with timed and signaled completion, reusable Move and Wait, and a Work sequence with Find, Reserve, Move, Perform and Release actions. No production controller or movement pipeline was replaced.

The ownership policy now directs future reusable NPC behavior into CommonAI and limits consumer code to thin authoritative-state and concrete-effect bindings. This policy update adds no speculative runtime API.

Completed bootstrap and first-slice task prompts were removed. The read-only audit prompt and the lasting architecture, source audit, acceptance gates and host bridge remain available.

The neutral authored Athena preset and Curve Table are deferred to the next narrow task by scope decision. Until they exist, normal utility scoring cannot select Work from packaged content. No runtime execution result is claimed.

## Verified evidence

- The manual and 86 media files are present and currently indexed in the CommonAI repository. There is no directory-specific ignore rule. This observed state is not authorization to change tracking, commit or distribute them.
- The local AthenaAI 2.6 manual and its media are present; installed AthenaAI 2.6 source and Unreal 5.8 native declarations were audited in `ATHENA_SOURCE_AUDIT_RESULTS.md`.
- The final CommonAI Runtime sources compiled and linked in the current host's Unreal 5.8 Win64 Development Editor and non-editor Game targets. Project files were regenerated after the final C++ file changes.
- The CommonAI descriptor, Build.cs and native module entry point agree on `CommonAI`; dependencies point toward AthenaAI. A text scan found no consumer class, `/Game/` path or runtime load in shared source. This does not inspect serialized asset references.
- At an earlier implementation checkpoint, an ignore rule was added and 87 manual/media files were removed from the nested index without deleting the working copies. A later change restored the tracked index state. Neither step rewrote history; future tracking changes require a specific owner decision.
- The host instruction bridge has one valid marked block. No unrelated host instruction text was replaced.

## Validation limits

- No CommonAI runtime tests remain in this change or were run, by scope decision. Normal completion, navigation failure, target loss, replacement, disable, repeated execution, stale callbacks and two-agent isolation remain unverified runtime behavior.
- The host's change-aware Plan selected 250 tests and 38 fixture jobs, beyond its affected-test budget, and reported 13 existing known failures. No test or full integration run is claimed.
- No authored preset, Curve Table, packaged-content check or independent minimal-host portability check has run.
- The host source-control agent was unavailable during this session, so parent tracking ownership could not be confirmed through its status/diff commands. No parent source-control strategy was changed.

## Next narrow task

Create and independently reload a neutral Athena preset and Curve Table with the Work task and an interruptible Wait fallback, then verify the activity in an authorized runtime host when tests are in scope again. Keep any consumer movement-authority adapter in that consumer.
