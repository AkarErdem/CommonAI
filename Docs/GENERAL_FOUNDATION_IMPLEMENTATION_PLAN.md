# General CommonAI Foundation Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans for inline implementation. Preserve the host's version-control and milestone validation rules; no commits, publication or automatic full-suite run.

**Goal:** Expose the installed single-player Athena capabilities through usable CommonAI definitions and safe execution while retaining the verified foundation.

**Architecture:** One Runtime module projects CommonAI definitions into private Athena adapters. Shared scoring, sequence and service mechanisms remain vendor-owned. Public configuration, commands, snapshots and extension contracts use CommonAI/engine types only.

**Tech Stack:** Unreal Engine 5.8, AthenaAI 2.6, C++, Gameplay Tags, native asset authoring; initial verification on Win64.

**Spec:** `GENERAL_FOUNDATION_DESIGN.md` in this directory. Implementation, focused regression and independent packaged acceptance are complete through M7 within the final minimal-validation scope. The original checklist below records planned obligations; `STATUS.md` is the current evidence ledger, including M7 acceptance and limits. Do not interpret individual unchecked planning bullets as fresh implementation requests or claim untested combinations from a milestone label.

## Global constraints

- Keep Game -> CommonAI -> AthenaAI, private vendor types and unchanged vendor source/content.
- Preserve source-owned tags, montage windows, safe cancellation, emergency teardown and per-execution isolation.
- Use one current saved profile contract; migrate effective authored values before deleting superseded fields.
- Keep runtime independent of editor/test modules and consumer-specific classes/assets/configuration.
- Author behavior/lifecycle in native CommonAI; normal user tuning is data-driven.
- Keep single-player scope and no mandatory GAS dependency.
- Keep code, instructions, assets and reports neutral and in English. Host evidence/validation ownership stays outside the shared plugin.

## Review focus

1. Same native behavior class with two activity IDs must dispatch the intended instance (M1/M3).
2. Skipped, unavailable, stale and conflicting numeric input must not accidentally become eligible or overwrite another owner (M2/M4).
3. Profile/command/stop requests during synchronous completion or delayed cancellation must produce one terminal result and preserve resource ownership (M3).
4. Blueprint/evaluator/service templates must not retain mutable state across agents or runs (M2/M4).
5. Curve regeneration and profile migration must preserve authored data and independent saved-load equivalence (M2/M6).

## Feature coverage ownership

| Vendor/manual capability | Milestone | Acceptance obligation |
|---|---|---|
| Preset identity, tasks, fallback, services, considerations | M2 | General profile and validated projection |
| Tags, naming shorteners, curve generation/reset semantics | M2/M6 | Exact IDs, collision-free rows, guarded authoring preserving existing curves |
| Raw/default/normalized values, ranges, safe getters and batch updates | M2/M4 | Numeric boundary tests and coherent scoring/service updates |
| Dynamic evaluators and typed contexts | M4 | Composition feasibility and per-agent isolation |
| Task settings, priority, score modifier, resistance, locking and momentum | M1/M3 | Native characterization plus authored/runtime controls |
| Task/action lifecycle, tick and sequences | M3 | Vendor sequence execution with generic safe cancellation |
| Service conditions, tick, start/stop and pause behavior | M4 | Conditional activation/deactivation with no timer leaks |
| Enable/disable, possession, pause/resume, rescore, explicit task command | M3/M5 | One controlled transition pipeline |
| Immediate versus deferred profile changes and common input retention | M3/M5 | Explicit safe modes and validated value preservation |
| Task start/end and query/interface access | M3/M6 | CommonAI-only, execution-aware events and snapshots |
| Behavior Tree/Blackboard task | M5 | Explicit ownership and acknowledged stop |
| Ability-system integration patterns | M5 | Typed optional binding; no mandatory GAS dependency |
| Debug component/widget/player selection | M6 | Usable neutral inspector and selected-agent controls |
| Visual Logger, Gameplay Debugger and Insights | M6 | Real values/reasons and opt-in diagnostics |
| C++ extension and Blueprint-facing authoring/integration | M2-M6 | No consumer vendor types; native lifecycle guards remain in control |
| Multiplayer | Excluded | Prior explicit single-player decision |
| Demo-specific gameplay | Not framework parity | Reference examples only; no automatic game-system expansion |

## Proposed shared file boundaries

All paths below are relative to the plugin. These are responsibility boundaries; do not generate empty abstractions before their consuming milestone.

- Modify `Source/CommonAI/Public/CommonAIProfile.h` and `Private/CommonAIProfile.cpp`: general authored profile and validation.
- Add `Public/Definitions/CommonAITaskDefinition.h`, `CommonAIActionDefinition.h`, `CommonAIConsiderationDefinition.h`, `CommonAIServiceDefinition.h`: typed immutable definitions and curve bindings, with matching Private implementations where behavior is required.
- Add `Public/Execution/CommonAIExecution.h` and `Private/Execution/CommonAIExecution.cpp`: common execution instance/context and terminal-result contract.
- Add `Public/Evaluation/CommonAIEvaluator.h` and `Private/Evaluation/CommonAIEvaluator.cpp`: stateless evaluator and validated context/result types.
- Add `Public/Evaluation/CommonAIServiceBehavior.h` and its Private implementation: reusable native start/tick/stop extension surface with vendor-owned activation/timers.
- Modify `Public/CommonAIAgentComponent.h` and `Private/CommonAIAgentComponent.cpp`: neutral control/query API, snapshots and pending intent coordination.
- Modify `Private/Athena/CommonAIBackend.h/.cpp`; add internal task/action/service adapters under the same directory only when their feature is implemented.
- Adapt `Private/Activities/CommonAIActivities.h/.cpp` to the common execution contract, preserving Work action mechanics and current subobject identity where serialized.
- Extend `Public/CommonAITags.h` and `Private/CommonAITags.cpp` for actual shared IDs/reasons. Keep existing tag contributions and montage-window implementation intact.
- Add opt-in BT/ability bindings under `Public/Contracts` and `Private/Integration`; reuse existing interaction/movement contracts where sufficient.
- Add neutral diagnostics under `Public/Diagnostics` and `Private/Diagnostics`; concrete editor-only UI belongs in a separately justified CommonAIEditor module.
- Update `Content/Profiles/DA_Work`, its curves and new neutral reference profiles through guarded engine tooling.
- Tests live in the authorized host's developer-test module, in a neutral `CommonAIFeatureCoverageTests.cpp` and test-owned fixtures. Register their exact host-owned names in that host's validation catalog; never put host identifiers in this shared plan.

## M1 — Characterize vendor behavior and settle supported integration seams

- [ ] Extend the existing test harness with multi-consideration curves, tie priority, resistance, momentum, failed-task unlock, empty/conditional services, pause/resume and duplicate-class task identities. Run the vendor behavior through its normal initialization/scoring pipeline.
- [ ] Compare results to manual sections 2-7 and installed `AthenaAIDataManager`, `AthenaAITask`, `AthenaAIService` and `AthenaAIComponent` sources. Record exact observed behavior and source-backed seams.
- [ ] Verify evaluator composition, exact-tag command dispatch, BT stop acknowledgement and debug access without vendor exports/private-state bypasses.
- [ ] Resolve any material documented/observed conflict before the affected implementation. No copied scorer, vendor fork or silent approximation.

**Exit:** a bounded capability ledger: supported seam, tested behavior and acceptance case for every feature family. A mismatch is an explicit blocker/decision, not a pass.

## M2 — General definitions, inputs and authored curves

**Contracts:** `FCommonAIConsiderationSnapshot` exposes exact ID, raw/normalized values, range, validity and source. `FCommonAIEvaluationResult` distinguishes Value, Skipped and Unavailable. `UCommonAIProfile` owns task/consideration/service definitions and a typed fallback. Task entries bind exact consideration IDs to typed curve-table/row references.

- [ ] Add rejection cases first: duplicate/invalid profile-scoped IDs, missing curves, invalid range, nonfinite input, absent fallback and multiple uncoordinated writers. Accept repeated action-type tags while rejecting duplicate sequence occurrence IDs.
- [ ] Implement neutral definitions and backend projection, including multiple tasks of the same native behavior class. Preserve the vendor's per-agent instancing.
- [ ] Implement safe query and raw/range/batch update operations on the agent. Validate input before publishing; never mutate authored templates.
- [ ] Project exact task-consideration pairs into internal rows and test distinct curves for a shared consideration across tasks.
- [ ] Inspect current saved profile values, prepare a guarded migration and verify on disposable copies. Switch the real Work profile only after equivalent saved reload and behavior are established. Remove the obsolete Work-only schema within this migration milestone.

**Exit:** Work remains equivalent; a profile can author competing tasks and continuous inputs without editing backend code or hand-generating vendor row names.

## M3 — General lifecycle, actions and task policies

**Contracts:** execution contexts carry agent, activity/action ID, entry occurrence, execution identity and typed target/bindings. A terminal result is Completed, Failed or Cancelled plus a reason, mapped to vendor termination using the spec's table. `RequestActivity(ActivityId, TransitionMode, RetentionPolicy, UnlockPolicy)` returns a tracked command result; retention defaults to HoldUntilCompletion and locked-task requests default to rejection. `RequestCancellation(Reason)` remains idempotent. Profile switch modes are CancelAtSafePoint and AfterNaturalCompletion.

- [ ] Pin stale callback, reentrant completion, competing pending intent, same-class/different-tag dispatch and two-agent ownership with tests.
- [ ] Replace Work-specific backend casts with the generic execution seam. Retain vendor sequence order and deferred action completion.
- [ ] Map authored task policies to verified vendor fields; expose safe per-instance adjustments and snapshots.
- [ ] Assert both public terminal results and subsequent vendor lock/momentum outcomes for natural success, action/setup failure, utility replacement, explicit cancellation, Stop, profile replacement and emergency teardown.
- [ ] Add data-authored action sequences with stable action IDs. Keep current Work as a configured reusable sequence and retain owned movement/reservation cleanup.
- [ ] Implement single pending explicit intent, superseded outcomes, Stop precedence and profile compatibility checks. Re-evaluate utility after drain; revalidate explicit commands before execution.
- [ ] Verify command completion releases only its own scoring hold, preserves an earlier manual pause, and respects the chosen locked-task policy. Expose indefinitely pending natural-completion switches and support safe supersession.
- [ ] Deliver CommonAI start/progress/end notifications once, without exposing raw vendor delegates or permitting reentrant backend mutation.

**Exit:** task/settings/sequence/command parity through the common safe lifecycle; the existing six foundation checks still pass.

## M4 — Evaluators, services and scoring control

**Contracts:** evaluator input is a validated typed context; output is the M2 evaluation result. Service definitions have exact IDs, normalized comparison conditions, interval and a designated evaluator/input route. Pause services policy is explicit and separate from execution cancellation.

- [ ] Test each comparison operator, zero interval, empty conditions, pause/resume, invalid context, unavailable targets and batched condition updates.
- [ ] Add the CommonAI evaluator composition bridge using supported vendor value APIs. Keep evaluator definitions stateless and runtime context isolated.
- [ ] Project service conditions/timers to private vendor-derived services; use their lifecycle rather than adding a scheduler.
- [ ] Bridge reusable native service start/tick/stop behavior as well as evaluator-driven services. Verify coherent batches do not transiently start/stop a service from mixed old/new inputs.
- [ ] Expose event/manual/interval update choices, initialization defaults and explicit rescore. Avoid service observation of unintended partial batches.
- [ ] Apply explicit value retention/reset on profile change and invalidate old producer callbacks.

**Exit:** several numeric inputs and conditional services drive real vendor task selection with correct cleanup and explainable values.

## M5 — Control modes and opt-in integrations

- [ ] Complete possession, pause/resume, manual rescore and profile-change policies with the M3 lifecycle; no unsafe immediate force path enters the public API. Verify new-pawn input defaults/generation, rejection of old-pawn producers, original-pawn effect ownership while draining, and emergency loss cleanup.
- [ ] Implement BT execution as an opt-in binding with engine asset references, explicit brain ownership and safe stop acknowledgement. Test occupied-brain rejection, normal finish and latent abort.
- [ ] Reuse or extend the typed execution binding for external abilities. Test acceptance, payload identity, synchronous/async result, cancellation and effect-owner destruction with a neutral fixture. Do not create a mandatory GAS dependency or claim a concrete GAS adapter is verified without a GAS fixture.
- [ ] Verify concrete consumer effects remain thin bindings and no reusable decision/cleanup mechanism escapes into the game.

**Exit:** selected integrations are usable without consumer Athena references; unsupported integrations remain explicitly identified.

## M6 — Human-friendly authoring, inspection and diagnostics

- [ ] Provide readable profile sections, units, advanced settings, per-entry validation and safe generation of only missing curve rows. Regeneration must preserve existing keys/tangents and reject ambiguous IDs.
- [ ] Expose one neutral runtime snapshot with all task scores, curve contributions, values/ranges, service states, current action and transition reasons. Data must reflect actual vendor evaluation rather than a second calculation with different semantics.
- [ ] Connect optional agent/selected-agent panels and player controls through CommonAI data. Reuse vendor debug internals when compatible; do not require vendor-derived consumer widgets.
- [ ] Integrate Visual Logger, Gameplay Debugger and Insights, with controlled cadence and disabled-by-default expensive capture.
- [ ] Introduce CommonAIEditor only for a demonstrated editor API/UI requirement. Validate new persistence tooling on disposable assets before saved production assets.

**Exit:** a human can configure and diagnose the multi-task reference profile from the profile/agent entry points without backend knowledge.

## M7 — Complete coverage and independent acceptance

- [ ] Check every feature-matrix row against actual evidence; retain blocked/untested rows honestly.
- [ ] Run a meaningful grouped regression for generalized lifecycle, numeric scoring, service/evaluator ownership, commands, authoring preservation and the original Work foundation. Reuse unchanged checks rather than rerunning per observation.
- [ ] Use the host's Plan/Affected workflow and exact ownership catalog. A scope-budget stop stays a stop; no automatic Full run.
- [ ] Verify saved assets independently and inspect hard/soft dependencies, public headers and module direction.
- [ ] Build and execute a clean independent consumer and packaged multi-task scenario using only CommonAI public APIs. Include a service/evaluator-driven transition and delayed cancellation, not only WorkAvailable.
- [ ] Update neutral status/decisions, retain supporting evidence and clean task-owned disposable exports. Leave changes uncommitted.

**Exit:** feature coverage, usability and portability are evidenced separately. Passing a subset must not be described as complete Athena coverage.
