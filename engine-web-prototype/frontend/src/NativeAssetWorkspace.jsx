import React, {useContext, useState} from 'react';
import {AssetReferenceField, AssetThumbnail, NativeAssetsContext} from './NativeAssets.jsx';
import NativeMaterialParameters from './NativeMaterialParameters.jsx';
import {Icon} from './StudioParts.jsx';
import ColorField from './ColorField.jsx';

function DocumentValue({label: Label, value: Value, type: Type, disabled: Disabled, onChange: Change}) {
  if (Type === 'Color') {
    return <ColorField label={Label} value={Value} disabled={Disabled} onChange={Change}/>;
  }
  if (typeof Value === 'boolean') { return <label>{Label}<input type="checkbox" disabled={Disabled} checked={Value} onChange={Event => Change(Event.target.checked)}/></label>; }
  if (typeof Value === 'number') { return <label>{Label}<input type="number" step="any" disabled={Disabled} defaultValue={Value} key={String(Value)} onBlur={Event => { const NumberValue = Number(Event.target.value); if (Number.isFinite(NumberValue) && NumberValue !== Value) { Change(NumberValue); } }}/></label>; }
  if (Array.isArray(Value)) { return <fieldset disabled={Disabled}><legend>{Label}</legend>{Value.map((Element, Index) => <DocumentValue key={Index} label={['X', 'Y', 'Z', 'W'][Index]} value={Element} disabled={Disabled} onChange={Next => Change(Value.map((Current, Position) => { if (Position === Index) { return Next; } return Current; }))}/>)}</fieldset>; }
  return <label>{Label}<input disabled={Disabled} defaultValue={Value || ''} key={String(Value)} onBlur={Event => { if (Event.target.value !== Value) { Change(Event.target.value); } }}/></label>;
}

export default function NativeAssetWorkspace({snapshot: Snapshot, execute: Execute, disabled: Disabled, interactive: Interactive, inspectorOnly: InspectorOnly = false}) {
  const Context = useContext(NativeAssetsContext);
  const [Tab, SetTab] = useState('materials');
  const Document = Snapshot.document;
  const ReadOnly = Disabled || Snapshot.readOnly;
  const Edit = Next => Execute('edit', Next);
  const Asset = Context.assets.find(Entry => Entry.id === Snapshot.assetId);
  const IsMesh = Snapshot.type === 'StaticMesh';
  const AssignSlot = (Slot, Identity) => {
    const Materials = Document.materials.filter(Entry => Entry.slot !== Slot);
    if (Identity.assetId) { Materials.push({slot: Slot, material: {assetId: Identity.assetId, subAssetId: null}}); }
    Edit({...Document, materials: Materials});
  };
  const ChangeCollision = (Index, Name, Value) => Edit({...Document, collision: {...Document.collision, elements: Document.collision.elements.map((Element, Position) => { if (Position === Index) { return {...Element, [Name]: Value}; } return Element; })}});
  let Slots = [];
  if (IsMesh) { Slots = [...new Set([...Snapshot.slots, ...Document.materials.map(Entry => Entry.slot)])]; }
  const ChangeParameter = (Index, Value) => Edit({...Document, parameters: Document.parameters.map((Parameter, Position) => { if (Position === Index) { return {...Parameter, default: Value}; } return Parameter; })});
  return <section className="native-asset-workspace" aria-label="Редактор ассета">
    {!InspectorOnly && <header className="native-document-toolbar"><button title="Сохранить (Ctrl+S)" aria-label="Сохранить ассет" disabled={ReadOnly || !Snapshot.dirty} onClick={() => Execute('save')}><Icon name="Save"/></button><strong>{Asset?.name}</strong>{Snapshot.dirty && <span aria-label="Несохранённые изменения">*</span>}
      <details><summary>Файл</summary><button disabled={ReadOnly} onClick={() => Execute('save')}>Сохранить</button><button disabled={ReadOnly || Snapshot.dirty} onClick={() => Execute('reload')}>Перечитать</button></details>
      <details><summary>Правка</summary><button disabled={ReadOnly} onClick={() => Execute('undo')}>Отменить</button><button disabled={ReadOnly} onClick={() => Execute('redo')}>Повторить</button></details>
      {!IsMesh && <button disabled={ReadOnly} onClick={() => Execute('compile')}>Компилировать</button>}
    </header>}
    {Snapshot.readOnly && <p>Ресурс движка доступен только для чтения.</p>}
    {!Interactive && <div className="native-document-preview"><AssetThumbnail key={Snapshot.revision} assetId={Snapshot.assetId} subAssetId={Snapshot.subAssetId} type={Snapshot.type}/></div>}
    {IsMesh && <><nav><button aria-pressed={Tab === 'materials'} onClick={() => {SetTab('materials'); if (Interactive) {Execute('preview', {showCollision:false,selectedCollision:-1});}}}>Материалы</button><button aria-pressed={Tab === 'collision'} onClick={() => {SetTab('collision'); if (Interactive) {Execute('preview', {showCollision:true,selectedCollision:0});}}}>Коллизия</button></nav>
      {Tab === 'materials' && Slots.map(Slot => { const Assignment = Document.materials.find(Entry => Entry.slot === Slot); return <div className="native-mesh-slot" key={Slot}><span>Слот {Slot}</span><AssetReferenceField assetId={Assignment?.material?.assetId} allowedTypes={['Material', 'MaterialInstance']} disabled={ReadOnly} onChange={Identity => AssignSlot(Slot, Identity)}/><button disabled={ReadOnly || !Assignment} onClick={() => AssignSlot(Slot, {assetId: null})}>Из модели</button></div>; })}
      {Tab === 'collision' && <><label><input type="checkbox" checked={Document.collision.enabled} disabled={ReadOnly} onChange={Event => Edit({...Document, collision: {...Document.collision, enabled: Event.target.checked}})}/>Коллизия меша</label>
        {Document.collision.elements.map((Element, Index) => <fieldset key={Index} disabled={ReadOnly}><legend><button type="button" aria-pressed={Snapshot.selectedCollision === Index} onClick={() => Execute('preview', {showCollision:true,selectedCollision:Index})}>Форма {Index + 1}</button></legend><label>Тип<select value={Element.shape} onChange={Event => ChangeCollision(Index, 'shape', Number(Event.target.value))}><option value={0}>Box</option><option value={1}>Sphere</option><option value={2}>Capsule</option><option value={3}>Triangle Mesh</option></select></label>
          <DocumentValue label="Центр" value={Element.center} onChange={Value => ChangeCollision(Index, 'center', Value)}/><DocumentValue label="Поворот, °" value={Element.rotation} onChange={Value => ChangeCollision(Index, 'rotation', Value)}/>
          {Element.shape === 0 && <DocumentValue label="Полуразмеры" value={Element.halfExtents} onChange={Value => ChangeCollision(Index, 'halfExtents', Value)}/>}
          {[1, 2].includes(Element.shape) && <DocumentValue label="Радиус" value={Element.radius} onChange={Value => ChangeCollision(Index, 'radius', Value)}/>}
          {Element.shape === 2 && <DocumentValue label="Полувысота цилиндра" value={Element.capsuleHalfHeight} onChange={Value => ChangeCollision(Index, 'capsuleHalfHeight', Value)}/>}
          <button disabled={ReadOnly || Document.collision.elements.length === 1} onClick={() => Edit({...Document, collision: {...Document.collision, elements: Document.collision.elements.filter((Entry, Position) => Position !== Index)}})}>Удалить форму</button>
        </fieldset>)}
        <button disabled={ReadOnly} onClick={() => Edit({...Document, collision: {...Document.collision, elements: [...Document.collision.elements, {shape: 0, center: [0, 0, 0], rotation: [0, 0, 0], halfExtents: [.5, .5, .5], radius: .5, capsuleHalfHeight: .5}]}})}>Добавить форму</button>
      </>}
    </>}
    {!IsMesh && <>
      {Snapshot.type === 'MaterialInstance' && <><h4>Родительский материал</h4><AssetReferenceField assetId={Document.parent?.assetId} allowedTypes={['Material', 'MaterialInstance']} disabled={ReadOnly} onChange={Identity => Edit({...Document, parent: {assetId: Identity.assetId, subAssetId: null}})}/></>}
      {Snapshot.type === 'Material' && <><label>Освещение<select disabled={ReadOnly} value={Document.shadingModel} onChange={Event => Edit({...Document, shadingModel: Event.target.value})}>{['Lit', 'Unlit', 'Toon'].map(Value => <option key={Value}>{Value}</option>)}</select></label><label>Смешивание<select disabled={ReadOnly} value={Document.blendMode} onChange={Event => Edit({...Document, blendMode: Event.target.value})}>{['Opaque', 'Masked', 'Translucent', 'Additive'].map(Value => <option key={Value}>{Value}</option>)}</select></label></>}
      {Snapshot.type === 'Material' && <>
        <p>Область применения: {Document.domain}</p>
        {Document.domain === 'PostProcess' && <label>Стадия<select disabled={ReadOnly} value={Document.postProcessStage || 'BeforeToneMapping'} onChange={Event => Edit({...Document,postProcessStage:Event.target.value})}><option>BeforeToneMapping</option><option>AfterToneMapping</option></select></label>}
        <DocumentValue label="Двусторонний" value={!!Document.doubleSided} disabled={ReadOnly} onChange={Value => Edit({...Document,doubleSided:Value})}/>
        <details><summary>Дополнительные настройки</summary>
        {['castShadows','receiveShadows','modifyVertex','animated'].map(Property => <DocumentValue key={Property} label={{doubleSided:'Двусторонний',castShadows:'Отбрасывать тени',receiveShadows:'Принимать тени',modifyVertex:'Изменять вершины',animated:'Анимация шейдера'}[Property]} value={!!Document[Property]} disabled={ReadOnly} onChange={Value => Edit({...Document,[Property]:Value})}/>)}
        <DocumentValue label="Порог маски" value={Document.alphaCutoff ?? .5} disabled={ReadOnly} onChange={Value => Edit({...Document,alphaCutoff:Value})}/>
        <DocumentValue label="Расширение границ" value={Document.boundsExpansion ?? 0} disabled={ReadOnly} onChange={Value => Edit({...Document,boundsExpansion:Value})}/>
        </details>
      </>}
      {Snapshot.type === 'Material' && [{Name: 'Параметры', Texture: false}, {Name: 'Текстуры', Texture: true}].map(Group => <details key={Group.Name} open><summary>{Group.Name}</summary>{(Document.parameters || []).map((Parameter, Index) => ({Parameter, Index})).filter(Entry => (Entry.Parameter.type === 'Texture2D') === Group.Texture).map(({Parameter, Index}) => <div key={Parameter.id}>{Parameter.type === 'Texture2D' && <><span>{Parameter.displayName || Parameter.id}</span><AssetReferenceField assetId={Parameter.default?.assetId} allowedTypes={['Texture']} disabled={ReadOnly} onChange={Identity => { let Value = null; if (Identity.assetId) { Value = {assetId: Identity.assetId, subAssetId: null}; } ChangeParameter(Index, Value); }}/></>}{Parameter.type !== 'Texture2D' && <DocumentValue label={Parameter.displayName || Parameter.id} value={Parameter.default} type={Parameter.type} disabled={ReadOnly} onChange={Value => ChangeParameter(Index, Value)}/>}</div>)}</details>)}
      {Snapshot.type === 'MaterialInstance' && (Snapshot.parameters || []).map(Parameter => {
        const Change = Value => Edit({...Document, overrides: {...Document.overrides, [Parameter.id]: {type:Parameter.type,value:Value}}});
        const Reset = () => {const Overrides = {...Document.overrides}; delete Overrides[Parameter.id]; Edit({...Document, overrides:Overrides});};
        const Overridden = Object.hasOwn(Document.overrides || {}, Parameter.id);
        return <fieldset className="native-instance-parameter" key={Parameter.id}><legend>{Parameter.group || 'Параметры'} / {Parameter.displayName || Parameter.id}</legend>
          {Parameter.type === 'Texture2D' && <AssetReferenceField assetId={Parameter.value?.assetId} allowedTypes={['Texture']} disabled={ReadOnly} onChange={Value => {if (Value.assetId) {Change({assetId:Value.assetId,subAssetId:null});} else {Change(null);}}}/>}
          {Parameter.type !== 'Texture2D' && <DocumentValue label={Parameter.displayName || Parameter.id} value={Parameter.value} type={Parameter.type} disabled={ReadOnly} onChange={Change}/>}
          <button disabled={ReadOnly || !Overridden} onClick={Reset}>Из родительского материала</button>
        </fieldset>;
      })}
      {Snapshot.type === 'Material' && <NativeMaterialParameters document={Document} edit={Edit} disabled={ReadOnly}/>}
      {!InspectorOnly && Snapshot.type === 'Material' && typeof Snapshot.source === 'string' && <details><summary>Исходник шейдера</summary><textarea aria-label="Исходник шейдера" rows={16} disabled={ReadOnly || Snapshot.sourceReadOnly} defaultValue={Snapshot.source} key={Snapshot.revision} onBlur={Event => { if (Event.target.value !== Snapshot.source) { Execute('source', Event.target.value); } }}/></details>}
    </>}
    {!InspectorOnly && Snapshot.preparing && <p role="status">Подготовка материала…</p>}
    {!InspectorOnly && Snapshot.diagnostic && <p role="status">{Snapshot.diagnostic}</p>}
  </section>;
}
