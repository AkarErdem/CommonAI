# CommonAI — Staged Roadmap and Acceptance Gates

## How to use this plan

Advance by evidence, not by the number of classes created. Keep confirmed source facts, proposed framework decisions and unresolved questions separate. The local first-slice implementation is recorded in `STATUS.md`; unverified runtime and asset gates remain open. This roadmap is not standing authorization for further implementation, source-control changes or release. The audit prompt remains read-only.

For every feature, apply `LAYER_PLACEMENT_POLICY.md`: prefer reusable mechanisms in CommonAI, use narrow consumer bindings, and ask before materially ambiguous placement or public-API choices. Each later milestone needs its own requested scope.

## M0 — Establish a clean development boundary

**Deliver:** a neutral workspace and dependency inventory.

- Preserve the established CommonAI plugin/runtime module name and check the existing implementation rather than regenerating it.
- Read host/plugin instructions and safely install the managed host bridge using HOST_INTEGRATION.md.
- Verify `Docs/AthenaAI_Manual/Athena_AI_Manual.md` and media remain available. Treat tracking, committing and distribution as separate owner decisions.
- Identify an authorized C++ host project; an existing application can be the initial development host.
- Keep the shared plugin repository independently versioned.
- Make the installed vendor source and manual available for development; keep vendor source separate and review the manual's sharing rights before distribution.
- Record actual Unreal, AthenaAI, compiler/platform and source-control versions.
- Record the current authorized host's existing dependency/versioning strategy. Defer other consumers and any source-control conversion to their own authorized integration task.
- Apply generic confidentiality rules without placing consumer names in a shared blocklist.

**Gate:** the common repository is independently versioned, parent tracking ownership is explicit, and consumer-specific files remain outside it. The initial host may be an existing application. Independent-host portability proof is a release gate.

## M1 — Audit native extension feasibility

**Deliver:** source evidence matrix and proposed integration plan.

Follow ATHENA_SOURCE_AUDIT.md. Identify actual native parents, public exports, supported overrides, lifecycle order, manager creation, preset instancing and scoring behavior.

Resolve the Data Manager substitution question explicitly. A class that compiles but is never constructed is not a successful extension.

Inspect the exact tag filters and curve-name generation. The manual's singular/plural examples are inconsistent; do not copy them unquestioned.

**Gate:** every planned native override has source evidence, and every unresolved point has an explicit feasibility decision. Do not implement a vendor fork merely to hide a missing hook.

## M2 — Build the smallest native foundation

**Deliver:** CommonAI plugin, CommonAI and minimal original content.

- Declare content and verified plugin/module dependencies.
- Add useful shared native extension bases and diagnostics.
- Bind the component through the approved host integration, preserving the existing controller and execution ownership.
- Install a safe fallback and one bounded Wait/Move task.
- Implement the minimal execution ID, state reset and cleanup policy.
- Add the required shared tags using an approach supported by the installed engine/vendor versions.
- Verify native runtime types, not only Editor class visibility.

**Gate:** the host builds and runs from a clean checkout with separately provisioned dependencies. Repeated start/stop and reexecution do not retain stale delegates or state.

## M3 — Prove one reusable activity

**Deliver:** one native Work sequence and neutral provider implementations.

Sequence: Find opportunity, reserve, move, perform, release.

Use two deliberately different neutral opportunities, such as a timed station and an event-completed station. Both must work with the same shared sequence through the provider/executor contract. This proves actual variation support without requiring access to a production consumer.

**Gate:**

- A path failure does not produce activity success.
- A busy station is not double-booked.
- Replaced/cancelled work releases its resources.
- A target destroyed during movement or interaction is handled.
- A late callback from an old run cannot complete a new run.
- Vendor Successful on a preset switch does not produce a work reward.
- A temporary lack of opportunities results in safe fallback or bounded retry.

Do not add a large library of tasks before this slice is stable.

## M4 — Add routine profiles and reproducible content

**Deliver:** small neutral Worker/Resident profiles, curves, and needed authoring tools.

- Add Work/Rest/Wait and only the other shared activities demonstrated by actual requirements.
- Feed needs and time from the selected authoritative context source.
- Use Score Modifier for task weighting; validate Priority tie behavior.
- Tune vendor Replace Resistance and Momentum before adding a parallel anti-switching policy.
- Add consumer bindings and limited overrides only where direct shared asset reuse is insufficient.
- If repeated asset authoring is expensive, implement a small Editor importer/generator with a declared recipe schema.
- Add Data Validation rules for duplicate IDs, missing curves, invalid references and incomplete presets.

**Gate:** two profiles produce distinct behavior through data with the same native activity classes. Generated assets, if introduced, correspond semantically to their declared sources and are available in a packaged host.

## M5 — Integrate each consumer privately

**Deliver:** a consumer-owned adapter and version pin in each authorized project.

Audit the real controller, movement, clock, interaction, persistence and any existing decision/execution systems first. Adapt to those systems rather than replacing them automatically.

Connect context, opportunity and execution contracts. Keep game profiles, game identifiers, animations and world content in the consumer.

**Gate:** the integration requires no consumer-specific edit to CommonAI. Any missing shared feature is restated as a neutral requirement and developed in the neutral workspace.

Where an application already has committed actions, verify control handoff, interruption and authoritative state ownership before enabling unrestricted routine switching.

## M6 — Release and adopt

**Deliver:** one immutable shared release containing matching code and content.

- Record the shared commit, recipe/schema version and tested Unreal/Athena matrix.
- Materialize actual LFS bytes before assembling release archives.
- Validate the standalone neutral host and packaged content.
- Exclude vendor source/content, consumer artifacts and generated local build data.
- Publish only neutral changelogs and results.
- Update each consumer's pin independently and run its own integration checks.
- Preserve rollback information and explain any asset/schema migration.

**Gate:** a clean consumer can reproduce the selected version, and can return to its previous compatible version. An uncommitted local shared checkout is not a release.

Publication, repository creation and collaborator changes require a separate user request; the documentation pack itself does not perform those actions.

## M7 — Expand only after measurement

Potential later work:

- Reusable Patrol/Socialize roles with concrete common requirements.
- Smart Objects execution adapter.
- StateTree execution bridge where sequences are insufficient.
- Batched observation scheduling and simulation LOD.
- Mass/world-simulation integration.
- Module separation when an actual consumer needs a vendor-free subset.

For every addition, state the observed limitation, the smallest extension and the acceptance scenario it enables.

## Compact validation matrix

| Area | Minimum evidence |
|---|---|
| Native extension | Source-backed override map and observed instantiated types |
| Agent isolation | Two agents cannot overwrite each other's runtime state |
| Task reentry | Same instance executes twice with fresh run state |
| Async completion | Old callback ignored; current run completes exactly once |
| Ownership | No simultaneous conflicting movement/interaction authority |
| Cancellation | Cleanup on replace, abort, disable, destruction and safe deferred stop |
| Resource access | No double reservation; every owned claim is eventually released |
| Business outcome | Vendor task termination never implies unverified activity success |
| Definitions | Shared assets/CDOs remain immutable during gameplay |
| Dependencies | No consumer code, hard reference, soft path or identifying metadata in shared package |
| Content | Packaged host can discover and load every selected required asset |
| Distribution | Real LFS bytes, pinned versions and documented rollback |
| Performance | Measured CPU/queries/scoring in representative host and consumer scenarios |

## Persistent work records

After implementation begins, maintain `Docs/DECISIONS.md`, `Docs/STATUS.md` and `Docs/ATHENA_SOURCE_AUDIT_RESULTS.md` with compact neutral decisions, verified evidence, current milestone, checks actually run, blockers and next steps. Preserve existing progress. Record only executed checks as passed. Keep application-specific bug reports, raw commands containing host identifiers and logs in the host, outside CommonAI.
