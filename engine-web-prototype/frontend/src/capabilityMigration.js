import {createRule} from './ruleModel.js';
import {reference} from './valueSchemas.js';
export const CAPABILITY_SCHEMA_VERSION=1;
const action=(id,parameters)=>({kind:'operation',call:{id,version:1,parameters}});
// This read migration changes only the owned project copy. IDs are deterministic;
// a second read does not add rules, bindings, resources or inventory instances.
export function migrateCapabilities(project,{legacy=false}={}){
 const g=project.gameplay;if(g.capabilitySchemaVersion!==undefined&&g.capabilitySchemaVersion!==1)return;
 g.inventory.owner??={kind:'project',id:project.id||''};g.inventories??=[];g.movements??=[];g.resources??=[];g.stateBindings??=[];
 g.cameraSettings??=Object.fromEntries(['eyeHeight','sensitivity','distance','fov'].map(k=>[k,g.controller[k]]));
 if(g.controller.ownerId===undefined||!g.controller.ownerId&&g.playerId)g.controller.ownerId=g.playerId||'';
 if(g.version===2&&!legacy)return;
 if(g.capabilitySchemaVersion===1&&!legacy&&g.legacyCompatibility!=='gameplay-v1')return;
 g.legacyCompatibility='gameplay-v1';g.compatibilityRules??=[];g.compatibilityBindings??=[];
 g.legacyOwnerId=g.playerId||'';g.legacyInventoryOwnerId=g.playerId||project.id;
 g.inventory.owner={kind:g.playerId?'object':'project',id:g.legacyInventoryOwnerId};g.inventory.enabled=true;
 for(const w of g.weapons)w.ownerId??=g.playerId||'';
 for(const i of g.items)i.inventoryOwner??={kind:g.playerId?'object':'project',id:g.legacyInventoryOwnerId};
 for(const a of Object.values(g.actors))if(a.ai&&!a.ai.targetId)a.ai.targetId=g.playerId||'';
 g.crosshair??={enabled:!!g.camera?.crosshair};
 const scope={kind:'project',id:project.id},enemies=Object.entries(g.actors).filter(([,a])=>a.team==='enemy'&&Number.isFinite(a.maxHP)).map(([id])=>id);
 let counterId=g.legacyCounterId||'legacy-kills';if(enemies.length&&!g.legacyCounterId){let n=2;while(g.resources.some(r=>r.id===counterId))counterId='legacy-kills-'+n++;g.legacyCounterId=counterId;}if(enemies.length&&!g.resources.some(r=>r.id===counterId))g.resources.push({id:counterId,name:'Побеждённые противники',owner:scope,type:'integer',initial:0});
 const add=(key,capability,owner,field,valueType,recordId)=>{
  const variable=g.variableBindings.find(b=>b.key===key);if(!variable||g.compatibilityBindings.includes('legacy-binding-'+key))return;
  g.compatibilityBindings.push('legacy-binding-'+key);g.stateBindings.push({id:'legacy-binding-'+key,variable:reference('variable',variable.id),source:'capability',capability,owner:reference(owner.kind,owner.id),field,valueType,...(recordId?{recordId}:{})});
 };
 if(Number.isFinite(g.actors[g.playerId]?.maxHP)){add('hp','sacura.health',{kind:'object',id:g.playerId},'hp','number');add('maxHp','sacura.health',{kind:'object',id:g.playerId},'maxHP','number');}
 if(g.weapons.some(w=>w.ownerId===g.playerId)){add('ammo','sacura.weapon',{kind:'object',id:g.playerId},'ammo','integer');add('reserve','sacura.weapon',{kind:'object',id:g.playerId},'reserve','integer');}
 if(enemies.length)add('kills','sacura.resource',scope,'value','integer',counterId);
 if(g.items.length)add('inventory','sacura.inventory',g.inventory.owner,'items','collection');
 const append=(id,fields)=>{if(!g.compatibilityRules.includes(id)){if(!g.rules.some(r=>r.id===id))g.rules.push({...createRule({signal:'death',scope,...fields}),id,compatibility:'gameplay-v1'});g.compatibilityRules.push(id);}};
 for(const [id,a]of Object.entries(g.actors)){
  if(!Number.isFinite(a.maxHP))continue;const owner=reference('object',id),actions=[action('sacura.movement.enabled',{owner,enabled:false}),action('sacura.behavior.enabled',{owner,enabled:false}),action('sacura.weapon.enabled',{owner,enabled:false})];
  if(id!==g.playerId)actions.push(action('sacura.object.visible',{object:owner,visible:false}));
  if(enemies.includes(id))actions.push(action('sacura.resource.add',{owner:reference('project',project.id),resource:reference('resource',counterId),amount:1}));
  append('legacy-death:'+id,{name:'После смерти · '+(project.objects.find(o=>o.id===id)?.name||id),filter:{target:owner},actions});
 }
 const victory=g.rules.find(r=>r.id==='legacy-victory');
 if(victory&&JSON.stringify(victory.conditionOperation?.parameters.objects)===JSON.stringify((g.legacyEnemyIds||[]).map(id=>reference('object',id))))victory.conditionOperation.parameters.objects=enemies.map(id=>reference('object',id));
 if(enemies.length)append('legacy-victory',{name:'Все противники побеждены',once:true,conditionOperation:{id:'sacura.health.all-dead',version:1,parameters:{objects:enemies.map(id=>reference('object',id))}},actions:[action('sacura.signal.victory',{})]});
 g.legacyEnemyIds=enemies;
 g.capabilitySchemaVersion=1;
}
