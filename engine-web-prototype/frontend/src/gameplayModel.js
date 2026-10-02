import {uid} from './model.js';
import {defaultInputSettings} from './inputModel.js';
export const GAME_PRESETS={quest:'Квест',gallery:'Тир',fps:'От первого лица',thirdPerson:'От третьего лица'};
export const GAME_COMMANDS={goTo:'Перейти к ноде истории',damage:'Нанести урон',heal:'Лечить',shoot:'Выстрелить',reload:'Перезарядить',jump:'Прыгнуть',impulse:'Толкнуть объект',addItem:'Добавить предмет',removeItem:'Убрать предмет',equip:'Экипировать',useItem:'Использовать предмет',combine:'Соединить предметы',startTimer:'Запустить таймер',stopTimer:'Остановить таймер',callFunction:'Вызвать функцию',save:'Сохранить прохождение',load:'Загрузить прохождение'};
export const GAME_TRIGGERS={start:'Начало игры',hit:'Попадание',death:'Смерть',victory:'Все противники побеждены',pickup:'Получен предмет',enter:'Вход в область',exit:'Выход из области',collision:'Столкновение',timer:'Таймер',tick:'Периодически'};
export function defaultGameplay(){return {version:1,enabled:false,preset:'quest',playerId:'',controller:{mode:'point-click',speed:3.5,jumpSpeed:5,gravity:16,eyeHeight:1.5,sensitivity:.003,distance:3,fov:70},actors:{},weapons:[],items:[],recipes:[],rules:[],inventory:{slots:12,starting:[],equipment:{}},hudWidgetId:null};}
export function ensureGameplay(p){p.gameplay??=defaultGameplay();p.functions??=[];return p;}
export function newWeapon(){return {id:uid('weapon'),name:'Пистолет',mode:'hitscan',damage:25,range:40,magazine:6,reserve:30,cooldown:.3,reloadTime:1,projectileSpeed:20,recoil:.025};}
export function newItem(){return {id:uid('item'),name:'Новый предмет',stackLimit:10,slot:'',weaponId:'',heal:0,consumable:false,useEventId:'',worldObjectId:''};}
export function newRule(){return {id:uid('rule'),name:'Новое правило',trigger:'enter',sceneId:'',objectId:'',eventId:'',functionId:'',nextBeatId:'',seconds:2,repeat:false,once:true,enabled:true,center:[0,0,0],size:[3,2,3]};}
export function applyGamePreset(p,preset,playerId){
 ensureGameplay(p);const g=p.gameplay;g.enabled=true;g.preset=preset;g.playerId=playerId;
 g.controller={...defaultGameplay().controller,mode:preset==='quest'?'point-click':preset==='gallery'?'none':'wasd'};
 if(playerId)g.actors[playerId]??={team:'player',maxHP:100};
 if(preset!=='quest'&&!g.weapons.length)g.weapons.push(newWeapon());
 p.variables??={};p.variableTypes??={};for(const [id,value,type]of [['hp',100,'number'],['maxHp',100,'number'],['ammo',6,'number'],['reserve',30,'number'],['kills',0,'integer'],['inventory',[],'array']]){if(!Object.hasOwn(p.variables,id))p.variables[id]=value;p.variableTypes[id]??=type;}
 p.input??=defaultInputSettings();
 const defs=[['fire','Стрелять','shoot'],['reload','Перезарядить','reload'],['jump','Прыгнуть','jump'],['look','Смотреть','look'],['saveGame','Сохранить прохождение','saveGame'],['loadGame','Загрузить прохождение','loadGame']];
 for(const [id,name,behavior]of defs)if(!p.input.actions.some(a=>a.id===id))p.input.actions.push({id,name,behavior,valueType:id==='look'?'axis2d':'boolean',trigger:id==='look'?'continuous':'pressed',holdSeconds:.4,consume:true});
 const bindings=[['fire','pointer','primary'],['reload','keyboard','KeyR'],['jump','keyboard','Space'],['saveGame','keyboard','F5'],['loadGame','keyboard','F9'],['look','pointer','delta',[1,1]],['look','gamepad-axis','2',[1,0]],['look','gamepad-axis','3',[0,1]],['fire','gamepad-button','7'],['reload','gamepad-button','2'],['jump','gamepad-button','0']].map(([actionId,device,control,scale=1],i)=>({id:'game-action-'+i,actionId,device,control,scale,deadZone:device==='gamepad-axis'?.18:0}));
 const context={id:'game-mechanics',name:'Игровые механики',priority:40,bindings:preset==='quest'?bindings.filter(b=>['saveGame','loadGame'].includes(b.actionId)):bindings};
 p.input.contexts=p.input.contexts.filter(c=>c.id!==context.id);p.input.contexts.push(context);
 return g;
}
export function validateGameplay(p){
 const g=p.gameplay,issues=[];if(!g)return issues;
 const bad=(id,detail)=>issues.push({id:'game-'+id,level:'error',title:'Проверьте игровые механики',detail});
 if(g.version!==1||!g.controller||!g.actors||Array.isArray(g.actors)||!g.inventory||!Array.isArray(g.inventory.starting)||!g.inventory.equipment||['weapons','items','recipes','rules'].some(k=>Array.isArray(g[k])&&g[k].some(x=>!x||typeof x!=='object'))||!['weapons','items','recipes','rules'].every(k=>Array.isArray(g[k]))){bad('shape','Повреждены настройки игровых механик.');return issues;}
 if(g.enabled&&(!p.objects.some(o=>o.id===g.playerId&&o.type==='Персонаж')||!GAME_PRESETS[g.preset]))bad('player','Выберите персонажа и готовый режим управления.');
 for(const key of ['weapons','items','recipes','rules']){const ids=new Set();for(const x of g[key]){if(!x?.id||ids.has(x.id))bad(key,'Уникальные ID обязательны.');ids.add(x?.id);}}
 const nonnegative=(x,keys)=>keys.every(k=>Number.isFinite(x[k])&&x[k]>=0);
 if(g.enabled&&!g.actors[g.playerId])bad('health','Задайте здоровье главного героя.');
 if(!['none','wasd','point-click','both'].includes(g.controller.mode)||g.controller.fov<1||g.controller.fov>179)bad('controller-mode','Проверьте движение и поле зрения.');
 if(!nonnegative(g.controller,['speed','jumpSpeed','gravity','eyeHeight','sensitivity','distance','fov']))bad('controller','Укажите неотрицательные параметры контроллера.');
 if(!Number.isInteger(g.inventory.slots)||g.inventory.slots<1||g.inventory.slots>200)bad('slots','Количество слотов: от 1 до 200.');
 for(const [id,a]of Object.entries(g.actors)){if(!a||typeof a!=='object'){bad(id,'Повреждены параметры здоровья.');continue;}if(!p.objects.some(o=>o.id===id))bad(id,'Игровой объект удалён.');if(!Number.isFinite(a.maxHP)||a.maxHP<=0)bad(id+'-hp','Здоровье должно быть больше нуля.');if(a.ai&&!nonnegative(a.ai,['speed','range','damage','interval','detection']))bad(id+'-ai','Проверьте параметры поведения противника.');}
 for(const w of g.weapons)if(!['hitscan','projectile'].includes(w.mode)||!nonnegative(w,['damage','range','reserve','cooldown','reloadTime','projectileSpeed','recoil'])||!Number.isInteger(w.magazine)||w.magazine<1||!Number.isInteger(w.reserve))bad(w.id,'Проверьте параметры оружия.');
 for(const i of g.items){if(!Number.isInteger(i.stackLimit)||i.stackLimit<1||!Number.isFinite(i.heal)||i.heal<0)bad(i.id,'Проверьте количество и лечение предмета.');if(i.weaponId&&!g.weapons.some(w=>w.id===i.weaponId))bad(i.id,'Оружие предмета удалено.');if(i.pickupEventId&&!p.events.some(e=>e.id===i.pickupEventId))bad(i.id,'Событие подбора предмета удалено.');if(i.worldObjectId&&!p.objects.some(o=>o.id===i.worldObjectId))bad(i.id,'Объект подбора предмета удалён.');if(i.useEventId&&!p.events.some(e=>e.id===i.useEventId))bad(i.id,'Событие использования предмета удалено.');}
 for(const r of g.recipes)if(!Array.isArray(r.ingredients)||!r.ingredients.length||r.ingredients.some(x=>!x)||!r.ingredients.every(x=>g.items.some(i=>i.id===x.itemId)&&Number.isInteger(x.count)&&x.count>0)||!g.items.some(i=>i.id===r.resultId)||(r.count!==undefined&&(!Number.isInteger(r.count)||r.count<1)))bad(r.id,'Выберите существующие ингредиенты и результат.');
 for(const r of g.rules){if(r.sceneId&&!p.subscenes.some(s=>s.id===r.sceneId))bad(r.id,'Сцена правила удалена.');if(r.objectId&&!p.objects.some(o=>o.id===r.objectId))bad(r.id,'Объект правила удалён.');if(!GAME_TRIGGERS[r.trigger]||!Number.isFinite(r.seconds)||r.seconds<0)bad(r.id,'Проверьте тип и время правила.');if(r.eventId&&!p.events.some(e=>e.id===r.eventId))bad(r.id,'Событие правила удалено.');if(r.functionId&&!p.functions?.some(f=>f.id===r.functionId))bad(r.id,'Функция правила удалена.');if(r.nextBeatId&&!p.chapters.flatMap(c=>c.beats).some(b=>b.id===r.nextBeatId))bad(r.id,'Продолжение правила удалено.');if(['enter','exit'].includes(r.trigger)&&(r.center?.length!==3||!r.center?.every(Number.isFinite)||r.size?.length!==3||!r.size?.every(v=>Number.isFinite(v)&&v>0)))bad(r.id,'Размер области должен быть больше нуля.');}
 for(const x of g.inventory.starting){const item=g.items.find(i=>i.id===x?.itemId);if(!item||!Number.isInteger(x.count)||x.count<1)bad('starting','Проверьте начальные предметы.');}
 const slots=g.inventory.starting.reduce((sum,x)=>{const item=g.items.find(i=>i.id===x?.itemId);return sum+(item?Math.ceil(x.count/item.stackLimit):0);},0);if(slots>g.inventory.slots)bad('starting-space','Начальные предметы не помещаются в инвентарь.');
 for(const [slot,id]of Object.entries(g.inventory.equipment))if(!g.items.some(i=>i.id===id&&i.slot===slot)||!g.inventory.starting.some(x=>x.itemId===id))bad('equipment','Начальную экипировку нужно добавить в инвентарь.');
 for(const o of p.objects)if(o.physics&&(!['static','dynamic'].includes(o.physics.mode)||!Number.isFinite(o.physics.gravity)||o.physics.gravity<0||!Number.isFinite(o.physics.bounce)||o.physics.bounce<0||o.physics.bounce>1))bad(o.id,'Проверьте гравитацию и отскок объекта.');
 return issues;
}
