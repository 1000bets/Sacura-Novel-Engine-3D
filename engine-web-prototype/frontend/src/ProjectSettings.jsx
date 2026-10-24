import React, {useEffect, useState} from 'react';
import {LocalizeEditorText, useLocale} from './i18n.jsx';
import {Button} from './StudioParts.jsx';
import './editorPreferences.css';
import {AssetReferenceField} from './NativeAssets.jsx';

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
      const {name, startupScene, startupStory, narrativeWidgets, narrativeInputActions, startupInputContexts} = Settings;
      SetSettings(await Adapter.command('project.settings.set', {settings:{name:name.trim(), startupScene:startupScene.trim(), startupStory:startupStory.trim(), narrativeWidgets, narrativeInputActions, startupInputContexts}}));
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
          <h3>{Label('NarrativeWidgets')}</h3>
          {['dialogue', 'hud', 'mainMenu', 'pauseMenu'].map(Slot => <div key={Slot}>
            <span>{Label('Widget.' + Slot)}</span>
            <AssetReferenceField label={Label('Widget.' + Slot)} allowedTypes={['Widget']} assetId={Settings.narrativeWidgets?.[Slot] || null}
              disabled={Busy || Disabled} onChange={Identity => Change('narrativeWidgets', {...Settings.narrativeWidgets, [Slot]:Identity.assetId || ''})}/>
          </div>)}
          <h3>{Label('NarrativeInput')}</h3>
          {['advance', 'nextChoice', 'previousChoice', 'confirmChoice', 'pause'].map(Role => <label key={Role}>{Label('Action.' + Role)}
            <select aria-label={Label('Action.' + Role)} value={Settings.narrativeInputActions?.[Role] || ''}
              onChange={Event => Change('narrativeInputActions', {...Settings.narrativeInputActions, [Role]:Event.target.value})}>
              <option value="">{Label('Unassigned')}</option>
              {(Settings.inputActions || []).map(Action => <option key={Action.id} value={Action.id}>{Action.name}</option>)}
            </select>
          </label>)}
          <h3>{Label('StartupInput')}</h3>
          {(Settings.startupInputContexts || []).map((Activation, Index) => <div className="project-startup-input" key={Index}>
            <select aria-label={Label('Context')} value={Activation.Context} onChange={Event => Change('startupInputContexts', Settings.startupInputContexts.map((Entry, EntryIndex) => {
              if (Index === EntryIndex) { return {...Entry, Context:Event.target.value}; }
              return Entry;
            }))}>
              {(Settings.inputContexts || []).map(Context => <option key={Context.id} value={Context.id}>{Context.name}</option>)}
            </select>
            <label>{Label('Priority')}<input type="number" step="1" value={Activation.Priority} onChange={Event => {
              const Priority = Number(Event.target.value);
              if (Number.isSafeInteger(Priority)) {
                Change('startupInputContexts', Settings.startupInputContexts.map((Entry, EntryIndex) => {
                  if (Index === EntryIndex) { return {...Entry, Priority}; }
                  return Entry;
                }));
              }
            }}/></label>
            <Button type="button" onClick={() => Change('startupInputContexts', Settings.startupInputContexts.filter((Entry, EntryIndex) => EntryIndex !== Index))}>{Label('RemoveContext')}</Button>
          </div>)}
          <Button type="button" disabled={!Settings.inputContexts?.some(Context => !Settings.startupInputContexts.some(Entry => Entry.Context === Context.id))} onClick={() => {
            const Context = Settings.inputContexts.find(Candidate => !Settings.startupInputContexts.some(Entry => Entry.Context === Candidate.id));
            if (Context) { Change('startupInputContexts', [...Settings.startupInputContexts, {Context:Context.id, Priority:0}]); }
          }}>{Label('AddContext')}</Button>
          <p>{Label('RuntimeHelp')}</p>
          <p>{Label('Help')}</p>
          <Button type="submit" disabled={!Settings.name.trim()}>{Label('Save')}</Button>
        </fieldset>
      </form>}
      {Disabled && <p>{Label('Play')}</p>}
      {Error && <p className="project-dialog-error" role="alert">{Label('Error')} {Error}</p>}
      {Saved && <p role="status">{Label('Saved')}</p>}
  </section>;
}
