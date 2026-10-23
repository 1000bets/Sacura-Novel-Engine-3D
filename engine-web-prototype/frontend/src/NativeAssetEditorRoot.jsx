import React, { useEffect, useRef, useState } from 'react';
import { createNativeBridgeClient } from './nativeBridgeClient.js';
import { NativeAssetsContext } from './NativeAssets.jsx';
import NativeAssetWorkspace from './NativeAssetWorkspace.jsx';
import NativeViewport from './NativeViewport.jsx';
import { Icon } from './StudioParts.jsx';
import NativeShaderEditor from './NativeShaderEditor.jsx';
import './nativeMaterialEditor.css';
import NativeTextureEditor from './NativeTextureEditor.jsx';
function MaterialDivider({
  horizontal: Horizontal = false,
  value: Value,
  change: Change,
  minimum: Minimum,
  maximum: Maximum,
  label: Label
}) {
  function Resize(Event) {
    const Bounds = Event.currentTarget.parentElement.getBoundingClientRect();
    let Next = (Event.clientX - Bounds.left) / Bounds.width * 100;
    if (Horizontal) {
      Next = (Event.clientY - Bounds.top) / Bounds.height * 100;
    }
    Change(Math.max(Minimum, Math.min(Maximum, Next)));
  }
  let ClassName = 'native-material-divider';
  let Orientation = 'vertical';
  if (Horizontal) {
    ClassName += ' horizontal';
    Orientation = 'horizontal';
  }
  return <div className={ClassName} role="separator" tabIndex={0} aria-label={Label} aria-orientation={Orientation} aria-valuenow={Math.round(Value)} aria-valuemin={Minimum} aria-valuemax={Maximum} onPointerDown={Event => {
    Event.preventDefault();
    Event.currentTarget.setPointerCapture(Event.pointerId);
    Resize(Event);
  }} onPointerMove={Event => {
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
      Resize(Event);
    }
  }} onPointerUp={Event => {
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
      Event.currentTarget.releasePointerCapture(Event.pointerId);
    }
  }} onKeyDown={Event => {
    let Direction = 0;
    if (Event.key === 'ArrowLeft' || Event.key === 'ArrowUp') {
      Direction = -2;
    }
    if (Event.key === 'ArrowRight' || Event.key === 'ArrowDown') {
      Direction = 2;
    }
    if (Direction) {
      Event.preventDefault();
      Change(Math.max(Minimum, Math.min(Maximum, Value + Direction)));
    }
  }} />;
}
export default function NativeAssetEditorRoot() {
  const [Snapshot, SetSnapshot] = useState(null);
  const [Assets, SetAssets] = useState([]);
  const [Session, SetSession] = useState('');
  const [ErrorMessage, SetErrorMessage] = useState('');
  const [Tool, SetTool] = useState('translate');
  const [Focus, SetFocus] = useState(0);
  const [CodeWidth, SetCodeWidth] = useState(72);
  const [PreviewHeight, SetPreviewHeight] = useState(40);
  const [Position, SetPosition] = useState({
    Line: 1,
    Column: 1
  });
  const [SourcePending, SetSourcePending] = useState(false);
  const Client = useRef(null);
  const Latest = useRef(null);
  const Queue = useRef(Promise.resolve());
  const SourceDraft = useRef(null);
  const SourceTimer = useRef(null);
  const Identity = window.sacuraDesktop.assetEditorIdentity;
  function Execute(Operation, Value) {
    const Work = Queue.current.catch(() => {}).then(async () => {
      if (!Client.current || !Latest.current) {
        return;
      }
      if (Operation !== 'get' && Operation !== 'preview' && SourceDraft.current !== null) {
        const Source = SourceDraft.current;
        const Flushed = await Client.current.request('assetDocument.execute', {
          ...Identity,
          operation: 'source',
          revision: Latest.current.revision,
          value: Source
        });
        if (!Flushed.ok) {
          SetErrorMessage(Flushed.error.message);
          return;
        }
        Latest.current = Flushed.value;
        SetSnapshot(Flushed.value);
        if (SourceDraft.current === Source) {
          SourceDraft.current = null;
          SetSourcePending(false);
        }
        SetErrorMessage('');
      }
      if (Latest.current.type === 'Texture' && ['save', 'undo', 'redo'].includes(Operation)) {
        return Latest.current;
      }
      if (Operation === 'source') {
        return Latest.current;
      }
      if (Operation === 'edit' && Latest.current.type === 'Material') {
        Value = {
          ...Value,
          shader: Latest.current.document.shader
        };
      }
      const Params = {
        ...Identity,
        operation: Operation,
        revision: Latest.current.revision
      };
      if (Value !== undefined) {
        Params.value = Value;
      }
      const Result = await Client.current.request('assetDocument.execute', Params);
      if (!Result.ok) {
        SetErrorMessage(Result.error.message);
        return;
      }
      Latest.current = Result.value;
      SetSnapshot(Result.value);
      if (Operation !== 'get') {
        SetErrorMessage('');
      }
      return Result.value;
    });
    Queue.current = Work;
    return Work;
  }
  function ChangeSource(Source) {
    SourceDraft.current = Source;
    SetSourcePending(true);
    clearTimeout(SourceTimer.current);
    SourceTimer.current = setTimeout(() => Execute('source'), 500);
  }
  useEffect(() => {
    let Disposed = false;
    const Bridge = createNativeBridgeClient();
    Client.current = Bridge;
    async function Load() {
      const Connection = await Bridge.connect();
      if (Disposed) {
        return;
      }
      if (!Connection.connected) {
        throw Error('Редактор ассета потерял соединение с движком.');
      }
      SetSession(Connection.context.session);
      const Result = await Bridge.request('assets.list');
      if (!Result.ok) {
        throw Error(Result.error.message);
      }
      SetAssets(Result.value);
      const Document = await Bridge.request('assetDocument.execute', {
        ...Identity,
        operation: 'get'
      });
      if (!Document.ok) {
        throw Error(Document.error.message);
      }
      if (!Disposed) {
        Latest.current = Document.value;
        SetSnapshot(Document.value);
      }
    }
    Load().catch(ErrorValue => SetErrorMessage(ErrorValue.message));
    const Timer = setInterval(() => {
      if (Latest.current?.type !== 'Texture' || Latest.current.preparing) {
        Execute('get');
      }
    }, 500);
    const Unsubscribe = window.sacuraDesktop.onAssetEditorAction(Operation => {
      if (Operation === 'fit') {
        SetFocus(Value => Value + 1);
        return;
      }
      return Execute(Operation);
    });
    const Keyboard = Event => {
      if (Event.defaultPrevented) {
        return;
      }
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
        Execute(Operation);
      }
    };
    window.addEventListener('keydown', Keyboard);
    return () => {
      Disposed = true;
      clearInterval(Timer);
      clearTimeout(SourceTimer.current);
      Unsubscribe();
      window.removeEventListener('keydown', Keyboard);
      Bridge.dispose();
    };
  }, []);
  if (Snapshot?.type === 'Texture') {
    return <NativeTextureEditor snapshot={Snapshot} asset={Assets.find(Entry => Entry.id === Snapshot.assetId)} focusRequest={Focus} error={ErrorMessage} />;
  }
  const IsMaterial = Snapshot && Snapshot.type !== 'StaticMesh';
  const ReadOnly = !Snapshot || Snapshot.readOnly;
  const Asset = Assets.find(Entry => Entry.id === Snapshot?.assetId);
  const Preview = <div className="native-asset-editor-viewport"><nav aria-label="Инструменты предпросмотра">
        {IsMaterial && <strong>Предпросмотр</strong>}
        {Snapshot?.previewDomain !== 'UserInterface' && [['select', 'MousePointer2', 'Выбор', 'Q'], ['translate', 'Move', 'Перемещение', 'W'], ['rotate', 'Rotate3D', 'Поворот', 'E'], ['scale', 'Scaling', 'Масштаб', 'R']].map(([Identifier, IconName, Caption, Shortcut]) => <button key={Identifier} aria-label={Caption} title={Caption + ' (' + Shortcut + ')'} aria-pressed={Tool === Identifier} onClick={() => SetTool(Identifier)}><Icon name={IconName} /></button>)}
        {Snapshot?.previewDomain !== 'UserInterface' && <button title="В кадр (F)" aria-label="В кадр" onClick={() => SetFocus(Value => Value + 1)}><Icon name="Focus" /></button>}
        {IsMaterial && Snapshot.previewDomain !== 'UserInterface' && <select aria-label="Форма предпросмотра" defaultValue="Sphere" onChange={Event => {
        Execute('preview', {
          previewShape: Event.target.value
        });
        SetFocus(Value => Value + 1);
      }}><option value="Sphere">Сфера</option><option value="Cube">Куб</option><option value="Plane">Плоскость</option></select>}
      </nav><div className="native-asset-editor-surface">{Snapshot?.previewDomain === 'UserInterface' && <div className="native-ui-material-preview">{Snapshot.previewPng && <img src={'data:image/png;base64,' + Snapshot.previewPng} alt="Предпросмотр UI-материала" />}{!Snapshot.previewPng && <p>Подготовка предпросмотра…</p>}</div>}{Snapshot?.previewDomain !== 'UserInterface' && <NativeViewport assetPreview editing editTool={Tool} showGrid focusRequest={Focus} />}</div></div>;
  let ClassName = 'native-asset-editor-root';
  let Status = 'HLSL';
  if (IsMaterial) {
    ClassName += ' native-material-editor';
  }
  if (Snapshot?.preparing) {
    Status = 'Подготовка материала…';
  }
  return <NativeAssetsContext.Provider value={{
    assets: Assets,
    session: Session,
    open: Asset => window.sacuraDesktop.nativeOpenAssetEditor(Asset).catch(ErrorValue => SetErrorMessage(ErrorValue.message)),
    reveal: Asset => window.sacuraDesktop.nativeRevealAsset(Asset)
  }}>
    <main className={ClassName}>
      {IsMaterial && <>
        <header className="native-material-toolbar native-document-toolbar">
          <Icon name="Box" /><span className="native-material-path" title={Asset?.path}>{Asset?.path || Asset?.name}</span>
          {(Snapshot.dirty || SourcePending) && <span aria-label="Несохранённые изменения">*</span>}
          <button aria-label="Сохранить ассет" title="Сохранить (Ctrl+S)" disabled={ReadOnly || !Snapshot.dirty && !SourcePending} onClick={() => Execute('save')}><Icon name="Save" /></button>
          <button disabled={ReadOnly} onClick={() => Execute('compile')}>Компилировать</button>
        </header>
        <div className="native-material-body" style={{
          gridTemplateColumns: `minmax(320px, ${CodeWidth}fr) 5px minmax(300px, ${100 - CodeWidth}fr)`
        }}>
          <section className="native-material-code" aria-label="Редактор шейдера">
            <header>Исходник шейдера <span>HLSL</span></header>
            {Snapshot.type === 'MaterialInstance' && <p className="native-material-notice">Код наследуется от родительского материала и доступен только для чтения.</p>}
            <NativeShaderEditor source={Snapshot.source || ''} disabled={ReadOnly || Snapshot.sourceReadOnly || Snapshot.type === 'MaterialInstance'} pending={SourcePending} onChange={ChangeSource} onPosition={SetPosition} />
            <details className="native-material-diagnostics" open>
              <summary>Вывод компилятора</summary>
              <pre role="status">{ErrorMessage || Snapshot.diagnostic || 'Нет сообщений компилятора.'}</pre>
              {Snapshot.preparing && <p role="status">Подготовка материала…</p>}
            </details>
          </section>
          <MaterialDivider value={CodeWidth} change={SetCodeWidth} minimum={45} maximum={82} label="Ширина редактора шейдера" />
          <div className="native-material-sidebar" style={{
            gridTemplateRows: `minmax(180px, ${PreviewHeight}fr) 5px minmax(150px, ${100 - PreviewHeight}fr)`
          }}>
            {Preview}
            <MaterialDivider horizontal value={PreviewHeight} change={SetPreviewHeight} minimum={25} maximum={70} label="Высота предпросмотра" />
            <aside aria-label="Параметры материала"><h3>Материал</h3><NativeAssetWorkspace snapshot={Snapshot} execute={Execute} interactive inspectorOnly /></aside>
          </div>
        </div>
        <footer className="native-material-status"><span>{Status}</span><span>Строка {Position.Line}, столбец {Position.Column}</span><span>UTF-8</span></footer>
      </>}
      {!IsMaterial && <>{Preview}<aside>{Snapshot && <NativeAssetWorkspace snapshot={Snapshot} execute={Execute} interactive />}{!Snapshot && <p>Загрузка ассета…</p>}{ErrorMessage && <p role="alert">{ErrorMessage}</p>}</aside></>}
    </main>
  </NativeAssetsContext.Provider>;
}
