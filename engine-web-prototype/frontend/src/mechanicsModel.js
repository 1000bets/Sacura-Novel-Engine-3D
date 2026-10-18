import {capabilityDataDefinitions,capabilityDataDefinition} from './capabilityRegistry.js';
import {schemaDefaults} from './valueSchemas.js';
import {addMechanicInstance,descriptionFor} from './mechanicDescriptions.js';
import {createRule} from './ruleModel.js';
import {inventoryConfig} from './capabilities/inventory.js';
import {uid, makeAction} from './model.js';
import {newItem, newWeapon, ensureGameplay} from './gameplayModel.js';

// Trusted engine registrations only. Imported authoring packages contain data, never installers.
const definitions = new Map();
export function registerMechanic(definition) {
  if (!definition?.id || definitions.has(definition.id) || typeof definition.add !== 'function') throw Error('Некорректная регистрация механики.');
  definitions.set(definition.id, Object.freeze(definition));
}
export const mechanicDefinitions = project => [...definitions.values(),...capabilityDataDefinitions(),...(project?.gameplay?.mechanicDefinitions||[]).map(d=>({...d,id:d.version===1?d.id:d.id+'@'+d.version,name:d.version===1?d.name:d.name+' · v'+d.version,description:d.description||'Редактируемая функция и параметры.'}))];
export const mechanicDefinition = (id,project,version=1) => definitions.get(id)||capabilityDataDefinition(id)||descriptionFor(project||{},id.split('@')[0],id.includes('@')?Number(id.split('@')[1]):version);
export function mechanicsFor(project) {
  const g=project.gameplay, records=[...(g.mechanics||[])];
  const add=(type,recordId,objectId='')=>{if(!records.some(m=>m.type===type&&m.recordId===recordId))records.push({id:`native:${type}:${recordId}`,type,recordId,scope:objectId?{kind:'object',id:objectId}:{kind:'project',id:project.id},native:true});};
  if(g.controller.ownerId&&g.controller.mode!=='none')add('movement',g.controller.ownerId,g.controller.ownerId);
  if(g.camera?.mode!=='authored')add('camera','camera',g.camera?.targetId||'');
  for(const id of Object.keys(g.actors))if(g.actors[id].maxHP!=null)add('health',id,id);
  for(const id of Object.keys(g.actors))if(g.actors[id].ai)add('pursuit',id,id);
  for(const w of g.weapons)add('weapon',w.id,w.ownerId);
  for(const inv of [g.inventory,...g.inventories||[]].filter(i=>i.enabled)){if(!records.some(m=>m.type==='inventory'&&m.scope.id===inv.owner.id))records.push({id:'native:inventory:'+inv.owner.id,type:'inventory',scope:structuredClone(inv.owner),recordId:inv.owner.id,native:true});}
  if(g.crosshair?.enabled)add('crosshair','crosshair');
  for(const r of g.resources||[])if(!records.some(m=>m.type==='resource'&&m.recordId===r.id))records.push({id:'native:resource:'+r.id,type:'resource',recordId:r.id,scope:structuredClone(r.owner),native:true});
  for(const i of g.items)add('pickup',i.id,i.worldObjectId);
  for(const r of g.rules)if(!records.some(m=>m.ruleId===r.id))records.push({id:'native:rule:'+r.id,type:'rule',ruleId:r.id,scope:r.scope|| (r.objectId?{kind:'object',id:r.objectId}:r.sceneId?{kind:'scene',id:r.sceneId}:{kind:'project',id:project.id}),native:true});
  return records;
}
export function addMechanic(project,type,scope) {
  const [descriptionId,descriptionVersion]=type.split('@');if(descriptionFor(project,descriptionId,Number(descriptionVersion||1)))return addMechanicInstance(project,descriptionId,scope,{},Number(descriptionVersion||1));
  const data=capabilityDataDefinitions().find(d=>d.id===type);if(data){if(!data.scopes.includes(scope.kind))throw Error('Выберите подходящее размещение.');const m={id:uid('mechanic'),type:'capability',name:data.name,capabilityId:data.id,capabilityVersion:data.version,scope:structuredClone(scope),values:schemaDefaults(data.parameters)};project.gameplay.mechanics.push(m);project.gameplay.enabled=true;return m;}
  const def=definitions.get(type);if(!def)throw Error('Эта механика не поддерживается.');
  if(!def.scopes.includes(scope.kind))throw Error('Выберите подходящий объект, сцену или проект.');
  if(scope.kind==='object'&&!project.objects.some(o=>o.id===scope.id))throw Error('Сначала выберите объект.');
  if(scope.kind==='scene'&&!project.subscenes.some(s=>s.id===scope.id))throw Error('Сначала выберите сцену.');
  ensureGameplay(project);const g=project.gameplay;
  if(type==='movement'&&g.controller.ownerId&&g.controller.mode!=='none')throw Error('Управление уже подключено. Измените существующую механику.');
  if(type==='inventory'&&inventoryConfig(g,scope.id)?.enabled)throw Error('Инвентарь уже подключён. Измените его параметры.');
  if(type==='crosshair'&&g.crosshair?.enabled)throw Error('Прицел уже подключён.');
  if(type==='camera'&&(g.mechanics?.some(m=>m.type==='camera')||g.camera.mode!=='authored'))throw Error('Камера уже подключена. Измените существующую механику.');
  if(type==='health'&&g.actors[scope.id]?.maxHP!=null||type==='pursuit'&&g.actors[scope.id]?.ai||type==='physics'&&project.objects.find(o=>o.id===scope.id)?.physics)throw Error('Эта возможность уже подключена к объекту.');
  const m={id:uid('mechanic'),type,scope:structuredClone(scope)};
  // Installers create ordinary native definitions; they never execute authoring logic.
  def.add(project,m);project.gameplay.mechanics??=[];project.gameplay.mechanics.push(m);project.gameplay.enabled=true;
  return m;
}
export function bindDoorKey(project,m,objectId) {
  let item=project.gameplay.items.find(i=>i.worldObjectId===objectId);
  if(!item&&objectId){const pickup=addMechanic(project,'pickup',{kind:'object',id:objectId});item=project.gameplay.items.find(i=>i.id===pickup.recordId);}
  project.gameplay.rules.find(r=>r.id===m.ruleId).conditionArgs={key:item?.id||''};
}
function connectInput(p,behaviors,m) {
  const controls={look:['Смотреть','axis2d','pointer','delta'],shoot:['Стрелять','boolean','pointer','primary'],reload:['Перезарядить','boolean','keyboard','KeyR'],jump:['Прыгнуть','boolean','keyboard','Space']};
  let context=p.input.contexts.find(c=>c.id==='game-mechanics');
  if(!context){context={id:'game-mechanics',name:'Действия механик',priority:40,bindings:[]};p.input.contexts.push(context);}
  for(const behavior of behaviors){const [name,valueType,device,control]=controls[behavior];const operationId=({look:'sacura.camera.look',shoot:'sacura.weapon.shoot',reload:'sacura.weapon.reload',jump:'sacura.movement.jump'})[behavior];let a=p.input.actions.find(a=>a.mechanicId===m.id&&a.operation?.id===operationId);if(!a){a={id:uid('input-action'),name,behavior:'operation',mechanicId:m.id,operation:{id:operationId,version:1,parameters:behavior==='look'?{delta:{op:'get',path:'args.value'},device:'pointer'}:{owner:{kind:'reference',type:'object',mode:'direct',id:m.scope.id},...(behavior==='shoot'||behavior==='reload'?{weapon:{kind:'reference',type:'weapon',mode:'direct',id:m.recordId}}:{})}},valueType,trigger:valueType==='axis2d'?'continuous':'pressed',holdSeconds:.4,consume:true};p.input.actions.push(a);}if(!context.bindings.some(b=>b.actionId===a.id))context.bindings.push({id:uid('binding'),actionId:a.id,device,control,scale:valueType==='axis2d'?[1,1]:1,deadZone:0});}
}
export function removeMechanic(project,m) {
  const g=project.gameplay;
  if(m.type==='movement'){g.controller.mode='none';}
  else if(m.type==='inventory'){if(g.inventory.owner.id===m.scope.id)g.inventory.enabled=false;else g.inventories=g.inventories.filter(i=>i.owner.id!==m.scope.id);}
  else if(m.type==='resource')g.resources=g.resources.filter(r=>r.id!==m.recordId);
  else if(m.type==='crosshair')g.crosshair.enabled=false;
  else if(m.type==='camera'){g.camera={...g.camera,mode:'authored',pointerLock:false};}
  else if(m.type==='health'){const actor=g.actors[m.recordId];if(actor?.ai)delete actor.maxHP;else delete g.actors[m.recordId];}
  else if(m.type==='pursuit'){if(g.actors[m.recordId]){if(g.actors[m.recordId].maxHP!=null)delete g.actors[m.recordId].ai;else delete g.actors[m.recordId];}}
  else if(m.type==='weapon')g.weapons=g.weapons.filter(x=>x.id!==m.recordId);
  else if(m.type==='pickup')g.items=g.items.filter(x=>x.id!==m.recordId);
  else if(m.type==='physics'){delete project.objects.find(o=>o.id===m.scope.id)?.physics;}
  if(m.ruleId)g.rules=g.rules.filter(x=>x.id!==m.ruleId);
  g.mechanics=(g.mechanics||[]).filter(x=>x.id!==m.id);
  const actionIds=pInputIds(project,m.id);project.input.actions=project.input.actions.filter(a=>!actionIds.has(a.id));for(const c of project.input.contexts)c.bindings=c.bindings.filter(b=>!actionIds.has(b.actionId));
  // Authored events/functions remain in their libraries: they may be used elsewhere.
}
const pInputIds=(p,id)=>new Set(p.input.actions.filter(a=>a.mechanicId===id).map(a=>a.id));
export function connectMechanicInput(p,m){connectInput(p,m.type==='weapon'?['shoot','reload']:m.type==='camera'?['look']:['jump'],m);}
function ruleFor(p,m,signal){const r=createRule({signal,scope:structuredClone(m.scope),once:true,name:definitions.get(m.type).name,filter:m.scope.kind==='object'?{[signal==='interact'?'target':'source']:{kind:'reference',type:'object',mode:'context',role:'self'}}:{}});p.gameplay.rules.push(r);m.ruleId=r.id;return r;}
function eventFor(p,m,name,actions){const e={id:uid('event'),name,description:'Редактируемая механика',owner:m.scope.kind==='project'?'GameSession':'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:uid('group'),name:'Сделать',actions}]};p.events.push(e);m.eventId=e.id;return e;}
const definition=(id,name,scopes,description,add)=>registerMechanic({id,name,scopes,description,add});
definition('rule','Своя реакция',['object','scene','project'],'Когда произойдёт сигнал, запустить событие или функцию.',(p,m)=>{const r=createRule({signal:'start',scope:structuredClone(m.scope)});p.gameplay.rules.push(r);m.ruleId=r.id;});
definition('timer','Таймер',['object','scene','project'],'Через заданное время выполнить действие.',(p,m)=>{const r=ruleFor(p,m,'timer');r.seconds=2;});
definition('interaction','Осмотр предмета',['object'],'Щелчок по объекту запускает обычное событие.',(p,m)=>{const r=ruleFor(p,m,'interact');r.once=false;const e=eventFor(p,m,'Осмотреть '+p.objects.find(o=>o.id===m.scope.id).name,[makeAction('highlight',m.scope.id,'Подсветить')]);r.actions=[{kind:'event',eventId:e.id,join:'FLOW_END'}];});
definition('door','Дверь с ключом',['object'],'Выберите ключ. Условие и действия доступны для редактирования.',(p,m)=>{
  const open=eventFor(p,m,'Открыть дверь',[makeAction('door',m.scope.id,'Открыть')]);
  const check=uid('node');const fn={id:uid('function'),name:'Можно открыть дверь',parameters:[{name:'key',label:'Ключ',type:'string',referenceType:'item',default:''}],locals:{},entry:check,nodes:[{id:check,kind:'return',name:'Ключ получен',value:{op:'gt',a:{op:'itemCount',itemId:{op:'get',path:'args.key'}},b:0},position:{x:0,y:0}}]};
  p.functions.push(fn);const r=ruleFor(p,m,'interact');r.once=false;r.conditionFunctionId=fn.id;r.conditionArgs={key:''};r.actions=[{kind:'event',eventId:open.id,join:'FLOW_END'}];m.functionId=fn.id;
});
definition('pickup','Предмет для подбора',['object'],'Добавить предмет в инвентарь по щелчку.',(p,m)=>{const item={...newItem(),name:p.objects.find(o=>o.id===m.scope.id).name,worldObjectId:m.scope.id,inventoryOwner:structuredClone(p.gameplay.inventory.owner)};p.gameplay.inventory.enabled=true;p.gameplay.items.push(item);m.recordId=item.id;});
definition('movement','Управление объектом',['object'],'Ходьба по щелчку или клавишами, без обязательного здоровья.',(p,m)=>{p.gameplay.controller.ownerId=m.scope.id;p.gameplay.controller.mode='point-click';m.recordId=m.scope.id;});
definition('camera','Камера',['object','scene','project'],'Независимая камера: авторская, от первого или третьего лица.',(p,m)=>{p.gameplay.camera={...p.gameplay.camera,mode:m.scope.kind==='object'?'thirdPerson':'authored',targetId:m.scope.kind==='object'?m.scope.id:'',pointerLock:false,crosshair:false};m.recordId='camera';connectInput(p,['look'],m);});
definition('health','Здоровье',['object'],'Добавить здоровье только выбранному объекту.',(p,m)=>{const old=p.gameplay.actors[m.scope.id]||{};p.gameplay.actors[m.scope.id]={...old,maxHP:old.maxHP??100,team:old.team||'neutral'};m.recordId=m.scope.id;});
definition('weapon','Оружие',['object'],'Стрельба подключается отдельно от камеры и движения.',(p,m)=>{const w={...newWeapon(),ownerId:m.scope.id};p.gameplay.weapons.push(w);m.recordId=w.id;});
definition('pursuit','Преследование',['object'],'Следовать за выбранной целью и атаковать в пределах дистанции.',(p,m)=>{p.gameplay.actors[m.scope.id]??={team:'neutral'};p.gameplay.actors[m.scope.id].ai={targetId:'',speed:1.5,range:1.8,damage:10,interval:2,detection:20};m.recordId=m.scope.id;});
definition('physics','Динамическое тело',['object'],'Гравитация и столкновения в обычной физической системе.',(p,m)=>{const o=p.objects.find(o=>o.id===m.scope.id);o.physics={mode:'dynamic',gravity:16,bounce:.2};o.collision??={enabled:true,custom:true,size:[.6,.6,.6],offset:[0,0,0]};});

definition('inventory','Инвентарь',['object','scene','project'],'Хранение предметов у выбранного владельца, без оружия и здоровья.',(p,m)=>{let inv=inventoryConfig(p.gameplay,m.scope.id);if(!inv){inv={owner:structuredClone(m.scope),slots:12,starting:[],equipment:{}};p.gameplay.inventories.push(inv);}inv.enabled=true;m.recordId=m.scope.id;});
definition('resource','Счётчик',['object','scene','project'],'Независимый целочисленный ресурс: очки, монеты или число найденных предметов.',(p,m)=>{const r={id:uid('resource'),name:'Счётчик',type:'integer',initial:0,owner:structuredClone(m.scope)};p.gameplay.resources.push(r);m.recordId=r.id;});
definition('crosshair','Прицел',['project'],'Независимый прицел; подключается к любой камере.',(p,m)=>{p.gameplay.crosshair={enabled:true};m.recordId='crosshair';});
