# CommonAI

A reusable C++ NPC behavior plugin above AthenaAI. Prefer shared NPC mechanisms in CommonAI; keep concrete application rules and bindings in the consuming project. The dependency direction is **Game -> CommonAI -> AthenaAI**.

## Start here

1. Read `AGENTS.md`, `Docs/STATUS.md` and `Docs/DECISIONS.md` before continuing development. General profiles, execution and diagnostics are implemented; STATUS records verification and limits.
2. Keep the manual at `Docs/AthenaAI_Manual/Athena_AI_Manual.md`, with `media/` beside it. Check its existing index and ignore state before proposing any tracking change. Tracking, committing and distribution require separate owner decisions.
3. To install the plugin and run a first role, follow [Set up your first NPC role](#set-up-your-first-npc-role). For agent instruction setup, see [Host integration and instruction bridge](Docs/HOST_INTEGRATION.md).

## Files that drive the workflow

| File | Purpose |
|---|---|
| AGENTS.md | Persistent CommonAI rules |
| Docs/HOST_AGENTS_BRIDGE.md | Neutral block to merge into the host's effective instruction file |
| Docs/HOST_INTEGRATION.md | Safe bridge installation and cross-repository workflow |
| Docs/LAYER_PLACEMENT_POLICY.md | Shared-first placement procedure and when to ask |
| Docs/DECISIONS.md | Accepted neutral design decisions |
| Docs/STATUS.md | Verified progress, blockers and next step |
| Docs/ATHENA_SOURCE_AUDIT.md | Native API and lifecycle audit requirements |
| Docs/COMMON_AI_ARCHITECTURE.md | Technical architecture and lifecycle contracts |
| Docs/ASSET_AND_RELEASE_POLICY.md | Content references, local references and releases |
| Docs/ROADMAP.md | Ordered milestones and acceptance criteria |
| Docs/GENERAL_FOUNDATION_DESIGN.md | General-foundation architecture |
| Docs/GENERAL_FOUNDATION_IMPLEMENTATION_PLAN.md | Feature coverage and implementation milestones |
| Docs/SOURCES.md | Local reference policy and external primary sources |

`Prompts/01_AUDIT_AND_PLAN.txt` remains an optional read-only audit entry point. Historical bootstrap and first-slice task prompts were removed after their work was recorded in the audit results, roadmap and status.

## Local requirements

- An authorized Unreal C++ host and its applicable instructions.
- The installed AthenaAI plugin, including source needed to verify native extension hooks.
- The manual at the exact path above. It may come from an authorized checkout or be provisioned separately; do not assume that a public release includes it.
- The actual engine/toolchain and required vendor dependencies.

The runtime module name is **CommonAI**, matching `Source/CommonAI/CommonAI.Build.cs`. Do not rename it based on older architecture drafts. The host can be an existing game; a separate neutral host is a portability check before release, not an initial setup requirement.

## What this package contains

This repository contains the CommonAI Runtime module with general task/action profiles, numeric considerations, evaluators, services, safe commands, optional Behavior Tree execution, runtime inspection and reusable daily-life behavior. Daily-life includes controller binding, owned movement, needs, routines, an optional clock and persistence. The separate CommonAITests Editor module owns neutral fixtures and automation. `Docs/STATUS.md` records verified checkpoints and current limits. AthenaAI remains a separately installed dependency. Consumers own their occupation profiles and concrete effects.

Keep the common repository separately versioned. Preserve the already chosen submodule or independent-checkout arrangement. Configure ownership in the host's actual source-control system; the plugin's .gitignore does not configure the host. Store Unreal binary assets through Git LFS using the supplied .gitattributes and an installed LFS client.

For a CommonAI-only session, open this repository. Explicitly load applicable host instructions before crossing into any authorized host. For a game integration session, open that host and follow its managed CommonAI bridge. Neither session implies access to an unrelated project.

## Using the foundation

The consumer module depends on CommonAI only. Add CommonAI Agent Component to an existing AIController, assign the supplied Profiles/DA_Work profile, and possess a Pawn. Athena is installed as the prerequisite plugin; no consumer Athena calls or new controller base class are required.

Add a Timed Work Site or Signaled Work Site component to a target actor and register that component with the agent through Add Interaction Target. Actors may instead implement the CommonAI Interaction interface directly. Discovery uses the registered targets, not a global actor scan. The default movement uses engine path following; Set Movement Adapter binds an optional existing locomotion implementation while no Work execution is active.

Activity tags may be specialized in authored profiles and targets. Hierarchical queries match their parents. The profile's availability curve row is projected to the backend task identity, so changing an activity tag does not require renaming curve rows or duplicating the scorer.

For animation-owned state, add **CommonAI Tag Window** to a montage and choose its tags. The window is a native branching point and owns only its playback contribution. Overlapping windows can use the same exact tag safely. The notify is montage-only. Other Blueprint sources may use Add/Remove Tag Contribution, retaining their returned token; removing one source never removes another source's state.

Signaled targets expose On Interaction Started and On Cancellation Requested. Retain the complete supplied request for later Signal Completion or Signal Safe Cancellation calls, including its execution and operation identities. Never fetch a new request when an old async callback finishes. Once cancellation was accepted, natural completion reports Cancelled rather than normal success. An effect owner should check the interaction result before granting success-only effects.

Stop Agent and Set Profile drain a running interaction before disabling or replacing it. A signaled target with safe cancellation enabled must eventually acknowledge a safe point or complete naturally. An overdue cancellation is visible in the agent snapshot; it does not force-release a still-running effect. On Interaction Abandoned is the emergency teardown signal for surviving effect owners to detach their work.

Inspect the Agent Component's State section for activity, phase, target, last result/reason, cancellation elapsed time and tag contributions with their sources. Use Get Snapshot and Get Effective Tags in Blueprint. A target remains reserved while cancellation is pending.

## Set up your first NPC role

This example uses an empty C++ project, one Gardener and one Watering Site. The Gardener travels to the site, completes a timed activity and releases the slot. Use the shared clock and the standard Unreal `AIController` for this walkthrough.

### Install and build

1. In a UE 5.8 Win64 C++ project, install AthenaAI 2.6 separately and put the matching CommonAI source and content in `<Project>/Plugins/CommonAI`. If the checkout uses Git LFS, obtain the actual asset files.
2. Enable `CommonAI` in **Edit > Plugins** or the `.uproject` plugin list. The CommonAI descriptor enables its `AthenaAI` and `SmartObjects` dependencies. Build the project's **Development Editor** target for Win64 and open it. If your own C++ module includes CommonAI headers, add `CommonAI` to that module's `Build.cs` dependencies. The example uses the existing Blueprint-facing types.
3. In the Content Browser, enable **Settings > Show Plugin Content** so you can find `SO_DailyLifeSingleSlot` under CommonAI's `DailyLife/SmartObjects` folder.

### Create the role profile

1. In **Project Settings > Gameplay Tags**, enable **Import Tags From Config**, open **Manage Gameplay Tags**, and register the six tags in the table below. Save them to the game's tag config. You can also register the same tags directly in `Config/DefaultGameplayTags.ini`.
2. In the game's Content Browser, create a **Data Asset** of class **Common AI Daily Life Profile** named `DA_Gardener`. Assign its `Occupation` and add one element to `Activities`. Set its label to `Water Plants`, `Kind` to `Site Activity`, and the tags as shown.

| Profile field | Gameplay Tag |
|---|---|
| `Occupation` | `Example.Occupation.Gardener` |
| `Activity` | `Example.Activity.WaterPlants` |
| `Need Input` | `Example.Consideration.WaterPlants.Need` |
| `Time Input` | `Example.Consideration.WaterPlants.Time` |
| `Availability Input` | `Example.Consideration.WaterPlants.Availability` |
| `Distance Input` | `Example.Consideration.WaterPlants.Distance` |

3. Set `Duration Hours` to `0.2` and `Cooldown Hours` to `1.0`. Leave `Preferred Times` empty, `Required Capabilities` empty and `Complete on Site Signal` off. The profile requires a valid occupation, at least one activity, unique activity tags other than the Wait fallback, and four valid input tags per entry that are unique across the profile. The daily-life component builds the scoring profile and fallback from this data.

### Place the NPC and bind its target

1. Make a level with a walkable floor, a `Player Start` clear of the NPC, and a **Nav Mesh Bounds Volume** covering the floor. Press **P** in the level viewport to check that both the NPC's starting position and the destination are on the green NavMesh.
2. Place a **Common AI Activity Location** actor on that floor and label it `Watering Site`. In its `SmartObject` component, assign `/CommonAI/DailyLife/SmartObjects/SO_DailyLifeSingleSlot` as the definition. Keep the slot at the actor origin. In its `Site` component, set a nonzero `Site ID` unique to the placed site and add `Example.Activity.WaterPlants` to `Activities`.
3. Create `BP_Gardener` with **Character** as its parent. Set **AI Controller Class** to `AIController` and **Auto Possess AI** to **Placed in World or Spawned**. Add a **Common AI Daily Life** component and assign `DA_Gardener` to its `Profile`. Leave `Routine Context` empty to use the shared clock.
4. In `BP_Gardener`, add an **Instance Editable** variable named `ActivityLocation` of type **Common AI Activity Location Object Reference**. Compile. In the **Construction Script**, guard that variable with **Is Valid**. On the valid execution pin, call **Set Destinations** on the Daily Life component. Feed it a **Make Array** with one element, the **Get Site** result from `ActivityLocation`. The array holds `CommonAISmartObjectSite` component references, so use the actor picker on `ActivityLocation` to choose the level actor.
5. Compile and save `BP_Gardener`. Place one instance several meters from `Watering Site`. Select the Gardener instance in the Outliner and assign **Activity Location** to the placed `Watering Site` in Details. Save the level. The Construction Script fills the Daily Life component's `Destinations` before the routine binds to the controller.

### Check the result in Play In Editor

Press **Play** and run `CommonAI.Debug.All 1` in the console. The Gardener should select `Water Plants`, approach `Watering Site`, interact for about 12 gameplay seconds at the default clock rate, then release the slot. The cooldown gives you time to inspect the completed run. Use `CommonAI.Debug.All 0` to turn the labels off.

| Symptom | Check |
|---|---|
| No routine starts | Check AI possession, the assigned profile and its registered tags. The profile's Blueprint `Validate Profile` function returns its configuration errors. An existing agent with its own profile, inputs or movement can prevent binding. |
| The NPC waits | Check the `Activity Location` reference on the placed instance, the `Site ID`, the slot definition and the site's matching activity tag. A capability query must match the site's capabilities when one is configured. |
| The NPC fails to arrive | Check the NavMesh between the NPC and the site origin, and verify that neither another move request nor a running behavior tree owns the controller. |

## Using daily-life behavior

The default integration uses an ordinary engine Character and AIController. The component owns routine selection inputs, controller handoff, bounded binding retries, movement request identity, interruption and cleanup; Athena remains the utility selector.

1. Add **CommonAI Daily Life Component** to a Character, assign its `Profile`, and arrange normal AI possession and navigable ground. Leave `RoutineContext` unset to use the shared clock. The component finds or creates a CommonAI Agent Component on the existing AIController; an agent already owned by another profile, input source or movement adapter is rejected.
2. Place **CommonAI Activity Location** actors and assign `/CommonAI/DailyLife/SmartObjects/SO_DailyLifeSingleSlot` to their Smart Object components. Set each site's stable `SiteId`, `Activities` and `Capabilities`, keeping the single slot at its actor's origin. Assign the character's `Home`, `Work` and additional `Destinations` through their site references. These are explicit references, not global discovery.
3. Set a stable `InitialState.Identity` for every resident that must survive unloading or respawning. A missing identity is generated for that representation; the caller must retain and reuse it to reconnect a later representation to its saved record. Keep profile data immutable during play and tune needs, preferred times, commitment, cooldowns and movement values in the profile asset.

Create a **Common AI Daily Life Profile** asset in the consumer and assign a registered occupation tag. Each activity entry has an exact `Activity` tag and four distinct input tags for need, time, availability and distance. An input tag belongs to one entry in a profile. Set `Kind` to the reusable behavior the entry needs. **Site Activity** selects from the character's authored `Destinations`. Multiple entries can share a kind and retain separate schedules, targets, scores, cooldowns and progress.

For existing profiles authored before these fields were explicit, assign the shared activity and four consideration tags previously associated with each kind. Keep the exact occupation and activity tag names when existing save records must still resolve to those entries.

For a consumer-owned site effect, advertise the activity tag and required capabilities on the site. Enable **Complete on Site Signal** in the profile. Bind the effect to the site's `OnInteractionStarted` and signal completion with the original request. To wait for a safe stop, enable **Wait for Safe Cancellation** on the site and acknowledge `OnCancellationRequested` with `SignalSafeCancellation` when the effect has stopped. Use `OnInteractionAbandoned` to detach pending effects during teardown. For routine-timed activities, author `DurationHours` and leave **Complete on Site Signal** off.

`UCommonAIDailyLifeClockSubsystem` provides continuous accumulated hours, pause, time scale and day length without a per-frame clock tick. Use its `InitialState` configuration or authority-only setters. A consumer with an existing clock assigns a `RoutineContext` implementing `ICommonAIDailyLifeContext`, or passes that provider to `InitializeRoutine`. Return accumulated hours from `GetDailyLifeTimeHours` (negative when unavailable) and a nonempty `GetDailyLifeInterruptionReason` while the existing authority blocks routines. An explicitly selected external context remains the selected authority across shutdown; losing it does not switch to the shared clock.

`UCommonAIDailyLifeSaveSubsystem` keeps weak registrations and retained records for unloaded residents. The component prepares/registers its state before binding and remembers/unregisters it before teardown. Registering the same component is idempotent; a replacement with the same stable identity restores the retained record once. `CreateSave()` and `SaveToSlot(SlotName, UserIndex)` include the shared clock. Consumers with an external clock use `CreateSave(false)` or `SaveToSlot(SlotName, UserIndex, false)`; `RestoreSave`/`LoadFromSlot` then restore residents without changing clock state. Disk saves are explicit. `ExportState`/`RestoreState` also allow integration with an existing persistence authority. A restore validates records and loaded identities before writing, but lifecycle callbacks can interrupt application; a false return can mean partial application and is not a rollback guarantee.

Existing systems need only narrow bindings:

- Supply time and game-specific interruption facts through `ICommonAIDailyLifeContext` or the component's native context overrides.
- For an existing path-following authority, override `GetNavigationAuthority` and `RequestNavigationMove`; use `OnControllerBound`/`OnControllerUnbinding` for concrete controller connections. Preserve request IDs and let the shared component manage request ownership and cancellation. The default uses AIController path following and yields to unrelated navigation or a running brain.
- Implement concrete effects through the existing interaction contract and completion/cancellation signals. Locomotion presentation may read `GetRoutineMoveFacingLocation`; shared routines do not choose character assets or animations.

`CommonAITests` includes neutral lifecycle, persistence-boundary and shared-content acceptance checks. See [Status](Docs/STATUS.md#consumer-owned-daily-life-authoring) for the independent Development Editor build and focused runtime results for consumer-owned profiles. The occupation profiles and their tags belong to each game. The plugin supplies the reusable single-slot Smart Object definition.

## Configuring general behavior

Use **Profiles/DA_GeneralWorker** as the multi-task example. It contains Station work and Standby, external work preference, a distance evaluator, two curves and an observation service. Register a CommonAI Observation Provider using `CommonAI.Source.External`; it returns an actor or location context for `CommonAI.Consideration.Distance.Target`. Register an interaction object under `CommonAI.Binding.Interaction` and let it accept `CommonAI.Activity.Work.Farming`. The profile owns selection and reserve/interact/release; the target owns the concrete effect. DA_Work demonstrates discovery and movement instead of a directly bound station.

Open a profile to edit **Considerations**, **Tasks**, **Services** and **Fallback**. IDs are exact gameplay tags; curve bindings use the engine's table/row picker. Values and ranges are numeric; tags identify their meaning and ownership. A parent `CommonAI.State.Busy` query matches `CommonAI.State.Busy.Working`. Different contributors can safely hold the same exact tag.

Use **Refresh Validation** before assigning a profile. **Generate Missing Curves** preserves existing rows. **Create General Example** fills an empty profile with an assigned authoring table and refuses to overwrite existing definitions. The example measures distance in centimeters. Duplicate profiles and curve tables when independent tuning is intended.

For external values, register their source and call **Apply Input Updates** with the current input generation. Batch related changes together. Handle **On Inputs Reset** to discard old callbacks. Evaluators run through services or explicit **Evaluate Input** calls. Select Interval, On Input Change or Manual scoring on the profile. Unavailable and Skipped inputs are distinct from zero.

Use **Request Activity** for an exact task tag and **Change Profile** for safe or natural-completion transitions. Commands, pauses and locks retain source ownership. **CommonAI Execution** is the extension point for custom asynchronous actions and external ability bindings. Complete the original invocation once; acknowledge cancellation at a safe point. Behavior Tree tasks use a typed tree reference and **CommonAI Complete Activity**; an unrelated running brain is rejected.

Add **CommonAI Debug Component** to a player controller for selected-agent inspection, or create **CommonAI Inspector Widget** and call **Inspect Actor**. The panel shows actual scores, curve contributions, input validity/ranges, services, actions and tag sources. Its pause/rescore controls preserve other owners' holds. World panels and aim selection are optional. No production controller, global discovery, combat system or concrete GAS adapter is installed automatically. See `Docs/STATUS.md` for verification limits.
