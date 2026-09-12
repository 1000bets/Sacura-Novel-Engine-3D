import React, {memo, useCallback, useEffect, useMemo, useRef, useState} from 'react';
import {ReactFlow, Background, Controls, MiniMap, Handle, Position, MarkerType, BaseEdge, EdgeLabelRenderer, useNodesInitialized, useReactFlow, useStore} from '@xyflow/react';
import dagre from '@dagrejs/dagre';
import {Icon, Button} from './StudioParts.jsx';
import {buildTimelineModel, cleanTimelinePositions, timelineNodeForBeat, timelineVariableNodeId, timelineVariableValue} from './timelineModel.js';
import '@xyflow/react/dist/style.css';
import './globalTimeline.css';

const NODE_WIDTH = 276;
const MIN_ZOOM = .005;
const FIT_ALL = {padding:.13,duration:350,minZoom:MIN_ZOOM,maxZoom:.85};
const nodeHeight = node => node.kind === 'choice' ? 152 + node.choices.length * 58 : node.kind === 'ending' ? 187 : node.kind === 'missing' ? 128 : 219;

function InitialTimelineViewport({nodeId}) {
  const initialized = useNodesInitialized();
  const {viewportInitialized,getNode,setCenter} = useReactFlow();
  // React Flow updates these dimensions from its container ResizeObserver.
  // onInit alone can fire before either the canvas or nodes have dimensions.
  const width = useStore(state => state.width), height = useStore(state => state.height);
  const focused = useRef(false);
  useEffect(() => {
    if (focused.current || !initialized || !viewportInitialized || !width || !height) return;
    const node = getNode(nodeId);
    if (!node?.measured?.width || !node.measured.height) return;
    focused.current = true;
    setCenter(node.position.x+node.measured.width/2,node.position.y+node.measured.height/2,{zoom:.85,duration:0});
  },[initialized,viewportInitialized,width,height,nodeId,getNode,setCenter]);
  return null;
}

function layoutTimeline(model) {
  const graph = new dagre.graphlib.Graph({multigraph:true});
  graph.setGraph({rankdir:'LR',ranksep:115,nodesep:75,edgesep:28,marginx:40,marginy:110});
  graph.setDefaultEdgeLabel(() => ({}));
  model.nodes.forEach(node => graph.setNode(node.id, {width:NODE_WIDTH,height:nodeHeight(node)}));
  // Exclude cycle edges and visits back to earlier subscenes. Every remaining
  // connection reads left to right; returns have separate lanes above the story.
  model.edges.filter(edge => !edge.isReturn).forEach(edge => graph.setEdge(edge.source,edge.target,{},edge.id));
  dagre.layout(graph);
  return new Map(model.nodes.map(node => {
    const position = graph.node(node.id);
    return [node.id,{x:position.x - NODE_WIDTH / 2,y:position.y - nodeHeight(node) / 2}];
  }));
}

const TimelineNode = memo(function TimelineNode({data}) {
  const {node,active,running,selected,dependent,onSelect,onOpenBeat,onOpenScene} = data;
  const open = () => node.entryBeatId ? onOpenBeat(node.entryBeatId) : onOpenScene?.(node.sceneId);
  return <article className={`global-timeline-node ${node.kind}${active?' current':''}${running && active?' playing':''}${selected?' inspected':''}${dependent?' dependent':''}${!node.reachable?' disconnected':''}`} style={{'--scene-color':node.color || '#bd94a9'}}>
    <Handle type="target" position={Position.Left}/>
    {node.kind !== 'choice' && !['ending','missing'].includes(node.kind) && <Handle type="source" position={Position.Right}/>}
    <div className="global-timeline-node-meta" title="Потяните карточку, чтобы изменить её место на таймлайне"><span>{node.kind === 'choice' ? 'ВЫБОР / УСЛОВИЯ' : node.kind === 'ending' ? 'КОНЕЦ ИГРЫ' : node.kind === 'missing' ? 'ОБРЫВ МАРШРУТА' : node.isSceneEntry ? 'НАЧАЛО САБСЦЕНЫ' : 'САБСЦЕНА · ФРАГМЕНТ'}</span><Icon name="GripHorizontal" size={17}/></div>
    {active && <div className="global-timeline-current"><i/>{running?'Сейчас в playtest':'Текущая реплика'}</div>}
    <div className="global-timeline-node-title" title="Потяните для перемещения · двойной щелчок открывает сценарий">{node.title}</div>
    {node.kind !== 'subscene' && <small className="global-timeline-node-scene">{node.sceneName}</small>}
    {node.kind === 'choice' ? <>
      <p className="global-timeline-question">{node.text}</p>
      <div className="global-timeline-answers">{node.choices.map((choice,index) => <div className="global-timeline-answer" key={choice.id || index}>
        <button className="nodrag" title="Открыть исходный выбор и изменить ответ" onClick={open}><b>{index+1}</b><span>{choice.label || 'Ответ без названия'}<small className={choice.condition && choice.condition !== 'always'?'conditional':''}>{choice.conditionLabel}</small></span></button>
        <Handle type="source" position={Position.Right} id={choice.id || String(index)}/>
      </div>)}</div>
      {node.emptyChoice && <p className="global-timeline-warning">У выбора нет ответов</p>}
    </> : <>
      <p className="global-timeline-excerpt">{node.empty?'Сабсцена ещё не содержит реплик':node.text}</p>
      {node.kind === 'subscene' && <div className="global-timeline-node-counts"><span>{node.beatIds.length} реплик{node.gates?` · ${node.gates} взаимодействий`:''}</span>{node.parts>1 && <span>{node.part}/{node.parts}</span>}</div>}
      {node.openEnd && <p className="global-timeline-warning">{node.empty?'Добавьте первую реплику':'Продолжение ещё не задано'}</p>}
      {node.kind !== 'missing' && <div className="global-timeline-node-actions"><Button className="nodrag" icon={node.kind==='ending'?'Flag':'Workflow'} onClick={open}>{node.kind === 'ending'?'Открыть концовку':'Сценарий'}</Button>{node.kind === 'subscene' && onOpenScene && <Button className="nodrag" icon="Box" title="Открыть 3D-сцену" onClick={() => onOpenScene(node.sceneId)}/>}</div>}
    </>}
    {!node.reachable && node.kind !== 'missing' && <span className="global-timeline-disconnected">Нет пути от начала истории</span>}
    <button className="nodrag global-timeline-inspect" aria-label={`Показать связи: ${node.title}`} title="Показать связи узла" onClick={() => onSelect(node.id)}><Icon name="ScanLine" size={13}/></button>
  </article>;
});

function VariableNode({data}) {
  return <div className="global-timeline-variable-node" title="Потяните для перемещения"><Handle type="source" position={Position.Right}/><small>ГЛОБАЛЬНАЯ ПЕРЕМЕННАЯ <Icon name="GripHorizontal" size={14}/></small><strong><Icon name="Variable"/>{data.variable.name}</strong><span>{data.variable.id} = {timelineVariableValue(data.value)}</span><small>Пунктир → условие ответа</small></div>;
}

function ReturnEdge(props) {
  const {sourceX,sourceY,targetX,targetY,data,markerEnd,style,label} = props;
  const lane = data.laneY, bend = 48;
  const path = `M ${sourceX} ${sourceY} C ${sourceX+bend} ${sourceY}, ${sourceX+bend} ${lane}, ${sourceX} ${lane} L ${targetX} ${lane} C ${targetX-bend} ${lane}, ${targetX-bend} ${targetY}, ${targetX} ${targetY}`;
  return <><BaseEdge id={props.id} path={path} markerEnd={markerEnd} style={style} interactionWidth={22}/><EdgeLabelRenderer><span className="global-timeline-return-label nodrag nopan" style={{transform:`translate(-50%, -50%) translate(${(sourceX+targetX)/2}px, ${lane}px)`}}>{label}</span></EdgeLabelRenderer></>;
}
const nodeTypes = {timeline:TimelineNode,variable:VariableNode};
const edgeTypes = {return:ReturnEdge};

export default function GlobalTimeline({project,currentSceneId,currentBeatId,running=false,variables,onOpenScene,onOpenBeat,onOpenVariables,onLayoutChange}) {
  const model = useMemo(() => buildTimelineModel(project),[project]);
  const autoPositions = useMemo(() => layoutTimeline(model),[model]);
  const savedPositions = useMemo(() => cleanTimelinePositions(project,project.editor?.timelinePositions,model),[project,model]);
  const [dragPositions,setDragPositions] = useState({});
  const positions = useMemo(() => new Map([...autoPositions].map(([id,position]) => [id,dragPositions[id] || savedPositions[id] || position])),[autoPositions,savedPositions,dragPositions]);
  const activeId = timelineNodeForBeat(model,currentBeatId);
  const [selected,setSelected] = useState(null), [variableId,setVariableId] = useState(null);
  const [follow,setFollow] = useState(true), [selectedEdge,setSelectedEdge] = useState(null);
  const flow = useRef(null);
  const dragStart = useRef(null), dragged = useRef(false), resetRequested = useRef(false);
  const [measurements,setMeasurements] = useState({});
  const onNodesChange = useCallback(changes => {
    const moved = changes.filter(change => change.type === 'position' && change.position);
    if (moved.length) setDragPositions(previous => ({...previous,...Object.fromEntries(moved.map(change => [change.id,{...change.position}]))}));
    const resized = changes.filter(change => change.type === 'dimensions' && change.dimensions);
    if (!resized.length) return;
    // Controlled nodes must retain ResizeObserver measurements. Discarding
    // dimension changes leaves useNodesInitialized false and fitView incomplete.
    setMeasurements(previous => {
      let next = previous;
      for (const {id,dimensions} of resized) if (previous[id]?.width !== dimensions.width || previous[id]?.height !== dimensions.height) {
        if (next === previous) next = {...previous};
        next[id] = dimensions;
      }
      return next;
    });
  },[]);
  useEffect(() => {
    setDragPositions({});
    if (resetRequested.current) {
      resetRequested.current = false;
      flow.current?.fitView(FIT_ALL);
    }
  },[project]);
  const onNodeDragStart = (_,node,draggedNodes) => {
    const moved = draggedNodes?.length ? draggedNodes : [node];
    dragStart.current = Object.fromEntries(moved.map(item => [item.id,{...(savedPositions[item.id] || autoPositions.get(item.id) || item.position)}]));
    dragged.current = true;
    if (running) setFollow(false);
  };
  const onNodeDragStop = (_,node,draggedNodes) => {
    const moved = draggedNodes?.length ? draggedNodes : [node];
    const changed = moved.filter(item => {
      const start = dragStart.current?.[item.id];
      return start && (start.x !== item.position.x || start.y !== item.position.y);
    });
    dragStart.current = null;
    if (!changed.length) {setDragPositions({});return;}
    const next = cleanTimelinePositions(project,{...savedPositions,...Object.fromEntries(changed.map(item => [item.id,item.position]))},model);
    // Pointer moves only affect local preview. One completed drag = one project
    // mutation, so save/load keeps the layout and Ctrl+Z undoes the whole move.
    onLayoutChange?.(next);
  };
  const restoreAutoLayout = () => {
    if (!onLayoutChange || !Object.keys(savedPositions).length) return;
    resetRequested.current = true;
    onLayoutChange({});
  };
  const activeNode = model.nodes.find(node => node.id === activeId);
  const shownNode = model.nodes.find(node => node.id === selected) || activeNode;
  const shownEdge = model.edges.find(edge => edge.id === selectedEdge);
  const variable = model.variables.find(item => item.id === variableId);
  const currentValues = variables || project.variables || {};
  const focusNode = useCallback(id => {
    const node = model.nodes.find(item => item.id === id), position = positions.get(id);
    if (position && flow.current) flow.current.setCenter(position.x+NODE_WIDTH/2,position.y+nodeHeight(node)/2,{zoom:.85,duration:320});
  },[model,positions]);
  useEffect(() => {
    if (running && follow && activeId) focusNode(activeId);
  },[running,follow,activeId,focusNode]);
  const selectNode = id => {setSelected(id);setSelectedEdge(null);};
  const nodes = model.nodes.map(node => ({id:node.id,type:'timeline',position:positions.get(node.id),measured:measurements[node.id],
    style:{width:NODE_WIDTH},data:{node,active:node.id === activeId,running,selected:node.id === selected,
      dependent:!!variable?.usedBy.includes(node.id),onSelect:selectNode,onOpenScene,onOpenBeat}}));
  const storyTop = Math.min(0,...[...positions.values()].map(position => position.y));
  let returnIndex = 0;
  const edges = model.edges.map(edge => {
    const highlighted = edge.id === selectedEdge || (selected && [edge.source,edge.target].includes(selected));
    const stroke = highlighted?'#efbfd5':edge.isReturn?'#c4a77c':edge.crossScene?'#b99aca':'#847989';
    const label = edge.isReturn ? `Возврат${edge.choiceId?' · '+edge.label:''}` : edge.crossScene ? 'Другая сабсцена' : undefined;
    return {id:edge.id,source:edge.source,target:edge.target,sourceHandle:edge.choiceId,
      type:edge.isReturn?'return':'smoothstep',data:{laneY:edge.isReturn?storyTop-45-(returnIndex++)*34:0},label,
      markerEnd:{type:MarkerType.ArrowClosed,color:stroke,width:16,height:16},
      style:{stroke,strokeWidth:highlighted?2.7:1.7,strokeDasharray:edge.isReturn?'7 5':undefined},
      labelStyle:{fill:'#d6cbd7',fontSize:11},labelBgStyle:{fill:'#292630'},
      pathOptions:{borderRadius:14,offset:28},interactionWidth:22};
  });
  if (variable) {
    const targets = variable.usedBy.map(id => autoPositions.get(id)).filter(Boolean);
    const id = timelineVariableNodeId(variable.id);
    const position = dragPositions[id] || savedPositions[id] || {x:Math.max(20,Math.min(...targets.map(item => item.x),100)-310),y:Math.min(0,...[...autoPositions.values()].map(item => item.y))-200};
    nodes.push({id,type:'variable',position,measured:measurements[id],data:{variable,value:currentValues[variable.id]},style:{width:250}});
    for (const dependency of model.dependencies.filter(item => item.variableId === variableId)) edges.push({
      id:dependency.id,source:id,target:dependency.target,type:'bezier',
      style:{stroke:'#a19bc8',strokeDasharray:'4 5',strokeWidth:1.7},
      markerEnd:{type:MarkerType.ArrowClosed,color:'#a19bc8'},label:'условие',
      labelStyle:{fill:'#c3bede',fontSize:11},labelBgStyle:{fill:'#292630'},
    });
  }
  const firstInScene = sceneId => model.nodes.find(node => node.sceneId === sceneId && node.isSceneEntry) || model.nodes.find(node => node.sceneId === sceneId);
  const localLinks = shownNode ? model.edges.filter(edge => [edge.source,edge.target].includes(shownNode.id)) : [];
  return <section className="global-timeline" aria-label="Общий таймлайн истории" onPointerDownCapture={() => {if (!dragStart.current) dragged.current = false;}}>
    <header className="global-timeline-header"><div><h2><Icon name="Waypoints" size={21}/>Общий таймлайн</h2><p>{model.counts.scenes} сабсцены · {model.counts.choices} выборов · {model.counts.endings} концовки</p></div><div className="global-timeline-tools">
      {running && <label><input type="checkbox" checked={follow} onChange={event => setFollow(event.target.checked)}/>Следить за playtest</label>}
      <Button icon="Focus" disabled={!activeId} onClick={() => focusNode(activeId)}>{running?'Сейчас в игре':'Текущий узел'}</Button>
      <Button icon="Maximize" onClick={() => flow.current?.fitView(FIT_ALL)}>Вся история</Button>
      <Button icon="LayoutGrid" disabled={!onLayoutChange || !Object.keys(savedPositions).length} title="Вернуть автоматическое расположение всех узлов. Ctrl+Z отменяет сброс." onClick={restoreAutoLayout}>Автораскладка</Button>
    </div></header>
    <div className="global-timeline-direction"><span>НАЧАЛО</span><i/><Icon name="ArrowRight" size={19}/><span>ХОД ИСТОРИИ</span><small>Порядок переходов; время зависит от чтения и выборов игрока</small></div>
    <nav className="global-timeline-scenes" aria-label="Сабсцены на таймлайне">{(project.subscenes || []).map(scene => <button key={scene.id} className={scene.id === (activeNode?.sceneId || currentSceneId)?'active':''} title="Показать начало сабсцены на таймлайне. Двойной щелчок — открыть сцену." onClick={() => {const node = firstInScene(scene.id);if (node) {selectNode(node.id);focusNode(node.id);}}} onDoubleClick={() => onOpenScene?.(scene.id)}><i style={{background:scene.color || '#bd94a9'}}/>{scene.name}</button>)}</nav>
    <div className="global-timeline-variables"><strong><Icon name="Variable" size={16}/>Глобальные переменные</strong>{model.variables.map(item => <button key={item.id} className={`${variableId === item.id?'active ':''}${item.missing?'missing':''}`} title={`${item.id} · ${item.usedBy.length} выборов. Нажмите, чтобы показать связи условий.`} onClick={() => setVariableId(current => current === item.id?null:item.id)}><span>{item.name}</span><b>{timelineVariableValue(currentValues[item.id])}</b><small>{item.usedBy.length}</small></button>)}{!model.variables.length && <span>Переменные ещё не созданы</span>}{onOpenVariables && <Button icon="SlidersHorizontal" title="Редактировать глобальные переменные" onClick={onOpenVariables}/>}</div>
    <div className="global-timeline-canvas"><ReactFlow nodes={nodes} edges={edges} nodeTypes={nodeTypes} edgeTypes={edgeTypes}
      nodesDraggable={true} nodeDragThreshold={4} nodesConnectable={false} edgesReconnectable={false} minZoom={MIN_ZOOM} maxZoom={1.65}
      onInit={instance => {flow.current=instance;}}
      onNodesChange={onNodesChange}
      onNodeDragStart={onNodeDragStart} onNodeDragStop={onNodeDragStop}
      onNodeClick={(_,node) => {if (!dragged.current && node.type === 'timeline') selectNode(node.id);}}
      onNodeDoubleClick={(_,node) => {if (!dragged.current && node.data.node?.entryBeatId) onOpenBeat(node.data.node.entryBeatId);}}
      onEdgeClick={(_,edge) => {setSelectedEdge(edge.id);setSelected(null);}}
      onPaneClick={() => {setSelected(null);setSelectedEdge(null);}}
      proOptions={{hideAttribution:true}}>
      <InitialTimelineViewport nodeId={activeId || model.entryNodeId}/>
      <Background color="#3b3441" gap={24}/><Controls showInteractive={false} fitViewOptions={FIT_ALL}/><MiniMap nodeColor={node => node.data.node?.kind === 'ending'?'#cbb080':node.data.node?.kind === 'choice'?'#909dc4':node.data.node?.color || '#ac9cbb'} maskColor="rgba(28,26,33,.77)" pannable zoomable/>
    </ReactFlow>
    {!model.nodes.length && <div className="global-timeline-empty">Добавьте сабсцену и первую реплику — здесь появится история.</div>}
    <div className="global-timeline-legend"><span><i/>Сабсцена</span><span><i className="choice"/>Выбор / условие</span><span><i className="ending"/>Концовка</span><span className="return">↶ Возврат</span><small>Потяните карточку · двойной щелчок — сценарий · Ctrl+Z — отмена</small></div></div>
    <footer className="global-timeline-details">{shownEdge?<>
      <div><strong><Icon name={shownEdge.isReturn?'Undo2':'ArrowRight'}/>Переход{shownEdge.isReturn?' с возвращением':''}</strong><span>{model.nodes.find(node => node.id === shownEdge.source)?.sceneName} → {model.nodes.find(node => node.id === shownEdge.target)?.sceneName}</span></div>
      <p>{shownEdge.label}<small>{shownEdge.conditionLabel} · {shownEdge.from} → {shownEdge.to || 'продолжение не задано'}</small></p>
      <Button icon="Pencil" onClick={() => onOpenBeat(shownEdge.from)}>Исходная реплика</Button>
      {shownEdge.to && model.beatToNode[shownEdge.to] && <Button icon="ArrowUpRight" onClick={() => onOpenBeat(shownEdge.to)}>Назначение</Button>}
    </>:shownNode?<>
      <div><strong><Icon name={running && shownNode.id === activeId?'CirclePlay':'Waypoints'}/>{shownNode.kind === 'ending'?'Концовка':shownNode.kind === 'choice'?'Выбор игрока':shownNode.sceneName}</strong><span>{shownNode.kind === 'subscene'?`${shownNode.beatIds.length} реплик в фрагменте` : shownNode.title}</span></div>
      <p>{shownNode.text}<small>{shownNode.id === activeId?`${running?'Playtest':'Текущая реплика'}: ${currentBeatId} · `:''}{localLinks.filter(edge => edge.target === shownNode.id).length} входящих · {localLinks.filter(edge => edge.source === shownNode.id).length} исходящих</small></p>
      {shownNode.entryBeatId && <Button icon="ArrowUpRight" onClick={() => onOpenBeat(shownNode.id === activeId?currentBeatId:shownNode.entryBeatId)}>Открыть реплику</Button>}
    </>:<p>Линейные реплики объединены в фрагменты сабсцен. Развилки, возвращения и концовки сохранены. Выберите узел или стрелку, чтобы открыть источник.</p>}</footer>
  </section>;
}
