import {runBuiltin} from '../builtinBehaviors.js';
import {reference} from '../valueSchemas.js';
import {registerCapability} from '../capabilityRegistry.js';
import {operation,ref,ownerSchema,ownerScope,ownerKey,session} from './shared.js';
export function inventoryConfig(g,owner){return g.inventory.owner?.id===owner?g.inventory:(g.inventories||[]).find(i=>i.owner.id===owner);}
export function inventoryState(g,s,owner,create=false){const config=inventoryConfig(g,owner);if(!config)throw Error('У владельца не подключён инвентарь.');const key=ownerKey(config.owner);if(create)s.inventories[key]??={items:[],equipment:{}};return s.inventories[key];}
const defaultOwner=g=>g.legacyInventoryOwnerId||g.inventory.owner.id;
export const countInventory=(g,s,id,owner=defaultOwner(g))=>inventoryState(g,s,owner?.id||owner)?.items.filter(x=>x.itemId===id).reduce((sum,x)=>sum+x.count,0)||0;
const countSchema={type:'integer',min:1,default:1,label:'Количество'};
registerCapability({id:'sacura.inventory',sync(){const owner=defaultOwner(this.config),inv=inventoryState(this.config,this.state,owner);this.state.inventory=inv?.items||[];this.state.equipment=inv?.equipment||{};},initialize(){
 for(const config of [this.config.inventory,...this.config.inventories||[]]){
  if(!config.enabled&&!config.starting.length&&this.config.legacyCompatibility!=='gameplay-v1')continue;
  inventoryState(this.config,this.state,config.owner.id,true);
  for(const x of config.starting)this.addItem(x.itemId,x.count||1,{ownerId:config.owner.id,publish:false});
  // Initial equipment is component state hydration, never an author callback.
  const equipment=inventoryState(this.config,this.state,config.owner.id,true).equipment;for(const [slot,id]of Object.entries(config.equipment)){equipment[slot]=id;const weaponId=this.config.items.find(i=>i.id===id)?.weaponId;if(weaponId)this.state.selectedWeapons[config.owner.id]=weaponId;}
 }
},methods:{
 inventoryFor(owner=defaultOwner(this.config)){return inventoryState(this.config,this.state,owner,true);},
 addItem(id,count=1,context={}){
  const owner=context.ownerId||defaultOwner(this.config),config=inventoryConfig(this.config,owner),item=this.config.items.find(i=>i.id===id);if(!item||!Number.isInteger(count)||count<1)throw Error('Предмет или количество не заданы.');
  const inv=this.inventoryFor(owner).items,space=inv.filter(x=>x.itemId===id).reduce((sum,x)=>sum+item.stackLimit-x.count,0)+(config.slots-inv.length)*item.stackLimit;if(count>space)throw Error('В инвентаре нет свободного места.');
  let remaining=count;for(const slot of inv.filter(x=>x.itemId===id)){const n=Math.min(remaining,item.stackLimit-slot.count);slot.count+=n;remaining-=n;}while(remaining){const n=Math.min(remaining,item.stackLimit);inv.push({itemId:id,count:n});remaining-=n;}
  this.sync();if(context.publish!==false)this.post('pickup',{objectId:'',itemId:id},undefined,{...context,target:{type:config.owner.kind,id:owner},...(config.owner.kind==='object'?{initiatorId:owner}:{})});return true;
 },
 count(id,owner=defaultOwner(this.config)){return countInventory(this.config,this.state,id,owner);},
 removeItem(id,count=1,owner=defaultOwner(this.config)){
  if(!Number.isInteger(count)||count<1||this.count(id,owner)<count)throw Error('Не хватает предметов.');const inv=this.inventoryFor(owner);
  for(const slot of inv.items.filter(x=>x.itemId===id)){const n=Math.min(count,slot.count);slot.count-=n;count-=n;}inv.items=inv.items.filter(x=>x.count);
  if(!this.count(id,owner))for(const [slot,value]of Object.entries(inv.equipment))if(value===id)delete inv.equipment[slot];
  if(!this.count(id,owner)&&this.config.items.find(i=>i.id===id)?.weaponId===this.state.selectedWeapons[owner])this.state.selectedWeapons[owner]=this.config.weapons.find(w=>w.ownerId===owner)?.id||'';
  this.sync();
 },
 equip(id,owner=defaultOwner(this.config),context={}){
  const i=this.config.items.find(i=>i.id===id);if(!i)throw Error('Этот предмет нельзя экипировать.');
  return runBuiltin(this,i.behavior,'equip',{owner,item:id},{...context,ownerId:owner});
 },
 async useItem(id,context={}){
  const owner=context.ownerId||defaultOwner(this.config),key='item:'+owner+':'+id;if(this.flights.has(key))return;this.flights.add(key);const generation=this.host.generation;
  const capabilityOwner=ownerScope(this.host.project,owner);context={...context,ownerId:owner,capabilityOwner,...(capabilityOwner.kind!=='project'?{capabilitySceneId:this.scene.id}:{})};
  try{this.assertReaction(context);const item=this.config.items.find(i=>i.id===id);if(!item)throw Error('Предмет отсутствует.');
   try{await runBuiltin(this,item.behavior,'use',{owner,item:id},context,false);}catch(error){if(!error.message.startsWith('INSTANCE_STOPPED')||!context.capabilitySceneId||context.capabilitySceneId===this.scene.id)throw error;}this.host.assert(generation);if(!context.capabilitySceneId||context.capabilitySceneId===this.scene.id)this.assertReaction(context);
   if(!context.capabilitySceneId||context.capabilitySceneId===this.scene.id)await this.flushTransition();this.host.emit();
  }finally{this.flights.delete(key);}
 },
 combine(id,context={}){
  const owner=context.ownerId||defaultOwner(this.config),r=this.config.recipes.find(r=>r.id===id);if(!r)throw Error('Рецепт отсутствует.');
  // Store transaction only: the graph owns ingredient conditions/removal/result.
  const inv=this.inventoryFor(owner),before=structuredClone(inv),selected=this.state.selectedWeapons[owner];
  try{const result=runBuiltin(this,r.behavior,'combine',{owner,recipe:r.id},{...context,ownerId:owner});return result;}catch(e){Object.assign(inv,before);if(selected===undefined)delete this.state.selectedWeapons[owner];else this.state.selectedWeapons[owner]=selected;this.sync();throw e;}
 }

},commands:{addItem(v,c){return this.addItem(v.itemId,v.count??1,{...c,ownerId:v.ownerId||c.ownerId});},removeItem(v,c){return this.removeItem(v.itemId,v.count??1,v.ownerId||c.ownerId);},equip(v,c){return this.equip(v.itemId,v.ownerId||c.ownerId,c);},useItem(v,c){return this.useItem(v.itemId,{...c,ownerId:v.ownerId||c.ownerId});},combine(v,c){return this.combine(v.recipeId,{...c,ownerId:v.ownerId||c.ownerId});}},
 bindings:{count:{schema:{type:'integer',min:0,label:'Количество предмета'},records:p=>p.gameplay.items,validate(g,id,itemId){if(!inventoryConfig(g,id)||!g.items.some(i=>i.id===itemId))throw Error('Инвентарь или предмет удалён.');},read:(s,g,id,itemId)=>countInventory(g,s,itemId,id)},items:{schema:{type:'collection',items:{type:'object',properties:{itemId:{type:'string'},count:{type:'integer',min:1}}}},validate(g,id){if(!inventoryConfig(g,id))throw Error('Инвентарь владельца удалён.');},read:(s,g,id)=>inventoryState(g,s,id)?.items||[]}},
 operations:[
  ...[['add','addItem'],['remove','removeItem'],['equip','equip'],['use','useItem']].map(([id,method])=>operation('sacura.inventory.'+id,({add:'Добавить предмет',remove:'Убрать предмет',equip:'Экипировать предмет',use:'Использовать предмет'})[id],{owner:ownerSchema,item:ref('item','Предмет'),...(['add','remove'].includes(id)?{count:countSchema}:{})},{type:'void'},(p,c)=>{const g=session(c);return id==='add'?void g.addItem(p.item,p.count,{...c,ownerId:p.owner}):id==='remove'?g.removeItem(p.item,p.count,p.owner):id==='equip'?g.equip(p.item,p.owner,c):g.useItem(p.item,{...c,ownerId:p.owner});})),
  operation('sacura.inventory.recipe-read','Прочитать состав сочетания',{recipe:ref('recipe','Сочетание')},{type:'object',properties:{ingredients:{type:'collection',label:'Ингредиенты',items:{type:'object',properties:{item:ref('item','Ингредиент'),count:{type:'integer',min:1,label:'Требуемое количество'}}}},result:ref('item','Результат'),count:{type:'integer',min:1,label:'Количество результата'}}},(p,c)=>{const r=session(c).config.recipes.find(r=>r.id===p.recipe);if(!r)throw Error('Сочетание удалено.');const counts=new Map();for(const x of r.ingredients)counts.set(x.itemId,(counts.get(x.itemId)||0)+x.count);return {ingredients:[...counts].map(([id,count])=>({item:reference('item',id),count})),result:reference('item',r.resultId),count:r.count||1};},{pure:true}),
  operation('sacura.inventory.item-read','Прочитать свойства предмета',{owner:ownerSchema,item:ref('item','Предмет')},{type:'object',properties:{count:{type:'integer',label:'Количество у владельца'},consumable:{type:'boolean',label:'Расходуется при использовании'},heal:{type:'number',label:'Восстановление здоровья'},slot:{type:'string',label:'Слот экипировки'},hasWeapon:{type:'boolean',label:'У предмета есть оружие'},weaponOwnerMatches:{type:'boolean',label:'Оружие принадлежит владельцу'},weapon:ref('weapon','Оружие',{optional:true}),hasUseEvent:{type:'boolean',label:'Есть Event использования'},useEvent:ref('event','Действие предмета',{optional:true})}},(p,c)=>{const g=session(c),i=g.config.items.find(i=>i.id===p.item);if(!i)throw Error('Предмет удалён.');return {count:g.count(p.item,p.owner),consumable:!!i.consumable,heal:i.heal||0,slot:i.slot||'',hasWeapon:!!i.weaponId,weaponOwnerMatches:!i.weaponId||g.config.weapons.some(w=>w.id===i.weaponId&&w.ownerId===p.owner),...(i.weaponId?{weapon:reference('weapon',i.weaponId)}:{}),hasUseEvent:!!i.useEventId,...(i.useEventId?{useEvent:reference('event',i.useEventId)}:{})};},{pure:true}),
  operation('sacura.inventory.equipment-write','Поместить предмет в слот',{owner:ownerSchema,item:ref('item','Предмет')},{type:'void'},(p,c)=>{const g=session(c),i=g.config.items.find(i=>i.id===p.item);if(!i?.slot||!g.count(p.item,p.owner))throw Error('Этот предмет нельзя экипировать.');if(i.weaponId&&!g.config.weapons.some(w=>w.id===i.weaponId&&w.ownerId===p.owner))throw Error('Оружие принадлежит другому владельцу.');g.inventoryFor(p.owner).equipment[i.slot]=i.id;g.sync();}),
  operation('sacura.inventory.store','Добавить предмет в хранилище',{owner:ownerSchema,item:ref('item','Предмет'),count:countSchema,publish:{type:'boolean',default:true,label:'Сообщить о получении'}},{type:'void'},(p,c)=>{session(c).addItem(p.item,p.count,{...c,ownerId:p.owner,publish:p.publish});}),
  operation('sacura.inventory.count','Количество предметов',{owner:ownerSchema,item:ref('item','Предмет')},{type:'integer'},(p,c)=>countInventory(c.project.gameplay,c.gameState,p.item,p.owner),{pure:true}),
  operation('sacura.inventory.combine','Соединить предметы',{owner:ownerSchema,recipe:ref('recipe','Сочетание')},{type:'void'},(p,c)=>session(c).combine(p.recipe,{...c,ownerId:p.owner}))
 ]});
