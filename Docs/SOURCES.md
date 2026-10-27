# CommonAI Official Source References

Reviewed: 2026-10-27. These sources support engine and distribution facts. The architecture and workflow choices in this pack are design recommendations. Inspect the installed AthenaAI source for exact vendor API signatures, class names, accessibility, and lifecycle behavior; generic Unreal documentation cannot establish those details.

## Local primary reference

The canonical AthenaAI behavioral source of truth is `Docs/AthenaAI_Manual/Athena_AI_Manual.md`, relative to CommonAI, with its adjacent `media/` directory. Read it explicitly for vendor-related work; it is ignored local reference material and is not distributed in this pack. Section references marked **M** in ATHENA_SOURCE_AUDIT.md refer to this manual.

Installed AthenaAI source establishes exact native symbols, exports, creation hooks and executable behavior. Record actual versions separately from the versionless manual filename. Material disagreement between the manual and source must be reported and resolved before implementing conflicting behavior; neither an old summary nor generic engine documentation silently replaces the manual.

## S1

**Epic Games — Plugins in Unreal Engine**  
https://dev.epicgames.com/documentation/unreal-engine/plugins-in-unreal-engine

Claim mapping: plugins can contain C++ modules and content; `CanContainContent` enables content; plugin descriptors can declare other plugin dependencies; Runtime and Editor module types have different loading scopes.

## S2

**Epic Games — Unreal Engine Modules**  
https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-modules

Claim mapping: `.Build.cs` expresses module dependencies. Types exposed through public headers require the relevant public dependency; private implementation dependencies can remain private.

## S3

**Epic Games — Asset Management**  
https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine

Claim mapping: ordinary Data Asset instances do not provide asset-instance inheritance; data-only Blueprint classes support inherited defaults. Asset discovery and loading require registration/configuration; a custom global Asset Manager is optional when no special functionality requires it.

## S4

**Epic Games — Referencing Assets**  
https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine

Claim mapping: hard references and soft references differ in loading behavior; soft references can store target paths and need explicit loading. A soft reference remains a reference.

## S5

**Epic Games — Cooking Content and Creating Chunks**  
https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine

Claim mapping: Primary Asset Rules and Labels participate in cooking and chunk assignment. Packaged-content availability must be tested against the chosen configuration.

## S6

**Git — gitsubmodules**  
https://git-scm.com/docs/gitsubmodules

Claim mapping: a submodule has its own repository history; the superproject records the expected submodule commit in a gitlink.

## S7

**Git — git-submodule**  
https://git-scm.com/docs/git-submodule

Claim mapping: update checks out the recorded commit by default, normally on detached HEAD; `--init` and `--recursive` control initialization and nested submodule updates.

## S8

**GitHub — About Git Large File Storage**  
https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage

Claim mapping: Git LFS uses pointer files in Git and stores the actual object separately.

## S9

**GitHub — Configuring Git Large File Storage**  
https://docs.github.com/en/repositories/working-with-files/managing-large-files/configuring-git-large-file-storage

Claim mapping: tracked patterns are recorded in `.gitattributes`; committing that file makes repository-level tracking available to collaborators.

## S10

**Epic Games — Using Perforce as Source Control**  
https://dev.epicgames.com/documentation/unreal-engine/using-perforce-as-source-control-for-unreal-engine

Claim mapping: `.uasset` and `.umap` files are binary and cannot be merged through ordinary text merge tools. This reference supports the file-format fact; the pack does not require Perforce.

## S11

**GitHub — Managing Git LFS Objects in Repository Archives**  
https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/managing-git-lfs-objects-in-archives-of-your-repository

Claim mapping: GitHub source archives contain LFS pointers by default; an archive setting can include actual LFS objects.

## S12

**Epic Games — Versioning of Assets and Packages**  
https://dev.epicgames.com/documentation/unreal-engine/versioning-of-assets-and-packages-in-unreal-engine

Claim mapping: saved assets include engine version information, and older engines cannot safely load assets saved by newer incompatible versions.

## S13

**Fab — Standard License**  
https://www.fab.com/eula

Claim mapping: the published summary distinguishes project collaboration from prohibited standalone redistribution. The actual acquired license, full applicable terms, and any seat conditions must be checked; this pack does not establish the license attached to a particular purchase.

## S14

**Epic Games — Data Validation**  
https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine

Claim mapping: custom validation can use `IsDataValid` or `UEditorValidatorBase`; command-line data validation supports CI and runs C++ validators by default.

## S15

**Epic Games — Reference Viewer**  
https://dev.epicgames.com/documentation/unreal-engine/reference-viewer-in-unreal-engine

Claim mapping: references are directional, and tooling can display hard, soft, editor-only, management, searchable-name, and C++ package references.

## S16

**GitHub — About Repositories**  
https://docs.github.com/en/repositories/creating-and-managing-repositories/about-repositories

Claim mapping: private repositories can grant selected collaborators access; public repositories are accessible to everyone.

## S17

**GitHub — Setting Repository Visibility**  
https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility

Claim mapping: making a repository public exposes its code and Actions history/logs; publication is a separate decision from technical reuse.

## S18

**OpenAI — Custom instructions with AGENTS.md**  
https://learn.chatgpt.com/docs/agent-configuration/agents-md

Claim mapping: discovery follows the project root to the current working directory; AGENTS.override.md can take precedence at a directory; nested Git roots and entry points affect automatic discovery; instructions have a configured size limit. The explicit host/plugin bridge is this package's integration design, not an automatic Codex cross-repository guarantee.
