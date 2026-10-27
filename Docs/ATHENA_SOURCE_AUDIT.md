# AthenaAI Native Extension Audit

## Purpose and evidence status

Establish the supported C++ extension surface before implementing CommonAI. CommonAI must add reusable behavior and lifecycle guarantees through supported inheritance and composition, while leaving the installed vendor plugin unchanged. Consuming projects supply their own gameplay implementations through common contracts.

This document is an audit specification, not a claim that the installed plugin has been inspected or compiled. The canonical behavioral source of truth is the local **AthenaAI Manual**, at `Docs/AthenaAI_Manual/Athena_AI_Manual.md` relative to CommonAI, cited below as **M** with section numbers. Preserve its adjacent media directory. The facts below were extracted from the supplied manual; recheck the relevant sections of the local copy for each task.

Installed source and build results establish exact native APIs, exports and actual executable behavior. If the manual and source materially disagree, record both pieces of evidence and ask before implementing a conflicting behavior; do not silently dismiss the manual. Record actual Unreal/plugin versions and module names independently of the versionless manual filename. The established CommonAI runtime module name is `CommonAI`.

Do not copy vendor implementation bodies, proprietary assets, screenshots or manual contents into other CommonAI files or distribution bundles. Keep the canonical manual directory intact; its tracking and distribution require separate owner decisions. Keep the vendor source separately installed. Record concise findings with manual sections and vendor-relative paths/symbols instead of raw host paths.

## Manual-confirmed facts that affect the design

1. **Native use is supported, but individual overrides remain unverified.** The manual describes a C++ implementation and C++ use. Its v1.1.3 changelog reports that many Component, Data Manager and Service members became protected or virtual; this does not establish that every relevant member is overridable. The v2.3 changelog specifically identifies the Behavior Tree Task's `StopBT()` as protected and overridable. [M §10; Appendix, Changelog]

2. **The Component creates the Data Manager.** Declaring a derived manager does not make the plugin instantiate it. The construction path and injection mechanism are a blocking audit item. Component setup requires an AIController-derived owner, a Pawn and a valid Preset. [M §2.1–2.2; §5.1]

3. **Preset and Curve Table are separate assets.** The Agent Preset is a Data Asset containing tasks, services, consideration configuration and a Curve Table reference. Tasks and services can have different settings in different presets. Presets are configuration; they must not become mutable per-agent state. [M §3.1–3.2]

4. **Task completion is not the only rescoring trigger.** Consideration updates, an optional scoring interval and explicit forced scoring can trigger evaluation. A running interruptible Task can be replaced. `FinishAction`, however, is required for an Action Sequence to advance. Long-running tasks still need an intentional completion/cancellation policy. [M §2.2–2.3; §5.2; §6.4; §6.9.1]

5. **Task controls have distinct meanings.** `Priority` breaks equal-score ties; `Score Modifier` changes weighting; `Replace Resistance` resists replacement; `Momentum` discourages consecutive repeats. The exact aggregation formula and modifier order require source inspection. Do not translate general role weights directly into `Priority`. [M §6.1.1; §6.6]

6. **Evaluators calculate raw values.** `ShouldEvaluate` gates `CalculateRawValue`; normalization follows evaluation. Evaluators are intended to be lightweight and not store gameplay state. The manual names `FBaseEvaluatorContext` and several derived contexts, and supports custom C++ context structs. Services can schedule evaluator calls. [M §5.2; §5.2.1–5.2.3]

7. **Sequence Tasks already exist.** They execute instanced atomic Actions in order. Behavior Tree Tasks are also documented. A built-in StateTree bridge is not established by this manual; treat any such bridge as separate integration work. [M §6.8–6.9]

8. **Runtime instances can survive multiple runs.** Task and Action variables and bindings persist unless reset. Their cleanup must therefore handle reuse, interruption and late callbacks. Preset changes can destroy these instances. [M §6.9.1; Appendix, Notes on usage]

9. **Plugin termination is not business completion.** The documented `Successful` result can accompany preset replacement or system disable. It must not automatically grant work output, rewards or interaction effects. `ForceTaskStart` can override an uninterruptible Task; scoring pause alone is not execution cancellation. [M §2.3–2.6]

10. **Gameplay Tag naming needs verification.** The Appendix uses `AthenaAI.Tasks.*`, `AthenaAI.Actions.*` and `AthenaAI.Considerations.*`. Some §3.4 examples use singular names, while the documented 15/24-character shorteners match the plural forms. Inspect actual filters and curve naming. Duplicate Task tags are ignored after the first accepted entry. [M §3.4; §6.1.1; Appendix, Conventions]

## Required source audit

For every row, record the source-relative header/implementation location, exact symbols, relevant declaration and the conclusion. Summarize declarations rather than copying implementation bodies. Use **Verified**, **Unsupported** or **Unresolved**, with evidence. A Blueprint display name is not proof of the C++ signature.

| Area | Inspect and record | Required conclusion |
|---|---|---|
| Plugin and modules | Descriptor, runtime/editor module types, Build.cs dependencies, supported targets | CommonAI can depend on the real exported runtime module without editor-only leakage. |
| Class visibility | Native class name, header path, export macro, MinimalAPI/export details, inheritance restrictions | Derived classes can compile and link from a separate module. |
| Component lifecycle | Constructor, registration, initialization, possession, enable/disable and teardown hooks | Identify safe overrides, mandatory base calls and ordering. |
| Data Manager construction | Every allocation and replacement site, configurable class property or virtual factory, owning UObject | A supported path constructs the derived manager, or this capability is explicitly unsupported. |
| Task hierarchy | Base, sequence and Behavior Tree classes; their inheritance and lifecycle declarations | Identify which common subclasses can inherit each behavior without losing vendor features. |
| Native events | Virtual functions, BlueprintNativeEvent/ImplementableEvent declarations, implementation entry points | Record exact callable/overridable signatures; do not invent `_Implementation` methods. |
| Runtime instancing | Preset templates, duplication, instanced properties, outer, references and destruction | Two agents do not share mutable task, action or service runtime state. |
| Task termination | Start failure, replacement, abort, finish, disable and preset-change ordering | Map vendor results to separate common execution outcomes without false success. |
| Actions | Start/stop/finish, sequence advancement, abort propagation, timers and async callbacks | Completion advances once; cancellation cannot revive an old run. |
| Services | Conditions, normalization, start/stop/tick, scoring pause and cleanup | Define when observations run and how stopped services reject delayed updates. |
| Evaluators | Context typing, raw-value return, failed ShouldEvaluate behavior, default ranges | Invalid/missing data cannot silently retain unsafe eligibility. |
| Scoring | Aggregation, zero values, score modifiers, momentum, replacement resistance, tie handling | Establish actual selection semantics and batching behavior. |
| Presets and curves | Custom property instancing, curve row generation, tag filters, naming shorteners | Native common types and authored assets remain consistent after reload. |
| Execution ownership | Controller brain, navigation requests, Behavior Tree replacement, authority checks | Integrations have one active owner for movement and each gameplay action. |

Also inspect call reentrancy: a consideration update can lead to scoring and termination while another callback is running. Determine whether changes are immediate or deferred and which code owns safe transition sequencing. Do not assume UObject operations can be moved to worker threads.

## Provisional common subclass families

The following names are **proposals**, not existing vendor API:

- `UCommonAIComponent`: supported initialization integration and common access points.
- `UCommonAIDataManager`: manager extensions only if replacement is proven.
- `UCommonAITask`: common ordinary-task execution and cleanup policy.
- `UCommonAISequenceTask`: supported sequence integration and common policy.
- `UCommonAIAction`: reusable atomic action lifecycle.
- `UCommonAIService`: observation scheduling and update conventions.
- `UCommonAIEvaluator`: common context/provider access and calculation conventions.
- `UCommonAIPreset`: validated common configuration, where inheritance is useful.

Resolve every native parent and override signature during the audit. These families are not a requirement to create empty wrappers for every vendor type.

A vendor sequence Task may already derive from the vendor ordinary Task. Making it also inherit a separate common UObject-derived Task base is not a safe way to share behavior. Keep the supported single UObject inheritance chain and share cleanup/execution policy through composition or a non-UObject helper where appropriate. Preserve sequence behavior rather than recreating it merely to force a uniform hierarchy.

## Data Manager replacement gate

Proceed with a derived manager only after all of these are demonstrated:

1. A supported construction or configuration hook selects its class.
2. The real runtime Component returns an instance of that derived class.
3. The correct manager receives initialization, scoring and shutdown calls.
4. Preset switches and Pawn lifecycle changes preserve the selected construction path.
5. No inaccessible vendor internals or copied vendor implementation are required.

If this gate fails, report the exact missing hook. Keep the vendor manager and implement the needed common policy through public APIs and composition where sufficient. Mark manager inheritance **Unsupported pending vendor extension**; do not rename a wrapper and claim inheritance succeeded. If the desired behavior requires inaccessible scoring internals, isolate that limitation and propose the smallest vendor extension request. Do not silently modify the dependency.

## Runtime proof and acceptance scenarios

Separate source investigation from implementation proof. `Prompts/01_AUDIT_AND_PLAN.txt` is read-only and marks unexecuted checks pending. A source audit does not authorize code or tracking changes; implementation needs its own user request and evidence. Do not describe runtime construction or lifecycle behavior as verified before its checks actually run.

Use neutral test fixtures in the authorized current host; it can be an existing game. Verify actual object classes at runtime, not just declarations. CommonAI must compile as its own module and run after a fresh editor restart. Independent minimal-host portability is a pre-release check, not a prerequisite to begin.

Meaningful scenarios are:

- Two agents use the same Preset while maintaining independent runtime state.
- A Task completes, is replaced, aborts, fails to start and is disabled; each path cleans up exactly once and records the correct common outcome.
- Reusing an Action resets per-run state. A delayed navigation/interaction callback from its previous run cannot finish or alter its next run.
- An interrupted sequence releases reservations, cancels owned requests and does not advance another Action afterward.
- Batched consideration updates cause the intended number of scoring passes; pause/resume does not accidentally suspend or restart unrelated execution.
- Preset switching and Pawn teardown leave no live delegate, reservation, timer or movement request owned by the old run.
- An unavailable opportunity leads to an interruptible fallback and a bounded retry policy instead of repeated immediate aborts.
- Duplicate tags, invalid evaluator contexts, missing curves and unintended external asset references produce useful validation errors.
- A packaged development build resolves the common assets and native classes; editor success alone is insufficient.

During the read-only phase, return results in the conversation and recommend **Inheritance feasible from source**, **Composition fallback required**, or **Blocked on a specific source/API question**. During the authorized implementation phase, record results in `Docs/ATHENA_SOURCE_AUDIT_RESULTS.md` with the verified version tuple, capability table, runtime evidence and remaining limitations. Final integration status is **Verified inheritance**, **Verified composition fallback**, or **Blocked on a specific vendor extension**. Do not describe runtime construction or lifecycle behavior as verified before its checks actually run.
