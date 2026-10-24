import React, {useEffect, useState} from 'react';
import {LocalizeEditorText, useLocale} from './i18n.jsx';
import {Button} from './StudioParts.jsx';
import './editorPreferences.css';

export default function ProjectSettings({Adapter, Disabled, Connected, Session}) {
  useLocale();
  const [Settings, SetSettings] = useState(null);
  const [Busy, SetBusy] = useState(false);
  const [Error, SetError] = useState('');
  const [Saved, SetSaved] = useState(false);
  const Label = Name => LocalizeEditorText('Editor.ProjectSettings.' + Name);
  useEffect(() => {
    SetSettings(null);
    SetError('');
    SetSaved(false);
    if (!Connected) {
      return;
    }
    let Active = true;
    Adapter.command('project.settings.get').then(Value => {
      if (Active) { SetSettings(Value); }
    }).catch(Failure => {
      if (Active) { SetError(Failure.message); }
    });
    return () => { Active = false; };
  }, [Adapter, Connected, Session]);
  function Change(Name, Value) {
    SetSettings(Current => ({...Current, [Name]:Value}));
    SetSaved(false);
  }
  async function Save(Event) {
    Event.preventDefault();
    SetBusy(true);
    SetError('');
    SetSaved(false);
    try {
      const {name, startupScene, startupStory} = Settings;
      SetSettings(await Adapter.command('project.settings.set', {settings:{name:name.trim(), startupScene:startupScene.trim(), startupStory:startupStory.trim()}}));
      SetSaved(true);
    } catch (Failure) {
      SetError(Failure.message);
    } finally {
      SetBusy(false);
    }
  }
  return <section className="settings-workspace editor-preferences" aria-label={Label('Title')} onKeyDown={Event=>Event.stopPropagation()}>
      <header><h2>{Label('Title')}</h2></header>
      {!Settings && !Error && <p role="status">{Label('Loading')}</p>}
      {Settings && <form onSubmit={Save}>
        <fieldset className="editor-preferences-body" style={{border:0, margin:0, minWidth:0}} disabled={Busy || Disabled}>
          <label>{Label('Name')}<input name="name" required maxLength={1024} value={Settings.name} onChange={Event=>Change('name',Event.target.value)}/></label>
          <label>{Label('Version')}<input readOnly value={Settings.engineVersion}/></label>
          <label>{Label('Scene')}<input name="startupScene" value={Settings.startupScene} onChange={Event=>Change('startupScene',Event.target.value)}/></label>
          <label>{Label('Story')}<input name="startupStory" value={Settings.startupStory} onChange={Event=>Change('startupStory',Event.target.value)}/></label>
          <p>{Label('Help')}</p>
          <Button type="submit" disabled={!Settings.name.trim()}>{Label('Save')}</Button>
        </fieldset>
      </form>}
      {Disabled && <p>{Label('Play')}</p>}
      {Error && <p className="project-dialog-error" role="alert">{Label('Error')} {Error}</p>}
      {Saved && <p role="status">{Label('Saved')}</p>}
  </section>;
}
