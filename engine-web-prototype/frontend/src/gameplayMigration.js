import {normalizeRule} from './ruleModel.js';
import {signalDefinition} from './signalModel.js';
import {reference} from './valueSchemas.js';

// One-way authoring migration. The backup is inert data, excluded from authoring packages.
// Existing native capability maps remain the only definitions; old fields never execute.
export function finalizeGameplayMigration(project,source){
 const g=project.gameplay;if(g.version!==1)return;
 const changes=[],diagnostics=[];
 const forwardSubject=rule=>{const type=rule.signal?.type,field=signalDefinition(project,type)?.payloadSchema?.properties?.objectId;if(!field||field.optional&&!(['death','hit','pickup','collision','interact'].includes(type)||['timer','tick','enter','exit'].includes(type)&&(rule.scope?.kind==='object'||rule.objectId)))return rule;for(const action of [...rule.actions||[],...rule.otherwise||[]])if(action.kind==='function'){const fn=project.functions.find(f=>f.id===action.functionId);if(fn?.parameters?.some(p=>p.name==='objectId'&&p.type==='string')&&!Object.hasOwn(action.args||{},'objectId'))action.args={...action.args,objectId:{op:'get',path:'args.objectId'}};}return rule;};
 g.rules=g.rules.map(raw=>{
  if(raw.version!==undefined&&raw.version!==1){diagnostics.push('Неизвестная версия правила «'+(raw.name||raw.id)+'»: данные сохранены, выполнение недоступно.');return raw;}
  if(raw.version===1)return forwardSubject(raw);
  const rule=normalizeRule(project,raw),trigger=rule.signal.type;
  if(raw.objectId&&trigger!=='start')rule.filter={...rule.filter,[['pickup','timer','tick','enter','exit'].includes(trigger)?'source':'target']:reference('object',raw.objectId)};
  rule.legacy=false;
  for(const key of ['trigger','eventId','functionId','args','operation','nextBeatId','elseEventId','elseFunctionId','elseArgs','elseOperation','elseNextBeatId','join','overrides','actionOverrides'])delete rule[key];
  // Region/timer generation still reads objectId; it is a subject binding, not an action copy.
  if(!['enter','exit','timer','tick'].includes(trigger))delete rule.objectId;
  changes.push('Правило «'+(raw.name||raw.id)+'»: сигнал, условия и единый список действий; ID сохранён.');return forwardSubject(rule);
 });
 if(g.preset!==undefined){if(!['fps','quest','gallery','thirdPerson'].includes(g.preset))diagnostics.push('Неизвестный preset «'+g.preset+'»: проверьте явные камеру и управление. Исходная настройка есть в резервной копии.');changes.push('Preset перенесён в явные камеру, управление и возможности; выбор жанра больше не исполняется.');delete g.preset;}
 // playerId stays as deprecated source metadata for legacy consumers/saves; controller.ownerId owns control.
 if(g.playerId)changes.push('Управление назначено controller.ownerId; playerId сохранён как устаревшая связь, редактор её не использует.');
 if(g.legacyCompatibility==='gameplay-v1')changes.push('Реакции смерти/победы и привязки hp/ammo/kills стали явными правилами и stateBindings. Совместимые поля прохождения — только вычисляемые представления.');
 for(const [id,a]of Object.entries(g.actors))if(a.ai&&!a.ai.targetId)diagnostics.push('Поведение «'+id+'» не имеет цели: выберите объект в редакторе.');
 for(const w of g.weapons)if(!w.ownerId)diagnostics.push('Оружие «'+w.name+'» не имеет владельца: выберите объект в редакторе.');
 const known=new Set(['version','behaviorSchemaVersion','capabilitySchemaVersion','resources','inventories','movements','stateBindings','mechanicSchemaVersion','mechanicDefinitions','signals','variableBindings','enabled','activation','camera','cameraSettings','mechanics','playerId','controller','actors','weapons','items','recipes','rules','inventory','hudWidgetId','crosshair','storyPolicy','ruleSettings','legacyCompatibility','legacyOwnerId','legacyInventoryOwnerId','legacyCounterId','legacyEnemyIds','compatibilityRules','compatibilityBindings']);
 for(const key of Object.keys(g))if(!known.has(key))diagnostics.push('Настройка «'+key+'» сохранена; автоматическое преобразование не определено.');
 g.version=2;
 if(!project.gameplayMigration)project.gameplayMigration={version:1,from:1,to:2,changes,diagnostics,source:source||{...structuredClone(project),gameplay:{...structuredClone(g),version:1}}};
 return project;
}
