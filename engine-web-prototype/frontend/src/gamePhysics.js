import {objectTransform} from './sceneEditing.js';
export const clamp=(v,min,max)=>Math.min(max,Math.max(min,v));
export function bodyBounds(o,position,sceneId){const t=objectTransform(o,sceneId),c=o.collision||{},size=c.custom?c.size:o.type==='Персонаж'?[.6,1.6,.6]:t.scale.map(v=>v*.5),offset=c.custom?c.offset:o.type==='Персонаж'?[0,.8,0]:[0,0,0];return {min:position.map((v,i)=>v+(offset?.[i]||0)-size[i]/2),max:position.map((v,i)=>v+(offset?.[i]||0)+size[i]/2)};}
export function intersects(a,b){return a.min.every((v,i)=>v<b.max[i]-1e-5&&a.max[i]>b.min[i]+1e-5);}
export function rayBox(origin,direction,box,range=Infinity){let near=0,far=range;for(let i=0;i<3;i++){if(Math.abs(direction[i])<1e-9){if(origin[i]<box.min[i]||origin[i]>box.max[i])return null;}else{let a=(box.min[i]-origin[i])/direction[i],b=(box.max[i]-origin[i])/direction[i];if(a>b)[a,b]=[b,a];near=Math.max(near,a);far=Math.min(far,b);if(near>far)return null;}}return near;}
// Swept AABB motion for the simple controller and box bodies. Each axis slides on contact.
export function moveBody(object,position,delta,colliders,sceneId,{bounce=0,velocity=[0,0,0]}={}){
 let at=[...position],grounded=false;const contacts=new Set();
 for(const axis of [0,2,1]){const step=delta[axis];if(!step)continue;const from=bodyBounds(object,at,sceneId),end=[...at];end[axis]+=step;let allowed=step;
 for(const c of colliders){if(c.id===object.id)continue;const other=[0,1,2].filter(i=>i!==axis);if(!other.every(i=>from.max[i]>c.box.min[i]+1e-5&&from.min[i]<c.box.max[i]-1e-5))continue;
 const gap=step>0?c.box.min[axis]-from.max[axis]:c.box.max[axis]-from.min[axis];if(step>0&&gap>=-1e-4&&gap<=allowed||step<0&&gap<=1e-4&&gap>=allowed){allowed=gap;contacts.add(c.id);if(axis===1&&step<0)grounded=true;}}
 at[axis]+=allowed;if(allowed!==step)velocity[axis]=-velocity[axis]*bounce;
 }
 return {position:at,velocity,grounded,contacts:[...contacts]};
}
