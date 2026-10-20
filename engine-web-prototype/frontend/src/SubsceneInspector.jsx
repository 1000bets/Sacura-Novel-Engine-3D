import React,{useEffect,useRef,useState} from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import Section from './InspectorSection.jsx';
import NavigationInspector from './NavigationInspector.jsx';
import {beatsInScene,sceneTransitions} from './subsceneModel.js';
import {isPureNode} from './logicModel.js';
import {beatPreview} from './storyLabels.js';
import {isObjectInScene} from './sceneEditing.js';
import './storyInspector.css';

// Commit complete names on blur; Escape and blank names leave the project intact.
export function InspectorText({value,onCommit,multiline=false,...props}){
 const [draft,setDraft]=useState(value),cancelled=useRef(false);
 useEffect(()=>setDraft(value),[value]);
 const Tag=multiline?'textarea':'input';
 return <Tag {...props} value={draft} onChange={e=>{cancelled.current=false;setDraft(e.target.value);}}
  onBlur={()=>{const next=multiline?draft:draft.trim();if(cancelled.current||(!multiline&&!next))setDraft(value);else if(next!==value)onCommit(next);cancelled.current=false;}}
  onKeyDown={e=>{if(e.nativeEvent.isComposing)return;if(e.key==='Escape'||(!multiline&&e.key==='Enter')){e.preventDefault();e.stopPropagation();cancelled.current=e.key==='Escape';if(cancelled.current)setDraft(value);e.currentTarget.blur();}}}/>;
}

export default function SubsceneInspector({project,scene,running,onPatch,onEntry,onOpenBeat,onScene,onCameras,onOpenEditor}){
 useLocale();
 const [reroute,setReroute]=useState(true),lines=beatsInScene(project,scene.id).filter(b=>!isPureNode(b)),cast=project.objects.filter(o=>o.type==='Персонаж'&&(!o.subsceneId||o.subsceneId===scene.id));
 const incoming=sceneTransitions(project,scene.id).filter(l=>l.target===scene.id&&l.to===scene.entry).length;
 const patch=values=>{if(!running)onPatch(values);};
 return <div className="story-inspector subscene-inspector">
  <div className="story-inspector-heading"><Icon name="PanelsTopLeft" size={16}/><strong>{tr('Сабсцена')}</strong></div>
  <fieldset className="inspector-primary-fields" disabled={running}>
   <Field label={tr('Название')}><InspectorText key={scene.id+':name'} aria-label={tr('Название сабсцены')} value={scene.name} onCommit={name=>patch({name})}/></Field>
   <Field label={tr('Место')}><InspectorText key={scene.id+':location'} aria-label={tr('Название места')} value={scene.location||''} onCommit={location=>patch({location})}/></Field>
   <Field label={tr('Погода')}><Select aria-label={tr('Погода')} value={scene.weather} options={['Ясно','Дождь','Гроза','Туман','Снег'].map(x=>[x,x])} onChange={weather=>patch({weather})}/></Field>
   <Field label={tr('Время суток')}><Select aria-label={tr('Время суток')} value={scene.time} options={['Рассвет','День','Закат','Ночь'].map(x=>[x,x])} onChange={time=>patch({time})}/></Field>
  </fieldset>
  <div className="inspector-shortcuts">
   <Button icon="Workflow" title={tr('Открыть сценарий')} onClick={()=>onOpenBeat(scene.entry)}>{tr('Сценарий')}</Button>
   <Button icon="Box" title={tr('Редактировать 3D-сцену')} onClick={onScene}>3D</Button>
   <Button icon="Video" onClick={onCameras}>{tr('Камеры')}</Button>
  </div>
  <Section title={tr('Заметка для автора')} open={false}><InspectorText key={scene.id+':description'} multiline rows={3} aria-label={tr('Заметка для автора')} value={scene.description||''} disabled={running} onCommit={description=>patch({description})}/></Section>
  <Section title={tr('Начало сабсцены')} open={false}>
   <fieldset disabled={running}>
    <Field label={tr('Реплика')}><Select aria-label={tr('С какой реплики начинать')} value={scene.entry} options={lines.map(b=>[b.id,beatPreview(b,8)])} onChange={id=>{if(!running)onEntry(id,reroute);}}/></Field>
    <label className="check" title={tr('Входящие переходы на прежнюю первую реплику будут вести на новое начало.')}><input type="checkbox" checked={reroute} onChange={e=>setReroute(e.target.checked)}/>{tr('Обновить входящие переходы')}<span className="inspector-count">{incoming}</span></label>
   </fieldset>
  </Section>
  {!!cast.length&&<Section title={tr('Персонажи')} open={false} extra={<span className="inspector-count">{cast.filter(o=>o.active!==false&&isObjectInScene(o,scene)).length}</span>}>
   <fieldset disabled={running}>{cast.map(o=><label className="check" key={o.id} title={o.active===false?tr('отключён в проекте'):undefined}><input type="checkbox" disabled={o.active===false} checked={o.active!==false&&isObjectInScene(o,scene)} onChange={e=>patch({excludedObjectIds:e.target.checked?(scene.excludedObjectIds||[]).filter(id=>id!==o.id):[...new Set([...(scene.excludedObjectIds||[]),o.id])]})}/><span>{o.name}</span></label>)}</fieldset>
  </Section>}
  <Section title={tr('Навигация и коллизии')} open={false}><NavigationInspector compact scene={scene} disabled={running} onChange={navMesh=>patch({navMesh})}/></Section>
  <div className="inspector-footer-action"><Button icon="Settings2" onClick={onOpenEditor}>{tr('Редактор сабсцены')}<Icon name="ArrowUpRight" size={13}/></Button></div>
 </div>;
}
