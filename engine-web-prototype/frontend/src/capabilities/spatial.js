import {registerCapability} from '../capabilityRegistry.js';
import {operation,ref,length,sub,session} from './shared.js';
// Geometry/clock only. Detection, attack conditions, damage and reactions belong to sources.
registerCapability({id:'sacura.spatial',operations:[
 operation('sacura.spatial.observe','Положение относительно объекта',{owner:ref('object','Наблюдатель'),target:ref('object','Цель')},{type:'object',properties:{available:{type:'boolean'},distance:{type:'number'},direction:{type:'vector',size:3},lineOfSight:{type:'boolean'},clock:{type:'number'}}},(p,c)=>{const g=session(c),from=g.position(p.owner),to=g.position(p.target),available=!!from&&!!to,diff=available?sub(to,from):[0,0,0],distance=length(diff),direction=diff.map((v,i)=>i===1?0:v/(distance||1));let lineOfSight=false;if(available){const eye=from.map((v,i)=>v+(i===1?1:0)),end=to.map((v,i)=>v+(i===1?1:0)),ray=sub(end,eye),n=length(ray),hit=g.raycast(eye,ray.map(v=>v/(n||1)),n,[p.owner]);lineOfSight=!hit||hit.id===p.target;}return {available,distance,direction,lineOfSight,clock:g.state.clock};},{pure:true}),
 operation('sacura.game.clock','Игровое время',{}, {type:'number'},(p,c)=>session(c).state.clock,{pure:true})
]});
