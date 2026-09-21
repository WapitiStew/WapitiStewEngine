# WSE Documentation Generation

> Canonical language: English
> Documentation version: WSE 1.0.0
> Last synchronized: 2026-09-20

## Selection and content

The documentation audience and visibility are independent selections. Each combination has a small
version 1 JSON profile with `audience`, `visibility`, and `source_root`; the common Python runner
resolves it into Doxygen configuration for Japanese and English. `source_root` is relative to the
profile file. Common formatting remains in the engine's `doxy/Resource` and `doxy/Style` directories.

| Profile | Parsed source | Other pages | Rendered source listings / private members |
| --- | --- | --- | --- |
| User Public | Engine `api/` headers | API landing pages | Disabled |
| Developer Public | Engine `api/`, `core/`, `platform/` | User guides and design documents | Enabled |
| User Confidential | Engine and selected overlay `api/` headers | API landing pages | Disabled |
| Developer Confidential | Engine and overlay `api/`, `core/`, `platform/` | User guides and design documents from both trees | Enabled |

User HTML/LaTeX output disables source browsing, inline sources, verbatim headers, private members
and internal documentation. Normal User generation does not emit raw XML, which Doxygen can populate
with private symbol metadata even when those members are hidden in rendered documentation. Only
`--check` emits diagnostic XML for User profiles, with program listings disabled; it is not a User site.
Before a full User run, stale XML from earlier runs is removed from that language's output directory.
`--generate-only` preserves existing output. Header-inline implementations remain in
the source headers, but are not rendered as source listings. Developer output enables those features.
Tests, vendor dependencies, build directories and historical reports are not parsed. The overlay's
existence or an environment variable never adds it to a public profile. Input files are enumerated
from the selected directories; include search and symlink traversal cannot expand the source scope.

## Entry points and output

The [Doxygen guide](../../../doxy/README.md) lists the Windows and shell launchers. The public runner
accepts `--preset user-public`, `--preset developer-public`, or `--profile <JSON>` for an explicit
profile. The default is Developer Public. `--language en|ja|both` selects languages, defaulting to both.
`--generate-only` retains reviewable Doxyfiles without invoking Doxygen. `--check` generates XML
without HTML, LaTeX or graphs and does not overwrite a full site's output.

The output root is `<source-root>/doxy/generated/<audience>-<visibility>/<full-or-check>/`.
Each `lang_en` or `lang_ja` directory owns its HTML/XML/LaTeX output, `warnings.log`, and
`doxygen.stdout.log`. Resolved Doxyfiles, layout and HTML header live in `config/`. A language switch
links only the selected languages of the same edition. The title includes audience and visibility.
Run only one command for a particular profile and mode at a time.

Configured input and presentation paths are absolute. Doxygen runs in the selected language's
output directory so a parent project cannot shadow relative Markdown links in a nested checkout.

Confidential profiles must live with an overlay outside the public source tree. All their output,
configuration and process logs stay outside the public tree, even though they reuse the common
runner and parse public API files. Missing overlays and invalid profiles fail without falling back
to a public edition. Public profiles may select only the engine root. Outputs are Git-ignored;
generation is not publication or authorization to export an access-controlled document.

A public SDK checkout may be a submodule under an overlay's `external/` directory. The SDK
must remain outside the overlay's selected `api/`, `core/`, `platform/` and `doc/` input trees
and outside every controlled output directory. Public profiles still select only the SDK;
controlled profiles select the two explicit source sets and keep all output outside the SDK.

## Verification and output configuration

Doxygen 1.13.2+ is required and full generation requires Graphviz. Tool provisioning remains an
explicit separate step for Doxygen and Graphviz. Windows batch entries reuse the pinned-Python
bootstrap launcher, which can provision its verified cache when missing. Offline use requires that
cache to be prepared, or the Python/shell entry with an existing interpreter. Both a successful exit status and an empty document-warning log are required.
Regression tests verify the four-way input matrix, disjoint output paths, restricted-source rejection,
User source-listing suppression, language selection, and failure propagation. Public CI generates
User and Developer editions in both languages. Overlay verification also exercises both controlled
profiles; its logs and evidence remain in the overlay.

The common entry defaults to Developer Public. Generated output uses the edition directories
above; open the reported `html/index.html` in the selected edition.
`doxy/project.json` version 2 catalogs public profile files instead of copying generated options.

<a id="detailed-design-authoring"></a>
## Detailed design authoring

Developer documentation combines generated symbol references with authored explanations of
responsibility, state and algorithms. Source listings alone do not specify how to reconstruct
the system. Start at [Architecture](Architecture.md); connect each detailed claim to
[Design Verification](DesignVerification.md).

Each new or substantially expanded component design covers the following topics. A topic that
does not apply is marked with a reason; a missing decision is identified rather than invented.

| Topic | Required content |
| --- | --- |
| Scope and responsibility | Use cases, boundary, caller responsibilities and non-goals |
| Structure and dependencies | Facade, owner, adapter and helper graph; source map; logical versus link dependencies |
| Data representation | Units, defaults, byte/element layout, coordinates, invariants and borrowed/owned values |
| Operation contract | Preconditions, success, failure, partial output and state preservation |
| State and sequence | Valid/invalid transitions; normal, failure, cancellation, restart and shutdown paths |
| Concurrency and lifetime | Execution thread, locks, callback re-entry/removal, joining and resource release |
| Algorithm or file format | Formula or pseudocode, rounding, limits, representative input and expected output |
| Platform/runtime differences | Backend selection, unsupported capabilities, marshaling and ABI |
| Evidence and limits | Contract ID/section, public symbol, tests, conditions and unverified cases |
| Design rationale | Reasons for the current structure, extension boundaries and compatibility constraints |
| Change history | Dated records of changes and their reasons, separate from the design body |

Mark observable requirements separately from current internal choices. Do not turn discovered
defects into desired requirements. The design body describes the SDK's current structure, behavior,
supported interfaces and limits in the present tense. Migration narratives, before/after comparisons,
and statements that an implementation was added, changed, fixed or removed belong only in the
change-history section. Current compatibility APIs and deprecation rules describe present contracts;
they are not a record of completed work. Keep execution reports and task plans outside normative prose.
English and Japanese content, Manifest dates and change histories move together.

Use small responsibility, ownership, state and sequence diagrams with a legend for arrows.
Fenced text diagrams and Markdown tables are the portable baseline supported by the current
Markdown/Doxygen pipeline. A different diagram renderer must be validated in both generated
languages before adoption. Link to source examples rather than maintaining divergent copies.
New documents are registered in the Manifest and the documentation index.

For each detailed-design batch, generate Developer Public HTML in both languages after synchronization
and inspect the actual browser presentation. Check entry navigation, cross-document links, ownership
diagrams, code, wide tables and formulas; retain the profile/tool versions, warning result, inspected
pages and screenshots with the acceptance record. Recheck affected pages after corrections.
First-reader reconstruction follows this display gate; see [Design Verification](DesignVerification.md).

For a shared-style change, check every top-level HTML page in both public editions and languages
at 320, 375 and 720 CSS pixels. Include API details, all-member lists, navigation and source pages;
exclude search fragments and crawler files. Verify successful loading, content/image availability,
and no document-wide horizontal overflow. Wide signatures, tables, code and graphs may scroll
inside their own containers; retain code whitespace and graph image-map coordinates. Review
representative screenshots separately from automated geometry results. Generation must finish
before inspection begins; incomplete page or stylesheet loads invalidate that measurement.

After full Developer generation, the runner resolves authored relative source links through the
selected Doxygen XML file map. Parsed files link to their generated source/reference page; build
scripts, tests and language sources outside the selected input remain labelled checkout paths.
This does not add, copy or expose extra source inputs. User generation does not run this step.

The same selected XML map resolves Markdown page links that Doxygen leaves as `.md` URLs,
preserving explicit section anchors. Add stable HTML anchors to referenced headings rather than
depending on a renderer's automatic heading IDs. Inspect local links of every file type, including
unresolved `.md` URLs, and verify fragments in the generated target page.

## Change history


| Date | Change |
| --- | --- |
| 2026-09-20 | Initial public baseline of the current contracts and procedures. |
