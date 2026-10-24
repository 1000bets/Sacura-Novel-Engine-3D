import React, {useId,useState} from 'react';
import {Languages} from 'lucide-react';
import {LANGUAGES, t, setLanguage, useLocale, PersistEditorPreferences, LocalizeEditorText} from './i18n.jsx';
import './languagePicker.css';

export default function LanguagePicker({onOpen}) {
 const language=useLocale(),id=useId(),[saved,setSaved]=useState(true);
 return <div className="language-picker">
  <label htmlFor={id} title={t('Язык интерфейса')}><Languages size={15} aria-hidden="true"/><span className="sr-only">{t('Язык интерфейса')}</span></label>
  <select id={id} aria-label={t('Язык интерфейса')} value={language} onFocus={onOpen} onChange={async Event => {
    const Saved = setLanguage(Event.target.value);
    try {
      await PersistEditorPreferences();
      setSaved(Saved);
    } catch {
      setSaved(false);
    }
  }}>
   {LANGUAGES.map(item=><option key={item.id} value={item.id}>{item.name}</option>)}
  </select>
  {!saved&&<span className="language-save-error" role="status">{LocalizeEditorText('Editor.Settings.StorageError')}</span>}
 </div>;
}
