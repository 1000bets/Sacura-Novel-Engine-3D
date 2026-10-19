import {registerCapability} from '../capabilityRegistry.js';
import {runBuiltin,installBuiltinBehaviors} from '../builtinBehaviors.js';
import {operation,ref,number,ownerSchema,session} from './shared.js';
function hydrate(){if(Object.values(this.config.actors).some(a=>Number.isFinite(a.maxHP)&&!a.behaviors?.health))installBuiltinBehaviors(this.host.project);for(const [id,a]of Object.entries(this.config.actors))if(!this.state.actors[id]&&Number.isFinite(a.maxHP)&&this.host.project.objects.some(o=>o.id===id))this.state.actors[id]={hp:a.maxHP,maxHP:a.maxHP,dead:false};}
const health={
 damage(id,amount,context={}){const a=this.config.actors[id];if(!a?.behaviors?.health||!Number.isFinite(amount)||amount<0)return false;return runBuiltin(this,a.behaviors.health,'damage',{owner:id,amount},context);},
 heal(id,amount,context={}){const a=this.config.actors[id];if(!a?.behaviors?.health||!Number.isFinite(amount)||amount<0)return false;return runBuiltin(this,a.behaviors.health,'heal',{owner:id,amount},context);}
};
const check=(g,id)=>{if(!Number.isFinite(g.actors[id]?.maxHP))throw Error('У владельца не подключено здоровье.');};
registerCapability({id:'sacura.health',initialize:hydrate,update:hydrate,methods:health,
 commands:{damage(v,c){return this.damage(v.target||this.config.legacyOwnerId,Number(v.amount??25),c);},heal(v,c){return this.heal(v.target||this.config.legacyOwnerId,Number(v.amount??25),c);}},
 bindings:Object.fromEntries(['hp','maxHP','dead'].map(field=>[field,{schema:{type:field==='dead'?'boolean':'number'},validate:check,read:(s,g,id)=>s.actors[id]?.[field]}])),
 operations:[
  ...['damage','heal'].map(name=>operation('sacura.health.'+name,name==='damage'?'Нанести урон':'Восстановить здоровье',{owner:ref('object','Владелец здоровья'),amount:number('Количество')},{type:'boolean'},(p,c)=>session(c)[name](p.owner,p.amount,c))),
  operation('sacura.health.all-dead','Все выбранные объекты погибли',{objects:{type:'collection',label:'Объекты',items:ref('object','Объект'),minItems:0}},{type:'boolean'},(p,c)=>p.objects.length>0&&p.objects.every(id=>c.gameState?.actors[id]?.dead===true),{pure:true}),
  operation('sacura.signal.victory','Отправить сигнал победы',{}, {type:'void'},(p,c)=>c.signal('victory',{})),
  operation('sacura.health.read','Прочитать здоровье',{owner:ownerSchema},{type:'object',properties:{valid:{type:'boolean',label:'Компонент здоровья доступен'},hp:{type:'number',label:'Текущее здоровье'},maxHP:{type:'number',label:'Максимальное здоровье'},dead:{type:'boolean',label:'Объект погиб'}}},(p,c)=>{const a=session(c).state.actors[p.owner];return {valid:!!a,hp:a?.hp??0,maxHP:a?.maxHP??0,dead:!!a?.dead};},{pure:true}),
  operation('sacura.health.write','Сохранить здоровье',{owner:ref('object','Владелец'),hp:{type:'number',min:0},dead:{type:'boolean',default:false,label:'Объект погиб'}},{type:'void'},(p,c)=>{const a=session(c).state.actors[p.owner];if(!a)throw Error('Здоровье не подключено.');if(p.hp>a.maxHP)throw Error('Здоровье превышает максимум.');a.hp=p.hp;a.dead=p.dead;}),
  operation('sacura.health.publish','Опубликовать изменение здоровья',{owner:ref('object','Владелец'),signal:{type:'string',enum:['hit','death'],default:'hit',label:'Сигнал'},amount:{type:'number',min:0,default:0}},{type:'void'},(p,c)=>{const game=session(c),context={...c,target:{type:'object',id:p.owner},targetId:p.owner,sceneId:game.scene.id};game.post(p.signal,{objectId:p.owner,...(p.signal==='hit'?{amount:p.amount}:{})},undefined,context);}),
  operation('sacura.game.fail','Прервать с объяснением',{message:{type:'string',label:'Причина',default:'Действие недоступно.'}},{type:'void'},p=>{throw Error(p.message);}),
  operation('sacura.game.commit','Обновить состояние и интерфейс',{}, {type:'void'},(p,c)=>{session(c).sync();c.emit();}),
  operation('sacura.game.message','Показать сообщение',{message:{type:'string',label:'Текст',default:''}},{type:'void'},(p,c)=>{session(c).state.message=p.message;c.emit();})
 ]});
