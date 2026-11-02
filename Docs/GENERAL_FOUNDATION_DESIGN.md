# General CommonAI foundation design

Status: architecture selected under the owner's delegated design judgment; implementation has not started. This extends, rather than replaces, the verified Work/Wait foundation.

## Objective and scope

Expose the installed AthenaAI 2.6 single-player feature set through a coherent CommonAI API and human-readable authoring model. Preserve Game -> CommonAI -> AthenaAI, one Runtime module, private vendor types, source-owned state tags and safe asynchronous cancellation. The vendor remains separately installed and unchanged.

Coverage means equivalent usable capabilities, not copying vendor internals, exposing vendor objects, or reproducing unsafe immediate transitions. Multiplayer remains excluded by the owner's decision. Demo gameplay systems are examples, not a requirement to build universal combat, inventory or economy systems.

## Selected approach and alternatives

Selected: a CommonAI profile/definition model projected into internal Athena adapters. The vendor retains scoring, task selection, sequence progression, service conditions/timers and supported task policies. CommonAI owns neutral behavior, contracts, authoring validation and safe resource transitions.

A method-for-method public vendor wrapper would be smaller initially but expose implementation concepts and unsafe transition semantics to authors. A backend-independent replacement framework would duplicate working mechanisms and add maintenance. Neither is selected.

Start with normal asset Details panels, instanced definition pickers, named curve references and the existing curve editor. Add an editor-only module only when concrete validation, migration or inspection UI requires UnrealEd/Slate editor APIs. Runtime must never depend on that module. No graph DSL, recursive profile merge system, general event bus or second planner is planned.

## Authoring experience

The normal setup remains one Agent Component plus one Profile. A profile presents these sections:

1. Identity and description.
2. Tasks: activity tag, reusable behavior/sequence, eligibility, consideration-to-curve bindings and optional execution bindings.
3. Considerations: tag, units, initial raw value, min/max and the designated update route.
4. Services: stable ID, conditions and update cadence.
5. Fallback: one validated, interruptible behavior.
6. Advanced: task policies, scoring cadence, transition policy and diagnostics.

Each field has an English label, units where applicable, an explanation and sensible defaults. Errors identify the profile entry and tag, not only a backend class. Adding a task should not require inventing curve-row strings or editing a vendor preset. Existing user-authored curves must survive regeneration.

The Agent Component remains the runtime inspection entry point: effective tags and contributors, all task scores and policy states, raw/normalized considerations, per-task curve contributions, services and their conditions, current action, pending command/profile transition and cancellation cause/wait state.

## Identity and state

- CommonAI.Activity.* identifies tasks; do not duplicate this identity in a Task.* namespace.
- CommonAI.Action.*, Consideration.*, Service.* and Profile.* identify their corresponding definitions.
- State.*, Capability.* and Reason.* retain their current semantic roles.
- IDs and numeric channels use exact matching. State/capability eligibility may use explicit hierarchical queries.
- Definitions may use specialized child tags without a new behavior class. Unknown IDs are diagnosed; missing registration is not silently repaired at runtime.
- Task, consideration and service IDs are unique within the resolved profile. Action tags identify reusable behavior and may repeat in a sequence (Move/Wait/Move/Wait). Each authored sequence entry has an automatically assigned stable occurrence ID; a running occurrence is identified by execution plus entry ID. Authors do not manually manage occurrence handles.
- State contributions retain existing source and execution ownership. Montage authors select ordinary tags; they do not manage handles.
- A numeric consideration has one designated authoritative update route. Do not sum, OR or last-writer-win unrelated numeric producers implicitly. A service may invoke an evaluator as that route; event-fed values use their configured binding.

## Considerations, curves and evaluation

Expose read, defaults/range initialization, raw-value update, explicit normalization range, evaluation, batch update and rescore through neutral types. Support finite values and min < max; reject invalid ranges and NaN/infinity before vendor calls. Boolean channels use a 0..1 range.

Definitions and mutable per-agent values are separate. Public snapshots include raw value, normalized value, range, validity and source. Missing/unavailable data defaults to making affected tasks ineligible with a reason; an explicit authored hold-last policy may retain an earlier value. A skipped evaluation is distinct from unavailable data. Preserve the vendor's default skip/hold behavior only when that policy was selected explicitly.

Each task-consideration pair references a typed curve table plus an existing row. Friendly display names are separate from stable IDs. Backend row names are generated from exact IDs and validated for collision. Source curves remain immutable. Batch updates publish a coherent set before a single scoring pass and avoid triggering services on an unintended partial input set.

Provide a stateless CommonAI evaluator contract with validated numeric, actor, location and event contexts. A validated FInstancedStruct extension may carry custom typed data; it must not become a string-keyed generic bag. Shared evaluation/lifecycle stays native. Blueprint hooks may provide narrow observations/effects while native code retains state ownership and transition guards.

The installed vendor evaluator lacks its module export macro and constrains contexts to its own base struct. Prefer composing a CommonAI evaluator and passing its numeric result through supported vendor consideration APIs. Do not assume native inheritance is linkable, use reflection to bypass a missing export, or modify the vendor.

## Tasks, actions and services

Expose authored priority, score modifier, replace resistance, ordinary interruptibility, tick settings, unlock delay and momentum settings. Distinguish ordinary utility interruption policy from explicit accepted cancellation. Current behavior remains the default where no new policy is selected.

Generic single-task and sequence adapters delegate behavior to CommonAI execution instances. The vendor owns action order. Every action has a tag, optional tick cadence and an execution-scoped result. Work remains a reusable sequence with its existing reservation/provider behavior, not a separate legacy runtime.

Services use the vendor's normalized condition comparisons and timer lifecycle, with CommonAI definitions/callbacks. Include start, tick, stop, pause interaction and cleanup. Empty conditions and a zero tick interval require explicit tested semantics; do not assume the vendor starts unconditional services automatically. Do not introduce an independent scheduler.

Services may perform reusable native work beyond evaluating a numeric input. Provide a narrow CommonAI service-behavior extension contract for start/tick/stop; the internal vendor service still owns activation and timers. A zero scoring-update flag alone is not an atomic service batch: characterize the installed update path and defer service transitions until the final coherent input set where necessary.

Read-only task, consideration and service queries return CommonAI snapshots. Runtime settings operations affect the agent instance, never shared definition assets. Task/action/service events are execution-aware and delivered once without reentrant backend mutation.

## Commands, cancellation and profile changes

All tasks, sequences, fallbacks and adapters use one safe transition contract. Preserve existing stale-signal checks, deferred completion, source-owned cleanup and synchronous callback/reentry guards.

A command requests an activity by exact tag, including when several entries share one native behavior class. It may bypass utility preference but never bypass resource ownership. Normal utility changes re-evaluate after drain. Explicit commands retain their requested identity and revalidate eligibility/capabilities before starting.

There is one pending explicit intent, not an unbounded command queue. A superseded request receives an explicit outcome. Stop prevents new starts; profile replacement invalidates incompatible pending task requests. Repeated cancellation is idempotent. Pause affects scoring according to its authored service policy; it does not itself cancel active effects.

Command retention is separate from the transition into the command: AllowUtilityReplacement or HoldUntilCompletion, with HoldUntilCompletion as the default for explicit orders. A command owns only its own scoring hold. Finishing/superseding it must not release an existing manual or other source-owned pause. Requesting a retry-locked task is rejected by default; an explicit unlock option follows the verified vendor policy and remains visible in diagnostics. Native automatic resume must respect the outstanding CommonAI-owned holds.

Profile change supports CancelAtSafePoint and AfterNaturalCompletion. Invalid profiles are rejected before draining. Shared consideration values may be preserved by exact tag under an explicit policy, with range changes re-normalized; removed values and old callbacks cannot affect the new profile.

AfterNaturalCompletion can remain pending indefinitely on an intentionally nonterminating fallback or activity. Show that waiting reason and allow the caller to supersede it with CancelAtSafePoint. Do not add a periodic fallback finish loop or a forced timeout.

Pawn replacement starts a new input/execution generation and defaults to the authored consideration values/ranges. Old-pawn callbacks are rejected. Surviving old-pawn effects drain against their original pawn/target references; they are not transferred to the replacement pawn. Destroyed-pawn/world teardown uses emergency cleanup. Only explicitly consumer-owned state may be reintroduced through fresh bindings. Characterize the manual's possession reset claim against the installed source before relying on automatic vendor reset.

Cancellation may wait for a safe animation/event point or natural completion. Reservations and required state remain owned until acknowledgement. Watchdog expiry reports the wait and does not silently force release. Destruction/world teardown follows the established emergency path. Completion after accepted cancellation does not become a success-only reward.

Terminal mapping must be tested alongside the following lock/momentum behavior; a public result is not a raw vendor result:

| Cause | Public result | Internal termination policy |
|---|---|---|
| Natural successful execution | Completed | Successful; normal vendor momentum/unlock policy |
| Action failure after execution starts | Failed | Aborted; preserve configured vendor retry/unlock behavior |
| Invalid setup/start context | Failed | Failed; reject activation and wait for corrected prerequisites |
| Utility replacement after safe drain | Cancelled | Replaced; no successful-work momentum or reward |
| Explicit cancellation, Stop or profile replacement after safe drain | Cancelled | Controlled replacement semantics; no successful-work momentum, with next scoring governed by current holds/intent |
| Destruction/world teardown | Failed with teardown reason | Failure cleanup only while vendor context is valid; do not schedule retries into a destroyed world |

CommonAI publishes one result only after ownership and internal termination are consistent. The mapping must not unlock source-owned manual constraints as a side effect of a vendor automatic unlock.

The vendor's ForceTaskStart selects by class and can stop an uninterruptible task immediately. A tag-based CommonAI command must resolve exactly one instance and use supported internal component methods after safe termination. Do not fake command selection by distorting utility scores or expose raw vendor ForceTaskStart to consumers.

## Integration and debugging coverage

Behavior Tree is an opt-in execution type. Its adapter must check controller/brain ownership, expose a typed engine tree reference and wait for safe stop acknowledgement. It must not replace an unrelated active brain silently. The engine owns BT/Blackboard semantics; CommonAI owns the task boundary and resource cleanup.

Ability systems use an opt-in typed execution binding sharing the same request/result/cancellation contract. CommonAI is not required to depend on GAS for every consumer. The manual's GAS appendix describes integration patterns rather than a built-in vendor ability implementation. A concrete GAS adapter is a separately selectable integration when required, not an implicit new combat subsystem.

Debug coverage includes a neutral agent inspection panel, selected-agent/player controls, Visual Logger, Gameplay Debugger and Insights instrumentation. Reuse vendor facilities internally where they satisfy isolation and data accuracy; expose CommonAI snapshots to custom UI. Do not embed vendor widget inheritance into consumer classes. Debug presentation must not become a gameplay dependency or change selection.

## Vendor feasibility gates

Before broad implementation, characterize multi-consideration score combination, normalization, momentum, priority ties, locking/unlock, fallback and service lifecycle against installed source. In particular, CalculateTasksScore uses integer division in its consideration-count factor; record observed results rather than assuming the intended floating-point formula.

Do not silently alter scoring to match an inferred formula. A material documented/observed mismatch requires a bounded decision before affected behavior changes. Prefer a documented supported extension when necessary, keeping the vendor scorer as owner and avoiding a copied algorithm.

Also verify same-class task identity dispatch, non-virtual immediate transition entry points, event duplication/reentrancy, service context lifetime, BT safe stopping and debug/UI exports. Findings block only the affected feature; never label unsupported rows complete.

## Preservation and acceptance

The six existing foundation checks remain mandatory regression coverage. Introduce a deterministic multi-task fixture with two differently tagged instances of one behavior, several numeric considerations, different curves, a conditional service and fallback. Exercise real vendor selection and actual production completion/cancellation paths.

Inspect and migrate the saved Work profile through guarded tooling before removing its old fields. Preserve effective authored values and runtime outcome. Keep one current serialized contract; no indefinite schema-version runtime branch. Test any new asset tooling on disposable assets before production packages.

Validation is grouped by substantial milestones. Reuse successful unchanged evidence, maintain host catalog ownership and bounded Plan/Affected controls, and do not escalate to a full suite automatically. Final acceptance includes a clean independent host and packaged run with no consumer vendor API dependency.

Record each feature as implemented+verified, implemented+unverified, blocked or excluded-by-explicit-decision. Completion requires coverage of the feature matrix; a successful Work-only demonstration is insufficient.
