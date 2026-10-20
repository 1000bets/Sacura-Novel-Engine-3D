import {uid} from './model.js';
import {descriptionFor} from './mechanicDescriptions.js';

// Copy initial configuration, never Play state or source definitions. References
// inside the copied selection follow its new identities; external links stay explicit.
export function copyObjectComponents(project,sources,copies,sourceProject=project){
 const g=project.gameplay,origin=sourceProject.gameplay;if(!g||!origin)return;
 const ids=new Map(sources.map((o,i)=>[o.id,copies[i].id])),owners=new Set(ids.keys()),instances=(origin.mechanics||[]).filter(m=>m.scope?.kind==='object'&&owners.has(m.scope.id)&&descriptionFor(sourceProject,m.definitionId,m.definitionVersion)?.componentVersion===1);
 if(instances.length&&sourceProject.id!==project.id)throw Error('Для переноса компонентов между проектами используйте авторский пакет с зависимостями.');
 for(const m of instances)ids.set(m.id,uid('mechanic'));
 const weapons=(origin.weapons||[]).filter(w=>owners.has(w.ownerId)&&w.component&&ids.has(w.component.instance?.id));for(const w of weapons)ids.set(w.id,uid('weapon'));
 const items=(origin.items||[]).filter(item=>[item.componentUse,item.componentPickup].some(a=>ids.has(a?.instance?.id)));for(const item of items)ids.set(item.id,uid('item'));
 const remap=value=>{if(!value||typeof value!=='object')return;for(const [key,child]of Object.entries(value)){if(value.kind==='reference'&&key==='id'&&['direct','internal',undefined].includes(value.mode)&&ids.has(child))value[key]=ids.get(child);else if(['ownerId','worldObjectId','weaponId','itemId'].includes(key)&&ids.has(child))value[key]=ids.get(child);else if(child&&typeof child==='object')remap(child);}};
 for(const original of instances){const m=structuredClone(original);m.id=ids.get(original.id);m.scope.id=ids.get(original.scope.id);remap(m);g.mechanics.push(m);}
 for(const [oldId,newId]of sources.map((o,i)=>[o.id,copies[i].id])){const actor=origin.actors?.[oldId];if(actor?.componentHealth&&ids.has(actor.componentHealth.instance?.id)){const a=structuredClone(actor);remap(a);g.actors[newId]=a;}}
 for(const w of weapons){const copy=structuredClone(w);copy.id=ids.get(w.id);remap(copy);g.weapons.push(copy);}
 for(const item of items){const copy=structuredClone(item);copy.id=ids.get(item.id);remap(copy);g.items.push(copy);}
 for(const inventory of [origin.inventory,...origin.inventories||[]])if(inventory?.owner?.kind==='object'&&owners.has(inventory.owner.id)){
  const copy=structuredClone(inventory);copy.owner.id=ids.get(inventory.owner.id);remap(copy);if(copy.equipment)for(const [slot,id]of Object.entries(copy.equipment))copy.equipment[slot]=ids.get(id)||id;(g.inventories??=[]).push(copy);
 }
 for(const copy of copies){remap(copy);const target=copy.interactionCall?.parameters?.instance?.id;if(target&&!g.mechanics.some(m=>m.id===target))delete copy.interactionCall;}
 return ids;
}
