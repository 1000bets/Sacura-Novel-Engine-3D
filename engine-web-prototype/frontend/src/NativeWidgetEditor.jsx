import React, {useContext, useMemo, useState} from 'react';
import {NativeAssetsContext} from './NativeAssets.jsx';
import WidgetWorkspace from './WidgetWorkspace.jsx';
import WidgetRenderer from './WidgetRenderer.jsx';
import NativeAssetCreateDialog from './NativeAssetCreateDialog.jsx';
import {t as Translate, useLocale} from './i18n.jsx';
import './nativeWidgetEditor.css';

export function NativeWidgetLibraries({Assets, OpenAsset, Create, CreationTypes, Disabled}) {
  useLocale();
  const [CreationDocument, SetCreationDocument] = useState(null);
  const [CreationType, SetCreationType] = useState(null);
  const [ErrorMessage, SetErrorMessage] = useState('');
  const Libraries = Assets.filter(Asset => Asset.typeId === 'WidgetLibrary');
  async function BeginCreate(Event) {
    SetCreationDocument(Event.currentTarget.ownerDocument);
    try {
      const Types = await CreationTypes();
      const Type = Types.find(Entry => Entry.typeId === 'WidgetLibrary');
      if (!Type) {
        throw new Error('Widget library factory is unavailable. Rebuild the native host.');
      }
      SetCreationType(Type);
    } catch (Failure) {
      SetErrorMessage(Failure.message);
    }
  }
  return <section className="native-widget-libraries">
    <h2>{Translate('Editor.Widgets.Title')}</h2>
    <p>{Translate('Editor.Widgets.Description')}</p>
    <button disabled={Disabled} onClick={BeginCreate}>{Translate('Editor.Widgets.Create')}</button>
    {!Libraries.length && <p role="status">{Translate('Editor.Widgets.Empty')}</p>}
    {Libraries.map(Asset => <button className="native-widget-library" key={Asset.id} onClick={() => OpenAsset({assetId:Asset.id})}><strong>{Asset.name}</strong><span>{Asset.path}</span></button>)}
    {ErrorMessage && <p role="alert">{ErrorMessage}</p>}
    {CreationType && <NativeAssetCreateDialog ownerDocument={CreationDocument} type={CreationType} folder="Game/UI" create={Create} close={() => SetCreationType(null)}/>}
  </section>;
}

export default function NativeWidgetEditor({Snapshot, Execute, Asset, ErrorMessage, DocumentId}) {
  useLocale();
  const AssetContext = useContext(NativeAssetsContext);
  function ImageSource(Identifier) {
    return `sacura://native-asset/${AssetContext.session}/${Identifier}`;
  }
  const [PreviewIdentifier, SetPreviewIdentifier] = useState(null);
  const [PreviewMessage, SetPreviewMessage] = useState('');
  const Project = useMemo(() => ({...Snapshot.document, variables:{}, objects:[], chapters:[], events:[], title:Asset?.name || '', gameplay:{items:[], recipes:[]}}), [Snapshot.document, Asset?.name]);
  const Preview = Project.widgets.find(Widget => Widget.id === PreviewIdentifier);
  function Change(Mutate) {
    Execute('edit', Current => {
      const Next = structuredClone(Current);
      Mutate(Next);
      return Next;
    });
  }
  return <main className="native-widget-editor" data-document-tab={DocumentId} onKeyDown={Event => {
    if (!(Event.ctrlKey || Event.metaKey)) {
      return;
    }
    let Operation;
    if (Event.code === 'KeyS') {
      Operation = 'save';
    }
    if (Event.code === 'KeyZ') {
      Operation = 'undo';
      if (Event.shiftKey) {
        Operation = 'redo';
      }
    }
    if (Event.code === 'KeyY') {
      Operation = 'redo';
    }
    if (Operation) {
      Event.preventDefault();
      Event.stopPropagation();
      Execute(Operation);
    }
  }}>
    <header className="native-document-toolbar">
      <span title={Asset?.path}>{Asset?.path}</span>
      {Snapshot.dirty && <span aria-label={Translate('Несохранённые изменения')}>*</span>}
      <button disabled={Snapshot.readOnly || !Snapshot.dirty} onClick={() => Execute('save')}>{Translate('Сохранить')}</button>
      <button disabled={Snapshot.readOnly || !Snapshot.canUndo} onClick={() => Execute('undo')}>{Translate('Отменить')}</button>
      <button disabled={Snapshot.readOnly || !Snapshot.canRedo} onClick={() => Execute('redo')}>{Translate('Повторить')}</button>
      <button disabled={Snapshot.dirty} onClick={() => Execute('reload')}>{Translate('Перечитать')}</button>
    </header>
    {Snapshot.readOnly && <p role="status">{Translate('Editor.Widgets.ReadOnly')}</p>}
    {ErrorMessage && <p role="alert">{ErrorMessage}</p>}
    {!Preview && <WidgetWorkspace imageSource={ImageSource} native project={Project} onChange={Change} running={Snapshot.readOnly} onPreview={Widget => {SetPreviewIdentifier(Widget.id); SetPreviewMessage('');}}/>}
    {Preview && <section className="native-widget-preview">
      <header><button onClick={() => SetPreviewIdentifier(null)}>{Translate('Editor.Widgets.Back')}</button><span>{Translate('Editor.Widgets.PreviewNote')}</span></header>
      <div className="native-widget-preview-canvas"><WidgetRenderer imageSource={ImageSource} widget={Preview} context={{project:Project, variables:{}, title:Asset?.name, speaker:'Алиса', text:Translate('Пример реплики'), paused:true, ready:true, canAdvance:true, choices:[{id:'sample',label:Translate('Пример ответа')}]}} onAction={Action => SetPreviewMessage(Action)} onChoice={Identifier => SetPreviewMessage(Identifier)}/></div>
      <output>{PreviewMessage}</output>
    </section>}
  </main>;
}
