import {allBeats,sceneFor} from './studioModel.js';

export function playbackStartId(project,selectedId,fromSelection=false){
 if(fromSelection)return allBeats(project).some(b=>b.id===selectedId)?selectedId:null;
 const scene=sceneFor(project,selectedId);
 return project.chapters.filter(c=>c.subsceneId===scene?.id).flatMap(c=>c.beats).find(b=>b.id===scene.entry)?.id||null;
}

export function deleteStoryNode(project,id){
 const owner=project.chapters.find(c=>c.beats.some(b=>b.id===id));
 if(!owner)return {error:'Реплика не найдена.'};
 const scene=project.subscenes.find(s=>s.id===owner.subsceneId);
 const remaining=project.chapters.filter(c=>c.subsceneId===owner.subsceneId).flatMap(c=>c.beats).filter(b=>b.id!==id);
 if(!remaining.length)return {error:'В сабсцене должна оставаться хотя бы одна реплика.'};
 const removed=owner.beats.find(b=>b.id===id);
 const nextId=remaining.find(b=>b.id===removed.next)?.id||remaining[0].id;
 owner.beats=owner.beats.filter(b=>b.id!==id);
 for(const beat of allBeats(project)){
  if(beat.next===id)beat.next=null;
  for(const choice of beat.choices||[])if(choice.next===id)choice.next=null;
 }
 if(scene?.entry===id)scene.entry=nextId;
 return {nextId};
}
