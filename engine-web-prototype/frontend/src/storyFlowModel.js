import {variableType} from './variableModel.js';
import {isPureNode,dataTargets,getInput,logicType} from './logicModel.js';
import {storyPorts} from './choiceModel.js';
import dagre from '@dagrejs/dagre';
import {allBeats, batchesFor, bindingActions, PHASES, TYPES} from './studioModel.js';
import {timelineCondition} from './timelineModel.js';

// One node is one playable moment: preparation, dialogue + concurrent events,
// player input, completion, then the authored outgoing route.
export function buildStoryFlow(project) {
  const beats = allBeats(project);
  const owners = new Map(project.chapters.flatMap(c => c.beats.map(b => [b.id, project.subscenes.find(s => s.id === c.subsceneId)])));
  const ids = new Set(beats.map(b => b.id));
  const nodes = beats.map(beat => {
    const scene = owners.get(beat.id);
    const phases = PHASES.map(phase => ({...phase, batches: batchesFor({...beat, bindings:beat.bindings || []}, phase.id).map(batch => ({
      ...batch,
      events: batch.bindings.map(binding => ({binding, event:project.events?.find(e => e.id === binding.eventId),
        actions: bindingActions(project, binding).map(action => ({...action, summary:`${project.objects?.find(o => o.id === action.target)?.name || TYPES[action.type]?.label || action.type} → ${action.type === 'camera' && action.cameraId ? project.subscenes.flatMap(s => s.cameras || []).find(c => c.id === action.cameraId)?.name || 'Камера недоступна' : Array.isArray(action.value) ? action.value.join(', ') : action.value ?? ''}`}))
      }))
    }))}));
    const ports=storyPorts(beat);
    const eventHeight = phases.reduce((total,phase) => total + phase.batches.reduce((n,b) => n + 34 + b.events.reduce((sum,e) => sum + 76 + Math.min(2,e.actions.length)*18,0),0),0);
    return {id:beat.id,type:'moment',position:{x:0,y:0},width:beat.kind==='variable'?220:beat.kind==='set-variable'?260:360,height:beat.kind==='variable'?90:beat.kind==='set-variable'?210:isPureNode(beat)||['branch','set-variable'].includes(beat.kind)?330:340+eventHeight+ports.length*175,
      data:{beat,scene,phases,ports,entry:scene?.entry===beat.id}};
  });
  const edges = [];
  for(const node of [...nodes]) for(const port of node.data.ports) {
    if(!port.next) continue;
    if(!ids.has(port.next)) {
      const id='missing:'+port.next;
      if(!nodes.some(n=>n.id===id))nodes.push({id,type:'missing',position:{x:0,y:0},width:260,height:100,data:{title:'Переход недоступен',text:port.next}});
    }
    const target=ids.has(port.next)?port.next:'missing:'+port.next;
    const targetScene=owners.get(port.next),crossScene=targetScene?.id!==node.data.scene?.id;
    edges.push({id:`${node.id}/${port.id}`,source:node.id,target,sourceHandle:port.id,targetHandle:'in',reconnectable:true,
      label:[node.data.beat.kind==='choice'?port.label:'',crossScene&&targetScene?`→ ${targetScene.name}`:''].filter(Boolean).join(' · '),
      data:{crossScene,from:node.id,to:port.next,broken:!ids.has(port.next),branch:node.data.beat.kind==='branch'?port.id:null}});
  }
  for(const node of nodes.filter(n=>n.data.beat))for(const port of dataTargets(node.data.beat)){
    const source=getInput(node.data.beat,port);if(!source)continue;
    const sourceBeat=nodes.find(n=>n.id===source)?.data.beat;
    const valueType=['variable','set-variable'].includes(sourceBeat?.kind)?variableType(project,sourceBeat.variable):logicType(project,sourceBeat);
    edges.push({id:`data:${source}/${node.id}/${port}`,source,target:node.id,sourceHandle:'value',targetHandle:port,reconnectable:true,data:{valueWire:true,valueType}});
  }
  return {nodes,edges};
}

export function arrangeStoryFlow(nodes,edges) {
  const graph=new dagre.graphlib.Graph();
  graph.setGraph({rankdir:'LR',nodesep:90,ranksep:150,marginx:50,marginy:50});
  graph.setDefaultEdgeLabel(()=>({}));
  nodes.forEach(n=>graph.setNode(n.id,{width:n.measured?.width || n.width || 360,height:n.measured?.height || n.height || 400}));
  edges.forEach(e=>graph.setEdge(e.source,e.target));
  dagre.layout(graph);
  return nodes.map(n=>{const p=graph.node(n.id);return {...n,position:{x:p.x-p.width/2,y:p.y-p.height/2}};});
}
