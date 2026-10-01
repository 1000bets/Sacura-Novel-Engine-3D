import {selectedStoryNodeIds} from './storyEditing.js';
import {blueprintEdgeTypes} from './BlueprintEdge.jsx';
import StoryDestination from './StoryDestination.jsx';
import NodeAnswer from './NodeAnswer.jsx';
import BlueprintVariables from './BlueprintVariables.jsx';
import LogicNode from './LogicNode.jsx';
import {isPureNode,logicType,dataTargetType,dataTargets} from './logicModel.js';
import {clipboardCommand} from './editorClipboard.js';
import React,{memo,useCallback,useEffect,useMemo,useRef,useState} from 'react';
import {ReactFlow,ReactFlowProvider,Handle,Position,Background,Controls,MiniMap,ConnectionLineType,useReactFlow,useUpdateNodeInternals,useNodesInitialized,useStore,applyNodeChanges} from '@xyflow/react';
import {Icon,Button} from './StudioParts.jsx';
import {buildStoryFlow,arrangeStoryFlow} from './storyFlowModel.js';
import './storyFlow.css';
import {choiceResultType} from './choiceModel.js';
import {connectionError,pinConnections,movePinConnections} from './storyConnections.js';

const Moment=memo(function Moment({data,selected}){
 const {beat,scene,phases,ports}=data;
 const updateNodeInternals=useUpdateNodeInternals();
 // Recalculate pin positions after text, event groups or output rows change.
 useEffect(()=>{updateNodeInternals(beat.id);},[beat,phases,ports,updateNodeInternals]);
 const events=phase=><div className="moment-phase" data-help-title={phase.label} data-help="События выполняются в порядке групп. Внутри группы можно запускать их вместе или по очереди. Выберите событие, чтобы изменить его свойства справа.">
  <div className="moment-phase-label"><span>{phase.id==='BEFORE'?'До текста':phase.id==='ON_START'?'Вместе с текстом':'После ответа игрока'}</span><button className="nodrag" title={`Добавить событие: ${phase.label} · ${beat.id}`} onClick={()=>data.onAdd(beat.id,phase.id)}><Icon name="Plus" size={13}/></button></div>
  {phase.batches.map(batch=><div className="moment-batch" key={batch.id}>
   <div className="moment-batch-order nodrag"><Icon name={batch.mode==='PARALLEL'?'GitFork':'ArrowDown'} size={12}/><select aria-label={`Порядок событий ${beat.id} ${phase.label}`} value={batch.mode} onClick={e=>e.stopPropagation()} onChange={e=>data.onBatch(batch.id,'mode',e.target.value,beat.id)}><option value="SEQUENTIAL">По очереди</option><option value="PARALLEL">Одновременно</option></select><button title={`Настроить группу ${beat.id} ${phase.label}`} onClick={e=>{e.stopPropagation();data.onBatch(batch.id,'select',null,beat.id);}}><Icon name="Settings2" size={12}/></button><button title={`Добавить в группу ${beat.id} ${phase.label}`} onClick={e=>{e.stopPropagation();data.onBatch(batch.id,'add',null,beat.id);}}><Icon name="Plus" size={12}/></button></div>
   {batch.events.map(({binding,event,actions})=><button key={binding.id} className={'moment-event nodrag'+(data.selectionId===binding.id?' selected':'')} title="Выбрать событие и изменить его свойства в инспекторе" onClick={e=>{e.stopPropagation();data.onSelect({binding,event:event||{id:binding.eventId},flowBeatId:beat.id});}} onDoubleClick={e=>{e.stopPropagation();event&&data.onOpen({kind:'event',binding,event,flowBeatId:beat.id});}}>
    <span><Icon name="Layers" size={12}/><strong>{event?.name || 'Событие недоступно'}</strong><span role="button" tabIndex={0} aria-label="Удалить событие из реплики" title="Удалить событие из реплики" onClick={e=>{e.stopPropagation();data.onBatch(batch.id,'removeBinding',binding.id,beat.id);}} onKeyDown={e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();e.stopPropagation();data.onBatch(batch.id,'removeBinding',binding.id,beat.id);}}}><Icon name="Trash2" size={13}/></span>{data.preview?.states?.[binding.id]==='running'&&<Icon name="Play" size={12}/>}</span>
    {actions.slice(0,2).map(a=><small key={a.id}>{a.summary}</small>)}{actions.length>2&&<small>Ещё {actions.length-2} действий</small>}
    <em>{binding.join==='EVENT_END'?'Ждать завершения':binding.join==='FLOW_END'?'Ждать шагов · фон продолжится':'Продолжить сразу'}</em>
   </button>)}
  </div>)}
 </div>;
 if(isPureNode(beat)||['branch','set-variable'].includes(beat.kind))return <LogicNode data={data} selected={selected}/>;
 return <article className={'story-moment'+(selected?' selected':'')+(data.preview?.phase!=='EDIT'&&data.preview?.beatId===beat.id?' playing':'')}>
  <Handle type="target" position={Position.Left} id="in" style={{top:40}} title="Вход: соедините с выходом другой реплики. Alt + щелчок — разорвать; Ctrl + перетаскивание — перенести входящие связи."/>
  <header className="moment-grab"><Icon name={data.entry?'MapPin':beat.kind==='choice'?'GitFork':beat.kind==='end'?'Flag':'MessageSquare'} size={15}/><span>{scene?.name || 'Без сабсцены'}</span><small>{({choice:'Выбор',end:'Концовка',gate:'Взаимодействие',merge:'Схождение'})[beat.kind]||'Реплика'}</small></header>
  {data.issue&&<div className="moment-issue"><Icon name="TriangleAlert" size={12}/>{data.issue.title}</div>}
  {data.entry&&<button className="moment-environment nodrag" title="Изменить начальное окружение сабсцены" onClick={()=>data.onScene(scene.id)}><Icon name="CloudSun" size={13}/>Начало · {scene.weather} · {scene.time}<Icon name="Settings2" size={12}/></button>}
  {events(phases[0])}
  <div className="moment-dialogue nodrag" onClick={()=>data.onSelect(beat.id)}><strong>{beat.speaker || (beat.kind==='choice'?'Выбор игрока':'Рассказчик')}</strong><p>{beat.text || 'Пустая реплика'}</p></div>
  {events(phases[1])}
  <div className="moment-input"><Icon name={beat.kind==='choice'?'GitFork':beat.kind==='gate'?'MousePointerClick':'MousePointer2'} size={12}/>{beat.kind==='end'?'Завершение истории':beat.kind==='choice'?'Игрок выбирает ответ':beat.kind==='gate'?'Ожидание взаимодействия':'Игрок нажимает «Дальше»'}</div>
  {events(phases[2])}
  {beat.kind==='choice'&&<div className="node-answers nodrag">{beat.choices.map((c,i)=><NodeAnswer key={c.id} choice={c} index={i} beatId={beat.id} data={data} available={data.project.chapters.some(ch=>ch.beats.some(b=>b.id===c.next))} onChange={patch=>data.onChoice(beat.id,c.id,patch)} onRemove={()=>data.onChoice(beat.id,c.id,null)} onDisconnect={()=>data.onConnect({source:beat.id,sourceHandle:'choice:'+c.id,target:null})}/>)}<Button icon="Plus" onClick={()=>data.onChoice(beat.id,null,{label:'Новый ответ'})}>Добавить ответ</Button></div>}
  {beat.kind==='choice'&&choiceResultType(beat)&&<div className={'choice-result-output type-'+choiceResultType(beat)}><span>Результат выбора · {{string:'Текст',number:'Число',boolean:'Да / нет',any:'Разные типы'}[choiceResultType(beat)]}</span><Handle className="data-pin" type="source" position={Position.Right} id="value" title="Результат этого выбора · выход значения"/></div>}
  <footer>{ports.filter(port=>beat.kind!=="choice"||port.common).map(port=><div className={'moment-route story-route'+(!port.next?' disconnected':'')} key={port.id}><span className="story-route-label">{port.label}</span><Handle type="source" position={Position.Right} id={port.id} title={port.label+' · соединить с входом ноды'}/></div>)}{beat.kind==='end'&&<strong><Icon name="Flag" size={13}/>{beat.ending || 'Конец'}</strong>}</footer>
 </article>;
});
const Missing=({data})=><article className="story-moment missing"><Handle type="target" position={Position.Left} id="in"/><strong>{data.title}</strong><p>Целевая нода удалена</p><button className="nodrag" onClick={e=>{e.stopPropagation();data.onRemove();}}><Icon name="Unplug" size={14}/>Убрать переход</button></article>;
const nodeTypes={moment:Moment,missing:Missing};
const NO_ISSUES=[];
function Canvas({running=false,onVariableEdit,onVariableAdd,onInspectVariable,selectedVariable,onVariableDrop,onLogicChange,onVariable,onChoice,onClipboard,contextNodeId,project,selectedId,selectionId,preview,issues=NO_ISSUES,onSelect,onOpen,onAdd,onBatch,onConnect,onDeleteNode,onScene,onContext,positions,onPositions,onReady}){
 const api=useReactFlow(),initialized=useNodesInitialized(),container=useRef(null),didFocus=useRef(false),lastSelected=useRef(selectedId),panGesture=useRef(null),placedNode=useRef(null),selectionKey=useRef(null);
 // React Flow registers a newly created node after the selection changes.
 // Observe that registration so a failed early focus is retried after measurement.
 const targetNode=useStore(state=>state.nodeLookup.get(selectedId));
 const model=useMemo(()=>buildStoryFlow(project),[project]);
 const layout=useMemo(()=>arrangeStoryFlow(model.nodes,model.edges),[model]);
 const [nodes,setNodes]=useState([]),[minimap,setMinimap]=useState(false),[edgeId,setEdgeId]=useState(null),[missingId,setMissingId]=useState(null),[trace,setTrace]=useState(null);
 useEffect(()=>{const selectionChanged=selectionKey.current!==selectedId;selectionKey.current=selectedId;setNodes(previous=>layout.map(n=>({...n,height:undefined,position:previous.find(p=>p.id===n.id)?.dragging?previous.find(p=>p.id===n.id).position:positions?.[n.id]||n.position,dragging:previous.find(p=>p.id===n.id)?.dragging,measured:previous.find(p=>p.id===n.id)?.measured,style:{width:n.width},dragHandle:'.moment-grab',selected:selectionChanged?n.id===selectedId:(previous.find(p=>p.id===n.id)?.selected??n.id===selectedId),data:{...n.data,onTrace:setTrace,onReveal:id=>{onSelect(id);api.fitView({nodes:[{id}],padding:.3,maxZoom:.8,duration:200});},onRemove:()=>onConnect({changes:model.edges.filter(e=>e.target===n.id).map(e=>({source:e.source,sourceHandle:e.sourceHandle,target:null}))}),issue:issues.find(i=>i.beatId===n.id),selectionId,preview,onSelect,onOpen,onAdd,onBatch,onScene,onLogicChange,onVariable,onChoice,onInspectVariable,onConnect,project,variables:project.variables}})));},[layout,positions,selectedId,selectionId,preview,issues,onSelect,onOpen,onAdd,onBatch,onScene,onLogicChange,onVariable,onChoice,onInspectVariable,onConnect,project]);
 const focus=useCallback((id=selectedId)=>{
  const node=api.getNode(id);if(!node?.measured?.width || !node?.measured?.height)return false;
  const zoom=.8;
  api.setViewport({x:40-node.position.x*zoom,y:30-node.position.y*zoom,zoom},{duration:200});
  return true;
 },[api,selectedId]);
 useEffect(()=>{
  if(!initialized || !targetNode)return;
  if(contextNodeId===selectedId&&placedNode.current!==contextNodeId){placedNode.current=contextNodeId;didFocus.current=true;lastSelected.current=selectedId;return;}
  if((!didFocus.current || lastSelected.current!==selectedId) && focus()){
   didFocus.current=true;
   lastSelected.current=selectedId;
  }
 },[initialized,selectedId,targetNode,focus,contextNodeId]);
 const arrange=useCallback(()=>{
  const arranged=arrangeStoryFlow(api.getNodes(),model.edges);
  onPositions(Object.fromEntries(arranged.map(n=>[n.id,n.position])));
 },[api,model.edges,onPositions]);
 // Measure once after mounting so long dialogue cannot overlap neighbouring routes.
 const measuredSignature=useRef('');
 useEffect(()=>{if(!initialized)return;const signature=model.nodes.map(n=>n.id).join('|');if(measuredSignature.current===signature)return;measuredSignature.current=signature;
  if(!Object.keys(positions||{}).length){const arranged=arrangeStoryFlow(api.getNodes(),model.edges);onPositions(Object.fromEntries(arranged.map(n=>[n.id,n.position])));}
 },[initialized,model,positions,api,onPositions]);
 useEffect(()=>{onReady?.({clipboard:()=>({nodes:api.getNodes(),graphKey:'flow:all'}),focus,arrange,addVariableNode:(variable,kind)=>onVariableAdd(variable,kind,{position:api.screenToFlowPosition({x:container.current.getBoundingClientRect().left+80,y:container.current.getBoundingClientRect().top+80}),graphKey:'flow:all'}),fit:()=>api.fitView({padding:.12,minZoom:.02,maxZoom:.8,duration:250})});},[focus,arrange,api,onReady,onVariableAdd]);
 const displayedNodes=useMemo(()=>nodes.map(n=>({...n,className:trace&&model.edges.some(e=>e.id===trace&&(e.source===n.id||e.target===n.id))?'story-traced-node':''})),[nodes,trace,model.edges]);
 const connecting=useRef(null);
 const connect=c=>{connecting.current=null;onConnect(c);};
 const [transfer,setTransfer]=useState(null),[hint,setHint]=useState('');
 const readPin=element=>{
  const handle=element?.closest?.('.react-flow__handle');
  return handle?{node:handle.dataset.nodeid,handle:handle.dataset.handleid,type:handle.classList.contains('source')?'source':'target'}:null;
 };
 const removeEdges=edges=>{if(edges.length)onConnect({changes:edges.map(e=>({source:e.source,sourceHandle:e.sourceHandle,target:e.data?.valueWire?e.target:null,targetHandle:e.targetHandle,disconnect:e.data?.valueWire}))});setEdgeId(null);};
 useEffect(()=>{
  if(!transfer)return;
  const move=e=>setTransfer(t=>t?{...t,x:e.clientX,y:e.clientY}:null);
  const finish=e=>{
   const to=readPin(document.elementFromPoint(e.clientX,e.clientY));
   const changes=to?movePinConnections(model.edges,transfer.pin,to):[];
   if(changes.length){const error=connectionError(project,changes);if(error)setHint(error);else {onConnect({changes});setHint('Связи перенесены. Ctrl + Z — отменить.');}}
   else setHint('Для переноса отпустите на другом пине того же типа: вход → вход или выход → выход.');
   setTransfer(null);
  };
  const cancel=e=>{if(e.key==='Escape'){e.stopPropagation();setTransfer(null);setHint('Перенос отменён.');}};
  const abort=()=>setTransfer(null);
  window.addEventListener('pointermove',move);window.addEventListener('pointerup',finish);window.addEventListener('pointercancel',abort);window.addEventListener('blur',abort);window.addEventListener('keydown',cancel,true);
  return ()=>{window.removeEventListener('pointermove',move);window.removeEventListener('pointerup',finish);window.removeEventListener('pointercancel',abort);window.removeEventListener('blur',abort);window.removeEventListener('keydown',cancel,true);};
 },[transfer?.pin,model.edges,project,onConnect]);
 return <div className="story-blueprint"><BlueprintVariables project={project} preview={preview} disabled={running} onEdit={onVariableEdit} onAddNode={(variable,kind)=>onVariableAdd(variable,kind,{position:api.screenToFlowPosition({x:container.current.getBoundingClientRect().left+80,y:container.current.getBoundingClientRect().top+80}),graphKey:'flow:all'})} onInspect={onInspectVariable} selectedVariable={selectedVariable}/><div className="story-flow" ref={container} tabIndex={0}
  onDragOver={e=>{if(e.dataTransfer.types.includes('application/sacura-variable')){e.preventDefault();e.dataTransfer.dropEffect='copy';}}}
  onDrop={e=>{const variable=e.dataTransfer.getData('application/sacura-variable');if(!variable)return;e.preventDefault();e.stopPropagation();onVariableDrop?.({variable,x:e.clientX,y:e.clientY,position:api.screenToFlowPosition({x:e.clientX,y:e.clientY}),graphKey:'flow:all',get:e.ctrlKey||e.metaKey,set:e.altKey});}}
  onClickCapture={e=>{if(readPin(e.target)){e.stopPropagation();}}}
  onPointerMoveCapture={e=>{const pan=panGesture.current;if(pan&&e.buttons&&Math.hypot(e.clientX-pan.x,e.clientY-pan.y)>5)pan.moved=true;}}
  onPointerDownCapture={e=>{
   panGesture.current={x:e.clientX,y:e.clientY,moved:false};
   if(e.button===0&&!e.target.closest('input,textarea,select,button,[contenteditable="true"]'))container.current.focus({preventScroll:true});
   const pin=readPin(e.target);if(!pin || e.button!==0 || (!e.altKey&&!e.ctrlKey))return;
   e.preventDefault();e.stopPropagation();container.current.focus();
   const wires=pinConnections(model.edges,pin);
   if(e.altKey){removeEdges(wires);setHint(wires.length?'Связи разорваны. Ctrl + Z — отменить.':'У этого пина нет связей.');}
   else if(wires.length)setTransfer({pin,startX:e.clientX,startY:e.clientY,x:e.clientX,y:e.clientY});
   else setHint('У этого пина нет связей для переноса. Тяните без Ctrl, чтобы создать связь.');
  }}
  onCopy={e=>{if(e.target.isContentEditable||e.target.closest('input,textarea,select,[role="textbox"]'))return;e.preventDefault();e.stopPropagation();onClipboard?.('copy',api.getNodes(),'flow:all');e.clipboardData?.setData('text/plain','Sacura · ноды');}}
  onPaste={e=>{if(e.target.isContentEditable||e.target.closest('input,textarea,select,[role="textbox"]'))return;e.preventDefault();e.stopPropagation();onClipboard?.('paste',api.getNodes(),'flow:all');}}
  onKeyDown={e=>{
   const command=clipboardCommand(e);if(command){e.preventDefault();e.stopPropagation();onClipboard?.(command,api.getNodes(),'flow:all');return;}
   if(running||!['Delete','Backspace'].includes(e.key)||e.repeat||e.target.closest('input,textarea,select,[contenteditable="true"]'))return;
   if(edgeId){e.preventDefault();e.stopPropagation();removeEdges(model.edges.filter(edge=>edge.id===edgeId));}
   else if(missingId){e.preventDefault();e.stopPropagation();removeEdges(model.edges.filter(edge=>edge.target===missingId));setMissingId(null);}
   else if(e.key==='Delete'){e.preventDefault();e.stopPropagation();const ids=selectedStoryNodeIds(api.getNodes());if(ids.length)onDeleteNode?.(ids);}
  }}
  data-help-title="Связи потока истории" data-help="Тяните от выхода справа ко входу слева. Один выход задаёт одно продолжение; во вход могут приходить несколько веток. Alt + щелчок по пину — разорвать его связи. Ctrl + перетаскивание — перенести связи на другой пин того же типа. Выберите ноду или провод и нажмите Delete для удаления. Ctrl + Z — отмена. ПКМ — панорама, колесо — масштаб.">
  <ReactFlow nodes={displayedNodes} edges={model.edges.map(e=>({...e,type:'blueprint',data:{...e.data,highlighted:e.id===edgeId||e.id===trace}}))} edgeTypes={blueprintEdgeTypes} nodeTypes={nodeTypes} connectionLineType={ConnectionLineType.Bezier} connectionLineStyle={{stroke:'var(--accent)',strokeWidth:2,strokeDasharray:'5 5'}}
   onConnectStart={(_,pin)=>{if(!running)connecting.current=pin;}}
   onConnectEnd={(event,state)=>{
     const pin=connecting.current;connecting.current=null;if(running||!pin||state.isValid||state.toNode)return;
     const point=event.changedTouches?.[0]||event;if(!Number.isFinite(point.clientX)||!Number.isFinite(point.clientY))return;
     const target=document.elementFromPoint(point.clientX,point.clientY);if(!target?.closest('.react-flow__pane'))return;
     const node=project.chapters.flatMap(c=>c.beats).find(n=>n.id===pin.nodeId);if(!node)return;
     const dataPin=pin.handleType==='source'?pin.handleId==='value':dataTargets(node).includes(pin.handleId);
     const valueType=dataPin?(pin.handleType==='source'?logicType(project,node):dataTargetType(project,node,pin.handleId)):'flow';
     onContext?.({kind:'wire-drop',pin,valueType,x:point.clientX,y:point.clientY,position:api.screenToFlowPosition({x:point.clientX,y:point.clientY}),graphKey:'flow:all'});
   }}
   onNodesChange={changes=>{if(changes.some(c=>c.type==='select'&&c.selected)){setEdgeId(null);setMissingId(null);}setNodes(ns=>applyNodeChanges(changes,ns));}} onNodeDragStop={(_,n)=>onPositions({...positions,[n.id]:n.position})}
   onNodeClick={(e,n)=>{setEdgeId(null);setMissingId(n.type==='missing'?n.id:null);if(n.data.beat)onSelect(n.id);if(!e.target.closest('input,textarea,select,button,[contenteditable="true"]'))container.current.focus({preventScroll:true});}} onConnect={connect} onReconnect={(edge,c)=>onConnect({changes:[{source:edge.source,sourceHandle:edge.sourceHandle,target:edge.data?.valueWire?edge.target:null,targetHandle:edge.targetHandle,disconnect:edge.data?.valueWire},c]})} isValidConnection={c=>!connectionError(project,[c])} onEdgeMouseEnter={(_,e)=>setTrace(e.id)} onEdgeMouseLeave={()=>setTrace(null)} onEdgeClick={(_,e)=>{setMissingId(null);setEdgeId(e.id);container.current.focus();}} onPaneClick={()=>{setMissingId(null);setEdgeId(null);container.current.focus({preventScroll:true});}}
   onPaneContextMenu={e=>{e.preventDefault();if(!panGesture.current?.moved)onContext?.({x:e.clientX,y:e.clientY,position:api.screenToFlowPosition({x:e.clientX,y:e.clientY}),graphKey:'flow:all'});}}
   deleteKeyCode={null} minZoom={.02} maxZoom={1.5} panOnScroll={false} panOnDrag={[1,2]} selectionOnDrag zoomOnScroll reconnectRadius={18} connectionRadius={28} connectOnClick={false} zoomActivationKeyCode="Control" colorMode="dark">
   <Background gap={28} size={.7}/><Controls showInteractive={false}/>{minimap&&<MiniMap pannable zoomable/>}
  </ReactFlow>
  <div className="blueprint-wire-legend" aria-hidden="true"><span><svg viewBox="0 0 30 12"><path d="M 0 6 H 27 M 22 2 L 28 6 L 22 10" fill="none" stroke="currentColor" strokeWidth="2"/></svg>Переход</span><span><svg viewBox="0 0 30 12"><path d="M 2 6 H 28" stroke="var(--info)" strokeWidth="2" strokeDasharray="3 4"/><circle cx="2" cy="6" r="2" fill="var(--info)"/><circle cx="28" cy="6" r="2" fill="var(--info)"/></svg>Значение</span></div>
  {transfer&&<svg className="story-wire-transfer"><path d={`M ${transfer.startX-container.current.getBoundingClientRect().left} ${transfer.startY-container.current.getBoundingClientRect().top} L ${transfer.x-container.current.getBoundingClientRect().left} ${transfer.y-container.current.getBoundingClientRect().top}`}/></svg>}
  {(transfer||hint)&&<div className="story-wire-help" role="status">{transfer?'Отпустите на другом пине того же типа · Esc — отмена':hint||'Пин → пин: соединить · Alt + щелчок: разорвать · Ctrl + перетаскивание: перенести'}</div>}
  <div className="story-flow-navigation"><Button icon="Focus" title="Показать выбранную реплику" onClick={()=>focus()}/><Button icon="Scan" title="Весь поток истории" onClick={()=>api.fitView({padding:.12,minZoom:.02,maxZoom:.8})}/><Button icon="Map" title="Миникарта истории" aria-pressed={minimap} onClick={()=>setMinimap(v=>!v)}/>{edgeId&&<Button icon="Unplug" onClick={()=>{const e=model.edges.find(e=>e.id===edgeId);if(e)onConnect({source:e.source,sourceHandle:e.sourceHandle,target:e.data?.valueWire?e.target:null,targetHandle:e.targetHandle,disconnect:e.data?.valueWire});setEdgeId(null);}}>Убрать переход</Button>}</div>
 </div></div>;
}
export default function StoryFlow(props){return <ReactFlowProvider><Canvas {...props}/></ReactFlowProvider>;}
