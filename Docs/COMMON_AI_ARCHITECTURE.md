# CommonAI — Proposed Architecture

## 1. Decision status and scope

This document recommends a reusable C++-first Unreal NPC behavior plugin that extends AthenaAI. It is an architecture proposal to validate against the installed vendor source. Nothing here asserts an existing native API or completed implementation.

The required separation is between **vendor**, **shared framework**, and **consumer integration**. It is not a requirement to build three different AI decision systems.

Apply `LAYER_PLACEMENT_POLICY.md` before choosing ownership. Prefer reusable NPC mechanisms in CommonAI, including needs, schedules, roles and activities, even on their first use. Split concrete world connections through shared contracts. Ask the user before materially ambiguous placement or public-API decisions; do not silently put uncertainty into the game layer.

The canonical Athena behavioral reference is `Docs/AthenaAI_Manual/Athena_AI_Manual.md` relative to the plugin. Consult its relevant sections and verify exact native APIs against installed source. Surface material disagreements for a decision instead of silently replacing either source.

| Layer | Owns | Depends on |
|---|---|---|
| Vendor AthenaAI | Utility scoring, vendor tasks/actions/services/evaluators, vendor presets and lifecycle | Unreal and its declared dependencies |
| Shared CommonAI | Original native extensions, reusable needs/routines/roles/activities, small integration contracts, neutral assets, diagnostics and validation | Unreal and the installed AthenaAI dependency |
| Consumer integration | Game-specific state/rules, concrete world/execution bindings, adapters to existing authoritative systems, and project content | CommonAI and that application's systems |

Dependencies point from consumers into CommonAI and from CommonAI into AthenaAI. CommonAI must not include a consumer module, cast to a consumer Pawn, load a consumer asset or select behavior through a consumer name.

The third layer need not subclass every shared class. It can implement interfaces, provide data and compose components. Keep these consumer bindings thin: reusable observation, selection policy, activity progression and lifecycle remain in CommonAI, while the consumer supplies authoritative facts and concrete effects. Native inheritance establishes the vendor extension seam; composition keeps actual game behavior replaceable.

## 2. Start with one plugin and one Runtime module

Current plugin and Runtime module name: **CommonAI**, with content enabled. Keep `CommonAI.uplugin`, `Source/CommonAI/CommonAI.Build.cs`, the ModuleRules class, module entry point and `COMMONAI_API` exports consistent. Module naming is established; native behavior designs below still require source verification.

Organize responsibilities internally:

| Source area | Responsibilities |
|---|---|
| Contracts | Context snapshots, activity requests/results, execution and opportunity interfaces |
| Model | Need definitions, role preferences and routine/profile data |
| Athena | Vendor-derived component, preset, task, action, service and evaluator classes |
| Activities | Shared activity sequences and native movement/wait/interaction adapters |
| Diagnostics | Neutral reason codes and per-execution traces |

Add CommonAIEditor when a validator or importer/generator actually needs Editor APIs. Runtime must not depend on the Editor module.

A separate Core plus Athena module split can follow when a concrete backend-free consumer requires it. For now, both layers of runtime code can live in one module with clear source boundaries. Do not add a parallel utility scorer, generalized planner or plugin replacement abstraction merely to obtain more modules.

Read the actual vendor plugin and module names before declaring dependencies. A public header that exposes an Athena base class also exposes a module dependency; the adapter is not vendor-independent. See ASSET_AND_RELEASE_POLICY.md and official sources S1/S2.

## 3. Native inheritance strategy

These are proposed extension roles, not verified vendor class names or signatures:

| Shared extension | Purpose |
|---|---|
| Common component | Standard initialization, provider binding, decision gating, shared diagnostics |
| Common preset class | Additional neutral metadata and validation while retaining vendor configuration |
| Common task base | Per-run state, truthful completion and cleanup policy |
| Common sequence task | Reusable sequential activities using the vendor sequence lifecycle |
| Common action base | Small native steps with owned async requests and cancellation |
| Common service base | Controlled observation refresh and batching |
| Common evaluator base | Stateless raw-value calculation using a neutral context |
| Common data manager subclass | Only the extensions that can actually be installed through verified vendor construction hooks |

Do not create an unused manager subclass and describe it as an integrated replacement. The first source audit must identify the Component's construction path and establish source-level feasibility. The first implementation must then prove the instantiated runtime type. If no supported substitution point exists, record the limitation and use supported composition/delegation for feasible features. A source fork is a separate architectural decision, not an invisible fallback.

Sequence task inheritance also needs care: the vendor sequence type already has its task ancestry. Do not attempt multiple UObject inheritance to combine a common task base with a vendor sequence base. Share cleanup/execution policy through composition or a non-UObject helper while using the correct single native superclass.

Only derive a class when its shared responsibility is clear. A vendor debug widget need not acquire a redundant wrapper merely to complete a mirror class hierarchy.

## 4. Three small integration contracts

### 4.1 Context source: what is currently true?

A context source supplies a read-only observation snapshot containing the facts an activity needs:

- Stable agent identity.
- Simulation clock or time-of-day, when supported.
- Supported need values and ranges.
- Role/profile reference and activity preferences.
- Capability and availability facts.

Use typed records for lifecycle-critical data. Extensible tags can identify needs and activities without turning every parameter into an unvalidated string-to-value dictionary.

Unavailable data must be distinguishable from a numeric zero. An agent with no food requirement is not equivalent to a hungry agent whose food value was accidentally omitted.

The raw NPC state belongs to the application's authoritative state model, or to an explicitly selected reusable component. Athena's consideration cache is a projection used for decisions. It is not the sole SaveGame, inventory, need simulation or world persistence authority.

A simple consumer may use a CommonAI needs component. Another may expose existing state through a provider. Only one system updates each actual need; avoid draining fatigue in both places.

### 4.2 Opportunity provider: where can the activity happen?

The provider finds opportunities by activity/capability tags and attempts to acquire a reservation. It also releases an owned reservation and reports invalidation.

Finding a candidate does not guarantee that it can still be reserved. Two agents can observe the same free station. The acquisition operation, performed by the authoritative world provider, decides which agent owns it.

Keep the token opaque to the generic sequence. A token must identify its owner and execution sufficiently to prevent one run from releasing another run's claim.

The first host can implement a small station component. A Smart Objects adapter can implement the same contract when a consumer needs it. Smart Objects should not become a mandatory dependency before this need is established.

### 4.3 Activity executor: how does the application perform it?

The executor accepts a neutral request such as Work on a reserved opportunity. It returns a handle and reports:

- Request accepted or rejected.
- Running.
- Actual activity success or failure.
- Cancellation requested and cancellation completed.

These describe distinct lifecycle events. They are not necessarily a large public state machine; a small handle/result contract is sufficient.

The game decides what Work produces, which animation plays, how inventory changes, whether an interaction may stop immediately and how presentation is replicated. CommonAI does not infer those rules from a role name.

The shared plugin can provide a normal AI MoveTo adapter for the neutral host. A consumer with existing movement authority can supply its own execution adapter without replacing its controller class.

## 5. One decision owner, explicit execution authority

Athena selects the next high-level activity among eligible candidates. An executor performs the selected activity. An opportunity provider manages the world resource. These responsibilities must not issue conflicting movement or action commands.

Within one controlled resource, such as an agent's locomotion, there must be one active owner. A task becoming the highest-scoring candidate does not authorize it to override another system's committed action.

Ordinary switching, player orders, scripted sequences and emergency behavior must pass through an explicit policy. ForceTaskStart is a vendor escape hatch; the manual says it can replace even an uninterruptible task. It should not become the unguarded general command API.

Scoring pause is separate from execution cancellation. Pausing decisions must not be assumed to stop an already-running movement, service or interaction.

If cancellation must wait for a safe execution boundary, the integration bridge retains ownership until that boundary is reached. A replacement task cannot start conflicting work during this interval. Keep the retiring execution record alive outside the replaced task if necessary; do not leave reservation cleanup dependent on an object that may be destroyed.

## 6. Shared activities and daily life

Prefer a small reusable sequence:

**Find opportunity → Reserve → Move → Perform → Release**

These are new shared actions to author. The manual documents Athena's sequence mechanism, not ready-made implementations of all these steps.

The release step is useful for the normal path, but it is not enough for cancellation. Termination cleanup must release every owned resource even when the sequence never reaches its final action.

Initial activities:

| Activity | Shared mechanism | Consumer-provided meaning |
|---|---|---|
| Work | Opportunity query, reservation, approach, bounded execution | Actual production, payment, station rules and animations |
| Rest | Rest availability, duration and completion protocol | Bed/chair rules, restored resources and presentation |
| Eat, if required | Food eligibility and interaction request | Inventory, consumption, nutrition and food assets |
| Wander/Wait | Neutral navigation or waiting | Allowed areas, preferred locations and presentation |
| Patrol, later | Route progress and interruption | Route sources, authority, threats and local rules |

Role similarity is evidence for reuse, not proof that game rules are identical. A reusable Worker profile can select Work/Rest activities. A guard's authority, hostility or punishment rules belong to a consumer unless a genuinely shared requirement emerges.

### Dynamic routine selection

Model the daily cycle through needs, time preferences, role affinity, opportunity availability and current commitments. Do not hardcode a universal timetable.

Time-of-day can influence a curve. An optional required shift is an explicit consumer policy. Hunger, fatigue or danger can change activity choice when the policy permits it.

Athena's existing Score Modifier, Replace Resistance, Momentum and task locking should be tested before introducing duplicate mechanisms. According to the manual, Priority breaks equal-score ties; it is not the general importance weight for a routine.

Do not pre-approve a mathematical score formula from a diagram. Audit normalization, curve handling, zero values, modifiers and replacement semantics in the installed source.

Use a single time authority. Decide whether activity durations use simulation time or real time, and validate pause/time acceleration behavior. The shared framework should not silently introduce its own world clock.

## 7. Assets are part of the reusable API

Shared Content may contain original neutral presets, consideration curves, role/profile definitions and generic demonstration assets. It must not depend on a consumer's Content directory.

Prefer references from the consumer into shared content. A shared preset cannot point back to a consuming game's Pawn, animation, item database, faction or map. Soft paths and string lookups still count as dependencies and can disclose asset names.

Do not assume an ordinary Data Asset instance can inherit another instance's values. Epic distinguishes class inheritance from Data Asset instances. A copied preset is a separate asset and stops automatically receiving edits to the original.

Use the smallest suitable configuration model:

1. Directly reuse an unchanged shared asset.
2. Use consumer-owned bindings for world objects, execution and presentation.
3. If values must differ, reference a shared profile plus a small explicit override record.

The proposed composition order is shared defaults, role configuration, then consumer overrides. It requires implementation and validation; it is not an existing Athena feature. Avoid deep inheritance or arbitrary recursive merge rules. Lists of tasks should merge by stable IDs/tags if merging is needed, not by array index.

If Athena requires a complete native preset, an audited initialization adapter or Editor resolver can produce that preset from the composition. It must never mutate a loaded shared asset or CDO per NPC. Verify how vendor task/action templates are instanced and copied before choosing the mechanism.

## 8. C++-first authoring and automation

Gameplay decisions, evaluators, services, actions, lifecycle and integration logic should be native C++. Data Assets and Curve Tables remain appropriate authoring surfaces. A data-only Blueprint, if supported and useful, is configuration rather than Blueprint gameplay logic.

Start with a few correctly authored assets and prove native execution. Then add a C++ Editor import/generation tool if text-driven authoring materially reduces repeated work.

For generated asset families, designate one source of truth: for example a versioned JSON recipe. Generated .uasset files are Unreal-serialized outputs produced by the Editor, not files a text model edits directly. Commit usable generated assets with their source recipes and validate semantic correspondence. Do not demand byte-identical packages unless the serializer/toolchain has demonstrated that property.

Do not maintain manually edited recipe data and independently edited generated asset values as equal authorities.

Use a small explicit schema and stable identifiers. Schema changes need a migration note and a regenerated reference report. A general-purpose content language is outside the initial scope.

## 9. Lifecycle guarantees supplied by CommonAI

The shared layer must establish these guarantees:

1. Each run receives a distinct execution handle/generation.
2. Reused task/action instances reset transient state at start.
3. Completion is delivered once to the current run.
4. Old async callbacks cannot finish or mutate a newer run.
5. Cleanup is idempotent and cancels only owned movement, timers and delegate bindings.
6. Reservations are released on normal completion, failure, replacement and destruction, subject to any explicit transfer of ownership.
7. An accepted non-interruptible execution is not silently discarded by utility reevaluation.
8. A vendor task result is not automatically a domain success.
9. Mutable per-agent data never resides in a shared definition asset.
10. Fallback remains interruptible and avoids immediate completion/rescoring loops.

These are proposed framework requirements, not guarantees already provided by Athena.

## 10. Performance and future adapters

Batch consideration changes and choose one deliberate scoring cadence per agent; avoid combining every-value-change scoring with unnecessary periodic scoring. Refresh expensive observations conditionally and stagger work when the source API permits it.

Record real measurements in the neutral host and consumer integration. A demo's agent count does not establish the budget for different navigation, animation and world-query workloads.

StateTree is an optional structured executor adapter, not a second competing routine selector. The supplied Athena manual documents a Behavior Tree bridge and Action Sequences; a StateTree bridge would be new work.

Mass Entity and unloaded-world simulation remain later work. The documented Athena runtime expects an AIController and Pawn; its behavior does not automatically transfer to controller-free entities. Stable identity, external authoritative state and small execution contracts prepare for that work without pretending it is implemented.

For networked consumers, keep decision and gameplay authority aligned with the server and route presentation through the consumer's replication systems. A server-side task does not automatically replicate its visual effects.

## 11. What is intentionally deferred

- A vendor-independent replacement for Athena.
- A universal combat, economy, inventory or faction framework.
- Deep role inheritance and unrestricted runtime preset merging.
- A generic event bus or dependency injection framework.
- Full distant simulation and automatic Actor/Mass conversion.
- A global Asset Manager replacement.
- A compulsory new AIController or Character base class.

Revisit a deferred item when a real consumer requirement and a measurable acceptance case justify it.
