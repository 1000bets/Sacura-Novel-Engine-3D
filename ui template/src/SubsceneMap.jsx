import React,{useMemo,useState,useRef,useEffect} from 'react';
import {ReactFlow,Background,Controls,Handle,Position,MarkerType} from '@xyflow/react';
import {allBeats,edgesFor,sceneFor} from './studioModel.js';
import {Icon,Button} from './StudioParts.jsx';

export function subsceneLinks(project){
 const beats=allBeats(project),result=[];
 for(const beat of beats)for(const edge of edgesFor(project,beat)){
  if(!beats.some(b=>b.id===edge.to))continue;
  const from=sceneFor(project,beat.id),to=sceneFor(project,edge.to);
  if(from.id!==to.id)result.push({...edge,source:from.id,target:to.id,beat,answer:beat.choices?.find(c=>c.id===edge.choiceId)});
 }return result;
}
function SceneNode({data}){
 const {scene,beats,onOpen,onEdit,active}=data;
 return <div className={'subscene-node '+(active?'current':'')}>
  <Handle type="target" position={Position.Left}/><Handle type="source" position={Position.Right}/>
  <div className="subscene-node-title"><Icon name={scene.kind==='living'?'Armchair':scene.kind==='garden'?'Trees':'TrainFront'} size={24}/><div><small>{scene.location}</small><strong>{scene.name}</strong></div></div>
  <div className="subscene-node-body"><span>{beats.length} блоков · {beats.filter(b=>b.kind==='choice').length} развилок</span><span>{scene.weather} · {scene.time}</span>
  {beats.filter(b=>b.kind==='end').map(b=><button className="nodrag ending-link" key={b.id} onClick={()=>onOpen(b.id)}><Icon name="Flag" size={12}/>{b.ending||'Концовка'}</button>)}
  <div className="subscene-node-actions"><Button className="nodrag" icon="Workflow" onClick={()=>onOpen(scene.entry)}>Сценарий</Button>{onEdit&&<Button className="nodrag" icon="Settings2" onClick={()=>onEdit(scene.id)}>Настройки</Button>}</div></div>
 </div>;
}
const nodeTypes={subscene:SceneNode};
export default function SubsceneMap({project,current,onOpen,onEdit}){
 const canvas=useRef(),flow=useRef();useEffect(()=>{let frame;const ro=new ResizeObserver(()=>{cancelAnimationFrame(frame);frame=requestAnimationFrame(()=>flow.current?.fitView({padding:.12,maxZoom:1.05}));});ro.observe(canvas.current);return()=>{ro.disconnect();cancelAnimationFrame(frame);};},[]);
 const [pair,setPair]=useState(null),links=useMemo(()=>subsceneLinks(project),[project]);
 const groups=useMemo(()=>{const map=new Map();for(const link of links){const id=link.source+'>'+link.target;map.set(id,[...(map.get(id)||[]),link]);}return [...map];},[links]);
 const nodes=project.subscenes.map((scene,i)=>({id:scene.id,type:'subscene',position:{x:i===0?40:440+Math.floor((i-1)/2)*400,y:i===0?155:(i-1)%2*290+20},data:{scene,beats:project.chapters.filter(c=>c.subsceneId===scene.id).flatMap(c=>c.beats),onOpen,onEdit,active:scene.id===current}}));
 const edges=groups.map(([id,list],i)=>({id,source:list[0].source,target:list[0].target,label:`${list.length} ${list.length===1?'переход':'перехода'}`,type:'smoothstep',style:{stroke:pair===id?'#e0b0c6':'#938699',strokeWidth:pair===id?3:1.5},markerEnd:{type:MarkerType.ArrowClosed,color:'#b698ac'},labelStyle:{fill:'#e1d9e0',fontSize:12},labelBgStyle:{fill:'#292832'},pathOptions:{offset:20+i*12,borderRadius:12}}));
 const shown=pair?groups.find(([id])=>id===pair)?.[1]||[]:links;
 return <div className="subscene-map"><div className="subscene-map-canvas" ref={canvas}><ReactFlow nodes={nodes} edges={edges} nodeTypes={nodeTypes} nodesConnectable={false} onInit={instance=>{flow.current=instance;}} fitView minZoom={.3} maxZoom={1.5} onEdgeClick={(_,e)=>setPair(e.id)} onNodeClick={(_,n)=>setPair(null)}><Background color="#38343e" gap={20}/><Controls showInteractive={false}/></ReactFlow></div>
 <div className="subscene-routes"><header><strong>Переходы между сабсценами</strong><span>Показаны реальные связи сценария, включая возвращения</span>{pair&&<button onClick={()=>setPair(null)}>Все переходы</button>}</header>
 {shown.map(e=><button className="subscene-route" key={e.from+e.to+e.choiceId} onClick={()=>onOpen(e.from)}><span>{project.subscenes.find(s=>s.id===e.source)?.location}<small>{e.from} · {e.beat.text.slice(0,45)}</small></span><span>{e.answer?.label||'После реплики'}<small>{e.answer?.condition==='trust'?`Доверие ≥ ${e.answer.threshold??3}`:e.answer?.condition==='letter'?'Письмо найдено':e.answer?.condition&&e.answer.condition!=='always'?e.answer.condition+' = да':'Без дополнительного условия'}</small></span><Icon name="ArrowRight" size={15}/><span>{project.subscenes.find(s=>s.id===e.target)?.location}<small>Вход: {e.to}</small></span></button>)}
 {!shown.length&&<p>Переходов пока нет. Выберите продолжение реплики в другой сабсцене — связь появится здесь.</p>}</div></div>;
}
