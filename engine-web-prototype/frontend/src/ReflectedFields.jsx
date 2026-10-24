import {t as tr, useLocale, LocalizeEditorText} from './i18n.jsx';
import {AssetReferenceField} from './NativeAssets.jsx';
import NativeMeshInspector from './NativeMeshInspector.jsx';
import React,{useCallback,useEffect,useState} from 'react';
import {ChevronRight,Plus,Puzzle,Trash2} from 'lucide-react';
import {SchemaValue} from './SchemaFields.jsx';
import ColorField from './ColorField.jsx';
import EditorContextMenu from './EditorContextMenu.jsx';

function ComponentAddButton({Types,OnAdd,Disabled}){
 const [Anchor,SetAnchor]=useState(null);
 const Close=useCallback(()=>{SetAnchor(null);},[]);
 useEffect(()=>{
  if(Disabled){
   Close();
  }
 },[Disabled,Close]);
 return <div className="component-add-form">
  <button className="component-add-button" disabled={Disabled} aria-haspopup="dialog" aria-expanded={!!Anchor} onClick={Event=>{
   if(Anchor){
    Close();
    return;
   }
   const Bounds=Event.currentTarget.getBoundingClientRect();
   SetAnchor({Document:Event.currentTarget.ownerDocument,Left:Bounds.left,Top:Bounds.bottom+4});
  }}><Plus size={15} aria-hidden="true"/>{tr("Добавить компонент")}</button>
  {Anchor&&!Disabled&&<EditorContextMenu x={Anchor.Left} y={Anchor.Top} ownerDocument={Anchor.Document} title={tr("Добавить компонент")} className="component-add-menu" onClose={Close} items={Types.map(ComponentType=>({id:ComponentType.typeId,label:LocalizeEditorText('Editor.Reflection.' + ComponentType.typeId + '.DisplayName', ComponentType.displayName || ComponentType.typeId),searchText:ComponentType.typeId,icon:'Puzzle',action:()=>OnAdd(ComponentType.typeId)}))}/>}
 </div>;
}
export function DocumentJsonField({label,value,onCommit,disabled}){
 const serialized=JSON.stringify(value,null,2),[text,setText]=useState(serialized),[error,setError]=useState('');
 useEffect(()=>{setText(serialized);setError('');},[serialized]);
 return <fieldset disabled={disabled} className="component-parameter-fields"><label className="component-property-field"><span>{label}</span><textarea aria-label={label} rows={8} value={text} onChange={e=>setText(e.target.value)}/></label><button onClick={()=>{try{const next=JSON.parse(text);setError('');onCommit(next);}catch(e){setError(e.message);}}}>{tr("Применить")}{label}</button>{error&&<p role="alert">{error}</p>}</fieldset>;
}
// Generic reflected property presentation: the native codec/PropertyAccess validates writes.
export function ReflectedValueField({property,onChange,disabled,project,assets=[],onAsset}){
 useLocale();
 property = {...property, label: LocalizeEditorText(property.localizationKey, tr(property.label))};
 const [draft,setDraft]=useState(property.value),[error,setError]=useState('');
 useEffect(()=>{setDraft(property.value);setError('');},[property.value]);
 const commit=()=>{if(typeof property.value !== 'boolean' && !property.enumValues && JSON.stringify(draft)!==JSON.stringify(property.value))onChange(draft);};
 const ChangeValue=Value=>{
  setDraft(Value);
  if(typeof Value === 'boolean' || property.enumValues){
   onChange(Value);
  }
 };
 const scalar={boolean:'boolean',number:'number',string:'string'}[typeof property.value];
 const schema=scalar?{type:scalar,label:property.label,...(property.enumValues?{enum:property.enumValues,options:Object.fromEntries(property.enumValues.map(Value=>[Value,LocalizeEditorText('Editor.Enum.'+property.typeId+'.'+Value,Value)]))}:{}),...(property.range?{min:property.range[0],max:property.range[1]}:{})}:Array.isArray(property.value)&&property.value.every(v=>typeof v==='number')?{type:'vector',size:property.value.length}:null;
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
  {schema?<SchemaValue project={project} schema={schema} value={draft} onChange={ChangeValue}/>:<DocumentJsonField label={property.label+' · '+property.typeId} value={property.value} onCommit={onChange}/>}
  {property.typeId==='engine.int64'&&<small>{tr("int64 · десятичная строка без потери точности")}</small>}
  {(error||property.error)&&<p role="alert">{error||property.error}</p>}
 </fieldset>;
}
export function ReflectedComponents({reflection,project,running}){
 useLocale();
 const [collapsed,setCollapsed]=useState([]);
 const {components,types,onAdd,onRemove,onEnabled,onProperty,onAsset,assets,objectId,command,revision}=reflection;
 useEffect(()=>{setCollapsed([]);},[objectId]);
 return <section className="component-instances component-instances-inspector" aria-label={tr("Компоненты C++")}>
  <header className="component-inspector-heading"><h3>{tr("Компоненты")}<span>{components.length}</span></h3></header>
  <div className="component-instance-list">{components.map(component=><section className="native-component-card" aria-label={LocalizeEditorText('Editor.Reflection.' + component.typeId + '.DisplayName', component.displayName || component.typeId)} key={component.id}><div className="native-component-header"><button type="button" className="component-instance-row" title={component.typeId} aria-expanded={!collapsed.includes(component.id)} onClick={()=>setCollapsed(Previous=>{if(Previous.includes(component.id)){return Previous.filter(Identity=>Identity!==component.id);}return [...Previous,component.id];})}><ChevronRight size={13} aria-hidden="true"/><Puzzle size={15} aria-hidden="true"/><span><strong>{LocalizeEditorText('Editor.Reflection.' + component.typeId + '.DisplayName', component.displayName || component.typeId)}</strong><small>{tr("Системный")}</small></span></button><button type="button" className="component-icon-button component-remove" aria-label={tr('Удалить компонент')+' '+(LocalizeEditorText('Editor.Reflection.' + component.typeId + '.DisplayName', component.displayName || component.typeId))} title={tr("Удалить компонент")} disabled={running} onClick={()=>onRemove(component.id)}><Trash2 size={14} aria-hidden="true"/></button></div>
   {!collapsed.includes(component.id)&&<div className="component-instance-properties">{onEnabled&&<label className="component-enabled"><input type="checkbox" aria-label={tr('Компоненты')+' '+(LocalizeEditorText('Editor.Reflection.' + component.typeId + '.DisplayName', component.displayName || component.typeId))+' '+tr('Включён')} checked={component.enabled!==false} disabled={running} onChange={e=>onEnabled(component.id,e.target.checked)}/>{tr("Включён")}</label>}{component.typeId==='engine.MeshRendererComponent'&&<NativeMeshInspector component={component} objectId={objectId} command={command} revision={revision} disabled={running}/>}{component.properties.filter(Property=>component.typeId!=='engine.MeshRendererComponent'||!['material_slots','material_asset_id'].includes(Property.id)).map(p=><ReflectedValueField key={p.id} property={p} project={project} assets={assets} onAsset={onAsset?value=>onAsset(component.id,p.id,value):undefined} disabled={running} onChange={value=>onProperty(component.id,p.id,value)}/>)}</div>}
  </section>)}</div>
  {!components.length&&<p className="component-empty">{tr("Нет компонентов")}</p>}
  <ComponentAddButton key={objectId} Types={types} OnAdd={onAdd} Disabled={running}/>
 </section>;
}
