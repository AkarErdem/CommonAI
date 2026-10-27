# CommonAI Asset and Release Policy

Status: implementation policy; actual native API behavior requires source verification. The established plugin/runtime module name is CommonAI. The local AthenaAI manual is the behavioral reference; follow its exact path and discrepancy policy in AGENTS.md.

## 1. Package boundary

CommonAI is one reusable Unreal Engine plugin containing original runtime code and generic behavior assets. Start with `CommonAI`; add `CommonAIEditor` when validation or authoring tools require it. Editor code must not become a Runtime dependency. A separate vendor-independent Core module is outside the initial scope.

The plugin descriptor enables content through `CanContainContent` and declares AthenaAI as a plugin dependency. Each module's `.Build.cs` declares the required modules. Read their exact names from installed descriptors and source. A public header exposing an AthenaAI base class requires the relevant public module dependency; moving unrelated implementation details to private headers does not remove that requirement. [S1](SOURCES.md#s1) [S2](SOURCES.md#s2)

Distribute original CommonAI source, approved original assets, configuration, documentation, and release metadata. Each consuming environment obtains AthenaAI separately under its applicable license. Do not bundle vendor source, binaries, manuals, sample content, or extracted demo assets. The dependency declaration is sufficient to express the technical prerequisite; it is not redistribution permission. Verify the actual purchase terms when provisioning collaborators and build environments. [S13](SOURCES.md#s13)

### Local vendor reference directory

Keep `Docs/AthenaAI_Manual/Athena_AI_Manual.md` and its adjacent `media/` locally. This exact path is the primary behavioral reference for agent work, but the entire `Docs/AthenaAI_Manual/` directory is ignored by Git and excluded from shared archives. Fresh authorized workspaces provision it separately. Do not delete it or rename it to add a version suffix.

Check both ignore rules and the Git index. An ignore rule does not remove tracked material or earlier history. The authorized bootstrap can remove this directory from the index while preserving local files; it cannot rewrite history or publish as an automatic cleanup. Keep consumer-specific data and diagnostics outside CommonAI, including any identifying scan terms.

## 2. Asset ownership and references

Suitable shared assets include neutral consideration curves, generic activity profiles, reusable role definitions, and deliberately generic sample configurations. Keep consumer characters, animations, world locations, faction definitions, economy rules, and authored production scenarios in the consuming project.

A reusable definition describes capabilities and configuration. Prefer common selection, schedule, role and activity mechanisms; the consumer supplies concrete services, animation bindings, world references and game-specific policies through common contracts. Apply LAYER_PLACEMENT_POLICY.md rather than moving the entire mechanic into the consumer merely because a binding is needed.

CommonAI content must not depend on consumer content. This covers hard references, soft object/class references, serialized strings, class defaults, configuration entries, metadata, import paths, and code-generated lookup paths. A soft reference can contain an asset path and changes loading behavior; it does not eliminate the dependency. [S4](SOURCES.md#s4)

Use an explicit approved dependency allowlist: CommonAI packages, necessary engine packages, and specifically approved prerequisite plugin packages/types. Reject arbitrary `/Game/` dependencies from shared production assets. A separate neutral test host may own its fixtures, but those fixtures must not become dependencies of the distributed runtime assets. Keep policy descriptions generic; do not embed lists of consumer identifiers in shared validation configuration.

## 3. Base profiles and consumer customization

Ordinary Data Asset instances do not inherit values from other instances. Unreal supports data-only Blueprint class inheritance when inherited defaults are required. [S3](SOURCES.md#s3)

The preferred initial design is explicit composition: a generic base definition plus consumer-owned bindings and overrides. Implement only the small amount of overlay behavior the first reusable activities need. Specify precedence, stable identifiers, missing-value behavior, and collection replacement rules. Validate the resolved result before activation. A copied preset becomes an independent asset; subsequent base edits will not automatically update it.

Do not assume AthenaAI already merges presets or supports every desired inheritance pattern. Inspect its actual preset type and initialization path. The CommonAI adapter must resolve the approved configuration into the shape that AthenaAI expects without changing vendor code.

Treat definition assets as immutable during play. Store mutable agent state in per-agent runtime instances. One agent changing its hunger, current task, or reservation must not mutate a shared profile or class default object.

## 4. Authoring and automation

Keep behavior mechanisms in C++. Use assets for designer-facing configuration and composition. Start with a few reviewable assets authored in Unreal; a generator is not a first-release prerequisite.

When repetitive authoring becomes a concrete bottleneck, add Editor tooling with one authoritative representation. For example, schema-validated JSON can become the source from which stable-path assets are generated. Generated assets are then outputs, not an independent editing surface. Alternatively, editor-authored assets can remain authoritative and export text for review. Do not maintain two independently editable sources of truth.

Generation must be repeatable, preserve stable asset identity, report meaningful changes, and reject invalid tags, missing classes, invalid normalized values, and unresolved bindings. Asset generation belongs to the Editor module, not gameplay startup.

## 5. Storage and review

Track `.uasset` and `.umap` through Git LFS from their first commit and commit the associated `.gitattributes`. Every developer and build agent must obtain the actual LFS objects. Pointer-only checkouts cannot serve as usable plugin content. LFS separates binary storage from Git history; it does not provide semantic asset merging. [S8](SOURCES.md#s8) [S9](SOURCES.md#s9)

Unreal assets are binary and cannot be merged with an ordinary text merge tool. Prefer small assets and coordinated editing; use supported locking where the team's tooling provides it. Review configuration changes through Unreal and useful text exports. [S10](SOURCES.md#s10)

Keep generated build products, caches, logs, local workspace settings, and dependency installations outside the shared source distribution. Source and content belong to the same CommonAI revision so an asset is never released without its matching class definitions.

## 6. Version selection and adoption

Release CommonAI with an immutable commit identifier and a readable release version. Record supported Unreal versions, exact tested AthenaAI versions, content/schema versions, platforms, and migration notes. A version label is a declaration of tested compatibility, not proof of compatibility.

For Git consumers, use a submodule under `Plugins/CommonAI`. The parent repository records its selected CommonAI commit. Publish a shared change once, then adopt that revision through a separate consumer update and integration check. Standard submodule update checks out the recorded commit, often with detached HEAD; create a branch before developing inside that checkout. [S6](SOURCES.md#s6) [S7](SOURCES.md#s7)

For consumers using another version-control system, distribute an immutable release archive and record its version, commit, and checksum in consumer-owned dependency metadata. The updater must detect local modifications before replacing files. Do not assume Git submodule support in another system.

A release archive must contain real asset bytes. GitHub-generated archives omit LFS object contents by default unless the relevant setting is enabled. An explicit bundle produced from a fully materialized checkout is also suitable. [S11](SOURCES.md#s11)

## 7. Compatibility and release gates

Maintain a small compatibility matrix listing engine version, vendor version, CommonAI revision, platform, and evidence. Assets saved by a newer engine cannot generally be opened by an older engine; establish the oldest supported authoring version and prevent accidental newer-version resaves. [S12](SOURCES.md#s12)

Before releasing, verify the concrete risks:

- Compile the Runtime module and any included Editor tooling in a neutral host.
- Validate shared assets, resolved presets, Gameplay Tags, and dependency boundaries.
- Run the representative activity and cancellation/cleanup scenario.
- Cook and launch a minimal packaged host to verify asset availability and runtime module separation.
- Check the distribution contains matching code/content, actual LFS bytes, and no excluded files.

`CanContainContent` alone is not evidence that every dynamically selected asset is cooked. Configure Asset Manager discovery/cook rules or deliberate reachable references as appropriate, then verify the packaged result. Do not replace a consumer's global Asset Manager merely to install CommonAI. [S3](SOURCES.md#s3) [S5](SOURCES.md#s5)

Unreal offers C++ data validators and command-line validation. Combine those with Asset Registry/reference inspection; text searches alone cannot establish the contents of serialized assets. [S14](SOURCES.md#s14) [S15](SOURCES.md#s15)

## 8. Collaboration and publication

Keep shared issues, examples, changesets, screenshots, build output, and documentation neutral. Consumer integration configuration, test artifacts, and project-specific diagnostics remain in their respective consumer environments. Shared CI exercises the neutral host; consumer CI exercises local integration. Publish only appropriately scoped results.

Repository visibility is a separate ownership decision. A private repository can support collaborators; a public repository exposes its code and Actions history/logs. Sharing CommonAI does not require public publication. [S16](SOURCES.md#s16) [S17](SOURCES.md#s17)

This document does not select a publication license, create remote repositories, grant access, or authorize deployment. Those decisions belong to the repository owner after the intended package is reviewable.
