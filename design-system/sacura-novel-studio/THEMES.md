# Sacura appearance system

Primary authority: project-local `.agents/skills/ui-ux-pro-max/SKILL.md`, explicitly reloaded before implementation. No other design skill used. The referenced image was not present in the supplied attachment directory; Sakura uses the written mood, hierarchy and palette requirements.

## Verified research, 2026-09-29

- `creative editor professional dark --design-system`: Minimalism & Swiss Style and Inter fit the existing desktop editor. Landing-page structure and saturated violet/cyan suggestions do not fit and were rejected.
- `Developer Tool IDE --domain color`: slate/navy surfaces and bright neutral text inform Slate Studio. Reduced saturation, lightened text and substituted a restrained blue accent for the database's run-green CTA.
- `visual novel storytelling --domain color`: no result. Retry `romance entertainment --domain color` returned Couple & Relationship App. Its light layout and red accent do not fit an editor; only the rose hue family is relevant to the user's explicit Sakura request. The dark adaptation is a design interpretation, not an exact database recommendation.
- `cinema warm editorial --domain color`, refined to `Theater Cinema --domain color`: verified dark cinematic surface hierarchy and gold accent. Adapted gold toward muted copper and navy toward warm charcoal as requested; rejected saturated indigo outlines.
- `dark contrast --domain ux`: minimum 4.5:1 normal-text contrast. Semantic states retain labels/icons rather than relying on color alone.
- `localStorage persistence --stack react` and `state initialization --stack react`: no verified match. Persistence uses ordinary guarded browser storage and React state; it is not attributed to a database rule.

## Four coherent palettes

| Theme | Base and surfaces | Accent/support | Text and semantics |
| --- | --- | --- | --- |
| Classic Dark | Existing #181A1D canvas, #222427 panel, #2B2E32 elevated/input | Existing #A5BDD3 accent, neutral tab indicator | Existing primary/secondary/muted text; graph connections slightly brighter for 3:1 contrast |
| Sakura Night | #11121B canvas, #1A1B26 panels, #252633 elevated; cool plum undertone | #D9A3BA sakura, muted plum selection | Soft neutral white/lilac gray; sage success, sand warning, rose-red error, slate info |
| Slate Studio | #131B24 canvas, #1C2733 panels, #273443 elevated | #9DBFE0 subdued blue, slate selection | Cool white and light slate gray; sea-green success, sand warning, rose error |
| Copper Cinema | #1B1918 canvas, #252220 panels, #302C28 elevated | #D0A589 copper, dark umber selection | Soft ivory and stone gray; sage success, amber warning, salmon error, blue-gray info |

`src/themes.js` is the single palette source. All themes implement the same token contract for app/panels/inputs, text, disabled state, accent and hover, separators, semantics, graph grid/nodes/selection/connections, shadows and scrim. Existing CSS names are semantic aliases. Authored scene/material/subscene colors and universal XYZ axis colors are independent of appearance.

## Behavior

Appearance is a small top-right disclosure. Four labeled radio choices include swatches and an explicit check mark. Changes apply immediately without remounting the editor or altering project data. Native arrow-key selection, Tab, outside click and Escape work; Escape returns focus to the trigger. The setting is separate from project JSON, saved under `sacura-appearance-v1` and loaded before React renders. Classic is the default and invalid/missing values fall back to it. Denied storage still permits session-only changes and displays a notice.

## Validation

- Main editor and diagnostics visually inspected in all four themes at 1440×900: hierarchy, viewport chrome, inspector forms, scenario graph, toolbars, selected items and disabled transport controls.
- Keyboard theme change and Escape, immediate application, and reload persistence verified in the browser.
- Additional Sakura timeline and compact appearance-menu checks completed.
- Automated tests cover every text/semantic token on app/panel/input/hover/active/node surfaces (4.5:1), graph connections on canvas and nodes (3:1), storage failure and unknown IDs, and complete token replacement between themes.
- Focused theme QA, not a full application accessibility audit.
