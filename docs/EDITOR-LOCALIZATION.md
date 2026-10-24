# Editor localization

English is the source language and the default for a new editor preference profile. Russian is the bundled translation. Existing valid language preferences are retained. The settings button in the main editor opens **Editor Settings → Interface language**. Switching language does not edit scene documents, rename objects, alter story text or restart Play.

## Catalog and identities

`Root/EditorCore/resources/Localization/Editor.json` is the shared Qt/React source. Each entry has an immutable identifier, `source`, `translations.ru`, and optional migration aliases. New UI calls `LocalizeEditorText("Editor.Feature.Caption")` in React or `EditorLocalization::Get().Text(...)` in C++. Do not derive new keys from mutable source text. The migrated `Editor.Legacy.*`, `Editor.Native.*` and `Editor.Qt.*` IDs are frozen; their suffixes must not be regenerated when wording changes.

Reflection component captions use `Editor.Reflection.<typeId>.DisplayName`; property captions use `Editor.Reflection.<typeId>.<propertyId>.DisplayName`. Native reflection metadata retains English source captions. The Inspector uses the key for presentation and the original property ID for edits. An unknown/custom key falls back to its supplied caption.

Existing Russian `t()` and Qt `tr()` call sites are supported through catalog aliases. They are a migration adapter, not a second catalog. Do not pass authored values through these functions. Context-sensitive new text must use an explicit key even when another entry has the same spelling.

Qt static captions use `BindEditorCaption` or `BindEditorProperty`; subscriptions are tied to QObject lifetime. React uses `useLocale`, whose revision changes for both language and translation edits. Language changes must not remount documents or refresh the Inspector selection.

## Preferences and translation work

The native service saves `Editor/Language` and `Editor/TranslationOverrides` in `QSettings("SakuraNovel", "SakuraNovel Studio")`. Electron calls this service through trusted preference IPC and the bounded `--editor-localization` CLI. Its JSON input/output contains only language and translation overrides; it does not open a project or execute arbitrary commands. The main process serializes writes and broadcasts successful changes. Browser-only operation stores the same preference model in localStorage.

Open **Translations** in Editor Settings to search entries, inspect their English source, and edit Russian text. Save writes a per-user override. Reset restores the bundled translation in the edit field; Save applies it. English continues to display the source. Installed catalog files and project descriptors are unchanged.

**Export translations** produces a UTF-8 JSON package with stable IDs, English source, translated text and the original translation baseline. A studio can edit `text` without engine sources or the editor. **Import translations** validates the complete package and displays changed entries before applying it. Baseline differences are marked as conflicts; applying the preview explicitly replaces those translations. A stale source, duplicate/unknown ID or changed interpolation parameter rejects the package as a whole. The preview displays the first 50 changes and the total count. This format is JSON, not XLIFF.

The native console/MCP paths are `editor_settings_get/set` (`language: "en" | "ru"`) and `editor_localization_get/set` (`language`, `overrides`). They use the same native service and validation. Search, previews and download/file-picking controls are UI-only.

## Scope and validation

This change localizes the editor. It does not introduce a serialized gameplay text type, translate story documents, or establish project runtime language catalogs. Those need a separate Engine/Project change.

Frontend tests cover default/fallback locale, persisted overrides, immutable authored data, Play continuity, source mismatch, duplicate IDs, interpolation validation and storage failures. `desktop-electron --localization-check` exercises the real editor: camera Inspector, settings, English/Russian switching, saved overrides, reload and compact layout, and writes screenshots/results under `build/localization-check-*`.

## Project settings

Electron exposes **Edit → Project Settings** separately from user preferences. Both settings sections are ordinary workspace tabs available in the Windows catalog, with shared docking, detaching and closing behavior. Name, startup scene and startup story are saved explicitly to the existing `.project` descriptor through `project.settings.get/set` and `ProjectSession::UpdateSettings`. Engine version is read-only. Empty startup paths disable automatic loading; nonempty paths must be existing project-relative documents with the matching extension. Startup changes apply at next project open. Play and external descriptor changes block writes. The Qt console/MCP equivalents remain `project_settings_get/set`.
