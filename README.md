# CommonAI

A reusable C++ NPC behavior plugin above AthenaAI. Prefer shared NPC mechanisms in CommonAI; keep concrete application rules and bindings in the consuming project. The dependency direction is **Game -> CommonAI -> AthenaAI**.

## Start here

1. Read `AGENTS.md`, `Docs/STATUS.md` and `Docs/DECISIONS.md` before continuing development. The first native slice is already present; its remaining evidence is recorded in STATUS.
2. Keep the manual at `Docs/AthenaAI_Manual/Athena_AI_Manual.md`, with `media/` beside it. Check its existing index and ignore state before proposing any tracking change. Tracking, committing and distribution require separate owner decisions.
3. For a separately authorized host integration, follow `Docs/HOST_INTEGRATION.md` and install only the managed bridge into that host's effective instructions.

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
| Docs/GENERAL_FOUNDATION_DESIGN.md | Selected general-foundation architecture; not yet implemented |
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

This repository contains the CommonAI runtime module, its current Work/Move/Wait foundation, and the documents needed to continue it. `Docs/STATUS.md` distinguishes completed builds from runtime behavior that has not been checked. AthenaAI remains a separately installed dependency; host assets and integration stay in the consumer.

Keep the common repository separately versioned. Preserve the already chosen submodule or independent-checkout arrangement. Configure ownership in the host's actual source-control system; the plugin's .gitignore does not configure the host. Store Unreal binary assets through Git LFS using the supplied .gitattributes and an installed LFS client.

For a CommonAI-only session, open this repository. Explicitly load applicable host instructions before crossing into any authorized host. For a game integration session, open that host and follow its managed CommonAI bridge. Neither session implies access to an unrelated project.

## Using the foundation

The consumer module depends on CommonAI only. Add CommonAI Agent Component to an existing AIController, assign the supplied Profiles/DA_Work profile, and possess a Pawn. Athena is installed as the prerequisite plugin; no consumer Athena calls or new controller base class are required.

Add a Timed Work Site or Signaled Work Site component to a target actor and register that component with the agent through Add Interaction Target. Actors may instead implement the CommonAI Interaction interface directly. Discovery uses the registered targets, not a global actor scan. The default movement uses engine path following; Set Movement Adapter binds an optional existing locomotion implementation while no Work execution is active.

Activity tags may be specialized in authored profiles and targets. Hierarchical queries match their parents. The profile's availability curve row is projected to the backend task identity, so changing an activity tag does not require renaming curve rows or duplicating the scorer.

For animation-owned state, add **CommonAI Tag Window** to a montage and choose its tags. The window is a native branching point and owns only its playback contribution. Overlapping windows can use the same exact tag safely. The notify is montage-only. Other Blueprint sources may use Add/Remove Tag Contribution, retaining their returned token; removing one source never removes another source's state.

Signaled targets expose On Interaction Started and On Cancellation Requested. Capture the supplied execution identity for later Signal Completion or Signal Safe Cancellation calls. Never fetch a new execution identity when an old async callback finishes. Once cancellation was accepted, natural completion reports Cancelled rather than normal success. An effect owner should check the interaction result before granting success-only effects.

Stop Agent and Set Profile drain a running interaction before disabling or replacing it. A signaled target with safe cancellation enabled must eventually acknowledge a safe point or complete naturally. An overdue cancellation is visible in the agent snapshot; it does not force-release a still-running effect. On Interaction Abandoned is the emergency teardown signal for surviving effect owners to detach their work.

Inspect the Agent Component's State section for activity, phase, target, last result/reason, cancellation elapsed time and tag contributions with their sources. Use Get Snapshot and Get Effective Tags in Blueprint. A target remains reserved while cancellation is pending.

The ready profile is a minimal Work/Wait foundation. It does not install production controllers, discover world targets automatically, or implement game-specific animations and effects. Current verification scope is recorded in Docs/STATUS.md.
