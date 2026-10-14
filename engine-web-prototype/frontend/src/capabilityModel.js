import {validateCapabilityData} from './capabilityRegistry.js';
import {validateCapabilityBinding} from './capabilityBindings.js';
import {inventoryConfig} from './capabilities/inventory.js';
import {ownerExists} from './ruleModel.js';
import {resolveReference,variableKey,schemaFromValue} from './valueSchemas.js';
export function validateCapabilities(project){
 const g=project.gameplay,issues=[],bad=detail=>issues.push({id:'capability-'+issues.length,level:'error',title:'Проверьте игровую возможность',detail});
 if(g.capabilitySchemaVersion!==1){if(g.capabilitySchemaVersion!==undefined)bad('Версия возможностей недоступна. Данные сохранены.');return issues;}
 if(['inventories','resources','stateBindings','movements'].some(k=>!Array.isArray(g[k]))){bad('Повреждены определения возможностей.');return issues;}
 if(!g.cameraSettings||['eyeHeight','sensitivity','distance','fov'].some(k=>!Number.isFinite(g.cameraSettings[k])||g.cameraSettings[k]<0)||g.cameraSettings.fov<1||g.cameraSettings.fov>179)bad('Проверьте параметры камеры.');
 const owners=new Set();for(const inv of [g.inventory,...g.inventories]){
  if(!inv?.owner){bad('Выберите владельца инвентаря.');continue;}
  if(inv!==g.inventory||inv.enabled||inv.starting?.length){if(!ownerExists(project,inv.owner))bad('Владелец инвентаря удалён.');}
  const key=inv.owner.kind+':'+inv.owner.id;if(owners.has(key))bad('Владельцу назначено несколько инвентарей.');owners.add(key);
  if(!Number.isInteger(inv.slots)||inv.slots<1||inv.slots>200||!Array.isArray(inv.starting)||!inv.equipment||typeof inv.equipment!=='object')bad('Проверьте слоты и начальные предметы инвентаря.');
  else{let slots=0;for(const x of inv.starting){const item=g.items.find(i=>i.id===x.itemId);if(!item||!Number.isInteger(x.count)||x.count<1)bad('Начальный предмет или количество недоступны.');else slots+=Math.ceil(x.count/item.stackLimit);}if(slots>inv.slots)bad('Начальные предметы не помещаются в инвентарь.');for(const [slot,id]of Object.entries(inv.equipment))if(!g.items.some(i=>i.id===id&&i.slot===slot)||!inv.starting.some(x=>x.itemId===id))bad('Начальная экипировка отсутствует в инвентаре.');}
 }
 for(const w of g.weapons)if(!w.ownerId||!project.objects.some(o=>o.id===w.ownerId))bad('Выберите владельца оружия.');
 for(const i of g.items)if(i.inventoryOwner&&!inventoryConfig(g,i.inventoryOwner.id))bad('Выберите подключённый инвентарь получателя.');
 if(g.controller.ownerId&&!project.objects.some(o=>o.id===g.controller.ownerId))bad('Владелец управления удалён.');
 const movementOwners=new Set(g.controller.ownerId?[g.controller.ownerId]:[]);for(const m of g.movements){if(!m?.ownerId||movementOwners.has(m.ownerId)||!project.objects.some(o=>o.id===m.ownerId))bad('Укажите уникального владельца движения.');movementOwners.add(m?.ownerId);if(!['none','wasd','point-click','both'].includes(m?.mode)||['speed','jumpSpeed','gravity','eyeHeight'].some(k=>!Number.isFinite(m[k])||m[k]<0))bad('Проверьте параметры движения объекта.');}
 const resources=new Set();for(const r of g.resources){if(!r?.id||resources.has(r.id)||r.type!=='integer'||!Number.isInteger(r.initial)||!r.owner||!ownerExists(project,r.owner))bad('Счётчику нужны уникальный ID, владелец и целое начальное значение.');resources.add(r?.id);}
 const ids=new Set();for(const b of g.stateBindings){if(!b?.id||ids.has(b.id))bad('Повторяющаяся привязка состояния.');ids.add(b?.id);for(const e of validateCapabilityBinding(project,b))bad(e.message);
  try{const key=variableKey(project,resolveReference(b.variable,'variable',{project})),type=project.variableTypes?.[key]||schemaFromValue(project.variables[key]).type;if(!(type===b.valueType||type==='number'&&b.valueType==='integer'||type==='integer'&&b.valueType==='number'||type==='array'&&b.valueType==='collection'))bad('Тип переменной истории не совпадает с привязкой.');}catch(e){bad(e.message);}
 }
 for(const e of validateCapabilityData(project))bad(e);return issues;
}
