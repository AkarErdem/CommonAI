# CommonAI foundation design and implementation plan

Status: approved foundation implemented. Six focused checks, saved-asset verification and an independent packaged smoke run passed. Host-wide Affected validation remains scope-blocked; see STATUS.md.

**Goal:** Provide one consumer-facing CommonAI API for tag-driven Work and safe deferred cancellation, retaining Athena scoring and sequence execution internally.

**Architecture:** One CommonAI Runtime module. Public components, profiles and interaction contracts expose engine/CommonAI types only. Athena-derived implementation classes and backend configuration stay internal; no second scorer or execution pipeline.

**Tech stack:** Unreal Engine 5.8, installed AthenaAI 2.6, C++, Gameplay Tags, original plugin assets; initial validation on Win64, single-player.

**Spec:** The design sections below are the specification for this plan.

## Global constraints

- Preserve the existing controller and the authoritative movement/interaction systems.
- Consumer code uses CommonAI APIs without Athena includes, types or direct module dependencies.
- No consumer identifiers, paths, assets or private evidence in this plugin.
- Keep vendor code unchanged. Verify supported lifecycle hooks before implementation.
- All code and persistent technical text is English.
- No commits, publishing, source-control conversion or production controller integration is authorized by this plan.
- Existing instructions permitting public Athena type exposure must be reconciled with the stricter approved boundary during implementation.

## Design: tags and authoring

Semantic tags use meaningful children, for example CommonAI.Activity.Work.Farming and CommonAI.State.Busy.Working. Hierarchical HasTag/query matching makes any matching child satisfy a parent query. Exact matching is used for identities and removal; removing one leaf must not remove its siblings.

A tag container alone does not retain ownership or counts for repeated additions of the same exact tag. The component therefore aggregates source-owned contributions into a read-only effective tag container. Sources release only their own contributions. Do not provide an unrestricted remove-from-all-sources gameplay API.

Artists select ordinary Gameplay Tags in profiles and a CommonAI montage notify-state. The notify-state adds its selected tags for the playback interval and removes only that interval's contribution. Internal identity distinguishes overlapping playback, repeated notify occurrences and interruptions; mutable playback state must not live on the shared notify object. Artists do not manage opaque IDs.

Blueprint integrations use source-bound tag contributions; advanced async producers retain a contribution token internally. Existing authoritative external tag systems are read through a provider where required rather than mirrored as independently mutable truth. Arbitrary external tag containers are not automatically discoverable.

The agent component exposes effective tags and their source/activity/reason for inspection. Reasons remain semantic tags; execution/contribution identity remains typed data. Generic defaults are registered by the plugin without consumer config copying. Specialized activity tags are data extensions, not separate algorithms.

## Design: interaction

Use an actor/component-capable CommonAI interaction interface and a reusable target component implementing it. The component owns generic reservation and completion bookkeeping; a consumer supplies only concrete effects when needed.

- Query returns activity/capability tags, eligibility and approach information without mutation.
- Reserve rechecks eligibility and returns a typed execution-owned reservation.
- Begin accepts the reservation and returns rejected, running, or a terminal result.
- RequestCancel accepts a reason tag and requests a safe end; repeated requests are idempotent.
- Completion identifies the execution and reports exactly one Completed, Failed or Cancelled result.
- Release affects only the matching reservation and contributions.

Keep discovery separate from target execution. Reuse the current registered-provider approach initially; do not introduce global actor scans or a general world registry. Supply timed and externally signaled target variants. External completion must identify its original execution.

## Design: cancellation and transitions

Visible phases: Idle, Approaching, Interacting, Cancelling. Terminal outcome is recorded separately. Cancellation is accepted and can complete immediately, at a safe animation/event boundary, or at natural completion. Once cancellation is accepted, reaching a safe terminal point yields Cancelled, not a normal activity reward.

During Cancelling, keep the execution record, reservation and required state contributions alive. Do not start a conflicting replacement operation. Normal utility changes use supported vendor locking/gating; do not add another selector. Disable/preset changes must respect this drain protocol rather than destroy the pending record. A pending replacement is re-evaluated after drain; do not blindly execute stale intent.

A watchdog reports an overdue cancellation but does not falsely release resources while effects continue. Force abort is allowed only through a target-supported safe abort operation. Destruction/world teardown is a distinct emergency cleanup path: invalidate execution, detach callbacks and release surviving owned resources without promising a final animation. Failures during cancellation remain failures with an explanatory reason.

The movement default uses ordinary engine path following and request-owned cancellation. A narrow replacement binding is available for an existing movement system. Single-player does not remove the need for one active owner of a movement request.

## Design: one inspection point

CommonAIAgentComponent is the normal setup and diagnostic entry point. Show assigned profile, active activity, phase, target, effective tags with sources, last outcome/reason, and pending cancellation duration/condition as available from the target. Start with reflected inspection and Blueprint-accessible snapshots; a custom editor/dashboard is outside scope. Internal backend objects must not be required in the consumer's ordinary workflow.

## Proposed file responsibilities

- Public/CommonAIAgentComponent.h and matching Private implementation: public lifecycle, profile binding, tag contributions and diagnostic snapshot; backend coordination remains private.
- Public/CommonAIProfile.h and matching Private implementation: neutral authored configuration and supported Work variation; vendor templates are internal.
- Public/CommonAITags.h and matching Private implementation: minimal native tag dictionary.
- Public/Contracts/CommonAIInteraction.h: typed request, execution/reservation identity, results and target interface.
- Public/Animation/CommonAIAnimNotifyState_Tags.h and matching Private implementation: artist-facing timed contributions, with playback state stored per runtime instance outside the notify asset.
- Existing Contracts/CommonAIWorkSite and CommonAIWorkProviderComponent: adapt reservation/target/provider implementation rather than duplicate it.
- Existing Activities/CommonAIActivities: retain sequence/action behavior, move vendor-dependent declarations behind the public boundary and connect deferred cancellation.
- CommonAI.Build.cs: private vendor dependency after all public exposure is removed.
- Content: original neutral profile, internal preset and curve assets; exact asset layout follows verified authoring support.
- Neutral tests in the existing authorized developer-test environment; independent minimal-host verification at acceptance. No testing dependency in Runtime.

## Implementation milestone

- [x] 1. Verify composition construction/registration, preset materialization, task locking and stop/cleanup ordering against installed source. Resolve cancellation-drain feasibility before changing runtime code. Check montage interruption/blending/overlapping notify lifecycle in installed engine source. Report material unsupported requirements before expanding scope.
- [x] 2. Implement the public component/profile boundary and internal Athena bridge. Reuse vendor task instancing and scoring. A minimal consumer compiles using only CommonAI public types.
- [x] 3. Implement source-owned tag aggregation and artist-facing notify-state. Preserve hierarchical queries; verify each exact contribution is removed independently, including overlapping montage windows and interruption.
- [x] 4. Adapt existing Work sites/provider/actions to the interface and execution-aware completion contract. Keep timed and signaled examples; preserve per-run reset, deferred action completion and request-owned cleanup.
- [x] 5. Integrate safe cancellation and replacement gating. Preserve the retiring execution until safe completion, including explicit stop/preset change. Add diagnostic snapshots to the same agent component.
- [x] 6. Author the neutral Work profile, eligibility observation/scoring inputs, curves and interruptible fallback. Use normal Athena selection; no forced-start substitute for scoring evidence. Register plugin tags and ensure original content is cooked through typed references.
- [x] 7. Run one relevant build and focused behavioral batch at the milestone boundary, respecting the host validation budget. Regenerate IDE files after native file changes. Verify saved assets with the applicable asset workflow and reuse unchanged evidence.
- [x] 8. Validate a clean independent minimal host and packaged content before claiming portability. Maintain neutral status/decisions and report incomplete checks honestly. Keep raw host evidence outside the plugin; clean task-owned disposable outputs after successful verification.

## Review focus and acceptance tests

- Same exact tag from two sources: removing either leaves the other active; Busy parent query remains true. Removing a sibling does not change other children.
- Repeated/overlapping montage notify occurrences: no shared notify state, no premature removal, no leak after interrupt, stop, blend-out, actor destruction or replay under the documented interval semantics.
- Safe cancellation while interacting: request accepted, reservation retained, no conflicting new work, terminal cancellation once, cleanup only after acknowledgement. Natural completion after cancellation does not become normal success.
- Duplicate/stale signals and synchronous callbacks: old execution cannot complete a new one; no sequence reentrancy regression.
- Disable, preset replacement and target/agent destruction: drain when possible; explicit emergency teardown when waiting is impossible; no dangling owned reservation or callback.
- Two agents and one target: exclusive reservation, losing agent falls back, subsequent availability resumes normal scoring without a failure loop.
- Movement failure and target loss: never report successful Work; abort only the owned movement request.
- Independent host: no consumer-specific dependencies, no direct consumer Athena usage, required tags/profile/curves load in a packaged run.

The plan is complete only when each acceptance result is backed by current evidence; historical compilation alone does not establish runtime safety.


