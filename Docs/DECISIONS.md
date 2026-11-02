# CommonAI decisions

Keep entries neutral and concise. Consumer-specific choices and identifying evidence belong in the host. Preserve existing entries when updating this file.

| ID | Decision | Reason / revisit condition |
|---|---|---|
| D001 | The current runtime module is CommonAI. | Matches the established descriptor/Build.cs naming; rename only through an explicit migration. |
| D002 | Prefer reusable NPC mechanisms in CommonAI. | First-use sharing is allowed; do not require another project's implementation as proof. |
| D003 | Use small shared contracts with consumer-owned concrete bindings. | Keeps shared behavior reusable without reverse dependencies. |
| D004 | Ask before materially ambiguous placement or public-contract choices. | The user retains architectural placement control; clear routine work can proceed. |
| D005 | The local manual is the behavioral source of truth; verify native API details in installed source. | Material disagreement is surfaced and resolved rather than silently hidden. |
| D006 | Keep vendor code unchanged and treat local reference tracking separately from distribution. | The manual's earlier local-only policy remains historical context. AthenaAI remains separately provisioned. |
| D007 | Preserve each host's instructions and append an idempotent neutral bridge. | Shared integration must coexist with the host's established workflow. |
| D008 | Keep Work opportunity, reservation, execution variants and sequence in CommonAI; consumers supply concrete sites and effects. | The mechanism is reusable and the consumer keeps world rules. Revisit if a concrete service cannot fit the small provider/site contract. |
| D009 | Use Athena's Data Manager through composition for this slice. | The virtual component factory makes replacement source-feasible, but no required behavior needs custom scoring or manager state. Revisit only for a proven extension need. |
| D010 | Defer Action completion to the next tick and use per-run identity plus owned cleanup. | The installed Sequence starts its first Action before its running flag is set; deferred completion preserves the vendor sequence without modifying it. |
| D011 | Keep the reusable observation-to-outcome path in CommonAI and make consumer bindings thin. | The consumer provides authoritative facts and concrete effects through narrow contracts; revisit a placement only for a specific game rule or verified framework constraint. |
| D012 | Require a specific owner decision before changing tracking or ignore rules for `Docs/AthenaAI_Manual/`. | A prior trackability statement was too broad. Existing indexed files are preserved until a separate tracking decision; commit, sharing and release each require their own authorization and rights review. |

For each later accepted decision, record the neutral responsibility, selected layer, reason, affected shared API/data, and conditions for reconsideration. Do not infer user approval for an unresolved choice from this template.

## Approved foundation decisions

- D013: One Runtime module with a consumer-facing composition API; Athena implementation types remain private.
- D014: Semantic tag hierarchies support parent queries. Source-owned contributions preserve overlapping use of the same exact tag; artists select tags in a native montage notify-state without managing identities.
- D015: Cancellation is accepted and completes at a safe point or natural end. Reservation and state ownership survive until terminal acknowledgement. Emergency teardown is explicit.
- D016: Initial scope is single-player. One movement/interaction owner remains necessary independently of networking.
- D017: The agent component is the setup and diagnostic entry point; no separate custom editor dashboard in this milestone.

## General foundation design choices

The owner delegated selection of an efficient, human-configurable direction. The following are design choices made under that delegation, not claims of implemented or verified feature coverage. See GENERAL_FOUNDATION_DESIGN.md and GENERAL_FOUNDATION_IMPLEMENTATION_PLAN.md.

- D018: Provide safe single-player feature equivalence through CommonAI definitions and private Athena adapters. Do not expose unsafe immediate vendor transitions or add a replacement scorer.
- D019: One Profile is the authoring entry point; Agent Component is the runtime inspection entry point. Use normal asset editors and advanced fields before introducing bespoke tooling.
- D020: Give numeric considerations one designated input authority, validated typed contexts, explicit validity and configurable retention. Existing state tags keep source-owned aggregation.
- D021: Separate explicit-command transition mode from command retention. Default explicit orders to HoldUntilCompletion; releasing a command's scoring hold never releases another source's pause.
- D022: Distinguish reusable action-type tags from stable sequence occurrence identity so Move/Wait/Move/Wait is valid without manual identity bookkeeping.
- D023: Prefer evaluator composition where vendor native exports are unavailable. Keep BT/ability bindings opt-in and avoid making GAS a mandatory dependency.
- D024: Generalize the tested lifecycle and migrate the saved Work profile; do not keep a parallel legacy execution/schema path.
- D025: Characterize vendor policy differences before changing affected semantics. Include public-to-vendor terminal mapping, pawn-generation reset and indefinite natural-completion waits in acceptance tests.
