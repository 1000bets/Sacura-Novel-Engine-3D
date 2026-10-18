import {registerCapability} from '../capabilityRegistry.js';
import {clamp} from '../gamePhysics.js';
import {operation,ref,number,session} from './shared.js';
function hydrate(){for(const [id,a]of Object.entries(this.config.actors))if(!this.state.actors[id]&&Number.isFinite(a.maxHP)&&this.host.project.objects.some(o=>o.id===id))this.state.actors[id]={hp:a.maxHP,maxHP:a.maxHP,dead:false};}
const health={
 damage(id,amount,context={}){
  const a=this.state.actors[id];if(!a||!Number.isFinite(a.maxHP)||a.dead||!Number.isFinite(amount)||amount<0)return false;
  a.hp=clamp(a.hp-amount,0,a.maxHP);const died=a.hp===0;if(died)a.dead=true;
  context={...context,target:{type:'object',id},targetId:id,sceneId:this.scene.id};this.sync();this.post('hit',{objectId:id,amount},undefined,context);if(died)this.post('death',{objectId:id},undefined,context);return true;
 },
 heal(id,amount){const a=this.state.actors[id];if(!a||!Number.isFinite(a.maxHP)||a.dead||!Number.isFinite(amount)||amount<0)return false;a.hp=clamp(a.hp+amount,0,a.maxHP);this.sync();return true;}
};
const check=(g,id)=>{if(!Number.isFinite(g.actors[id]?.maxHP))throw Error('У владельца не подключено здоровье.');};
registerCapability({id:'sacura.health',initialize:hydrate,update:hydrate,methods:health,
 commands:{damage(v,c){return this.damage(v.target||this.config.legacyOwnerId,Number(v.amount??25),c);},heal(v){return this.heal(v.target||this.config.legacyOwnerId,Number(v.amount??25));}},
 bindings:Object.fromEntries(['hp','maxHP','dead'].map(field=>[field,{schema:{type:field==='dead'?'boolean':'number'},validate:check,read:(s,g,id)=>s.actors[id]?.[field]}])),
 operations:[
  ...['damage','heal'].map(name=>operation('sacura.health.'+name,name==='damage'?'Нанести урон':'Восстановить здоровье',{owner:ref('object','Владелец здоровья'),amount:number('Количество')},{type:'boolean'},(p,c)=>session(c)[name](p.owner,p.amount,c))),
  operation('sacura.health.all-dead','Все выбранные объекты погибли',{objects:{type:'collection',label:'Объекты',items:ref('object','Объект'),minItems:0}},{type:'boolean'},(p,c)=>p.objects.length>0&&p.objects.every(id=>c.gameState?.actors[id]?.dead===true),{pure:true}),
  operation('sacura.signal.victory','Отправить сигнал победы',{}, {type:'void'},(p,c)=>c.signal('victory',{}))
 ]});
