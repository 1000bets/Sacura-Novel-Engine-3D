import Test, {afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {EDITOR_CATALOG, LocalizeEditorText, GetEditorTranslation, SaveEditorTranslation, ExportEditorTranslations, ImportEditorTranslations, PreviewEditorTranslations, GetLocaleRevision, initializeLanguage, setLanguage} from '../src/i18n.js';

const Values = new Map();
Test('caption aliases resolve identically regardless of catalog iteration order', () => {
  const Aliases = Object.values(EDITOR_CATALOG.entries).flatMap(Entry => Entry.aliases || []);
  Assert.equal(new Set(Aliases).size, Aliases.length);
});
const Storage = {getItem: Key => Values.get(Key), setItem: (Key, Value) => Values.set(Key, Value)};
afterEach(() => { Values.clear(); initializeLanguage(Storage); });

Test('English source, Russian translation and unknown-key fallback are independent of document data', () => {
  initializeLanguage(Storage);
  Assert.equal(LocalizeEditorText('Editor.Settings.Title'), 'Editor Settings');
  setLanguage('ru', Storage);
  Assert.equal(LocalizeEditorText('Editor.Settings.Title'), 'Настройки редактора');
  Assert.equal(LocalizeEditorText('Project.Unknown', 'Authored value'), 'Authored value');
  Assert.equal(LocalizeEditorText('Editor.Reflection.engine.CameraComponent.field_of_view.DisplayName'), 'Угол обзора');
});

Test('translation edits refresh subscribers, persist and leave the English source intact', () => {
  setLanguage('ru', Storage);
  const Revision = GetLocaleRevision();
  SaveEditorTranslation('Editor.Settings.Title', 'Параметры редактора', Storage);
  Assert.ok(GetLocaleRevision() > Revision);
  Assert.equal(LocalizeEditorText('Editor.Settings.Title'), 'Параметры редактора');
  initializeLanguage(Storage);
  Assert.equal(GetEditorTranslation('Editor.Settings.Title'), 'Параметры редактора');
  setLanguage('en', Storage);
  Assert.equal(LocalizeEditorText('Editor.Settings.Title'), 'Editor Settings');
});

Test('invalid or outdated studio imports are rejected as a whole', () => {
  const Package = ExportEditorTranslations();
  Package.entries = Package.entries.slice(0, 2);
  Package.entries[0].text = 'Changed';
  Package.entries[1].source = 'Old source';
  const Before = JSON.stringify(ExportEditorTranslations());
  Assert.throws(() => ImportEditorTranslations(Package, Storage));
  Assert.equal(JSON.stringify(ExportEditorTranslations()), Before);
  Package.entries[1].source = EDITOR_CATALOG.entries[Package.entries[1].id].source;
  Package.entries.push(Package.entries[0]);
  Assert.throws(() => ImportEditorTranslations(Package, Storage));
});

Test('imports preserve Qt and named interpolation parameters', () => {
  const [Identifier, Entry] = Object.entries(EDITOR_CATALOG.entries).find(([, Value]) => Value.source.includes('%1'));
  Assert.throws(() => SaveEditorTranslation(Identifier, 'Missing parameter', Storage));
  SaveEditorTranslation(Identifier, Entry.translations.ru, Storage);
  Assert.equal(GetEditorTranslation(Identifier), Entry.translations.ru);
  const Before = GetEditorTranslation('Editor.Settings.Title');
  Assert.throws(() => SaveEditorTranslation('Editor.Settings.Title', 'Changed', {setItem() { throw new Error('Storage denied'); }}));
  Assert.equal(GetEditorTranslation('Editor.Settings.Title'), Before);
});

Test('studio import preview marks concurrent translation changes without applying them', () => {
  const Package = ExportEditorTranslations();
  Package.entries = Package.entries.filter(Entry => Entry.id === 'Editor.Settings.Title');
  Package.entries[0].text = 'Перевод студии';
  SaveEditorTranslation('Editor.Settings.Title', 'Локальная правка', Storage);
  const Preview = PreviewEditorTranslations(Package);
  Assert.equal(Preview.length, 1);
  Assert.equal(Preview[0].conflict, true);
  Assert.equal(Preview[0].current, 'Локальная правка');
  Assert.equal(GetEditorTranslation('Editor.Settings.Title'), 'Локальная правка');
});
