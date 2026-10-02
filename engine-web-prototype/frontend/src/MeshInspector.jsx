import React,{useEffect,useRef,useState} from 'react';
import {Button,Field,Select} from './StudioParts.jsx';
import {importMeshFile} from './meshAssets.js';
import {MATERIAL_PRESETS} from './meshVisual.js';
import './meshInspector.css';

export default function MeshInspector({object,onChange,disabled,allowUpload=true}){
 const fileInput=useRef(),request=useRef(0),[busy,setBusy]=useState(false),[error,setError]=useState(''),[slot,setSlot]=useState('all');
 useEffect(()=>()=>{request.current++;},[]);
 useEffect(()=>{setSlot('all');},[object.model?.src]);
 const upload=async file=>{
  if(!file||busy||disabled)return;const token=++request.current;setBusy(true);setError('');
  try{const model=await importMeshFile(file);if(token===request.current)await onChange({model,materialOverrides:{}});}catch(e){if(token===request.current)setError(e.message);}finally{if(token===request.current)setBusy(false);}
 };
 const commit=values=>Promise.resolve(onChange(values)).catch(e=>setError(e.message));
 const config=object.materialOverrides?.[slot],effective=config||(slot!=='all'?object.materialOverrides?.all:null)||MATERIAL_PRESETS.matte;
 const assign=value=>{const overrides={...object.materialOverrides};if(value)overrides[slot]=value;else delete overrides[slot];commit({materialOverrides:overrides});};
 return <fieldset className="mesh-inspector" disabled={disabled||busy} aria-busy={busy}>
  <legend>Меш и материалы</legend>
  {allowUpload&&<><input ref={fileInput} type="file" hidden accept=".glb,.obj" onChange={event=>{upload(event.target.files?.[0]);event.target.value='';}}/>
   <div className="mesh-import" onDragOver={e=>e.preventDefault()} onDrop={e=>{e.preventDefault();upload(e.dataTransfer.files?.[0]);}}>
    <strong>{object.model?.name||'Custom mesh'}</strong>
    <small>GLB / OBJ · до 100 МБ. GLB со встроенными текстурами; OBJ — геометрия без MTL. Размеры и начало координат сохраняются.</small>
    <Button icon="Upload" onClick={()=>fileInput.current.click()}>{busy?'Загрузка…':object.model?'Заменить меш':'Загрузить меш'}</Button>
    {object.model&&<Button icon="RotateCcw" onClick={()=>{setError('');commit({model:null,materialOverrides:{}});}}>Вернуть примитив</Button>}
   </div></>}
  {error&&<p className="error-box" role="alert">{error}</p>}
  {busy&&<p role="status">Проверяем модель…</p>}
  <Field label="Назначить материал"><Select value={slot} options={[['all','Весь объект'],...(object.model?.slots||[]).map(item=>[item.id,item.name])]} onChange={setSlot}/></Field>
  <Field label="Материал"><Select value={config?.preset|| (config?'custom':'original')} options={[["original",slot==='all'?'Исходные материалы':'По умолчанию'],...Object.entries(MATERIAL_PRESETS).map(([id,p])=>[id,p.name]),...(config&&!config.preset?[["custom","Свой материал"]]:[])]} onChange={id=>assign(id==='original'?null:{...MATERIAL_PRESETS[id],preset:id})}/></Field>
  {config&&<><Field label="Цвет материала"><input type="color" value={effective.color} onChange={e=>assign({...effective,color:e.target.value,preset:undefined})}/></Field>
   {[['roughness','Шероховатость'],['metalness','Металличность']].map(([key,label])=><Field key={key} label={`${label} · ${effective[key]}`}><input type="range" min="0" max="1" step=".01" value={effective[key]} onChange={e=>assign({...effective,[key]:Number(e.target.value),preset:undefined})}/></Field>)}
   <label className="check"><input type="checkbox" checked={!!effective.doubleSided} onChange={e=>assign({...effective,doubleSided:e.target.checked,preset:undefined})}/> Двусторонний материал</label>
  </>}
  <small>Назначения сохраняются в проекте. Материал слота имеет приоритет над материалом всего объекта.</small>
 </fieldset>;
}
