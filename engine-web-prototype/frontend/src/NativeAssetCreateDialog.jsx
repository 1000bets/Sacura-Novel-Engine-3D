import {ActiveWorkspaceDocument} from './workspaceDocuments.js';
import React, {useEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {AssetReferenceField} from './NativeAssets.jsx';
import {SchemaValue} from './SchemaFields.jsx';
import ColorField from './ColorField.jsx';

export default function NativeAssetCreateDialog({type: Type, folder: Folder, selectedAsset: SelectedAsset, create: Create, close: Close, ownerDocument:InvokingDocument}) {
  const [OwnerDocument] = useState(() => InvokingDocument || ActiveWorkspaceDocument());
  const [Name, SetName] = useState('');
  const IsPythonScript = Type.typeId === 'PythonScript';
  let DestinationFolder = Folder || 'Game';
  if (IsPythonScript && DestinationFolder !== 'Game/Scripts' && !DestinationFolder.startsWith('Game/Scripts/')) {
    DestinationFolder = 'Game/Scripts';
  }
  if (Type.typeId === 'Widget' && DestinationFolder === 'Game') {
    DestinationFolder = 'Game/UI';
  }
  const [Settings, SetSettings] = useState(() => {
    const Defaults = Object.fromEntries(Type.settings.map(Field => [Field.id, Field.value]));
    if (SelectedAsset) {
      for (const Field of Type.settings) {
        if (!Field.assetTypes || Defaults[Field.id]?.assetId) {continue;}
        if (Field.assetTypes.includes(SelectedAsset.typeId)) {Defaults[Field.id] = {assetId:SelectedAsset.id,subAssetId:null};}
        else {
          const Subasset = SelectedAsset.subassets?.find(Entry => Field.assetTypes.includes(Entry.typeId));
          if (Subasset) {Defaults[Field.id] = {assetId:SelectedAsset.id,subAssetId:Subasset.id};}
        }
      }
    }
    return Defaults;
  });
  const ChangeSetting = (Identifier, Value) => SetSettings(Previous => ({...Previous,[Identifier]:Value}));
  const MissingRequired = Type.settings.some(Field => Field.required && (Settings[Field.id] == null || Settings[Field.id] === '' || Field.assetTypes && !Settings[Field.id]?.assetId));
  const [Busy, SetBusy] = useState(false);
  const [ErrorMessage, SetErrorMessage] = useState('');
  const Input = useRef(null);
  useEffect(() => {Input.current?.focus();}, []);
  async function Submit(Event) {
    Event.preventDefault();
    if (Busy) { return; }
    SetBusy(true); SetErrorMessage('');
    try {
      let Directory = DestinationFolder.replace(/^Game\/?/, '');
      if (Directory) { Directory += '/'; }
      await Create({factoryId:Type.id, path:Directory + Name.trim() + Type.extension, settings:Settings});
      Close();
    } catch (ErrorValue) { SetErrorMessage(ErrorValue.message); }
    finally { SetBusy(false); }
  }
  return createPortal(<div className="asset-create-backdrop" onPointerDown={Event => {if (Event.target === Event.currentTarget && !Busy) {Close();}}}>
    <form className="asset-create-dialog" role="dialog" aria-modal="true" aria-label={'Создать: '+Type.label} onSubmit={Submit}
      onContextMenu={Event => Event.stopPropagation()} onKeyDown={Event => {Event.stopPropagation();if (Event.key === 'Escape' && !Busy) {Event.stopPropagation();Close();}}}>
      <strong>Создать: {Type.label}</strong><small>/{DestinationFolder}</small>
      <label>Имя ассета<input ref={Input} aria-label="Имя ассета" aria-describedby={IsPythonScript ? 'python-script-name-help' : undefined} pattern={IsPythonScript ? '[A-Za-z_][A-Za-z0-9_]*' : undefined} title={IsPythonScript ? 'Латинские буквы, цифры и _. Первый символ — буква или _.' : undefined} required disabled={Busy} value={Name} onChange={Event => SetName(Event.target.value)}/></label>
      {IsPythonScript && <small id="python-script-name-help">Имя модуля без .py: латинские буквы, цифры и _. Первый символ — буква или _. Скрипт создаётся в папке Scripts.</small>}
      {Type.settings.map(Field => {
        const Value = Settings[Field.id];
        if (Field.typeId === 'engine.Color') {
          return <ColorField key={Field.id} label={Field.label} value={Value} disabled={Busy} onChange={Next => ChangeSetting(Field.id, Next)}/>;
        }
        if (Field.assetTypes) {return <div key={Field.id}><span>{Field.label}</span><AssetReferenceField label={Field.label} assetId={Value?.assetId} subAssetId={Value?.subAssetId} allowedTypes={Field.assetTypes} disabled={Busy} onChange={Identity => ChangeSetting(Field.id,Identity)}/></div>;}
        let Schema = {type:typeof Value,label:Field.label};
        if (Field.enumValues) {Schema.enum = Field.enumValues;}
        if (typeof Value === 'number') {Schema.type = 'number';}
        if (Array.isArray(Value) && Value.every(Element => typeof Element === 'number')) {Schema = {type:'vector',size:Value.length,label:Field.label};}
        if (Field.range) {Schema.min = Field.range[0];Schema.max = Field.range[1];}
        if (['string','boolean','number','vector'].includes(Schema.type)) {return <fieldset disabled={Busy} key={Field.id}><SchemaValue schema={Schema} value={Value} onChange={Next => ChangeSetting(Field.id,Next)}/></fieldset>;}
        return <label key={Field.id}>{Field.label}<textarea disabled={Busy} defaultValue={JSON.stringify(Value,null,2)} onChange={Event => {try {ChangeSetting(Field.id,JSON.parse(Event.target.value));Event.target.setCustomValidity('');}catch {Event.target.setCustomValidity('Введите корректный JSON');}}}/></label>;
      })}
      {ErrorMessage && <p role="alert">{ErrorMessage}</p>}
      <footer><button type="button" disabled={Busy} onClick={Close}>Отмена</button><button type="submit" disabled={Busy || !Name.trim() || MissingRequired}>{Busy && 'Создание…'}{!Busy && 'Создать'}</button></footer>
    </form>
  </div>,OwnerDocument.body);
}
