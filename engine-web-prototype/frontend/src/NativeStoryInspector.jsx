import {NativeNarrativeInspector} from './NativeNarrative.jsx';
import {AssetReferenceField} from './NativeAssets.jsx';
import React from 'react';
import Fold from './InspectorSection.jsx';
import {Button,Field,Icon,Select} from './StudioParts.jsx';
import {t as tr,useLocale} from './i18n.jsx';
import './nativeStoryInspector.css';

function SpeakerAssetIdentifier(Value) {
  if (Value?.startsWith('character:')) { return Value.slice(10); }
  return null;
}
const kinds=[['Line','Реплика'],['Choice','Выбор игрока'],['Action','Действие'],['End','Концовка']];
const phases=[['BEFORE','До текста'],['ON_START','Вместе с текстом'],['AFTER','После ответа игрока']];
const actionKinds=[['Wait','Ожидание'],['MoveTo','Перемещение объекта'],['CameraCut','Переход камеры'],['NavigateTo','Движение по навигации']];
const heading={Line:['MessageSquare','Реплика'],Choice:['GitFork','Выбор игрока'],Action:['Zap','Действие'],End:['Flag','Концовка']};

export function NativeInspectorEmpty({children}){
 useLocale();
 return <div className="native-inspector-empty" role="status"><Icon name="MousePointer2" size={24}/><strong>{tr('Ничего не выбрано')}</strong><p>{children||tr('Выберите элемент на холсте или в списке слева.')}</p></div>;
}

function CommitText({label,value='',onCommit,disabled,multiline=false}){
 const Properties={defaultValue:value,disabled,'aria-label':label,onBlur:Event=>{
  if (!disabled && Event.target.value!==value) {
   onCommit(Event.target.value);
  }
 },onKeyDown:Event=>{
  if (Event.key==='Escape') {
   Event.preventDefault();
   Event.stopPropagation();
   Event.currentTarget.value=value;
   Event.currentTarget.blur();
  }
  if (Event.key==='Enter' && (!multiline || Event.ctrlKey || Event.metaKey)) {
   Event.preventDefault();
   Event.currentTarget.blur();
  }
 }};
 if (multiline) {
  return <textarea key={label+value} {...Properties} className="dialogue-text-field" rows={4}/>;
 }
 return <input key={label+value} {...Properties}/>;
}

function ActionFields({action,onChange,objects,disabled}){
 const patch=value=>onChange({...action,...value});
 return <fieldset disabled={disabled} className="native-action-fields">
  <Field label="Действие"><Select aria-label="Действие" value={action.kind} options={actionKinds} onChange={kind=>patch({kind})}/></Field>
  <Field label="Длительность, сек"><input aria-label="Длительность, сек" type="number" min="0" step="0.1" value={action.durationSeconds??0} onChange={e=>patch({durationSeconds:Number(e.target.value)})}/></Field>
  {action.kind!=='Wait'&&<><Field label="Объект"><Select aria-label="Объект действия" value={action.targetObjectId||''} options={[["",'Не назначен'],...objects.map(o=>[o.id,o.name])]} onChange={targetObjectId=>patch({targetObjectId})}/></Field>
   {action.kind==='CameraCut'&&<label className="check"><input type="checkbox" checked={!!action.useTargetObjectTransform} onChange={e=>patch({useTargetObjectTransform:e.target.checked})}/>Копировать трансформацию объекта</label>}
   {action.kind!=='CameraCut'&&<Field label="Позиция"><span className="native-vector">{['X','Y','Z'].map((axis,i)=><input key={axis} aria-label={'Позиция '+axis} type="number" step="0.1" value={action.position?.[i]??0} onChange={e=>patch({position:[0,1,2].map(j=>j===i?Number(e.target.value):action.position?.[j]??0)})}/>)}</span></Field>}</>}
 </fieldset>;
}

export default function NativeStoryInspector({story,selection,objects=[],disabled=false,command,onSelect,onEvents,onFocus}){
 useLocale();
 const document=story?.document,node=document?.nodes?.find(n=>n.id===selection?.nodeId);
 if(!node)return <NativeInspectorEmpty/>;
 const binding=node.bindings?.find(b=>b.id===selection?.bindingId);
 const event=binding&&document.events?.find(e=>e.id===selection?.eventId);
 const edit=patch=>command('story.updateNode',{nodeId:node.id,patch});
 const nextOptions=[["",'Выход не подключён'],...document.nodes.filter(n=>n.id!==node.id).map(n=>[n.id,(n.kind==='Line'?n.text:n.kind==='Choice'?n.prompt:n.kind)+' · '+n.id])];
 const connect=(port,targetId)=>command('story.connect',{sourceId:node.id,port,targetId:targetId||null});
 const updateChoices=choices=>edit({choices});
 const updateEvent=patch=>command('story.updateEvent',{eventId:event.id,event:{...event,...patch}});
 const updateOverride=action=>command('story.setOverrides',{nodeId:node.id,bindingId:binding.id,actions:[...Object.values(binding.actionOverrides||{}).filter(a=>a.id!==action.id),action]});
 const icon=event?'Layers':heading[node.kind]?.[0]||'Circle';
 const title=event?'Событие':heading[node.kind]?.[1]||node.kind;
 const selectedBatch=Object.entries(node.batches||{}).flatMap(([phase,batches])=>batches.map(batch=>({phase,batch}))).find(({batch})=>batch.bindingIds?.includes(binding?.id));
 const editPlacement=patch=>command('story.placeEvent',{nodeId:node.id,bindingId:binding.id,phase:patch.phase||selectedBatch?.phase||'ON_START',join:patch.join||binding.join||'EVENT_END'});
 return <div className="story-inspector beat-inspector native-story-inspector" key={node.id+(event?.id||'')}>
  <div className="story-inspector-heading"><Icon name={icon} size={16}/><strong>{tr(title)}</strong><small title={document.name}>{document.name}</small></div>
  {event?<>
   <fieldset className="inspector-primary-fields" disabled={disabled}>
    <Field label="Название"><CommitText label="Название события" value={event.name} onCommit={name=>updateEvent({name})} disabled={disabled}/></Field>
    <Field label="Когда запустить"><Select aria-label="Когда запустить" value={selectedBatch?.phase||'ON_START'} options={phases} onChange={phase=>editPlacement({phase})}/></Field>
    <Field label="Продолжить"><Select aria-label="Когда пропустить поток дальше" value={binding?.join||'EVENT_END'} options={[["NONE",'Сразу'],['STARTED','После запуска'],['FLOW_END','После потока'],['EVENT_END','После события']]} onChange={join=>editPlacement({join})}/></Field>
   </fieldset>
   {event.groups?.map(group=><Fold key={group.id} title={group.name||'Группа действий'}>{group.actions.map(action=><div key={action.id} className="native-action-row"><ActionFields action={binding.actionOverrides?.[action.id]||action} objects={objects} disabled={disabled} onChange={updateOverride}/></div>)}</Fold>)}
   <div className="native-inspector-actions"><Button icon="RotateCcw" disabled={disabled||!Object.keys(binding.actionOverrides||{}).length} onClick={()=>command('story.setOverrides',{nodeId:node.id,bindingId:binding.id,actions:[]})}>Параметры шаблона</Button><Button icon="ArrowLeft" onClick={()=>onSelect({nodeId:node.id})}>К реплике</Button><Button icon="Trash2" disabled={disabled} onClick={()=>command('story.removeEvent',{nodeId:node.id,bindingId:binding.id}).then(result=>{if(result)onSelect({nodeId:node.id});})}>Удалить размещение</Button></div>
  </>:<>
   <fieldset className="inspector-primary-fields" disabled={disabled}>
    {node.kind === 'Narrative' && <NativeNarrativeInspector Node={node} Story={story} Disabled={disabled} Command={command}/>}
    <NativeStoryNodeFields Node={node} Objects={objects} Disabled={disabled} Edit={edit}/>
   </fieldset>
   {['Line','Choice'].includes(node.kind)&&<><Fold title="Постановка реплики" open={false} extra={<span className="inspector-count">{node.bindings?.length||0}</span>}>
    {phases.map(([phase,label])=><button key={phase} className="phase-inspector-row" disabled={disabled} onClick={()=>onEvents({nodeId:node.id,phase})}><Icon name="CornerDownRight" size={14}/><span>{label}</span><b>{node.batches?.[phase]?.reduce((sum,b)=>sum+b.bindingIds.length,0)||0}</b><Icon name="ChevronRight" size={13}/></button>)}
    <Button icon="Workflow" onClick={onFocus}>Показать реплику в потоке</Button>
   </Fold><Fold title="Контексты ввода" open={false}><p className="native-inspector-note">Для C++ истории ввод настраивается в игровом проекте.</p></Fold></>}
   {node.kind==='Choice'&&<Fold title="Ответы игрока" extra={<span className="inspector-count">{node.choices?.length||0}</span>}>
    {(node.choices||[]).map((choice,i)=><div className="native-choice-row" key={choice.id||i}><CommitText label={'Ответ '+(i+1)} value={choice.label} disabled={disabled} onCommit={label=>updateChoices(node.choices.map((c,j)=>j===i?{...c,label}:c))}/><Select aria-label={'Продолжение ответа '+(i+1)} disabled={disabled} value={choice.nextNodeId||''} options={nextOptions} onChange={nextNodeId=>updateChoices(node.choices.map((c,j)=>j===i?{...c,nextNodeId}:c))}/><Button icon="Trash2" title="Удалить ответ" disabled={disabled} onClick={()=>updateChoices(node.choices.filter((_,j)=>j!==i))}/></div>)}
    <Button icon="Plus" disabled={disabled} onClick={()=>updateChoices([...(node.choices||[]),{label:'Новый ответ',nextNodeId:''}])}>Добавить ответ</Button>
   </Fold>}
   {node.kind==='Action'&&<Fold title="Действия"><NativeStoryActionFields Node={node} Objects={objects} Disabled={disabled} Edit={edit}/></Fold>}
   {['Line','Action'].includes(node.kind)&&<Fold title="Продолжение"><Select aria-label="Продолжение реплики" disabled={disabled} value={node.nextNodeId||''} options={nextOptions} onChange={next=>connect(-1,next)}/></Fold>}
   {['Line','Choice'].includes(node.kind)&&<Fold title="Виджет диалогового окна" open={false}><p className="native-inspector-note">C++ история использует общий виджет диалога проекта.</p></Fold>}
   <div className="native-inspector-actions"><Button icon="Focus" onClick={onFocus}>Показать ноду</Button><Button icon="Flag" disabled={disabled||document.startNodeId===node.id} onClick={()=>command('story.setStart',{nodeId:node.id})}>Сделать начальной</Button><Button icon="Trash2" disabled={disabled} onClick={()=>command('story.deleteNode',{nodeId:node.id}).then(result=>{if(result)onSelect(null);})}>Удалить ноду</Button></div>
  </>}
 </div>;
}

export function NativeStoryNodeFields({Node:node,Objects:objects=[],Disabled:disabled,Edit:edit}) {
 useLocale();
 return <>    <Field label="Тип блока"><Select disabled={node.kind === 'Narrative'} aria-label="Тип блока" value={node.kind} options={node.kind === 'Narrative' ? [['Narrative','Python Narrative']] : kinds} onChange={kind=>edit({kind})}/></Field>
    {node.kind==='Line'&&<><Field label="Говорит"><AssetReferenceField label="Персонаж-говорящий" assetId={SpeakerAssetIdentifier(node.speaker)} allowedTypes={['Character']} disabled={disabled} onChange={Identity=>{let Speaker='Narrator';if(Identity.assetId){Speaker='character:'+Identity.assetId;}edit({speaker:Speaker});}}/><Select aria-label="Говорит" value={node.speaker||'Narrator'} options={[['Narrator','Рассказчик'],...objects.map(o=>[o.name,o.name]),...(node.speaker&&!objects.some(o=>o.name===node.speaker)&&node.speaker!=='Narrator'?[[node.speaker,node.speaker]]:[])]} onChange={speaker=>edit({speaker})}/></Field><CommitText label="Текст выбранной реплики" value={node.text} onCommit={text=>edit({text})} disabled={disabled} multiline/></>}
    {node.kind==='Choice'&&<CommitText label="Текст выбора" value={node.prompt} onCommit={prompt=>edit({prompt})} disabled={disabled} multiline/>}</>;
}

export function NativeStoryActionFields({Node:node,Objects:objects=[],Disabled:disabled,Edit:edit}) {
 const updateActions=actions=>edit({actions});
 return <>{(node.actions||[]).map((action,i)=><div key={action.id||i} className="native-action-row"><ActionFields action={action} objects={objects} disabled={disabled} onChange={changed=>updateActions(node.actions.map((a,j)=>j===i?changed:a))}/><Button icon="Trash2" disabled={disabled} onClick={()=>updateActions(node.actions.filter((_,j)=>j!==i))}>Удалить действие</Button></div>)}<Button icon="Plus" disabled={disabled} onClick={()=>updateActions([...(node.actions||[]),{kind:'Wait',durationSeconds:1}])}>Добавить действие</Button></>;
}
