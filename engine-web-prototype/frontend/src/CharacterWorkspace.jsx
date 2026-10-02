import React,{useEffect,useRef,useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
import LocationScene from './LocationScene.jsx';
import CharacterAnimationPool from './CharacterAnimationPool.jsx';
import CharacterModelUpload from './CharacterModelUpload.jsx';
import {uid} from './model.js';
import {characterAnimations,enabledCharacterAnimations,characterScenes} from './characterModel.js';
import {importCharacterFile} from './characterAssets.js';
import {isObjectInScene} from './sceneEditing.js';

export default function CharacterWorkspace({project,scene,selected,onSelect,onCreate,onPatch,onPresence,onTransform,onReset,onPlace,onDuplicate,onDelete,running,onStop}){
 const characters=project.objects.filter(object=>object.type==='Персонаж'),character=characters.find(item=>item.id===selected)||characters[0];
 const [query,setQuery]=useState(''),[name,setName]=useState(''),[error,setError]=useState(''),[busy,setBusy]=useState(false),[previewAnimation,setPreviewAnimation]=useState(null),[playing,setPlaying]=useState(true);
 const [previewToken,setPreviewToken]=useState(0),[mixamoInPlace,setMixamoInPlace]=useState(true);
 const fileRef=useRef(),animationRef=useRef(),mainRef=useRef(),previewRef=useRef(),operation=useRef(0);
 useEffect(()=>{operation.current++;setBusy(false);setPreviewAnimation(null);setError('');setPlaying(true);if(mainRef.current)mainRef.current.scrollTop=0;},[character?.id]);
 useEffect(()=>()=>{operation.current++;},[]);
 const pool=characterAnimations(character),enabled=enabledCharacterAnimations(character),locations=character?characterScenes(project,character):[],transformScene=locations.find(item=>item.id===scene.id)||locations[0]||scene;
 const patch=value=>onPatch(character.id,value);
 const upload=async(event,animationsOnly)=>{
  const file=event.target.files?.[0];event.target.value='';if(!file||!character)return;
  const id=character.id,token=++operation.current;setBusy(true);setError('');
  try{
   const result=await importCharacterFile(file,{animationsOnly,targetModel:character.model,inPlace:mixamoInPlace});if(operation.current!==token)return;
   if(animationsOnly){
    const clips=result.map(clip=>({...clip,id:uid('animation')}));
    onPatch(id,{extraAnimations:[...(character.extraAnimations||[]),...clips],enabledAnimations:[...enabled.map(clip=>clip.id),...clips.map(clip=>clip.id)]});setPreviewAnimation(clips[0].id);setPlaying(true);setPreviewToken(value=>value+1);
   }else{
    onPatch(id,{model:result,animationDetails:{},extraAnimations:[],enabledAnimations:result.animations.map(clip=>clip.id),animationSettings:{},idleAnimation:result.animations[0]?.id||'',walkAnimation:result.animations.find(clip=>/walk|ходь|шаг/i.test(clip.name))?.id||''});setPreviewAnimation(result.animations[0]?.id||'');
   }
  }catch(cause){if(operation.current===token)setError(cause.message);}
  finally{if(operation.current===token)setBusy(false);}
 };
 const create=event=>{event.preventDefault();setError('');try{onCreate(name);setName('');}catch(cause){setError(cause.message);}};
 const animation=previewAnimation===null?(enabled.find(clip=>clip.id===character?.idleAnimation)?.id||enabled[0]?.id||''):(enabled.some(clip=>clip.id===previewAnimation)?previewAnimation:'');
 const previewObject=character?{...character,active:true,animationPreviewToken:previewToken,transforms:{'character-preview':{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}}:null;
 return <div className="character-workspace">
  <header><div><strong><Icon name="Users"/>Персонажи</strong><small>{characters.length} в проекте · модели, анимации и размещение</small></div></header>
  {running&&<div className="camera-running">Для изменения персонажей остановите воспроизведение.<Button icon="Square" onClick={onStop}>Остановить</Button></div>}
  {busy&&<p className="subscene-field-note" role="status">Импортируем файл и подготавливаем анимации…</p>}
  {error&&<p className="subscene-form-error" role="alert">{error}</p>}
  <div className="character-workspace-body"><aside>
   <label className="subscene-search"><Icon name="Search" size={14}/><input aria-label="Найти персонажа" placeholder="Найти персонажа…" value={query} onChange={event=>setQuery(event.target.value)}/></label>
   <form onSubmit={create}><label>Новый персонаж<input aria-label="Имя нового персонажа" required placeholder="Например, Алиса" value={name} onChange={event=>setName(event.target.value)}/></label><Button icon="UserPlus" type="submit" disabled={running||busy}>Создать персонажа</Button></form>
   <div className="character-list">{characters.filter(item=>item.name.toLowerCase().includes(query.toLowerCase())).map(item=><button key={item.id} className={character?.id===item.id?'active':''} onClick={()=>onSelect(item.id)}><Icon name="PersonStanding" style={{color:item.color}}/><span>{item.name}<small>{item.model?.name||'Стандартная модель'} · {enabledCharacterAnimations(item).length} анимаций{item.active===false?' · выключен':''}</small></span></button>)}</div>
   {!characters.length&&<p>Создайте персонажа по имени выше, затем загрузите его модель и настройте анимации.</p>}
  </aside><main ref={mainRef}>{character?<>
   <div className="subscene-edit-heading"><div><small>ПЕРСОНАЖ ПРОЕКТА</small><h2>{character.name}</h2></div><div className="character-buttons"><Button icon="Copy" disabled={running||busy} onClick={()=>onDuplicate(character.id)}>Дублировать</Button><Button icon="Trash2" disabled={running||busy} onClick={()=>onDelete(character.id)}>Удалить персонажа</Button></div></div>
   <fieldset disabled={running||busy} className="character-details"><div className="subscene-form-grid two"><label>Имя<input value={character.name} onChange={event=>patch({name:event.target.value})}/></label><label>Цвет стандартной модели<input type="color" value={character.color||'#bb99aa'} onChange={event=>patch({color:event.target.value})}/></label></div><label>Описание<textarea rows={2} value={character.description||''} placeholder="Кто этот персонаж?" onChange={event=>patch({description:event.target.value})}/></label><label className="check"><input type="checkbox" checked={character.active!==false} onChange={event=>patch({active:event.target.checked})}/>Персонаж включён в проекте</label>
   <CharacterModelUpload model={character.model} disabled={running||busy} busy={busy} onChoose={()=>fileRef.current.click()} onFile={file=>upload({target:{files:[file],value:''}},false)} onReset={()=>{patch({model:null,animationDetails:{},extraAnimations:[],enabledAnimations:null,idleAnimation:'стоит',walkAnimation:'',animationSettings:{}});setPreviewAnimation(null);}}/>
   <input ref={fileRef} hidden type="file" accept=".glb" onChange={event=>upload(event,false)}/>
   </fieldset>
   <section ref={previewRef} className="character-preview"><div className="character-preview-canvas">{previewObject&&<LocationScene key={character.id} kind="empty" sceneId="character-preview" objects={[previewObject]} mode="scene" editing={!animation} showGrid selected={character.id} state={{location:'character-preview',weather:'Ясно',time:'День',poses:{[character.id]:animation},paused:!playing}} focusRequest={character.id}/>}</div><div className="character-buttons"><strong>Предпросмотр анимации</strong><select aria-label="Анимация предпросмотра" value={animation||''} onChange={event=>{setPreviewAnimation(event.target.value);setPlaying(true);}}><option value="">Без анимации</option>{enabled.map(clip=><option key={clip.id} value={clip.id}>{clip.name}</option>)}</select><Button icon={playing?'Pause':'Play'} onClick={()=>setPlaying(value=>!value)}>{playing?'Пауза':'Проиграть'}</Button></div></section>
   <fieldset disabled={running||busy} className="character-details">
   <CharacterAnimationPool mixamoInPlace={mixamoInPlace} onMixamoInPlace={setMixamoInPlace} key={character.id} character={character} pool={pool} enabled={enabled} patch={patch} current={animation} onUpload={()=>animationRef.current.click()} onModelUpload={()=>fileRef.current.click()} onPreview={id=>{setPreviewAnimation(id);setPreviewToken(value=>value+1);setPlaying(true);previewRef.current?.scrollIntoView({block:'nearest',behavior:'smooth'});}}/>
   <input ref={animationRef} hidden type="file" accept=".glb,.fbx" onChange={event=>upload(event,true)}/>
   <div className="subscene-form-grid two">{[['idleAnimation','Ожидание'],['walkAnimation','Ходьба при перемещении']].map(([key,label])=><label key={key}>{label}<select value={enabled.some(clip=>clip.id===character[key])?character[key]:''} onChange={event=>patch({[key]:event.target.value})}><option value="">Не назначена</option>{enabled.map(clip=><option key={clip.id} value={clip.id}>{clip.name}</option>)}</select></label>)}</div>
   <h3>Локации персонажа</h3><div className="subscene-cast">{project.subscenes.map(item=><label key={item.id}><input type="checkbox" checked={isObjectInScene(character,item)} onChange={event=>onPresence(character.id,item.id,event.target.checked)}/>{item.name}</label>)}</div><Button icon="Focus" disabled={!locations.length} onClick={()=>onPlace(character.id,transformScene.id)}>Открыть персонажа в локации</Button>
   </fieldset>
   <p className="subscene-save-note">Изменения сохраняются автоматически · Ctrl+Z — отменить</p>
  </>:<div className="character-empty"><Icon name="Users" size={40}/><h2>Персонажи вашей истории</h2><p>Введите имя слева и нажмите «Создать персонажа». Затем загрузите меш, выберите анимации и локации.</p></div>}</main></div>
 </div>;
}
