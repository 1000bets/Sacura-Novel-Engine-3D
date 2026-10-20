import {uid} from './model.js';
export function createObjectGroup(project,sceneId,ids,name='Новая группа',{allowEmpty=false}={}){
 const members=[...new Set(ids)].filter(id=>project.objects.some(o=>o.id===id));
 if(members.length<2&&!allowEmpty)return null;

 project.objectGroups||=[];
 for(const group of project.objectGroups.filter(g=>g.sceneId===sceneId))group.objectIds=group.objectIds.filter(id=>!members.includes(id));

 const group={id:uid('object-group'),sceneId,name,objectIds:members};project.objectGroups.push(group);return group;
}
export function addObjectsToGroup(project,groupId,ids){
 const group=project.objectGroups?.find(g=>g.id===groupId);if(!group)return;
 const members=ids.filter(id=>project.objects.some(o=>o.id===id));
 for(const other of project.objectGroups.filter(g=>g.sceneId===group.sceneId&&g.id!==groupId))other.objectIds=other.objectIds.filter(id=>!members.includes(id));
 group.objectIds=[...new Set([...group.objectIds,...members])];

}
export function sceneObjectGroups(project,sceneId,objects){
 const ids=new Set(objects.map(o=>o.id));
 return (project.objectGroups||[]).filter(g=>g.sceneId===sceneId).map(g=>({...g,objectIds:g.objectIds.filter(id=>ids.has(id))}));
}

export function removeObjectsFromGroups(project,sceneId,ids){
 for(const group of project.objectGroups||[])if(group.sceneId===sceneId)group.objectIds=group.objectIds.filter(id=>!ids.includes(id));
}
