import React,{useEffect,useRef,useState} from 'react';
import MechanicScriptEditor from './MechanicScriptEditor.jsx';
import './nativeGameplay.css';

const api=()=>window.sacuraDesktop.nativeMechanics;
export default function NativeGameplayWorkspace({session,objects,selectedObjectId,playing,onPlay,onStop}) {
  const [records,setRecords]=useState([]),[selected,setSelected]=useState(''),[draft,setDraft]=useState('');
  const [status,setStatus]=useState(''),[error,setError]=useState(''),[results,setResults]=useState([]);
  const queue=useRef(Promise.resolve()),current=records.find(item=>item.id===selected);
  useEffect(()=>{
    let active=true;setRecords([]);setSelected('');setDraft('');setError('');
    if(!session)return;
    api()('list').then(list=>{if(active){setRecords(list);setSelected(list[0]?.id||'');setDraft(list[0]?.source||'');}}).catch(e=>{if(active)setError(e.message)});
    const off=window.sacuraDesktop.onNativeMechanicsResult(outcomes=>{if(active)setResults(outcomes)});
    return()=>{active=false;off();};
  },[session]);
  const select=id=>{setSelected(id);setDraft(records.find(item=>item.id===id)?.source||'');setError('');};
  const run=async(action,params,after)=>{try{await queue.current;setError('');const list=await api()(action,params);setRecords(list);after?.(list);return list;}catch(e){setError(e.message);return null;}};
  const update=patch=>{if(!current||playing)return;const id=current.id;
    if('source' in patch){setDraft(patch.source);setStatus('Черновик сохраняется…');}
    setRecords(list=>list.map(item=>item.id===id?{...item,...patch}:item));
    queue.current=queue.current.catch(()=>{}).then(()=>api()('update',{id,patch}));
    queue.current.then(()=>{setStatus('Черновик сохранён');setError('');}).catch(e=>setError(e.message));
  };
  const compile=async()=>{if(!current||playing)return;try{await queue.current;const list=await api()('compile',{id:current.id});setRecords(list);setStatus('Скомпилировано. Скрипт выполнится при следующем запуске.');setError('');}catch(e){setError(e.message);setStatus('Последняя рабочая версия сохранена.');}};
  const create=()=>run('create',{objectId:selectedObjectId||null},list=>selectNew(list));
  const selectNew=list=>{const item=list.at(-1);setSelected(item?.id||'');setDraft(item?.source||'');};
  const imported=()=>run('import',{objectId:selectedObjectId||null},selectNew);
  const remove=()=>{if(!current||!confirm(`Удалить механику «${current.name}»?`))return;run('remove',{id:current.id},list=>{setSelected(list[0]?.id||'');setDraft(list[0]?.source||'');});};
  const applied=current?.enabled&&current?.source===current?.compiledSource;
  return <section className="native-mechanics" aria-label="Игровые механики C++ проекта">
    <aside className="native-mechanics-list">
      <header><strong>Игровые механики</strong><button type="button" disabled={playing} onClick={create}>+ Добавить скрипт</button></header>
      <div className="native-mechanics-items">{records.map(item=><button type="button" key={item.id} className={item.id===selected?'active':''} onClick={()=>select(item.id)}><strong>{item.name}</strong><small>{objects.find(object=>object.id===item.objectId)?.name||'Объект не выбран'} · {item.enabled?'Включена':'Не скомпилирована'}</small></button>)}</div>
      {!records.length&&<p>Пока нет механик. Добавьте скрипт и выберите объект сцены.</p>}
      <button type="button" disabled={playing} onClick={imported}>Импортировать…</button>
    </aside>
    <div className="native-mechanics-editor">
      {current?<>
        <header className="native-mechanics-toolbar"><input aria-label="Название механики" value={current.name} disabled={playing} onChange={e=>update({name:e.target.value})}/><button type="button" disabled={playing} onClick={compile}>Скомпилировать</button><button type="button" onClick={playing?onStop:onPlay}>{playing?'Остановить':'Запустить'}</button></header>
        <div className="native-mechanics-settings"><label>Объект <select aria-label="Объект механики" value={current.objectId||''} disabled={playing} onChange={e=>update({objectId:e.target.value||null})}><option value="">Выберите объект…</option>{objects.map(object=><option key={object.id} value={object.id}>{object.name}</option>)}</select></label><span>Запуск: при старте игры</span><span>{applied?'Код скомпилирован':current.compiledSource?'Есть несобранные изменения':'Код не скомпилирован'}</span></div>
        <details className="native-mechanics-help"><summary>API скрипта</summary><p>Код выполняется в PlayWorld. <code>engine.self.translate(x, y, z)</code> перемещает выбранный объект, <code>engine.self.setActive(false)</code> скрывает его, <code>await engine.self.position()</code> возвращает позицию. Черновик сохраняется автоматически; Ctrl+S компилирует.</p></details>
        <div className="native-mechanics-code"><MechanicScriptEditor key={current.id} source={draft} onChange={source=>update({source})} onCompile={compile} diagnostic={error?{line:Number(error.match(/Строка (\d+)/)?.[1]||1),message:error}:null} disabled={playing} parseSource={()=>null}/></div>
        <footer><span role={error?'alert':'status'}>{error||status||'JavaScript механики · запуск в C++ PlaySession'}</span>{results.filter(item=>item.id===current.id).map((item,index)=><span key={index} role={item.error?'alert':'status'}>{item.error||'Выполнено в PlayWorld'}</span>)}<button type="button" disabled={playing} onClick={()=>run('export',{id:current.id})}>Экспортировать…</button><button type="button" disabled={playing} onClick={remove}>Удалить</button></footer>
      </>:<div className="native-mechanics-empty">Добавьте скрипт, чтобы настроить игровую механику.</div>}
    </div>
  </section>;
}
