# Host integration and instruction bridge

## Scope

The host is the explicitly authorized current Unreal project. CommonAI normally lives at `Plugins/CommonAI` and may have its own Git repository. This document defines a safe setup workflow; it does not grant access to an unrelated project or replace that project's instructions.

## Install or refresh the bridge

The combined start prompt authorizes the following local instruction edits:

1. Read the current host AGENTS.md, any applicable AGENTS.override.md, configured instruction fallback files and relevant nested guidance. Read CommonAI/AGENTS.md explicitly. Preserve current user changes, encoding and newline conventions. Do not modify global agent settings.
2. Read `Docs/HOST_AGENTS_BRIDGE.md`. It contains only the managed block bounded by `<!-- COMMONAI:HOST-BRIDGE:BEGIN -->` and `<!-- COMMONAI:HOST-BRIDGE:END -->`.
3. In an existing host AGENTS.md, append the block once. If exactly one valid block already exists, update only the text inside that block, including its boundary markers. Preserve every other byte when feasible. Never replace the host file with CommonAI/AGENTS.md.
4. A nonempty AGENTS.override.md can shadow AGENTS.md at the same directory. If a root override is effective, also install/update the same small block there, preserving its other instructions. Do not create or remove an override. If only a configured fallback file is effective, update that file rather than introducing a new AGENTS.md that would silently shadow it. If no host instruction file exists, create a small AGENTS.md containing the block.
5. Check relevant nearer overrides/instructions for NPC work. Do not rewrite them wholesale. If they prevent the bridge from being followed, propose the smallest targeted reconciliation; use the current user instruction to resolve already authorized exceptions. Ask only about a material remaining conflict.
6. If markers are duplicated, malformed or overlap, report the ambiguity and avoid replacing surrounding text. An existing equivalent bridge under another heading should be reconciled locally with a small reviewed patch, not duplicated blindly.
7. Show a concise diff summary identifying the host instruction files and managed-block changes. Re-running setup should not duplicate the block or reset any progress.

The package's root AGENTS.md belongs inside CommonAI. No ready-made host AGENTS.md is shipped because each host already owns its instructions.

## Instruction loading for both entry points

**Starting at the host:** explicitly read `Plugins/CommonAI/AGENTS.md` and the placement policy before NPC/AI work, including work that ends up entirely in game code. Root-to-current-directory discovery does not guarantee that deeper plugin guidance was loaded.

**Starting inside CommonAI:** the nested Git root may prevent automatic discovery of parent host instructions. Read the authorized host's applicable guidance before inspecting or editing it. If the task only concerns shared code, do not import a host's private design unnecessarily. Do not copy host instructions into CommonAI.

A new Codex run can rebuild automatically discovered guidance. During the bootstrap run, explicitly read and apply the updated bridge/rules immediately; do not assume that writing AGENTS.md automatically refreshed already-loaded context. A follow-up session should confirm the instruction sources it actually read. These are workflow rules, not a sandbox or an absolute secrecy guarantee. [Official discovery guidance](https://learn.chatgpt.com/docs/agent-configuration/agents-md)

## Manual and local references

The exact plugin-relative manual path is `Docs/AthenaAI_Manual/Athena_AI_Manual.md`. Its adjacent media directory is part of the local reference. Keep it intact and read it directly even when ignored by Git.

The manual is the primary behavioral/authoring reference. Installed source is evidence of exact C++ feasibility. Report meaningful disagreement instead of silently selecting a new behavior. A filename without a version number does not establish the installed version; inspect the descriptor/source and the manual's own metadata where available.

The entire `Docs/AthenaAI_Manual/` folder is ignored and omitted from shared releases. Every authorized fresh checkout provisions its own local copy at the same path. Check with both `git check-ignore` and `git ls-files`: already tracked files remain tracked. The start task may remove this exact reference directory from the Git index while preserving every local file. Never delete the working manual or rewrite old history. Report previously committed reference material privately; do not claim that a new ignore rule erases prior exposure.

## Source-control ownership and privacy

Use the existing repository. Do not reinitialize Git, change remotes, convert to a submodule, commit or push as part of bootstrap. Check whether the host intentionally registers CommonAI as a submodule or excludes an independently managed checkout in its own source-control system. Do not add a parent ignore rule to a registered submodule or assume Git rules configure another VCS.

Keep raw host build logs and private integration notes in the host, outside CommonAI. Shared status uses neutral relative paths and summarized outcomes. Any consumer-specific scan terms are derived locally and remain outside tracked shared files; do not add a private-name denylist to this package. Check the shared working files and staged/tracked paths. Before publication, separately inspect history and serialized asset metadata. No publication is authorized by this workflow.

## Verification before completion

Confirm the effective host bridge, the plugin rules, manual location, runtime module name `CommonAI`, and downward plugin/module dependencies. Explain the feature placement before coding. Validate the first slice in the current authorized host without replacing its production AI/controller pipeline. Validate a separate minimal host before release.

Keep host-specific integration evidence in the host. Update CommonAI's neutral `Docs/STATUS.md`, `Docs/DECISIONS.md` and source-audit results as appropriate. Do not reset existing records when installing an updated instruction pack.
