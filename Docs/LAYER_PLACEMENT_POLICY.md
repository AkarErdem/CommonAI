# Layer placement policy

## Default: reusable NPC mechanisms belong in CommonAI

Prefer sharing useful behavior, not maximizing the number of abstract classes. A second live implementation is not required before a clearly reusable mechanic can belong in CommonAI. Do not require access to another consumer to justify neutral reuse.

CommonAI owns shared mechanisms, contracts and neutral data. The consuming game owns its concrete world connections, authored content and application rules. AthenaAI remains the unmodified vendor implementation. Aim for the smallest consumer adapter that can read its authoritative state and perform its concrete effect; agent behavior belongs in CommonAI even if one game is its first user.

## Default implementation boundary

For a new NPC behavior, CommonAI owns the reusable path from observation to outcome: eligibility, utility inputs, selection support, state transitions, sequencing, timing, retries, interruption, cancellation, resource ownership and neutral results. It also owns reusable role/need/routine data and neutral examples. Reuse AthenaAI's verified scoring and lifecycle instead of duplicating them.

The game owns only the facts and operations that are inherently its own: access to its clock, inventory, combat or persistence authority; concrete actors and assets; world-specific movement or interaction calls; rewards, damage, quests and presentation; and values that encode game policy. Keep these bindings small and delegate behavior decisions back to CommonAI. A component or interface implementation in the game is not a reason to copy the shared sequence into it.

When an existing CommonAI contract cannot express a concrete need, add the narrowest typed input/output or capability contract in CommonAI and implement the game side through the existing system. Add a contract when a behavior actually needs it, with explicit failure/cancellation semantics and ownership. Do not add a generic service layer, parallel planner or broad future-proof interface solely to make the game layer look smaller.

Before finishing a behavior change, inspect the game diff: if it contains general selection, routine progression, retry, cooldown, reservation or cleanup logic, move that logic into CommonAI unless a specific game rule requires it there. Record a justified exception and its coupling cost. The number of files or lines is not the goal; one authoritative owner and reusable behavior are.

## Decision procedure before coding

1. Describe the requested behavior without application-specific names. Separate mechanism, configuration and world effects.
2. Check whether an existing shared task, contract, provider or profile can express it. Extend that mechanism where the extension is useful and remains neutral.
3. If differences are values, schedules, needs, role preferences or capabilities, keep the mechanism shared and provide data. Neutral reusable defaults may be shared; tuned game presets stay in the consumer when they contain game policy or references.
4. If the mechanism needs a game-owned service, define or reuse a small CommonAI contract for the exact fact or operation. Implement the concrete clock, inventory, interaction, combat or presentation binding in the game. Calling a game service is a reason for an adapter, not a reason to move the whole mechanism out of CommonAI.
5. Keep concrete game rules and content in the game. Generic interfaces must not expose game classes, modules, enums, asset paths or disguised consumer selectors.
6. If two placements remain materially reasonable, or sharing would require guessing a broad public API, ask the user before implementing that portion. Present a recommendation and the consequence for reuse versus coupling. Continue unrelated clear work.

Before implementation, present a short placement map: **behavior -> CommonAI mechanism / consumer adapter or data / vendor API used -> reason**. This is a disclosure step, not a mandatory approval gate for obvious choices. The user retains the right to choose another valid placement.

## Typical placements

| Requirement | CommonAI | Consuming game |
|---|---|---|
| Daily routine | Selection, schedule windows, needs influence, interruption and resumption | Clock binding and authored local timetable |
| Hunger/fatigue | Reusable optional simulation component and evaluation logic | Existing authoritative state adapter, game balance and food definitions |
| Work | Find/reserve/move/perform/release sequence, timing, cancellation and generic station behavior | Production recipes, rewards, economy effects and concrete station bindings |
| Roles | Reusable Worker/Resident/Guard behavior and neutral profile schema | Named occupations, factions, authority rules and game-specific profile values |
| Move/Wait/Wander | Native reusable tasks/actions and execution ownership | Region restrictions and integration with an existing movement authority |
| Patrol | Route traversal, progress and interruption mechanics | World route assets and local response rules |
| Interaction | Shared request/result/cancellation protocol and reusable generic executors | Actual animation, item exchange and quest consequences |
| Threat response | Reusable threat/context contracts and generic selection where required | Concrete hostility, combat commitments and game combat-system adapter |
| Shared assets | Original curves, neutral presets, generic role data and neutral examples | Character classes, meshes, animations, maps and content-bound variants |
| Debugging | Generic reason codes and lifecycle diagnostics | Host logs, private reproduction details and application-specific overlays |

The table is a default, not a prohibition on useful future reuse. A previously game-owned mechanism can move into CommonAI when its neutral contract is understood and the user approves a material scope/API change. Do not move an existing production system simply because its domain appears in this table.

## Ambiguity protocol

Ask one concise question that names the disputed responsibility, your recommended placement, and the tradeoff. Avoid asking about every helper function or routine dependency. Do not implement both alternatives, choose the game as a silent fallback, or hide a consumer branch behind a generic-looking flag.

Record accepted neutral placement decisions in `Docs/DECISIONS.md`, including the reason and conditions for reconsideration. Keep game-specific names, examples and decisions in the host's own documentation. Never copy private context into shared decisions.

## Dependency and asset check

The directed ownership chain is **Game -> CommonAI -> AthenaAI**. All layers can use ordinary Unreal Engine modules. CommonAI public Athena inheritance may expose vendor types transitively; this is compatible with the chain. New game-specific logic should enter through CommonAI's public contracts. Ask before introducing a new direct game-to-vendor integration bypass.

Check C++ includes/casts, Build.cs dependencies, plugin references, config, dynamic loads, hard/soft asset references and serialized metadata. CommonAI must contain no reverse references to a consumer. A string-based load, soft pointer, tag containing a consumer name or runtime cast is still coupling. Tests must also use neutral fixtures.

Use one authoritative owner for each mutable need, reservation and execution. A reusable state component may live in CommonAI; an existing game state model may instead supply that state through an adapter. Never update both.
