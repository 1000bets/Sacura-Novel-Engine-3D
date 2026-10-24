import EditorCatalog from '../../../Root/EditorCore/resources/Localization/Editor.json' with {type:'json'};

export const EDITOR_CATALOG = EditorCatalog;
const CatalogAliases = new Map();
for (const [Identifier, Entry] of Object.entries(EditorCatalog.entries)) {
  for (const Alias of Entry.aliases || []) {
    CatalogAliases.set(Alias, Identifier);
  }
}
const english = Object.fromEntries(Object.values(EditorCatalog.entries).filter(Entry => Entry.translations.ru).map(Entry => [Entry.translations.ru, Entry.source]));
export const TRANSLATION_STORAGE_KEY = 'sacura-editor-translations-v1';
let TranslationOverrides = {};


export const LANGUAGE_STORAGE_KEY = 'sacura-language-v1';
export const LANGUAGES = Object.freeze([{id:'en',name:'English'},{id:'ru',name:'Русский'}]);
export const DEFAULT_LANGUAGE = 'en';
let language = DEFAULT_LANGUAGE;
const listeners = new Set();
let LocaleRevision = 0;
export const GetLocaleRevision = () => LocaleRevision;
export const getLanguage = () => language;
export const localeTag = () => language === 'en' ? 'en-US' : 'ru-RU';
export function subscribeLanguage(listener) { listeners.add(listener); return () => listeners.delete(listener); }
export function readLanguage(storage) {
  try { const value=storage.getItem(LANGUAGE_STORAGE_KEY); return LANGUAGES.some(item=>item.id===value)?value:DEFAULT_LANGUAGE; }
  catch { return DEFAULT_LANGUAGE; }
}
export function setLanguage(id, storage, root = globalThis.document?.documentElement) {
  if (!LANGUAGES.some(item=>item.id===id)) return false;
  language=id;
  if(root)root.lang=id;
  let saved=true;
  try { (storage ?? globalThis.localStorage)?.setItem(LANGUAGE_STORAGE_KEY,id); } catch { saved=false; }
  LocaleRevision++;
  for(const listener of listeners)listener();
  return saved;
}
export function initializeLanguage(storage) {
  try { language=readLanguage(storage ?? globalThis.localStorage); } catch { language=DEFAULT_LANGUAGE; }
  try { TranslationOverrides = JSON.parse((storage ?? globalThis.localStorage)?.getItem(TRANSLATION_STORAGE_KEY) || '{}'); } catch { TranslationOverrides = {}; }
  if(globalThis.document)document.documentElement.lang=language;
  return language;
}
// Translate explicit UI copy only. Never pass authored dialogue or project names here.
export function t(source, values) {
  if(typeof source!=='string')return source;
  const key=source.trim();
  if(!key)return source;
  const Identifier = CatalogAliases.get(key) || key;
  const translated = LocalizeEditorText(Identifier, key);
  const padded=source.slice(0,source.length-source.trimStart().length)+translated+source.slice(source.trimEnd().length);
  return values ? padded.replace(/\{(\d+)\}/g,(match,index)=>String(values[index]??match)) : padded;
}
// Mark option labels coming from authored data so a name matching UI copy stays intact.
export const literalLabel = text => ({text});
export const optionLabel = value => value && typeof value==='object' && 'text' in value ? value.text : t(value);
export function plural(count, one, few, many, englishOne, englishMany=englishOne+'s') {
  if(language==='en')return `${count} ${count===1?englishOne:englishMany}`;
  return `${count} ${{one,few,many,other:few}[new Intl.PluralRules('ru').select(count)]}`;
}
// Diagnostics can outlive a language switch. Match only known UI templates;
// interpolate their captured values verbatim, including filenames and authored names.
function diagnosticPattern(source,target) {
 const slots=[];
 const escaped=source.replace(/[.*+?^${}()|[\]\\]/g,'\\$&').replace(/\\\{(\d+)\\\}/g,(_,index)=>{slots.push(Number(index));return '(.*?)';});
 return {regex:new RegExp('^'+escaped+'$'),slots,target};
}
const entries=Object.entries(english);
const reverse=new Map(entries.map(([ru,en])=>[en,ru]));
const patterns={
 en:entries.filter(([key])=>/\{\d+\}/.test(key)).map(([ru,en])=>diagnosticPattern(ru,en)),
 ru:entries.filter(([key])=>/\{\d+\}/.test(key)).map(([ru,en])=>diagnosticPattern(en,ru)),
};
export function message(source) {
 if(typeof source!=='string')return source;
 const key=source.trim(),exact=language==='en'?english[key]:reverse.get(key);
 if(exact)return source.slice(0,source.length-source.trimStart().length)+exact+source.slice(source.trimEnd().length);
 for(const {regex,slots,target} of patterns[language]){
  const match=regex.exec(source);
  if(match){const values={};slots.forEach((slot,i)=>values[slot]=match[i+1]);return target.replace(/\{(\d+)\}/g,(placeholder,index)=>values[index]??placeholder);}
 }
 return source;
}

export function LocalizeEditorText(Identifier, Fallback = Identifier, Values) {
  const Entry = EditorCatalog.entries[Identifier];
  let Result = Fallback;
  if (Entry) {
    Result = Entry.source;
    if (language !== 'en') {
      const Override = TranslationOverrides[Identifier];
      if (Override?.source === Entry.source && typeof Override.text === 'string' && Override.text) {
        Result = Override.text;
      } else if (Entry.translations[language]) {
        Result = Entry.translations[language];
      }
    }
  }
  if (Values) {
    Result = Result.replace(/\{([a-zA-Z_][a-zA-Z_0-9]*|[0-9]+)\}/g, (Placeholder, Name) => String(Values[Name] ?? Placeholder));
  }
  return Result;
}

export function GetEditorTranslation(Identifier) {
  const Entry = EditorCatalog.entries[Identifier];
  const Override = TranslationOverrides[Identifier];
  if (Override?.source === Entry?.source) {
    return Override.text;
  }
  return Entry?.translations.ru || '';
}

function TranslationSlots(Text) {
  return JSON.stringify((Text.match(/\{[a-zA-Z_0-9]+\}|%[1-9][0-9]*/g) || []).sort());
}

function ValidateEditorTranslations(Package) {
  if (Package?.schemaVersion !== 1 || Package.sourceLanguage !== 'en' || Package.language !== 'ru' || !Array.isArray(Package.entries)) {
    throw new Error('Invalid translation package');
  }
  const Updated = {...TranslationOverrides};
  const Identifiers = new Set();
  for (const Entry of Package.entries) {
    const Source = EditorCatalog.entries[Entry.id];
    if (!Source || Identifiers.has(Entry.id) || Source.source !== Entry.source || typeof Entry.text !== 'string' || Entry.text.length > 16384 || (Entry.text && TranslationSlots(Entry.text) !== TranslationSlots(Source.source))) {
      throw new Error('Invalid or outdated translation entry');
    }
    Identifiers.add(Entry.id);
    Updated[Entry.id] = {source: Entry.source, text: Entry.text};
  }
  return Updated;
}

export function PreviewEditorTranslations(Package) {
  ValidateEditorTranslations(Package);
  return Package.entries.filter(Entry => Entry.text !== GetEditorTranslation(Entry.id)).map(Entry => ({...Entry, current: GetEditorTranslation(Entry.id), conflict: typeof Entry.originalTranslation === 'string' && Entry.originalTranslation !== GetEditorTranslation(Entry.id)}));
}

export function ImportEditorTranslations(Package, Storage = globalThis.localStorage) {
  const Updated = ValidateEditorTranslations(Package);
  Storage.setItem(TRANSLATION_STORAGE_KEY, JSON.stringify(Updated));
  TranslationOverrides = Updated;
  LocaleRevision++;
  for (const Listener of listeners) {
    Listener();
  }
}

export function ExportEditorTranslations() {
  return {schemaVersion: 1, sourceLanguage: 'en', language: 'ru', entries: Object.entries(EditorCatalog.entries).map(([Identifier, Entry]) => ({id: Identifier, source: Entry.source, text: GetEditorTranslation(Identifier), originalTranslation: GetEditorTranslation(Identifier)}))};
}

export function SaveEditorTranslation(Identifier, Text, Storage) {
  ImportEditorTranslations({schemaVersion: 1, sourceLanguage: 'en', language: 'ru', entries: [{id: Identifier, source: EditorCatalog.entries[Identifier]?.source, text: Text}]}, Storage);
}

if (globalThis.addEventListener) {
  globalThis.addEventListener('storage', Event => {
    if (Event.key === LANGUAGE_STORAGE_KEY || Event.key === TRANSLATION_STORAGE_KEY) {
      initializeLanguage();
      LocaleRevision++;
      for (const Listener of listeners) {
        Listener();
      }
    }
  });
}

function ApplyEditorPreferences(Values) {
  TranslationOverrides = Values.overrides || {};
  try {
    globalThis.localStorage?.setItem(TRANSLATION_STORAGE_KEY, JSON.stringify(TranslationOverrides));
  } catch {
  }
  setLanguage(Values.language);
}

export async function InitializeEditorPreferences() {
  const Desktop = globalThis.window?.sacuraDesktop;
  if (!Desktop?.editorLocalization) {
    return;
  }
  Desktop.onEditorLocalization?.(ApplyEditorPreferences);
  ApplyEditorPreferences(await Desktop.editorLocalization());
}

export async function PersistEditorPreferences() {
  const Desktop = globalThis.window?.sacuraDesktop;
  if (Desktop?.editorLocalization) {
    await Desktop.editorLocalization({language, overrides: TranslationOverrides});
  }
}
