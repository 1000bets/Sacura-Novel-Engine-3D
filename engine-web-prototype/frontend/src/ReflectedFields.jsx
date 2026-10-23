import React,{useEffect,useState} from 'react';
import {SchemaValue} from './SchemaFields.jsx';
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
 if(property.assetTypes&&onAsset){
  const choices=assets.flatMap(a=>[...(property.assetTypes.includes(a.typeId)?[{assetId:a.id,subAssetId:null,label:a.path}]:[]),...a.subassets.filter(s=>property.assetTypes.includes(s.typeId)).map(s=>({assetId:a.id,subAssetId:s.id,label:a.path+' / '+s.name}))]);
  const selected=choices.findIndex(c=>c.assetId===property.value&&(c.subAssetId||'')===(property.subAssetId||''));
  return <label className="component-property-field"><span>{property.label}</span><select aria-label={property.label} disabled={disabled||!property.editable} value={selected<0?'':String(selected)} onChange={e=>onAsset(e.target.value===''?{assetId:null,subAssetId:null}:choices[Number(e.target.value)])}><option value="">{property.value&&selected<0?'Недоступный ресурс':'Не назначено'}</option>{choices.map((c,i)=><option key={c.assetId+(c.subAssetId||'')} value={i}>{c.label}</option>)}</select></label>;
 }
 return <fieldset disabled={disabled||!property.editable||!!property.error} className="component-parameter-fields" onBlur={e=>{if(!e.currentTarget.contains(e.relatedTarget))commit();}}>
  {schema?<SchemaValue project={project} schema={schema} value={draft} onChange={setDraft}/>:<DocumentJsonField label={property.label+' · '+property.typeId} value={property.value} onCommit={onChange}/>}
  {property.typeId==='engine.int64'&&<small>int64 · десятичная строка без потери точности</small>}
  {(error||property.error)&&<p role="alert">{error||property.error}</p>}
 </fieldset>;
}
export function ReflectedComponents({reflection,project,running}){
 const [type,setType]=useState(''),[selected,setSelected]=useState(null);
 const {components,types,onAdd,onRemove,onProperty,onAsset,assets}=reflection;
 return <section className="component-instances-inspector" aria-label="Компоненты C++">
  <header className="component-inspector-heading"><strong>Компоненты · {components.length}</strong></header>
  <select aria-label="Тип компонента" value={type} disabled={running} onChange={e=>setType(e.target.value)}><option value="">Добавить компонент…</option>{types.map(t=><option key={t.typeId} value={t.typeId}>{t.typeId}</option>)}</select>
  <button disabled={running||!type} onClick={()=>onAdd(type)}>Добавить</button>
  {components.map(component=><div key={component.id}><button className="component-instance-row" aria-expanded={selected===component.id} onClick={()=>setSelected(selected===component.id?null:component.id)}>{component.typeId}</button>
   {selected===component.id&&<div className="component-instance-properties">{component.properties.map(p=><ReflectedValueField key={p.id} property={p} project={project} assets={assets} onAsset={onAsset?value=>onAsset(component.id,p.id,value):undefined} disabled={running} onChange={value=>onProperty(component.id,p.id,value)}/>)}<button disabled={running} onClick={()=>onRemove(component.id)}>Удалить компонент</button></div>}
  </div>)}
 </section>;
}
