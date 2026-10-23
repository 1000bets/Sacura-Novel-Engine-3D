import React,{useEffect,useRef,useState} from 'react';
import {createNativeBridgeClient} from './nativeBridgeClient.js';

// Small contract workbench. Existing Hierarchy/Inspector/Story are not rewritten.
export default function NativeBridgePanel() {
  const client=useRef(null),generation=useRef(0);
  const [rows,setRows]=useState([]),[state,setState]=useState(null),[error,setError]=useState(''),[name,setName]=useState('Object');
  const [snapshotContext,setSnapshotContext]=useState(null);
  async function refresh() {
    const serial=++generation.current, c=client.current;if(!c)return;
    const [h,s]=await Promise.all([c.request('scene.getHierarchy',{}),c.request('bridge.describe',{})]);
    if(serial!==generation.current)return;
    if(h.ok&&s.ok&&h.context.session===s.context.session&&h.context.epoch===s.context.epoch){setRows(h.value);setState(s.value);setSnapshotContext(h.context);}else if(!h.ok||!s.ok)setError((!h.ok?h:s).error.message);
  }
  async function command(method,params={}) {
    if(!client.current)return;
    const result=await client.current.request(method,params,snapshotContext);
    setError(result.ok?'':`${result.error.code}: ${result.error.message}`);
    if(result.ok)await refresh();return result;
  }
  useEffect(()=>{
    const c=createNativeBridgeClient();if(!c)return;client.current=c;
    const off=c.subscribe(e=>{if(e.event==='disconnected'){generation.current++;setRows([]);setState(null);setError(e.message);}else refresh();});
    c.connect().then(info=>{if(info.connected)refresh();});
    // Explicit test-only hook runs requests from React through the normal preload.
    const untest=window.sacuraDesktop.onNativeTest?.(async message=>{
      if(message.action!=='bridge-react')return;
      try {
        const must=async(m,p={})=>{const r=await c.request(m,p);if(!r.ok)throw Error(r.error.code+': '+r.error.message);return r.value;};
        const created=await must('object.create',{name:'React Bridge Test'});
        await must('selection.set',{objectId:created.id});
        await must('transform.set',{objectId:created.id,transform:{position:[2,3,4],rotation:[0,0,0,1],scale:[1,1,1]}});
        await must('editor.undo');await must('editor.redo');
        const transform=await must('transform.get',{objectId:created.id});
        if(transform.position.join(',')!=='2,3,4')throw Error('Transform did not roundtrip');
        await must('object.destroy',{objectId:created.id});
        const invalid=await c.request('transform.get',{objectId:created.id});
        if(invalid.ok||invalid.error.code!=='NOT_FOUND')throw Error('Deleted ID accepted');
        await refresh();await window.sacuraDesktop.bridgeTestResult({ok:true,id:created.id});
      }catch(e){await window.sacuraDesktop.bridgeTestResult({ok:false,error:e.message});}
    });
    return ()=>{generation.current++;untest?.();off();c.dispose();client.current=null;};
  },[]);
  const selected=rows.find(r=>r.id===state?.selection);
  return <details style={{position:'absolute',right:10,top:45,zIndex:3,maxWidth:350,background:'#202832ed',padding:8,fontSize:12}} onKeyDown={e=>e.stopPropagation()} onPointerDown={e=>e.stopPropagation()}>
    <summary>Native Bridge · {state?.play||'disconnected'}{state?.dirty?' · изменено':''}</summary>
    <p>C++ SceneDocument · изменения выполняет EditorCore</p>
    <input aria-label="Имя нативного объекта" value={name} onChange={e=>setName(e.target.value)} style={{width:150}}/>
    <button disabled={!state||state.play!=='stopped'} onClick={()=>command('object.create',{name})}>Создать</button>
    <select aria-label="Нативный объект" value={state?.selection||''} onChange={e=>command('selection.set',{objectId:e.target.value||null})}>
      <option value="">Нет выделения</option>{rows.map(o=><option key={o.id} value={o.id}>{o.name}</option>)}
    </select>
    <div><button disabled={!selected||state?.play!=='stopped'} onClick={()=>command('object.rename',{objectId:selected.id,name})}>Переименовать</button>
      <button disabled={!selected||state?.play!=='stopped'} onClick={()=>command('object.destroy',{objectId:selected.id})}>Удалить</button></div>
    {selected&&<div>{['X','Y','Z'].map((axis,i)=><label key={axis}>{axis}<input type="number" key={selected.id+':'+selected.transform.position[i]} defaultValue={selected.transform.position[i]} disabled={state.play!=='stopped'} style={{width:65}}
      onBlur={e=>{const value=Number(e.target.value);if(Number.isFinite(value)&&value!==selected.transform.position[i]){const position=[...selected.transform.position];position[i]=value;command('transform.set',{objectId:selected.id,transform:{...selected.transform,position}});}}}/></label>)}</div>}
    <div><button disabled={!state?.canUndo||state.play!=='stopped'} onClick={()=>command('editor.undo')}>Undo</button><button disabled={!state?.canRedo||state.play!=='stopped'} onClick={()=>command('editor.redo')}>Redo</button>
      <button disabled={!state||state.play!=='stopped'} onClick={()=>command('scene.save')}>Сохранить сцену</button></div>
    <div><button disabled={!state||state.play==='playing'} onClick={()=>command('play.start')}>Play</button><button disabled={state?.play!=='playing'} onClick={()=>command('play.pause')}>Pause</button><button disabled={!state||state.play==='stopped'} onClick={()=>command('play.stop')}>Stop</button></div>
    {error&&<p role="alert" style={{color:'#ff9a9a'}}>{error}</p>}
  </details>;
}
