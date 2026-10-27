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
