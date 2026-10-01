import React,{useState} from 'react';
import {Button} from './StudioParts.jsx';
import AnimationThumbnail from './AnimationThumbnail.jsx';
import {BUILTIN_CHARACTER_ANIMATIONS} from './characterModel.js';
import {uid} from './model.js';

export default function CharacterAnimationPool({character,pool,enabled,patch,onUpload,onModelUpload,onPreview,current,mixamoInPlace=true,onMixamoInPlace}){
 const [adding,setAdding]=useState(false),[name,setName]=useState(''),[description,setDescription]=useState(''),[basePose,setBasePose]=useState('танец'),[editing,setEditing]=useState(null);
 const details=(id,value)=>patch({animationDetails:{...character.animationDetails,[id]:{...character.animationDetails?.[id],...value}}});
 const settings=(id,value)=>patch({animationSettings:{...character.animationSettings,[id]:{...character.animationSettings?.[id],...value}}});
 const create=event=>{
  event.preventDefault();if(!name.trim())return;
  const base=BUILTIN_CHARACTER_ANIMATIONS.find(clip=>clip.id===basePose),clip={id:uid('animation'),name:name.trim(),description:description.trim()||base.description,basePose,icon:base.icon,duration:0};
  patch({extraAnimations:[...(character.extraAnimations||[]),clip],enabledAnimations:[...enabled.map(item=>item.id),clip.id]});setAdding(false);setName('');setDescription('');
 };
 return <section className="character-animation-pool" data-help-title="Пул анимаций" data-help="Добавляйте движения, просматривайте их и отмечайте те, которые хотите использовать в сценарии. Описание на карточке объясняет, что делает персонаж.">
  <div className="character-pool-heading"><div><h3>Пул анимаций</h3><p>Выберите движения, которые будут доступны в сценарии. Нажмите «Посмотреть», чтобы увидеть движение на персонаже выше.</p></div><Button icon={character.model?'Upload':'Plus'} onClick={()=>character.model?onUpload():setAdding(value=>!value)}>{character.model?'Добавить анимации из файла':'Добавить анимацию'}</Button></div>
  <p className="subscene-field-note">{character.model?'Mixamo: скачайте движение как FBX Binary → Without Skin (до 20 МБ). Движение автоматически переносится на humanoid-скелет модели. Также можно загрузить GLB до 3 МБ с тем же скелетом и именами костей.':'Для стандартной фигуры можно создать вариант готового движения. Для собственных движений из GLB сначала загрузите модель персонажа.'}</p>
  {character.model&&<label className="character-animation-enabled"><input type="checkbox" checked={mixamoInPlace} onChange={event=>onMixamoInPlace?.(event.target.checked)}/>Импорт Mixamo: движение на месте (сохраняет прыжки; перемещение задаёт сценарий)</label>}
  {!character.model&&<Button icon="Upload" onClick={onModelUpload}>Загрузить модель для своих анимаций</Button>}
  {adding&&!character.model&&<form className="character-animation-create" onSubmit={create}><strong>Новое движение на основе готового</strong><label>Название<input required value={name} placeholder="Например, радостное приветствие" onChange={event=>setName(event.target.value)}/></label><label>Основа движения<select value={basePose} onChange={event=>setBasePose(event.target.value)}>{BUILTIN_CHARACTER_ANIMATIONS.map(clip=><option key={clip.id} value={clip.id}>{clip.name}</option>)}</select></label><label>Что делает персонаж<textarea value={description} placeholder={BUILTIN_CHARACTER_ANIMATIONS.find(clip=>clip.id===basePose).description} onChange={event=>setDescription(event.target.value)}/></label><div className="character-buttons"><Button icon="Plus" type="submit">Добавить в пул</Button><Button type="button" onClick={()=>setAdding(false)}>Отмена</Button></div></form>}
  <div className="character-animation-cards">{pool.map(clip=>{
   const available=enabled.some(item=>item.id===clip.id),custom=character.extraAnimations?.some(item=>item.id===clip.id),isEditing=editing===clip.id;
   return <article key={clip.id} className={'character-animation-card'+(current===clip.id?' previewing':'')}>
    <AnimationThumbnail character={character} clip={clip}/>
    <div className="character-animation-card-heading"><div><h4>{clip.name}</h4><small>{clip.basePose?`Вариант движения «${clip.basePose}»`:clip.source==='mixamo'?`Mixamo · ${clip.inPlace?'на месте':'с перемещением'}`:character.model?'Из файла модели':'Готовое движение'}{clip.duration?` · ${clip.duration.toFixed(2)} сек`:''}</small></div></div>
    <p className="character-animation-description">{clip.description||'Описание движения ещё не заполнено. Посмотрите анимацию и добавьте, что делает персонаж.'}</p>
    {isEditing&&<div className="character-animation-edit"><label>Название<input aria-label={'Название анимации '+clip.name} value={clip.name} onChange={event=>details(clip.id,{name:event.target.value})}/></label><label>Суть анимации<textarea aria-label={'Описание анимации '+clip.name} value={clip.description||''} placeholder="Например: машет правой рукой, приветствуя собеседника" onChange={event=>details(clip.id,{description:event.target.value})}/></label><Button icon="Check" onClick={()=>setEditing(null)}>Готово</Button></div>}
    <label className="character-animation-enabled"><input type="checkbox" checked={available} onChange={event=>patch({enabledAnimations:event.target.checked?[...enabled.map(item=>item.id),clip.id]:enabled.filter(item=>item.id!==clip.id).map(item=>item.id)})}/>Доступна в сценарии</label>
    <div className="character-animation-settings">{character.model&&<label><input type="checkbox" checked={character.animationSettings?.[clip.id]?.loop!==false} onChange={event=>settings(clip.id,{loop:event.target.checked})}/>Повторять</label>}<label>Скорость<input aria-label={'Скорость '+clip.name} type="number" min="0.1" max="4" step="0.1" value={character.animationSettings?.[clip.id]?.speed??1} onChange={event=>settings(clip.id,{speed:Number(event.target.value)})}/>×</label></div>
    <div className="character-animation-card-actions"><Button icon="Play" disabled={!available} onClick={()=>onPreview(clip.id)}>Посмотреть</Button><Button icon="Pencil" onClick={()=>setEditing(isEditing?null:clip.id)}>Описание</Button>{custom&&<Button icon="Trash2" title={'Удалить анимацию '+clip.name} onClick={()=>{patch({extraAnimations:character.extraAnimations.filter(item=>item.id!==clip.id),enabledAnimations:enabled.filter(item=>item.id!==clip.id).map(item=>item.id)});}}>Удалить</Button>}</div>
   </article>;
  })}</div>
  {!pool.length&&<p className="character-animation-empty">В модели пока нет движений. Нажмите «Добавить анимации из файла», чтобы загрузить их.</p>}
 </section>;
}
