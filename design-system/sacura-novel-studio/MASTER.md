# Sacura Novel Studio — Quiet Studio

Primary design authority: `.agents/skills/ui-ux-pro-max/SKILL.md`.

## Direction

Calm professional creative editor. The viewport and active work take priority. Use alignment, spacing, typography and small surface differences; omit decorative framing. No gradients, glow, glass, orange panel fills, or purple dashboard styling.

## Research and interpretation

Local skill searches, 2026-09-29:
- `desktop creative editor minimal dark --design-system`: verified **Minimalism & Swiss Style** as applicable to professional tools. Rejected the generated landing-page structure and violet palette because they contradict the editor brief. No unverified generated output was persisted.
- `minimal dark --domain style`: confirmed minimalism, one accent, clear typography, no unnecessary shadows or gradients.
- `creative professional dark --domain color`, then `code editor IDE --domain color`: the second query returned **Developer Tool / IDE**, an applicable cool dark foundation. Adapted its navy surfaces toward neutral graphite and reduced accent saturation for Sacura.
- `dark contrast --domain ux`: **Contrast Readability / Color Contrast**, minimum 4.5:1 for normal text.
- `dragging movements --domain ux`: preserve keyboard and single-pointer alternatives, including existing graph and splitter controls.
- `controlled inputs --stack react`: preserve controlled field values and their existing update handlers.
- Desktop density / side-panel searches and one retry did not produce a verified domain match. Use the skill Quick Reference sections 1, 4, 5, 6 and 8 as general guidance for grouping and progressive disclosure.

## Semantic tokens

| Role | Value |
| --- | --- |
| App background | #181A1D |
| Surface 1 | #222427 |
| Surface 2 / menus | #2B2E32 |
| Hover | #34383D |
| Active | #38434E |
| Structural divider | #33363B |
| Primary text | #E8EAED |
| Secondary text | #C1C5CB |
| Muted text | #AAB1BA |
| Restrained accent / focus | #A5BDD3 |
| Error | #EEA2A2 |
| Warning | #D6BE8D |
| Success | #A0C8B0 |
| Chrome / global bar | #222427 |
| Panel headers | #222427 |
| Recessed inputs | #2B2E32 |
| Graph nodes | #292C30 |
| Logo only | #C6AA94 |

Palette source: `engine-react-template/src/themes.js`; semantic aliases and component styling: `engine-react-template/src/editorTheme.css`. These values describe Classic Dark. See [THEMES.md](THEMES.md) for the four-theme system, research and validation.

## Hierarchy and components

- Spacing: 4, 8, 12, 16, 24, 32px. Compact control interiors; larger space between functional groups.
- Typography: existing local/system sans serif stack. Regular 400, section titles 500. Metadata 11px, UI labels 12–13px, dialogue 14px or larger.
- Side panels: one continuous surface. Inspector sections use disclosure headings and whitespace, not rectangular cards.
- Tree: neutral selection fill. Visibility controls appear on hover or keyboard focus; hidden-object state remains visible.
- Five everyday workspaces remain visible. Secondary tools use a disclosure that opens upward near the window bottom. Escape restores focus; Tab accesses its buttons.
- Graph: borderless neutral nodes, one thin selected outline, quiet dot grid, simple connections. Phase counts stay operable but have no enclosing boxes.
- Diagnostics: persistent status-bar entry and small semantic warning icon. Existing issue details and fixes remain available.
- Focus rings are intentional accessibility indicators, not decoration. No layout-shifting hover effects; reduced-motion removes transitions.
- Warm lighting and character colors inside authored 3D scenes are content, independent of the neutral editor chrome.

## Clarity refinement, 2026-09-29

Latest user direction supersedes the earlier very flat surface treatment: distinguish functional zones with tone, not outlines. Header bands, recessed fields and a small graph-node shadow provide restrained depth. The dark-mode search returned OLED/neon guidance that does not fit the brief; the explicit minimalism retry confirmed Minimalism & Swiss Style. The Developer Tool / IDE palette remains the researched foundation, adapted to neutral graphite. No other design skill was used.

- Open in scene-editing mode. Game preview and its dialogue remain available through the explicit Game tab.
- Label the primary transport action Start/Stop. Remove duplicated camera navigation and the redundant global authoring shortcut; those destinations remain in the workspace tabs / More.
- Put scene objects before auxiliary systems. Systems and staging points default collapsed; existing saved disclosure preferences win, and search automatically opens groups.
- Runtime/audio status appears only while running or while tracks are playing / in error. Diagnostics always remain in the bottom status bar.
- Keep tool selection stronger than independent visibility toggles. Expose their pressed states to assistive technology.
- Use 13px tree labels, clear 13px section headings and 15px inspector identity. Maintain smaller, secondary metadata without sacrificing text contrast.

## Continuous workspace revision — current direction

The latest brief supersedes the previous header-band treatment. Re-ran the local design-system search `desktop creative editor minimal`, verified Minimalism & Swiss Style with `minimal desktop applications --domain style`, and verified the Developer Tool / IDE color result. Adapted its cool palette to nearly neutral charcoal. Landing-page composition and violet colors were rejected. `progressive disclosure` and the editor-panel retry did not yield an applicable result: use the skill Quick Reference grouping guidance. Contrast and dragging-movement results and React controlled-input guidance were verified separately.

- One 44px global command row; the scene selector belongs to the viewport header.
- Panel headers share their surrounding surface. Inspector identity has no separate band or decorative icon.
- Transform tools use compact icons with named controls/tooltips; weather selectors retain their text without redundant icons.
- Graph nodes have no shadows or phase-count bands. Selection uses one inset accent line. Minimap is an explicit toggle.
- Main menus anchor to their actual trigger position, independent of the brand width.
- Three desktop viewports and existing behavior are checked in QA-QUIET-STUDIO.md.
