import {capabilityBinding,capabilityDefinitions} from './capabilityRegistry.js';
import {resolveReference,validateValue,variableKey,schemaFromValue,reference} from './valueSchemas.js';

export function availableCapabilityBindings(project){
 const options=[],owners=[{kind:'project',id:project.id,name:'Проект'},...project.subscenes.map(s=>({kind:'scene',...s})),...project.objects.map(o=>({kind:'object',...o}))];
 const names={hp:'Здоровье',maxHP:'Максимум здоровья',dead:'Погиб',ammo:'Патроны',reserve:'Запас патронов',items:'Предметы',value:'Счётчик'};
 for(const d of capabilityDefinitions())for(const [field,schema]of Object.entries(d.bindings))for(const o of owners){
  const records=capabilityBinding(d.id,field).records?.(project)|| (d.id==='sacura.resource'?project.gameplay.resources:d.id==='sacura.weapon'?[null,...project.gameplay.weapons]:[null]);
  for(const r of records){const b={source:'capability',capability:d.id,owner:reference(o.kind,o.id),field,valueType:schema.type,...(r?{recordId:r.id}:{})};if(!validateCapabilityBinding(project,b).length)options.push({binding:b,name:o.name+' · '+(r?.name||schema.label||names[field]||field)+(r&&d.id==='sacura.weapon'?' · '+names[field]:'')});}
 }
 return options;
}

export function bindingValue(project,game,binding){
 const reader=capabilityBinding(binding?.capability,binding?.field);if(!reader||!game)return undefined;
 const owner=resolveReference(binding.owner,binding.owner.type,{project});
 return reader.read(game,project.gameplay,owner,binding.recordId);
}
export function validateCapabilityBinding(project,binding){
 try{
  const reader=capabilityBinding(binding.capability,binding.field);if(!reader)throw Error('Поле возможности недоступно.');
  if(reader.schema.type!==binding.valueType)throw Error('Тип привязки не совпадает с полем возможности.');
  resolveReference(binding.owner,binding.owner.type,{project});
  if(!['object','scene','project'].includes(binding.owner.type))throw Error('Выберите владельца значения.');
  reader.validate?.(project.gameplay,binding.owner.id,binding.recordId);
  return [];
 }catch(e){return [{message:e.message}];}
}
export function syncCapabilityBindings(session){
 const {project,snapshot}=session.host;
 for(const b of session.config.stateBindings||[]){
  if(validateCapabilityBinding(project,b).length)continue;
  let key;try{key=variableKey(project,resolveReference(b.variable,'variable',{project}));}catch{continue;}
  const value=bindingValue(project,session.state,b),reader=capabilityBinding(b.capability,b.field);
  const declared=project.variableTypes?.[key]||schemaFromValue(project.variables[key]).type;
  const compatible=declared===b.valueType||declared==='number'&&b.valueType==='integer'||declared==='integer'&&b.valueType==='number'&&Number.isInteger(value)||declared==='array'&&b.valueType==='collection';
  if(compatible&&value!==undefined&&!validateValue(reader.schema,value).length)snapshot.variables[key]=structuredClone(value);
 }
}
