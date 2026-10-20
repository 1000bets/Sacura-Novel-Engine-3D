import {ensureGameplay} from './gameplayModel.js';
import {descriptionFor} from './mechanicDescriptions.js';
import {variableType} from './variableModel.js';
import {ownerExists} from './ruleModel.js';
import {capabilityDefinitions} from './capabilityRegistry.js';
import {operationDefinition} from './operationRegistry.js';

// Compatibility token, not a security checksum. Authoring changes require a new run;
// a title, graph coordinates or node comments do not change execution.
export function saveCompatibility(project){
 const source={...project};delete source.gameplayMigration;checkSavedData(source);const p=structuredClone(source);ensureGameplay(p);
 const content=Object.fromEntries(['gameplay','input','events','functions','logicResources','objects','subscenes','chapters','variables','variableTypes','widgets','ui'].map(key=>[key,p[key]]));
 content.engineModules=capabilityDefinitions().map(({id,version})=>({id,version}));
 const contracts=new Map(),collect=value=>{if(!value||typeof value!=='object')return;const id=value.command||(value.parameters&&value.version?value.id:null),version=value.operationVersion??value.version??1,d=operationDefinition(id,version);if(d)contracts.set(id+'@'+version,{id:d.id,version:d.version,schemaVersion:d.schemaVersion,parameters:d.parameters,result:d.result,contexts:d.contexts,pure:!!d.pure});for(const child of Object.values(value))collect(child);};collect(content);
 content.operationContracts=[...contracts.values()].sort((a,b)=>a.id.localeCompare(b.id)||a.version-b.version);
 for(const f of content.functions||[]){delete f.viewport;for(const n of f.nodes||[]){delete n.position;delete n.comment;}}
 for(const r of content.logicResources||[]){for(const key of ['name','path','status','approved','error','locations','fingerprint'])delete r[key];for(const n of r.nodes||[]){delete n.position;delete n.comment;}}
 const canonical=value=>Array.isArray(value)?value.map(canonical):value&&typeof value==='object'?Object.fromEntries(Object.keys(value).sort().filter(k=>value[k]!==undefined).map(k=>[k,canonical(value[k])])):value;
 const text=JSON.stringify(canonical(content));let a=2166136261,b=5381;for(let i=0;i<text.length;i++){a=Math.imul(a^text.charCodeAt(i),16777619);b=Math.imul(b,33)^text.charCodeAt(i);}
 return {version:1,fingerprint:text.length+':'+(a>>>0).toString(16)+':'+(b>>>0).toString(16)};
}

export function checkSavedData(value,depth=0){
 if(depth>80||typeof value==='number'&&!Number.isFinite(value)||['function','symbol','bigint'].includes(typeof value))throw Error('Сохранение содержит неподдержанные данные.');
 if(value&&typeof value==='object'){
  if(!Array.isArray(value)&&Object.getPrototypeOf(value)!==Object.prototype&&Object.getPrototypeOf(value)!==null)throw Error('Сохранение содержит несериализуемые данные. Дождитесь завершения выполнения.');
  for(const [key,child]of Object.entries(value)){if(['__proto__','constructor','prototype'].includes(key))throw Error('Сохранение повреждено.');checkSavedData(child,depth+1);}
 }
}
export function validateSavedLifecycle(project,s){
 if(!s.variables||Array.isArray(s.variables)||!project.subscenes.some(scene=>scene.id===s.world.location))throw Error('Сохранение переменных или сцены повреждено.');
 for(const key of Object.keys(project.variables)){const value=s.variables[key],type=variableType(project,key);if(type==='integer'?!Number.isInteger(value):type==='number'?!Number.isFinite(value):type==='array'?!Array.isArray(value):type==='struct'?!value||typeof value!=='object'||Array.isArray(value):typeof value!==type)throw Error('Тип переменной «'+key+'» в сохранении изменился.');}
 if(s.version>=2){
  for(const [id,i]of Object.entries(s.retainedInstances||{}))if(!i||i.id!==id||!project.events.some(e=>e.id===i.eventId)||!project.subscenes.some(scene=>scene.id===i.sceneId)||!(i.status==='held'||i.status==='paused'&&i.resumeStatus==='held')||!Array.isArray(i.audioKeys)||i.audioKeys.length||i.operationContext?.reactionId)throw Error('Удерживаемое событие в сохранении повреждено.');
  for(const e of Object.values(s.effects||{}))if(!e||!['held','paused','stopped'].includes(e.status)||e.audioKey||e.eventId&&!project.events.some(event=>event.id===e.eventId))throw Error('Эффект в сохранении повреждён.');
 }
 const g=s.world.game;if(!g)return;
 if(!Number.isFinite(g.clock)||g.clock<0)throw Error('Игровое время в сохранении повреждено.');
 const ruleIds=new Set((project.gameplay.rules||[]).map(r=>r.id));
 for(const m of project.gameplay.mechanics||[]){const d=descriptionFor(project,m.definitionId,m.definitionVersion);for(const r of [...d?.rules||[],...d?.machine?.transitions||[]])ruleIds.add(m.id+':'+r.id);if(d?.machine)ruleIds.add(m.id+':initial-enter');}
 for(const [id,r]of Object.entries(g.rules||{})){if(!ruleIds.has(id)||!r||typeof r!=='object'||['fired','inside'].some(k=>r[k]!==undefined&&typeof r[k]!=='boolean')||['elapsed','cooldownUntil'].some(k=>r[k]!==undefined&&(!Number.isFinite(r[k])||r[k]<0)))throw Error('Состояние правила в сохранении повреждено.');}
 for(const t of Object.values(g.timers||{}))if(!t||!Number.isFinite(t.remaining)||t.remaining<0||!Number.isFinite(t.seconds)||t.seconds<0||typeof t.repeat!=='boolean'||t.owner&&!ownerExists(project,t.owner,t.owner.kind==='project'?undefined:t.sceneId,t.mechanicInstanceId)||t.sceneId&&!project.subscenes.some(scene=>scene.id===t.sceneId))throw Error('Таймер в сохранении повреждён.');
 for(const a of Object.values(g.actors||{}))if(typeof a.dead!=='boolean')throw Error('Состояние здоровья в сохранении повреждено.');
}
