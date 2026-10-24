import React from 'react';
import {Icon} from './StudioParts.jsx';
import {LocalizeEditorText, useLocale, t as Translate} from './i18n.jsx';
import ReflectedPropertyTree from './ReflectedPropertyTree.jsx';
import './reflectedAssetEditor.css';

export default function NativeReflectedAssetEditor({Snapshot, Asset, Execute, Error, DocumentId}) {
  useLocale();
  return <main className="native-reflected-asset-editor" data-document-tab={DocumentId} onKeyDown={Event => {
    if (!Event.defaultPrevented && (Event.ctrlKey || Event.metaKey) && ['KeyS', 'KeyZ', 'KeyY'].includes(Event.code)) {
      Event.preventDefault();
      let Operation = 'undo';
      if (Event.code === 'KeyS') {
        Operation = 'save';
      } else if (Event.code === 'KeyY' || Event.shiftKey) {
        Operation = 'redo';
      }
      if (!Snapshot.readOnly) {
        Event.target.blur?.();
        Execute(Operation);
      }
    }
  }}>
    <header className="native-document-toolbar">
      <button title={Translate('Сохранить (Ctrl+S)')} aria-label={Translate('Сохранить ассет')} disabled={Snapshot.readOnly || !Snapshot.dirty} onClick={() => Execute('save')}><Icon name="Save"/></button>
      <strong title={Asset?.path}>{Asset?.name || Snapshot.type}</strong>
      {Snapshot.dirty && <span aria-label={Translate('Несохранённые изменения')}>*</span>}
      <details><summary>{Translate('Правка')}</summary><button disabled={Snapshot.readOnly || !Snapshot.canUndo} onClick={() => Execute('undo')}>{Translate('Отменить')}</button><button disabled={Snapshot.readOnly || !Snapshot.canRedo} onClick={() => Execute('redo')}>{Translate('Повторить')}</button><button disabled={Snapshot.dirty} onClick={() => Execute('reload')}>{Translate('Перечитать')}</button></details>
    </header>
    {Error && <p className="reflected-document-error" role="alert">{Error}</p>}
    <section className="native-reflected-asset-fields" aria-label={LocalizeEditorText('Editor.Properties.Title', 'Asset properties')}>
      {Snapshot.schema && <ReflectedPropertyTree Schema={Snapshot.schema} Value={Snapshot.document} Disabled={Snapshot.readOnly} Change={Change => Execute('property', Change)}/>}
      {!Snapshot.schema && <><dl><dt>{LocalizeEditorText('Editor.Properties.Type', 'Asset type')}</dt><dd>{Snapshot.type}</dd><dt>{LocalizeEditorText('Editor.Properties.Path', 'Path')}</dt><dd>{Asset?.path}</dd></dl><p>{LocalizeEditorText('Editor.Properties.Unavailable', 'No reflected document is registered for this asset type.')}</p></>}
    </section>
  </main>;
}
