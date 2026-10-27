# CommonAI

A reusable C++ NPC behavior plugin above AthenaAI. Prefer shared NPC mechanisms in CommonAI; keep concrete application rules and bindings in the consuming project. The dependency direction is **Game -> CommonAI -> AthenaAI**.

## Start here

1. Merge this package into the existing `Plugins/CommonAI` folder. The ZIP contains files directly at its root; do not add a wrapper folder. Its AGENTS.md belongs to CommonAI, never to the game root. Preserve local instruction additions, ignore patterns, implementation code and existing progress when merging.
2. Keep the local manual at `Docs/AthenaAI_Manual/Athena_AI_Manual.md`, with `media/` beside it. This locally provisioned reference directory is deliberately ignored by Git and excluded from this package.
3. Open the authorized host project in Codex and ask it to execute `Plugins/CommonAI/Prompts/00_START_HERE.txt`.

The start task preserves existing host instructions, installs only the small managed host bridge, audits the installed vendor API, then implements the bounded first slice when the relevant decisions are clear. It asks before ambiguous layer placement or a material unresolved behavior decision. It does not publish or commit.

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
| Docs/SOURCES.md | Local reference policy and external primary sources |

`Prompts/01_AUDIT_AND_PLAN.txt` is available for an audit-only task. `Prompts/02_IMPLEMENT_FIRST_SLICE.txt` is available for a separate implementation task. The combined start task already authorizes both stages; do not stop merely to request the same authorization again.

## Local requirements

- An authorized Unreal C++ host and its applicable instructions.
- The installed AthenaAI plugin, including source needed to verify native extension hooks.
- The local manual at the exact path above. Provision it separately in each new authorized checkout; the public repository does not distribute it.
- The actual engine/toolchain and required vendor dependencies.

The runtime module name is **CommonAI**, matching `Source/CommonAI/CommonAI.Build.cs`. Do not rename it based on older architecture drafts. The host can be an existing game; a separate neutral host is a portability check before release, not an initial setup requirement.

## What this package contains

This is a workflow/documentation update, not a replacement of the user's implementation. It does not contain the vendor manual, vendor code, host instructions, host assets, build outputs or a Git history. Existing CommonAI.uplugin, Source, Content and Resources are managed in the local project. Verify their actual state; this package does not establish that a build or runtime test has passed.

Keep the common repository separately versioned. Preserve the already chosen submodule or independent-checkout arrangement. Configure ownership in the host's actual source-control system; the plugin's .gitignore does not configure the host. Store Unreal binary assets through Git LFS using the supplied .gitattributes and an installed LFS client.

For a CommonAI-only session, open this repository. Explicitly load applicable host instructions before crossing into any authorized host. For a game integration session, open that host and follow its managed CommonAI bridge. Neither session implies access to an unrelated project.
