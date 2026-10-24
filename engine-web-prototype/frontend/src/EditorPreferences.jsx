import React, {useRef, useState} from 'react';
import {EDITOR_CATALOG, LocalizeEditorText, GetEditorTranslation, SaveEditorTranslation, ExportEditorTranslations, ImportEditorTranslations, PreviewEditorTranslations, PersistEditorPreferences, useLocale} from './i18n.jsx';
import LanguagePicker from './LanguagePicker.jsx';
import {Button} from './StudioParts.jsx';
import './editorPreferences.css';

export default function EditorPreferences() {
  useLocale();
  const [Search, SetSearch] = useState('');
  const [Identifier, SetIdentifier] = useState('Editor.Settings.Title');
  const [Translation, SetTranslation] = useState(GetEditorTranslation(Identifier));
  const [Status, SetStatus] = useState('');
  const [PendingImport, SetPendingImport] = useState(null);
  const FileInput = useRef(null);
  const Entry = EDITOR_CATALOG.entries[Identifier];
  const Label = Name => LocalizeEditorText('Editor.Settings.' + Name);
  function SelectEntry(Value) {
    SetIdentifier(Value);
    SetTranslation(GetEditorTranslation(Value));
    SetStatus('');
  }
  function Export() {
    const Address = URL.createObjectURL(new Blob([JSON.stringify(ExportEditorTranslations(), null, 2)], {type: 'application/json'}));
    const Link = document.createElement('a');
    Link.href = Address;
    Link.download = 'Editor.ru.json';
    Link.click();
    setTimeout(() => URL.revokeObjectURL(Address), 1000);
  }
  async function Import(Event) {
    const File = Event.target.files[0];
    if (!File) {
      return;
    }
    try {
      const Package = JSON.parse(await File.text());
      const Changes = PreviewEditorTranslations(Package);
      SetPendingImport({Package, Changes});
      SetStatus('');
    } catch {
      SetStatus('ImportError');
    }
    Event.target.value = '';
  }
  const Matches = Object.entries(EDITOR_CATALOG.entries).filter(([Key, Value]) => (Key + ' ' + Value.source + ' ' + GetEditorTranslation(Key)).toLowerCase().includes(Search.toLowerCase())).slice(0, 100);
  return <section className="settings-workspace editor-preferences" aria-label={Label('Title')} onKeyDown={Event=>Event.stopPropagation()}>
      <header><h2>{Label('Title')}</h2></header>
      <div className="editor-preferences-body">
        <label>{Label('Language')}<LanguagePicker/></label>
        <p>{Label('LanguageHelp')}</p>
        <details><summary>{Label('Translations')}</summary>
          <input aria-label={Label('Search')} placeholder={Label('Search')} value={Search} onChange={Event => SetSearch(Event.target.value)}/>
          <select size={6} aria-label={Label('Translations')} value={Identifier} onChange={Event => SelectEntry(Event.target.value)}>
            {!Matches.some(([Key]) => Key === Identifier) && <option value={Identifier}>{Entry.source}</option>}
            {Matches.map(([Key, Value]) => <option key={Key} value={Key}>{Value.source}</option>)}
          </select>
          <small>{Identifier}</small>
          <label>{Label('Source')}<textarea readOnly value={Entry.source}/></label>
          <label>{Label('Russian')}<textarea value={Translation} onChange={Event => SetTranslation(Event.target.value)}/></label>
          <div className="editor-preferences-actions">
            <Button onClick={async () => {
              try {
                SaveEditorTranslation(Identifier, Translation);
                await PersistEditorPreferences();
                SetStatus('Saved');
              } catch {
                SetStatus('ImportError');
              }
            }}>{Label('Save')}</Button>
            <Button onClick={() => SetTranslation(Entry.translations.ru || '')}>{Label('Reset')}</Button>
            <Button onClick={Export}>{Label('Export')}</Button>
            <Button onClick={() => FileInput.current.click()}>{Label('Import')}</Button>
            <input ref={FileInput} hidden type="file" accept=".json,application/json" onChange={Import}/>
          </div>
          {PendingImport && <section aria-label={Label('ImportPreview')}>
            <h3>{Label('ImportPreview')} ({PendingImport.Changes.length})</h3>
            <p>{Label('ImportHelp')}</p>
            <ul>{PendingImport.Changes.slice(0, 50).map(Change => <li key={Change.id}>
              <strong>{Change.source}</strong><div>{Change.current} → {Change.text}</div>
              {Change.conflict && <span>{Label('Conflict')}</span>}
            </li>)}</ul>
            <Button onClick={async () => {
              try {
                ImportEditorTranslations(PendingImport.Package);
                await PersistEditorPreferences();
                SetTranslation(GetEditorTranslation(Identifier));
                SetPendingImport(null);
                SetStatus('Saved');
              } catch {
                SetStatus('ImportError');
              }
            }}>{Label('ApplyImport')}</Button>
            <Button onClick={() => SetPendingImport(null)}>{Label('Cancel')}</Button>
          </section>}
          {Status && <p role="status">{Label(Status)}</p>}
        </details>
      </div>
  </section>;
}
