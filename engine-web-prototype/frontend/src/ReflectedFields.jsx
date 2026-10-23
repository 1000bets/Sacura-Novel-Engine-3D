import {AssetReferenceField} from './NativeAssets.jsx';
import NativeMeshInspector from './NativeMeshInspector.jsx';
import React,{useEffect,useState} from 'react';
import {ChevronRight,Plus,Puzzle,Trash2} from 'lucide-react';
import {SchemaValue} from './SchemaFields.jsx';
import ColorField from './ColorField.jsx';
export function DocumentJsonField({label,value,onCommit,disabled}){
 const serialized=JSON.stringify(value,null,2),[text,setText]=useState(serialized),[error,setError]=useState('');
 useEffect(()=>{setText(serialized);setError('');},[serialized]);
 return <fieldset disabled={disabled} className="component-parameter-fields"><label className="component-property-field"><span>{label}</span><textarea aria-label={label} rows={8} value={text} onChange={e=>setText(e.target.value)}/></label><button onClick={()=>{try{const next=JSON.parse(text);setError('');onCommit(next);}catch(e){setError(e.message);}}}>Применить {label}</button>{error&&<p role="alert">{error}</p>}</fieldset>;
}
// Generic reflected property presentation: the native codec/PropertyAccess validates writes.
export function ReflectedValueField({property,onChange,disabled,project,assets=[],onAsset}){
 const [draft,setDraft]=useState(property.value),[error,setError]=useState('');
 useEffect(()=>{setDraft(property.value);setError('');},[property.value]);
 const commit=()=>{if(JSON.stringify(draft)!==JSON.stringify(property.value))onChange(draft);};
 const scalar={boolean:'boolean',number:'number',string:'string'}[typeof property.value];
 const schema=scalar?{type:scalar,label:property.label,...(property.range?{min:property.range[0],max:property.range[1]}:{})}:Array.isArray(property.value)&&property.value.every(v=>typeof v==='number')?{type:'vector',size:property.value.length}:null;
 if (property.typeId === 'engine.Color') {
  return <fieldset disabled={disabled || !property.editable || !!property.error} className="component-parameter-fields">
   {!property.error && <ColorField label={property.label} value={property.value} disabled={disabled || !property.editable} onChange={onChange}/>}
   {property.error && <p role="alert">{property.error}</p>}
  </fieldset>;
 }
 if(property.assetTypes&&onAsset){
  let CurrentAsset=property.value;
  if(Object.hasOwn(property,'assetId')){CurrentAsset=property.assetId;}
  return <div className="component-property-field"><span>{property.label}</span><AssetReferenceField label={property.label} assetId={CurrentAsset} subAssetId={property.subAssetId} allowedTypes={property.assetTypes} disabled={disabled||!property.editable} onChange={onAsset}/></div>;
 }
 return <fieldset disabled={disabled||!property.editable||!!property.error} className="component-parameter-fields" onBlur={e=>{if(!e.currentTarget.contains(e.relatedTarget))commit();}}>
  {schema?<SchemaValue project={project} schema={schema} value={draft} onChange={setDraft}/>:<DocumentJsonField label={property.label+' · '+property.typeId} value={property.value} onCommit={onChange}/>}
  {property.typeId==='engine.int64'&&<small>int64 · десятичная строка без потери точности</small>}
  {(error||property.error)&&<p role="alert">{error||property.error}</p>}
 </fieldset>;
}
export function ReflectedComponents({reflection,project,running}){
 const [type,setType]=useState(''),[selected,setSelected]=useState([]),[search,setSearch]=useState(''),[collapsed,setCollapsed]=useState([]);
 const {components,types,onAdd,onRemove,onEnabled,onProperty,onAsset,assets,objectId,command,revision}=reflection;
 useEffect(()=>{setSelected([]);setCollapsed([]);},[objectId]);
 return <section className="component-instances component-instances-inspector" aria-label="Компоненты C++">
  <header className="component-inspector-heading"><h3>Компоненты <span>{components.length}</span></h3></header>
  <input aria-label="Поиск компонентов" placeholder="Поиск компонентов…" value={search} onChange={Event=>setSearch(Event.target.value)}/>
  <div className="native-component-overview">{components.filter(Component=>(Component.displayName+" "+Component.typeId).toLowerCase().includes(search.toLowerCase())).map(Component=><button key={Component.id} title={Component.typeId} aria-pressed={selected.includes(Component.id)} onClick={()=>setSelected(Previous=>{if(Previous.includes(Component.id)){return Previous.filter(Identity=>Identity!==Component.id);}return [...Previous,Component.id];})}><Puzzle size={13}/>{Component.displayName || Component.typeId}</button>)}</div>
  <div className="component-instance-list">{components.map(component=><div key={component.id}><button className="component-instance-row" title={component.typeId} aria-expanded={!collapsed.includes(component.id)} onClick={()=>setCollapsed(Previous=>{if(Previous.includes(component.id)){return Previous.filter(Identity=>Identity!==component.id);}return [...Previous,component.id];})}><ChevronRight size={13} aria-hidden="true"/><Puzzle size={15} aria-hidden="true"/><span><strong>{component.displayName || component.typeId}</strong><small>Системный</small></span></button>
   {!collapsed.includes(component.id)&&(selected.length===0||selected.includes(component.id))&&<div className="component-instance-properties">{onEnabled&&<label className="component-enabled"><input type="checkbox" aria-label={'Компонент '+(component.displayName || component.typeId)+' включён'} checked={component.enabled!==false} disabled={running} onChange={e=>onEnabled(component.id,e.target.checked)}/>Включён</label>}{component.typeId==='engine.MeshRendererComponent'&&<NativeMeshInspector component={component} objectId={objectId} command={command} revision={revision} disabled={running}/>}{component.properties.filter(Property=>component.typeId!=='engine.MeshRendererComponent'||!['material_slots','material_asset_id'].includes(Property.id)).map(p=><ReflectedValueField key={p.id} property={p} project={project} assets={assets} onAsset={onAsset?value=>onAsset(component.id,p.id,value):undefined} disabled={running} onChange={value=>onProperty(component.id,p.id,value)}/>)}<button disabled={running} onClick={()=>onRemove(component.id)}><Trash2 size={14} aria-hidden="true"/>Удалить компонент</button></div>}
  </div>)}</div>
  {!components.length&&<p className="component-empty">Нет компонентов</p>}
  <div className="component-add-form"><label className="component-property-field"><span>Компонент</span><select aria-label="Тип компонента" value={type} disabled={running} onChange={e=>setType(e.target.value)}><option value="">Выберите компонент…</option>{types.map(t=><option key={t.typeId} value={t.typeId} title={t.typeId}>{t.displayName || t.typeId}</option>)}</select></label>
   <button className="component-add-button" disabled={running||!type} onClick={()=>onAdd(type)}><Plus size={15} aria-hidden="true"/>Добавить</button></div>
 </section>;
}
