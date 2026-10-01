import {isPureNode,dataTargets,getInput,setInput} from './logicModel.js';
import {allBeats,sceneFor,uid} from './studioModel.js';

export const STORY_NODE_KINDS=[['dialogue','Реплика'],['choice','Выбор'],['branch','If · Если'],['gate','Взаимодействие'],['merge','Схождение'],['end','Концовка']];

export function changeStoryNodeKind(node,kind){
 if(!STORY_NODE_KINDS.some(([id])=>id===kind))return;
 node.kind=kind;
 if(kind==='branch'){
  node.condition??=false;node.trueNext??=null;node.falseNext??=null;
 }else{
  node.next=node.next||node.trueNext||null;
  delete node.inputs;delete node.test;
 }
 if(kind==='end'){node.next=null;node.ending||='Конец истории';}
 if(kind==='choice'&&!node.choices?.length){node.choiceMode='value';node.choices=[{id:uid('choice'),label:'Продолжить',condition:'always',next:null,result:{type:'string',value:'Продолжить'},availability:null}];}
 if(kind==='gate'){node.signal||='';node.timeout??=30;}
}

// Delete only actual canvas selection, including selection made with a drag box.
export function selectedStoryNodeIds(nodes){
 return nodes.filter(node=>node.selected&&node.data?.beat).map(node=>node.id);
}

export function playbackStartId(project,selectedId,fromSelection=false){
 if(fromSelection)return allBeats(project).some(b=>b.id===selectedId&&!isPureNode(b))?selectedId:null;
 const scene=sceneFor(project,selectedId);
 return project.chapters.filter(c=>c.subsceneId===scene?.id).flatMap(c=>c.beats).find(b=>b.id===scene.entry)?.id||null;
}

export function deleteStoryNode(project,id){
 const owner=project.chapters.find(c=>c.beats.some(b=>b.id===id));
 if(!owner)return {error:'Реплика не найдена.'};
 const scene=project.subscenes.find(s=>s.id===owner.subsceneId);
 const remaining=project.chapters.filter(c=>c.subsceneId===owner.subsceneId).flatMap(c=>c.beats).filter(b=>b.id!==id&&!isPureNode(b));
 if(!remaining.length)return {error:'В сабсцене должна оставаться хотя бы одна реплика.'};
 const removed=owner.beats.find(b=>b.id===id);
 const nextId=remaining.find(b=>b.id===removed.next)?.id||remaining[0].id;
 owner.beats=owner.beats.filter(b=>b.id!==id);
 for(const beat of allBeats(project)){
  for(const port of dataTargets(beat))if(getInput(beat,port)===id)setInput(beat,port,null);
  if(beat.next===id)beat.next=null;
  if(beat.trueNext===id)beat.trueNext=null;if(beat.falseNext===id)beat.falseNext=null;
  for(const choice of beat.choices||[])if(choice.next===id)choice.next=null;
 }
 if(scene?.entry===id)scene.entry=nextId;
 return {nextId};
}

// Creating a node only changes chapter membership, never authored routes.
export function insertStoryNode(project,chapterId,afterId,node){
 const chapter=project.chapters.find(c=>c.id===chapterId);
 const index=chapter.beats.findIndex(b=>b.id===afterId);
 chapter.beats.splice(index+1,0,node);
}
