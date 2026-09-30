import {uid} from './model.js';
import {isObjectInScene} from './sceneEditing.js';

export const BUILTIN_CHARACTER_ANIMATIONS=[
 ['стоит','Спокойно стоит: руки опущены, голова прямо. Подходит для ожидания и нейтрального диалога.','PersonStanding'],
 ['улыбка','Наклоняет голову и приподнимает руку; выражение лица становится мягче. Для радостной реакции.','Smile'],
 ['задумчивость','Слегка разворачивается и поднимает руку к голове. Для размышления или сомнения.','Brain'],
 ['танец','Ритмично покачивается, поворачивается и двигает поднятыми руками. Для танца и празднования.','Music2'],
 ['грусть','Наклоняет голову набок; выражение лица становится грустным. Для печали и разочарования.','Frown'],
].map(([id,description,icon])=>({id,name:id,duration:0,description,icon}));
export function characterAnimations(character){
 const clips=[...(character?.model?character.model.animations||[]:BUILTIN_CHARACTER_ANIMATIONS),...(character?.extraAnimations||[])];
 return clips.map(clip=>({...clip,...character?.animationDetails?.[clip.id]}));
}
export function enabledCharacterAnimations(character){
 return characterAnimations(character).filter(clip=>!Array.isArray(character?.enabledAnimations)||character.enabledAnimations.includes(clip.id));
}
export function characterAnimationOptions(character,current){
 const options=enabledCharacterAnimations(character).map(clip=>[clip.id,clip.name]);
 if(current&&!options.some(([id])=>id===current))options.unshift([current,'Недоступная анимация · выберите другую']);
 return options;
}
export function characterAnimationId(character,pose,moving){
 const available=enabledCharacterAnimations(character),has=id=>available.some(clip=>clip.id===id);
 if(moving&&has(character?.walkAnimation))return character.walkAnimation;
 if(pose)return has(pose)?pose:null;
 if(has(character?.idleAnimation))return character.idleAnimation;
 if(character?.idleAnimation!=null)return null;
 return available[0]?.id||null;
}
export function createCharacter(project,sceneId,name){
 if(!name?.trim())throw new Error('Укажите имя персонажа.');
 if(!project.subscenes.some(scene=>scene.id===sceneId))throw new Error('Выберите локацию персонажа.');
 const character={id:uid('character'),name:name.trim(),type:'Персонаж',color:'#bb99aa',active:true,subsceneId:sceneId,description:'',idleAnimation:'стоит',enabledAnimations:BUILTIN_CHARACTER_ANIMATIONS.map(clip=>clip.id),transforms:{[sceneId]:{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}};
 project.objects.push(character);return character;
}
export function setCharacterInScene(project,id,sceneId,present){
 const character=project.objects.find(object=>object.id===id&&object.type==='Персонаж'),scene=project.subscenes.find(item=>item.id===sceneId);
 if(!character||!scene)throw new Error('Персонаж или локация не найдены.');
 if(character.subsceneId&&character.subsceneId!==sceneId){
  const previous=character.subsceneId;character.subsceneId='';
  for(const item of project.subscenes){
   if(item.id!==previous)item.excludedObjectIds=[...new Set([...(item.excludedObjectIds||[]),id])];
  }
 }
 scene.excludedObjectIds=present?(scene.excludedObjectIds||[]).filter(objectId=>objectId!==id):[...new Set([...(scene.excludedObjectIds||[]),id])];
}
export const characterScenes=(project,character)=>project.subscenes.filter(scene=>isObjectInScene(character,scene));
