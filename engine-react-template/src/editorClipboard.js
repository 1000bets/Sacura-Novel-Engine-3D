import {uid} from './studioModel.js';
import {objectTransform} from './sceneEditing.js';

export function copySceneObjects(objects,scene){
 return objects.map(object=>({...structuredClone(object),builtin:object.builtin||(['letter','door','fireplace','garden-note','ticket'].includes(object.id)?object.id:undefined),clipboardTransform:objectTransform(object,scene.id,scene.kind)}));
}
export function pasteSceneObjects(snapshot,scene,step=1){
 return snapshot.map(source=>{const copy=structuredClone(source),transform=copy.clipboardTransform;delete copy.clipboardTransform;copy.id=uid('object');copy.name+=' · копия';copy.subsceneId=scene.id;transform.position[0]+=.65*step;copy.transforms={[scene.id]:transform};return copy;});
}
export function cloneStoryNodes(snapshot){
 const ids=new Map(snapshot.map(beat=>[beat.id,uid('beat')]));
 return snapshot.map(source=>{const beat=structuredClone(source),bindings=new Map((beat.bindings||[]).map(b=>[b.id,uid('bind')]));beat.id=ids.get(source.id);
 if('next'in beat)beat.next=ids.get(beat.next)||null;
 for(const choice of beat.choices||[]){choice.id=uid('choice');choice.next=ids.get(choice.next)||null;}
 for(const binding of beat.bindings||[])binding.id=bindings.get(binding.id);
 for(const batches of Object.values(beat.batches||{}))for(const batch of batches){batch.id=uid('batch');batch.bindingIds=batch.bindingIds.map(id=>bindings.get(id)).filter(Boolean);}
 return beat;});
}
export function clipboardCommand(e){
 if(e.defaultPrevented||e.repeat||!(e.ctrlKey||e.metaKey)||e.altKey||e.shiftKey||e.target.isContentEditable||e.target.closest?.('input,textarea,select,[role="textbox"],[role="dialog"]'))return null;
 return ({KeyC:'copy',KeyV:'paste'})[e.code]||({c:'copy',v:'paste'})[e.key.toLowerCase()]||null;
}
