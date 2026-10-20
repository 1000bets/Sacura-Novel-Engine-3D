# Quiet Studio — visual QA, 2026-09-29

Design authority: project-local UI/UX Pro Max only. Design direction and research decisions are recorded in `../design-system/sacura-novel-studio/MASTER.md`.

## Rendered checks

Inspected the running Vite app in the browser at 1920×1080, 1440×900 and 1366×768. Confirmed viewport dimensions in the DOM. At 1440 and 1366 the document width matches the viewport; the primary dock tabs fit without horizontal scrolling.

- Neutral editor surfaces, no orange panel fills; warm illumination remains in the authored 3D scene.
- Hierarchy rows use indentation and one selection fill; visibility buttons remain keyboard-accessible.
- Inspector fields, dialogue sections and object transforms retain their handlers while losing section outlines.
- Scenario nodes use a single selected outline, quiet metadata, no internal counter boxes and a faint dot grid.
- Secondary tools remain accessible through More; diagnostics remain permanently accessible from the status bar.
- Checked project library, diagnostics, object selection/transform inspector, timeline, and scene/game modes.
- Checked preview start, pause and stop. No captured browser console errors.
- Keyboard: Enter opens More, Tab reaches its enabled buttons, Escape closes it and restores focus to the trigger.

## Second-pass fixes

At 1366×768 the initial More popup extended below the window. It now opens upward when less than 270px remain below its trigger; checked popup bounds (top 252, bottom 486 in a 768px window). Shortened hierarchy group labels and removed the redundant tab icon to prevent header crowding. Increased muted-text contrast on the active surface.

## Contrast and motion

Semantic text colors were checked numerically against the five opaque surface tokens. Primary text: at least 9.03:1. Secondary: at least 5.44:1. Muted #A0A9B6 is above 4.5:1 on the darkest-to-lightest surface range, including active #353D48. These measurements apply to the editor tokens, not arbitrary user-authored scene colors or every legacy secondary-editor style.

Focus indicators remain explicit. Reduced-motion CSS disables transitions and animations; OS-level reduced-motion emulation was not separately exercised.

## Automated validation

Production build succeeds. All 119 existing tests pass. Vite still reports the large JavaScript bundle warning and the ignored dependency `use client` directive. No bundle architecture work was included in this visual redesign.

Screenshots are saved in the task visualization directory: `sacura-1920.png`, `sacura-1440.png`, `sacura-1366.png`.

## Follow-up: clearer regions and context, 2026-09-29

Rechecked at 1920×1080, 1440×900 and 1366×768 after the second refinement. The document has no horizontal overflow; at 1366 the dock tabs fit their container. Inspected scene mode, game dialogue, object transforms and the secondary-tools popup. Search for “камера” exposes the camera inside the normally collapsed systems group. Systems also expand/collapse directly. Start, pause and stop work; runtime status is visible during playback and absent after stop. No browser console errors were captured.

Second pass removed the redundant tree heading row, differentiated visibility toggles from transform-tool selection, added pressed semantics and raised muted text to #AFBBC9. Small node shadows and tonal header/input surfaces distinguish regions without adding outlines. The lightest active surface is #3B4B5E; semantic primary/secondary/muted/accent text all meet 4.5:1 on it. Production build succeeds; 119 tests passed after functional presentation changes. Existing bundle-size warning remains.

Final screenshots: sacura-clarity-1920.png, sacura-clarity-1440.png, sacura-clarity-1366.png in the task visualization directory.


## Continuous workspace redesign — 2026-09-29

- Actual rendered UI inspected at 1920×1080, 1440×900 and 1366×768. No document horizontal overflow; side panels and tool selectors remain available.
- First pass removed the extra global row, separate panel-header/identity fills, node phase bands and node shadows. Second pass corrected main-menu placement for the new header geometry and increased menu-button targets to 28px.
- Verified: File popup aligns with its trigger; scene-selector keyboard open/Escape; object selection updates transform inspector; graph node selection restores dialogue inspector; minimap toggle; Start → Pause → Stop; scene mode; additional-tool disclosure.
- Browser console: no errors during checks. Existing 119 tests passed; production build passed. Existing bundle-size and React Flow use-client warnings remain.
- Semantic primary/secondary/muted text contrast checked against app, panel, input, active and node surfaces; minimum ratios exceed 4.5:1. Focused visual/interaction check, not a complete accessibility audit.
- Evidence: sacura-minimal-1920.png, sacura-minimal-1440.png, sacura-minimal-1366.png in the task visualization directory.


## Selectable appearance themes — 2026-09-29

Classic Dark, Sakura Night, Slate Studio and Copper Cinema checked in the main editor and diagnostics at 1440×900; Sakura also checked in the full timeline and the appearance popover at 1366×768. Tokens apply to ports, arrows, connections, forms, panel chrome, selection and semantic diagnostics. Authored Three.js and scene colors remain project data.

Fixed label-click blur prematurely closing the popover during the first browser pass. Verified pointer selection, native radio ArrowUp, Tab focus, Escape restoring the trigger, and Copper surviving a full reload. Denied storage and invalid preference cases covered in automated tests. Total: 125 tests pass; production build passes with existing large-chunk and React Flow directive warnings. Screenshots: theme-classic.png, theme-sakura.png, theme-slate.png, theme-copper.png and theme-picker.png in the task visualization directory.
