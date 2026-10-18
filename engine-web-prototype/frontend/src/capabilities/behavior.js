import {registerCapability} from '../capabilityRegistry.js';
import {operation,ref,length,sub,session} from './shared.js';
// Behaviour chooses intent; movement retains the existing body integration.
registerCapability({id:'sacura.behavior',initialize(){for(const [id,a]of Object.entries(this.config.actors))if(a.ai)this.state.behaviors[id]={nextAttack:0,direction:[0,0,0],enabled:true};},
 update(){for(const o of this.objects){const ai=this.config.actors[o.id]?.ai;if(!ai)continue;const b=this.state.behaviors[o.id]??={nextAttack:0,enabled:true};b.direction=[0,0,0];if(!b.enabled||this.state.disabled[o.id])continue;
  const targetId=ai.targetId,targetPos=targetId&&this.position(targetId),pos=this.position(o.id);if(!targetPos||!pos)continue;
  const diff=sub(targetPos,pos),distance=length(diff);if(distance>=ai.detection)continue;
  if(distance>ai.range)b.direction=[diff[0]/(distance||1),0,diff[2]/(distance||1)];
  else if(this.state.clock>=b.nextAttack){b.nextAttack=this.state.clock+Math.max(.05,ai.interval);const eye=pos.map((v,i)=>v+(i===1?1:0)),target=targetPos.map((v,i)=>v+(i===1?1:0)),d=sub(target,eye),hit=this.raycast(eye,d.map(v=>v/(length(d)||1)),length(d),[o.id]);if(!hit||hit.id===targetId)this.damage(targetId,ai.damage,{sourceId:o.id,initiatorId:o.id});}
 }},operations:[operation('sacura.behavior.enabled','Разрешить поведение объекта',{owner:ref('object','Объект'),enabled:{type:'boolean',default:false,label:'Разрешить'}},{type:'void'},(p,c)=>{const s=session(c).state.behaviors[p.owner];if(s){s.enabled=p.enabled;if(!p.enabled)s.direction=[0,0,0];}})]});
