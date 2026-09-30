import {isPureNode,dataTargets,getInput,setInput} from './logicModel.js';
import {allBeats,sceneFor} from './studioModel.js';

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
