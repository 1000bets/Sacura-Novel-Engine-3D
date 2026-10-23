import React,{useEffect,useRef,useState} from 'react';
import WidgetRenderer from './WidgetRenderer.jsx';
import {createWidgetElement,rectTransform,reanchor,removeWidgetElement,widgetRects} from './widgetModel.js';
import './widgets.css';

const kinds={dialogue:'Диалог',mainMenu:'Главное меню',pauseMenu:'Меню паузы',hud:'Игровой HUD'};
const types={panel:'Панель',text:'Текст',button:'Кнопка',image:'Изображение',choices:'Ответы игрока'};
const roles={none:'Без привязки',speaker:'Имя персонажа',dialogueText:'Текст реплики',choices:'Ответы игрока',status:'Подсказка',title:'Название игры'};
const actions={none:'Без действия',advance:'Продолжить диалог',start:'Начать игру',resume:'Снять паузу'};
const id=()=>crypto.randomUUID();
const uniqueName=(library,base)=>{let name=base,index=2;while(library.widgets.some(widget=>widget.name.toLocaleLowerCase()===name.toLocaleLowerCase()))name=base+' ('+(index++)+')';return name;};
const copyWidget=(source,name)=>{
  const widget=structuredClone(source),ids=new Map(widget.elements.map(e=>[e.id,id()]));
  widget.id=id();widget.name=name;
  widget.elements.forEach(e=>{e.parentId=ids.get(e.parentId)||'';e.id=ids.get(e.id);});
  return widget;
};
function NumberInput({label,value,onChange,min,max,step=1}){
  return <label>{label}<input aria-label={label} type="number" min={min} max={max} step={step} value={value??0} onChange={event=>{const next=Number(event.target.value);if(event.target.value!==''&&Number.isFinite(next))onChange(Math.max(min??-Infinity,Math.min(max??Infinity,next)));}}/></label>;
}
export default function NativeWidgetWorkspace({snapshot,command,running,onStop,onPreview,request}){
  const [draft,setDraft]=useState(null),[widgetId,setWidgetId]=useState(null),[elementId,setElementId]=useState(null);
  const [frame,setFrame]=useState([1280,720]),[fit,setFit]=useState(1),[snap,setSnap]=useState(true),[error,setError]=useState('');
  const viewport=useRef(null),drag=useRef(null),latest=useRef(null),queue=useRef(Promise.resolve()),session=useRef(null);
  const library=draft||snapshot?.library;
  useEffect(()=>{if(!snapshot?.library)return;if(session.current!==snapshot.library){
    if(!draft){latest.current=snapshot.library;session.current=snapshot.library;}
  }},[snapshot?.library,draft]);
  useEffect(()=>{if(request?.id){setWidgetId(request.id);setElementId(null);}},[request]);
  useEffect(()=>{const node=viewport.current;if(!node)return;const observer=new ResizeObserver(()=>setFit(Math.max(.05,Math.min((node.clientWidth-30)/frame[0],(node.clientHeight-30)/frame[1]))));observer.observe(node);return()=>observer.disconnect();},[frame,snapshot?.open]);
  const commit=change=>{
    if(running||!latest.current)return;
    const next=structuredClone(latest.current);change(next);latest.current=next;setDraft(next);setError('');
    queue.current=queue.current.catch(()=>{}).then(async()=>{
      try{const value=await command('widgets.apply',{library:next});
        if(latest.current===next){latest.current=value.library;setDraft(null);}
      }catch(cause){if(latest.current===next){latest.current=snapshot?.library;setDraft(null);}setError(cause.message);}
    });
  };
  const operation=async method=>{await queue.current;try{const value=await command(method);latest.current=value.library;setDraft(null);setError('');return true;}catch(cause){setError(cause.message);return false;}};
  if(!snapshot?.open)return <p role="alert">{snapshot?.error||'Библиотека виджетов загружается из C++ Engine…'}</p>;
  if(!library?.widgets?.length)return <p role="alert">Библиотека UI пуста.</p>;
  const saved=library.widgets.find(w=>w.id===widgetId)||library.widgets[0];
  const element=saved.elements.find(e=>e.id===elementId);
  const rects=widgetRects(saved,frame[0],frame[1]),logical=widgetRects(saved);
  const parentSize=element?.parentId?logical[element.parentId]:{width:saved.referenceSize[0],height:saved.referenceSize[1]};
  const editWidget=change=>commit(p=>change(p.widgets.find(w=>w.id===saved.id),p));
  const editElement=change=>editWidget(w=>change(w.elements.find(e=>e.id===elementId),w));
  const patch=(key,value)=>editElement(e=>{e[key]=value;});
  const layout=(key,value)=>editElement(e=>{e.layout[key]=value;});
  const style=(key,value)=>editElement(e=>{e.style[key]=value;});
  const changeAnchor=(min,max,pivot=element.layout.pivot)=>editElement(e=>{e.layout=reanchor(e.layout,parentSize,min,max,pivot);});
  const add=type=>{if(!type)return;const newId=id();editWidget(w=>{const e=createWidgetElement(newId,type,{parentId:element?.type==='panel'?element.id:''});w.elements.push(e);});setElementId(newId);};
  const begin=(event,item,resize=false)=>{if(running||event.button!==0)return;event.preventDefault();event.stopPropagation();event.currentTarget.setPointerCapture(event.pointerId);setElementId(item.id);drag.current={id:item.id,x:event.clientX,y:event.clientY,resize,layout:{...item.layout},scale:fit*Math.min(frame[0]/saved.referenceSize[0],frame[1]/saved.referenceSize[1])};};
  const move=event=>{const d=drag.current;if(!d)return;const step=snap?8:1,dx=Math.round((event.clientX-d.x)/d.scale/step)*step,dy=Math.round((event.clientY-d.y)/d.scale/step)*step;
    const current={...d.layout,x:d.layout.x+(d.resize?dx:dx),y:d.layout.y+(d.resize?dy:dy)};
    if(d.resize){current.width=Math.max(20,d.layout.width+dx);current.height=Math.max(20,d.layout.height+dy);}
    drag.current.preview=current;const hit=event.currentTarget;hit.style.transform=`translate(${dx*d.scale}px,${dy*d.scale}px)`;
  };
  const finish=event=>{const d=drag.current;if(!d)return;drag.current=null;event.currentTarget.style.transform='';if(d.preview)editWidget(w=>Object.assign(w.elements.find(e=>e.id===d.id).layout,d.preview));if(event.currentTarget.hasPointerCapture(event.pointerId))event.currentTarget.releasePointerCapture(event.pointerId);};
  const remove=()=>{editWidget(w=>removeWidgetElement(w,element.id));setElementId(null);};
  return <div className="widget-workspace native-widget-workspace">
    <header><strong>Редактор виджетов · C++ Engine</strong><select aria-label="Виджет для редактирования" value={saved.id} onChange={e=>{setWidgetId(e.target.value);setElementId(null);}}>{library.widgets.map(w=><option key={w.id} value={w.id}>{w.name} · {kinds[w.kind]}</option>)}</select>
      <button disabled={running} onClick={()=>{const next=copyWidget(saved,uniqueName(library,saved.name+' · копия'));commit(p=>p.widgets.push(next));setWidgetId(next.id);setElementId(null);}}>Копия</button>
      <button disabled={running} onClick={()=>{const next=copyWidget(saved,uniqueName(library,'Новый '+kinds[saved.kind].toLowerCase()));commit(p=>p.widgets.push(next));setWidgetId(next.id);setElementId(null);}}>Создать</button>
      <button disabled={running||saved.kind!=='dialogue'||library.ui.dialogue!==saved.id} title={saved.kind!=='dialogue'?'В игре сейчас поддержан предпросмотр диалога':library.ui.dialogue!==saved.id?'Сначала выберите этот виджет как стандартный диалог':'Запустить C++ PlaySession'} onClick={async()=>{if(await operation('widgets.save'))onPreview();}}>Проверить в игре</button>
      <button disabled={!snapshot.dirty||running} onClick={()=>operation('widgets.save')}>Сохранить</button>
      <button disabled={!snapshot.canUndo||running} onClick={()=>operation('widgets.undo')}>↶</button><button disabled={!snapshot.canRedo||running} onClick={()=>operation('widgets.redo')}>↷</button>
    </header>
    {running&&<p role="status">Остановите игру для редактирования. <button onClick={onStop}>Остановить</button></p>}
    {saved.kind!=='dialogue'&&<p role="status" className="widget-save-note">Этот тип сохраняется в библиотеке UI. Игровой предпросмотр C++ сейчас доступен для диалога.</p>}
    {error&&<p className="widget-css-error" role="alert">{error}</p>}
    <div className="widget-body"><aside className="widget-library"><h3>Интерфейс проекта</h3>{Object.entries(kinds).map(([kind,label])=><label key={kind}>{label}<select disabled={running} value={library.ui[kind]} onChange={e=>commit(p=>{p.ui[kind]=e.target.value;})}>{library.widgets.filter(w=>w.kind===kind).map(w=><option key={w.id} value={w.id}>{w.name}</option>)}</select></label>)}
      <h3>Элементы</h3>{saved.elements.map(e=><button key={e.id} aria-pressed={e.id===elementId} className={e.id===elementId?'active':''} style={{paddingLeft:e.parentId?22:8}} onClick={()=>setElementId(e.id)}>{e.name}{!e.visible?' ◌':''}</button>)}
      <select disabled={running} aria-label="Добавить элемент виджета" value="" onChange={e=>add(e.target.value)}><option value="">＋ Добавить элемент…</option>{Object.entries(types).map(([type,label])=><option key={type} value={type}>{label}</option>)}</select><small>Стрелки перемещают выбранный элемент; Shift — шаг 10.</small>
    </aside><main className="widget-canvas-area"><div className="widget-canvas-toolbar"><select aria-label="Разрешение предпросмотра виджета" value={frame.join('x')} onChange={e=>setFrame(e.target.value.split('x').map(Number))}>{[[1280,720],[1920,1080],[1024,768],[720,1280],[2560,1080]].map(s=><option key={s.join('x')}>{s.join('x')}</option>)}</select><label><input type="checkbox" checked={snap} onChange={e=>setSnap(e.target.checked)}/> Привязка 8 px</label><span className="micro">{Math.round(fit*100)}%</span></div>
      <div className="widget-canvas-viewport" ref={viewport}><div className="widget-canvas" style={{width:frame[0],height:frame[1],transform:`translate(${-frame[0]*fit/2}px,${-frame[1]*fit/2}px) scale(${fit})`}}><div className="widget-grid"/><WidgetRenderer widget={saved} context={{speaker:'Алиса',text:'Так будет выглядеть диалог в вашей игре.',status:'Нажмите, чтобы продолжить',title:'Название игры',choices:[{id:'a',label:'Первый ответ'},{id:'b',label:'Второй ответ'}],ready:true,canAdvance:true}} editing width={frame[0]} height={frame[1]}/>
      {saved.elements.map(e=>{const r=rects[e.id];return r&&<div key={e.id} role="button" tabIndex={0} aria-label={'Выбрать и переместить: '+e.name} className={'widget-hit'+(e.id===elementId?' selected':'')} style={{left:r.x,top:r.y,width:r.width,height:r.height}} onClick={()=>setElementId(e.id)} onPointerDown={event=>begin(event,e)} onPointerMove={move} onPointerUp={finish} onPointerCancel={()=>{drag.current=null;}} onKeyDown={event=>{const delta={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[event.key];if(!delta||running)return;event.preventDefault();const step=event.shiftKey?10:1;editWidget(w=>{const l=w.elements.find(x=>x.id===e.id).layout;l.x+=delta[0]*step;l.y+=delta[1]*step;});}}>{e.id===elementId&&<span role="button" aria-label={'Изменить размер: '+e.name} className="widget-resize" onPointerDown={event=>begin(event,e,true)} onPointerMove={move} onPointerUp={finish}/>}</div>;})}</div></div></main>
      <aside className="widget-properties"><fieldset disabled={running} style={{border:0,padding:0,margin:0,minWidth:0}}><h3>Виджет</h3><label>Название<input value={saved.name} onChange={e=>editWidget(w=>{w.name=e.target.value;})}/></label><label>Тип<select value={saved.kind} onChange={e=>editWidget((w,p)=>{const old=w.kind,next=e.target.value;w.kind=next;if(p.ui[old]===w.id)p.ui[old]=p.widgets.find(other=>other.id!==w.id&&other.kind===old)?.id;p.ui[next]=w.id;})}>{Object.entries(kinds).map(([kind,label])=><option key={kind} value={kind} disabled={kind!==saved.kind&&library.widgets.filter(w=>w.kind===saved.kind).length<2}>{label}</option>)}</select></label><div className="widget-number-row">{saved.referenceSize.map((value,i)=><NumberInput key={i} label={i?'Высота холста':'Ширина холста'} min={100} max={7680} value={value} onChange={v=>editWidget(w=>{w.referenceSize[i]=v;})}/>)}</div>
      {element?<><h3>Элемент</h3><label>Название<input value={element.name} onChange={e=>patch('name',e.target.value)}/></label><label><input type="checkbox" checked={element.visible} onChange={e=>patch('visible',e.target.checked)}/> Видимый</label><label>Родитель<select value={element.parentId||''} onChange={event=>{const parentId=event.target.value,rect=logical[element.id],parent=parentId?logical[parentId]:{x:0,y:0,width:saved.referenceSize[0],height:saved.referenceSize[1]};editElement(e=>{e.parentId=parentId;e.layout=rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:rect.x-parent.x,y:rect.y-parent.y,width:rect.width,height:rect.height});});}}><option value="">Холст</option>{saved.elements.filter(candidate=>{if(candidate.type!=='panel'||candidate.id===element.id)return false;let parent=candidate,seen=new Set();while(parent&&!seen.has(parent.id)){if(parent.parentId===element.id)return false;seen.add(parent.id);parent=saved.elements.find(other=>other.id===parent.parentId);}return true;}).map(e=><option key={e.id} value={e.id}>{e.name}</option>)}</select></label>
      <details open><summary>Положение и анкеры</summary><div className="widget-presets">{[[0,0],[.5,.5],[1,1]].map(a=><button key={a.join()} onClick={()=>changeAnchor(a,a,a)}>{a.join(' · ')}</button>)}<button onClick={()=>changeAnchor([0,0],[1,1])}>Заполнить</button></div><div className="widget-number-row">{[['x','X'],['y','Y'],['width','Ширина / Δ'],['height','Высота / Δ']].map(([key,label])=><NumberInput key={key} label={label} value={element.layout[key]} onChange={v=>layout(key,v)}/>)}</div>{['anchorMin','anchorMax','pivot'].map(key=><div className="widget-number-row" key={key}>{[0,1].map(axis=><NumberInput key={axis} label={key+' '+(axis?'Y':'X')} min={0} max={1} step={.05} value={element.layout[key][axis]} onChange={v=>{const a=[...element.layout[key]];a[axis]=v;changeAnchor(key==='anchorMin'?a:element.layout.anchorMin,key==='anchorMax'?a:element.layout.anchorMax,key==='pivot'?a:element.layout.pivot);}}/>)}</div>)}</details>
      <details open><summary>Содержимое и действия</summary><label>Привязка<select value={element.role} onChange={e=>patch('role',e.target.value)}>{Object.entries(roles).map(([key,label])=><option key={key} value={key}>{label}</option>)}</select></label>{element.type!=='panel'&&<label>Текст / подпись<textarea rows={2} value={element.text||''} onChange={e=>patch('text',e.target.value)}/></label>}{element.type==='button'&&<label>Действие<select value={element.action} onChange={e=>patch('action',e.target.value)}>{Object.entries(actions).filter(([key])=>saved.kind!=='dialogue'||key==='none'||key==='advance'||key===element.action).map(([key,label])=><option key={key} value={key}>{label}</option>)}</select></label>}{element.type==='image'&&<label>Загрузить PNG/JPEG<input type="file" accept="image/png,image/jpeg" onChange={async e=>{const file=e.target.files?.[0];if(!file)return;if(file.size>2*1024*1024){setError('Выберите изображение до 2 МБ.');return;}const src=await new Promise(resolve=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.readAsDataURL(file);});patch('src',src);}}/></label>}</details>
      <details open><summary>Оформление</summary>{[['background','Фон'],['color','Цвет текста'],['borderColor','Цвет рамки']].map(([key,label])=><label key={key}>{label}<input value={element.style[key]||''} onChange={e=>style(key,e.target.value)}/></label>)}{[['fontSize','Размер текста'],['borderRadius','Скругление'],['borderWidth','Рамка'],['padding','Отступы'],['gap','Расстояние ответов'],['opacity','Непрозрачность']].map(([key,label])=><NumberInput key={key} label={label} min={0} max={key==='opacity'?1:1000} step={key==='opacity'?.01:1} value={element.style[key]} onChange={v=>style(key,v)}/>)}<label>Шрифт<input value={element.style.fontFamily||''} onChange={e=>style('fontFamily',e.target.value)}/></label><label>Начертание<select value={element.style.fontWeight||400} onChange={e=>style('fontWeight',Number(e.target.value))}>{[400,500,600,700].map(n=><option key={n}>{n}</option>)}</select></label><label>Выравнивание<select value={element.style.textAlign||'left'} onChange={e=>style('textAlign',e.target.value)}><option value="left">Слева</option><option value="center">По центру</option><option value="right">Справа</option></select></label></details>
      <div className="button-row"><button disabled={saved.elements.indexOf(element)===0} onClick={()=>editWidget(w=>{const i=w.elements.findIndex(e=>e.id===element.id);[w.elements[i-1],w.elements[i]]=[w.elements[i],w.elements[i-1]];})}>Назад</button><button disabled={saved.elements.indexOf(element)===saved.elements.length-1} onClick={()=>editWidget(w=>{const i=w.elements.findIndex(e=>e.id===element.id);[w.elements[i+1],w.elements[i]]=[w.elements[i],w.elements[i+1]];})}>Вперёд</button></div><button onClick={remove}>Удалить элемент и детей</button></>:<p>Выберите элемент на холсте или в списке слева.</p>}
      <button disabled={library.widgets.filter(w=>w.kind===saved.kind).length<2} onClick={()=>{commit(p=>{p.widgets=p.widgets.filter(w=>w.id!==saved.id);for(const kind of Object.keys(kinds))if(p.ui[kind]===saved.id)p.ui[kind]=p.widgets.find(w=>w.kind===kind)?.id;});setWidgetId(null);setElementId(null);}}>Удалить виджет</button></fieldset></aside></div><p className="widget-save-note">Изменения проверяет C++ Engine · Сохранить записывает Content/UI/Project.widgets</p>
  </div>;
}
