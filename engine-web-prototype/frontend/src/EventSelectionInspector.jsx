import React from 'react';
import {t as tr,useLocale} from './i18n.jsx';
import {Icon,Button,Field,Select} from './StudioParts.jsx';
import Section from './InspectorSection.jsx';
import EventInspector,{EventLifecycleFields,ActionExecutionFields} from './EventInspector.jsx';
import ActionFields from './ActionFields.jsx';
import {actionSummary} from './authoringPresentation.js';
import {beatPreview} from './storyLabels.js';
import {TYPES,PHASES,JOIN_LABELS,makeAction} from './studioModel.js';
import './eventInspector.css';

export default function EventSelectionInspector({event,action,binding,project,beat,sceneId,running,onEdit,onAction,onBinding,onReset,onOpenSource,onOpenEditor,onAdd,onRemove,onPickPosition,onSelectAction,onBack}){
 useLocale();
 const edit=fn=>{if(!running)onEdit(fn);},patch=value=>edit(e=>Object.assign(e,value)),changeAction=(id,v)=>{if(!running)onAction(id,v);};
 const hasSource=binding&&(binding.sourceEventId||!binding.localEventId);
 const placement=<><Field label={tr('Момент запуска')}><Select aria-label={tr('Момент запуска')} value={binding?.hook} options={PHASES.map(p=>[p.id,p.label])} onChange={hook=>onBinding({hook})}/></Field><Field label={tr('Продолжение')}><Select aria-label={tr('Ожидание события')} value={binding?.join} options={Object.entries(JOIN_LABELS)} onChange={join=>onBinding({join})}/></Field><Button icon="RotateCcw" disabled={!Object.keys(binding?.overrides||{}).length&&!Object.keys(binding?.actionOverrides||{}).length} onClick={onReset}>{tr('Сбросить изменения здесь')}</Button></>;
 return <div className="story-inspector event-inspector" key={action?.id||binding?.id||event.id}>
  <div className="story-inspector-heading"><Icon name={action?TYPES[action.type]?.icon||'Zap':'Layers'} size={16}/><strong>{tr(action?'Действие':'Событие')}</strong>{binding&&<small title={beatPreview(beat,20)}>{tr('В этой реплике')}</small>}</div>
  {action&&<button className="event-back" title={event.name} onClick={onBack}><Icon name="ChevronLeft" size={13}/><span>{event.name}</span></button>}
  <div className="event-context-row"><span title={binding?beatPreview(beat,20):tr('Изменения общего события действуют во всех его размещениях.')}>{action?actionSummary(action,project):binding?beatPreview(beat,8):tr('Общее событие')}</span>{hasSource&&<Button icon="ExternalLink" title={tr('Открыть исходный шаблон')} onClick={onOpenSource}/>}</div>
  {action?<>
   <Section title={tr('Параметры действия')}><fieldset disabled={running}><Field label={tr('Тип')}><Select aria-label={tr('Тип действия')} value={action.type} options={Object.entries(TYPES).map(([id,t])=>[id,t.label])} onChange={type=>changeAction(action.id,{...makeAction(type),id:action.id})}/></Field><ActionFields inspector action={action} project={project} sceneId={sceneId} onPickPosition={onPickPosition} onChange={v=>changeAction(action.id,v)}/></fieldset></Section>
   <Section title={tr('Выполнение действия')} open={false}><fieldset disabled={running}><ActionExecutionFields action={action} onChange={v=>changeAction(action.id,v)}/></fieldset></Section>
  </>:<>
   <fieldset className="inspector-primary-fields" disabled={running}><Field label={tr('Название')}><input aria-label={tr('Название события')} value={event.name} onChange={e=>patch({name:e.target.value})}/></Field>{binding?placement:<Button icon="Plus" onClick={onAdd}>{tr('Добавить в реплику')}</Button>}</fieldset>
   <Section title={tr('Действия')} extra={<span className="inspector-count">{event.groups.reduce((n,g)=>n+g.actions.length,0)}</span>}><EventInspector event={event} project={project} sceneId={sceneId} binding={binding} showProperties={false} disabled={running} onSelectAction={onSelectAction} onEdit={edit} onAction={changeAction} onPickPosition={onPickPosition}/></Section>
  </>}
  {binding&&action&&<Section title={tr('Размещение события')} open={false}><fieldset disabled={running}>{placement}</fieldset></Section>}
  {!action&&<>
   <Section title={tr('Время жизни события')} open={false}><fieldset disabled={running}><EventLifecycleFields event={event} onChange={patch}/></fieldset></Section>
   <Section title={tr('Описание')} open={false}><textarea aria-label={tr('Описание события')} disabled={running} rows={3} value={event.description||''} onChange={e=>patch({description:e.target.value})}/></Section>
  </>}
  <div className="inspector-footer-action"><Button icon="Workflow" onClick={onOpenEditor}>{tr('Открыть событие в редакторе')}</Button>{binding&&<Button className="event-remove" icon="Trash2" disabled={running} onClick={onRemove}>{tr('Убрать из реплики')}</Button>}</div>
 </div>;
}
