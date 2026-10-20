import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {addMechanic,mechanicsFor} from '../src/mechanicsModel.js';
import {ensureGameplay,newItem} from '../src/gameplayModel.js';
import {installBuiltinBehaviors} from '../src/builtinBehaviors.js';
import {graphNodeSize} from '../src/graphNodeLayout.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {validateStudio} from '../src/studioModel.js';
import {reference} from '../src/valueSchemas.js';
import {createRule} from '../src/ruleModel.js';
import {readAuthoringPackage,exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
const scope=id=>({kind:'object',id});
const wire=nodeId=>({op:'node',nodeId,port:'value'});
function fixture(){const p=createMechanicsProject('Поведения'),scene=p.subscenes[0];scene.kind='empty';for(const [id,pos]of [['owner',[0,0,0]],['target',[0,0,-3]],['near',[0,0,-.25]]])p.objects.push({id,name:id,type:'Меш',active:true,subsceneId:scene.id,collision:{enabled:true,custom:true,size:[.2,.2,.2],offset:[0,1,0]},transforms:{[scene.id]:{position:pos,rotation:[0,0,0],scale:[1,1,1]}}});return p;}
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('Тест не использует звук.');}));
const fn=(p,b,key)=>p.functions.find(f=>f.id===b.functions[key]);
const node=(f,key)=>f.nodes.find(n=>n.id.endsWith(':'+key));
async function settle(rt){for(let i=0;i<100&&!rt.game.ruleRuntime.idle;i++)await new Promise(r=>setTimeout(r,1));assert.equal(rt.snapshot.error,null);}

test('fresh built-in graphs reserve card height and retain author positions on reopen',()=>{
 const p=fixture();for(const type of ['health','weapon','pursuit','pickup'])addMechanic(p,type,scope('owner'));ensureGameplay(p);
 for(const graph of p.functions.filter(f=>f.builtin))for(let i=0;i<graph.nodes.length;i++)for(let j=i+1;j<graph.nodes.length;j++){const a={...graph.nodes[i].position,...graphNodeSize(graph.nodes[i])},b={...graph.nodes[j].position,...graphNodeSize(graph.nodes[j])};assert.ok(a.x+a.width+60<=b.x||b.x+b.width+60<=a.x||a.y+a.height+60<=b.y||b.y+b.height+60<=a.y,graph.name+' cards have room for their inputs');}
 const edited=p.functions.find(f=>f.builtin).nodes[0];edited.position={x:-432,y:876};ensureGameplay(p);assert.deepEqual(edited.position,{x:-432,y:876});
});

test('standard behavior migration is deterministic, idempotent, preserves edits and refuses unknown versions intact',()=>{
 const p=fixture();for(const type of ['health','weapon','pursuit','pickup'])addMechanic(p,type,scope('owner'));ensureGameplay(p);const first=JSON.stringify(p);ensureGameplay(p);assert.equal(JSON.stringify(p),first);
 const weapon=p.gameplay.weapons[0],shoot=fn(p,weapon.behavior,'shoot');node(shoot,'ammo-cost').value.value=2;ensureGameplay(p);assert.equal(node(fn(p,weapon.behavior,'shoot'),'ammo-cost').value.value,2);
 const cloned=structuredClone(p);delete cloned.gameplay.behaviorSchemaVersion;for(const a of Object.values(cloned.gameplay.actors))delete a.behaviors;for(const w of cloned.gameplay.weapons)delete w.behavior;for(const i of cloned.gameplay.items){delete i.behavior;delete i.pickupBehavior;}cloned.functions=cloned.functions.filter(f=>!f.builtin);cloned.gameplay.mechanicDefinitions=cloned.gameplay.mechanicDefinitions.filter(d=>!d.builtin);installBuiltinBehaviors(cloned);assert.equal(cloned.gameplay.weapons[0].behavior.functions.shoot,weapon.behavior.functions.shoot);
 p.gameplay.behaviorSchemaVersion=99;const before=JSON.stringify(p);assert.throws(()=>installBuiltinBehaviors(p),/Версия/);assert.equal(JSON.stringify(p),before);
 assert.ok(mechanicsFor(cloned).filter(m=>['weapon','health','pursuit','pickup'].includes(m.type)).every(m=>Object.keys(m.implementation.functions).length));
});

test('editing a project shot graph changes its condition, ammo cost, computed damage, death and reward through old and registered entries',async()=>{
 const p=fixture();p.objects=p.objects.filter(o=>o.id!=='near');for(const id of ['owner','target'])addMechanic(p,'health',scope(id));const m=addMechanic(p,'weapon',scope('owner')),counter=addMechanic(p,'resource',{kind:'project',id:p.id}),weapon=p.gameplay.weapons.find(w=>w.id===m.recordId);weapon.magazine=6;p.gameplay.actors.target.maxHP=50;p.variables.allowed=false;p.variableTypes??={};p.variableTypes.allowed='boolean';
 const shoot=fn(p,weapon.behavior,'shoot');node(shoot,'ammo-cost').value.value=2;node(shoot,'damage-multiplier').value.value=2;const old=node(shoot,'enabled').condition;shoot.nodes.push({id:'custom-shot-allowed',kind:'value',name:'Разрешение автора',value:{op:'get',path:'global.allowed'}},{id:'custom-shot-condition',kind:'value',name:'Разрешено и включено',value:{op:'and',a:old,b:wire('custom-shot-allowed')}});node(shoot,'enabled').condition=wire('custom-shot-condition');
 p.gameplay.rules.push(createRule({signal:'death',scope:scope('target'),filter:{target:reference('object','target')},actions:[{kind:'operation',call:{id:'sacura.resource.add',version:1,parameters:{owner:reference('project',p.id),resource:reference('resource',counter.recordId),amount:1}}}]}));ensureGameplay(p);assert.deepEqual(validateStudio(p),[]);
 const rt=runtime();try{await rt.start(p);assert.equal(rt.game.shoot({ownerId:'owner',direction:[0,0,-1],origin:[0,1,0]}),false);assert.equal(rt.game.state.weapons[weapon.id].ammo,6);rt.snapshot.variables.allowed=true;assert.equal(await rt.game.execute({id:'sacura.weapon.shoot',version:1,parameters:{owner:reference('object','owner'),direction:[0,0,-1],origin:[0,1,0]}}),true);await settle(rt);assert.equal(rt.game.state.weapons[weapon.id].ammo,4);assert.equal(rt.game.state.actors.target.dead,true);assert.equal(rt.game.state.resources[counter.recordId],1);assert.equal(rt.game.damage('target',50),false);}finally{rt.stop();}
});

test('reload completion uses the editable transfer graph and leaves another weapon independent',async()=>{
 const p=fixture(),a=addMechanic(p,'weapon',scope('owner')),b=addMechanic(p,'weapon',scope('target')),wa=p.gameplay.weapons.find(w=>w.id===a.recordId),wb=p.gameplay.weapons.find(w=>w.id===b.recordId);Object.assign(wa,{magazine:3,reserve:4,reloadTime:.1});const reload=fn(p,wa.behavior,'reload');node(reload,'duration').value.b=.2;
 const rt=runtime();try{await rt.start(p);rt.game.state.weapons[wa.id].ammo=0;assert.equal(rt.game.reload('owner'),true);rt.game.tick(.05);rt.game.tick(.05);assert.equal(rt.game.state.weapons[wa.id].ammo,0);rt.game.tick(.05);rt.game.tick(.05);assert.equal(rt.game.state.weapons[wa.id].ammo,3);assert.equal(rt.game.state.weapons[wa.id].reserve,1);assert.equal(rt.game.state.weapons[wb.id].ammo,6);}finally{rt.stop();}
});

test('AI uses the common reactive BT, editable attack/chase conditions, a changed target, game-time waiting and cancellation',async()=>{
 const p=fixture();for(const id of ['owner','target','near'])addMechanic(p,'health',scope(id));addMechanic(p,'pursuit',scope('owner'));const actor=p.gameplay.actors.owner;Object.assign(actor.ai,{targetId:'target',range:5,damage:10,interval:1,speed:0});
 const attack=fn(p,actor.behaviors.pursuit,'attack'),chase=fn(p,actor.behaviors.pursuit,'chase');node(attack,'in-range').value.b=.5;node(chase,'far').value.b=.5;
 const rt=runtime();try{await rt.start(p);rt.game.tick(.05);assert.equal(rt.game.state.actors.target.hp,100);assert.equal(rt.game.state.behaviors.owner.status,'Running');assert.ok(rt.game.state.behaviors.owner.direction[2]<0);
 await rt.game.execute({id:'sacura.behavior.target',version:1,parameters:{owner:reference('object','owner'),target:reference('object','near')}});rt.game.tick(.05);assert.equal(rt.game.state.actors.near.hp,90);assert.equal(rt.game.state.behaviors.owner.status,'Success');rt.game.tick(.05);assert.equal(rt.game.state.actors.near.hp,90);assert.equal(rt.game.state.behaviors.owner.status,'Running');rt.togglePause();const clock=rt.game.state.clock;rt.game.tick(.05);assert.equal(rt.game.state.clock,clock);rt.togglePause();rt.game.damage('owner',100);rt.game.tick(.05);assert.equal(rt.game.state.behaviors.owner.status,'Failure');assert.deepEqual(rt.game.state.behaviors.owner.direction,[0,0,0]);assert.equal(rt.game.state.actors.near.hp,90);}finally{rt.stop();}
});

test('recipe graph loops through live parameters, combines duplicate ingredients and rolls storage back on an impossible result',async()=>{
 const p=fixture();addMechanic(p,'inventory',scope('owner'));p.gameplay.items=[{...newItem(),id:'part',stackLimit:10},{...newItem(),id:'result',stackLimit:1}];p.gameplay.recipes=[{id:'craft',ingredients:[{itemId:'part',count:1},{itemId:'part',count:1}],resultId:'result',count:1}];ensureGameplay(p);p.gameplay.recipes[0].ingredients[0].count=2;
 const rt=runtime();try{await rt.start(p);rt.game.addItem('part',3,{ownerId:'owner',publish:false});rt.game.combine('craft',{ownerId:'owner'});assert.equal(rt.game.count('part','owner'),0);assert.equal(rt.game.count('result','owner'),1);rt.game.addItem('part',3,{ownerId:'owner',publish:false});rt.game.config.recipes[0].count=100;const before=structuredClone(rt.game.inventoryFor('owner'));assert.throws(()=>rt.game.combine('craft',{ownerId:'owner'}),/свободного места/);assert.deepEqual(rt.game.inventoryFor('owner'),before);}finally{rt.stop();}
});

test('all published standard templates contain graphs and UI/novel projects have no mandatory combat component',async()=>{
 for(const name of ['shooter','point-and-click','novel','ui-clicker']){const pkg=await readAuthoringPackage(JSON.parse(readFileSync(new URL('../public/game-templates/'+name+'.sacura-template.json',import.meta.url),'utf8')));assert.ok(pkg.content.functions.length);assert.ok(pkg.content.functions.every(f=>f.graphVersion===1));if(['novel','ui-clicker'].includes(name)){assert.equal(pkg.content.gameplay?.weapons?.length||0,0);assert.equal(Object.keys(pkg.content.gameplay?.actors||{}).length,0);}}
});

test('a Wait added to the actual weapon graph uses game time, skips reentry and cancels on owner deletion before any late shot',async()=>{
 const p=fixture();p.objects=p.objects.filter(o=>o.id!=='near');addMechanic(p,'health',scope('target'));const m=addMechanic(p,'weapon',scope('owner')),w=p.gameplay.weapons.find(w=>w.id===m.recordId),shoot=fn(p,w.behavior,'shoot'),entry=shoot.entry;shoot.nodes.push({id:'author-wait-before-shot',kind:'wait',name:'Задержка автора',duration:.1,next:entry});shoot.entry='author-wait-before-shot';
 const rt=runtime();try{await rt.start(p);const task=rt.game.shoot({ownerId:'owner',origin:[0,1,0],direction:[0,0,-1]});assert.equal(typeof task.then,'function');assert.equal(rt.game.shoot({ownerId:'owner'}),false);assert.equal(rt.game.state.weapons[w.id].ammo,6);rt.togglePause();rt.game.tick(.05);await new Promise(r=>setTimeout(r,12));assert.equal(rt.game.state.weapons[w.id].ammo,6);rt.togglePause();rt.project.objects=rt.project.objects.filter(o=>o.id!=='owner');rt.game.cleanupOwners();await assert.rejects(task,/INSTANCE_STOPPED/);for(let i=0;i<4;i++)rt.game.tick(.05);await new Promise(r=>setTimeout(r,8));assert.equal(rt.game.state.weapons[w.id].ammo,6);assert.equal(rt.game.state.actors.target.hp,100);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

test('a delayed AI attack remains one task across ticks and death cancels it before damage',async()=>{
 const p=fixture();p.objects=p.objects.filter(o=>o.id!=='target');for(const id of ['owner','near'])addMechanic(p,'health',scope(id));addMechanic(p,'pursuit',scope('owner'));const a=p.gameplay.actors.owner;Object.assign(a.ai,{targetId:'near',damage:25,range:2});const attack=fn(p,a.behaviors.pursuit,'attack'),damage=node(attack,'damage'),previous=attack.nodes.find(n=>n.next===damage.id||n.trueNext===damage.id);attack.nodes.push({id:'author-wait-attack',kind:'wait',name:'Подготовить атаку',duration:.2,next:damage.id});previous[previous.next===damage.id?'next':'trueNext']='author-wait-attack';
 const rt=runtime();try{await rt.start(p);rt.game.tick(.05);assert.equal(rt.game.behaviorTasks.size,1);rt.game.tick(.05);rt.game.tick(.05);assert.equal(rt.game.behaviorTasks.size,1);assert.equal(rt.game.state.actors.near.hp,100);rt.game.damage('owner',100);rt.game.tick(.05);await new Promise(r=>setTimeout(r,12));assert.equal(rt.game.state.actors.near.hp,100);assert.equal(rt.game.state.behaviors.owner.status,'Failure');assert.deepEqual(rt.game.state.behaviors.owner.direction,[0,0,0]);assert.equal(rt.game.behaviorTasks.size,0);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.builtinRuns.size,0);}finally{rt.stop();}
});

test('queued delayed damage rereads current state for each invocation and the complete graph can resume successfully',async()=>{
 const p=fixture();addMechanic(p,'health',scope('target'));const behavior=p.gameplay.actors.target.behaviors.health,damage=fn(p,behavior,'damage');damage.nodes.push({id:'author-delay-damage',kind:'wait',duration:.1,next:damage.entry});damage.entry='author-delay-damage';
 const rt=runtime();try{await rt.start(p);const first=rt.game.damage('target',10),second=rt.game.damage('target',20);assert.equal(typeof first.then,'function');assert.equal(typeof second.then,'function');assert.equal(rt.game.state.actors.target.hp,100);for(let i=0;i<6;i++){rt.game.tick(.05);await new Promise(r=>setTimeout(r,8));}assert.deepEqual(await Promise.all([first,second]),[true,true]);assert.equal(rt.game.state.actors.target.hp,70);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.flights.size,0);}finally{rt.stop();}
});

function prefixedFixture(prefix){const p=fixture();for(const o of p.objects)o.id=prefix+o.id;return p;}
const packageMeta={name:'Редактируемое стандартное поведение',author:'Автор',license:'MIT'};
const importFor=async(p,pkg,mapping)=>{const bindings=Object.fromEntries(pkg.inputs.map(i=>[i.id,mapping[i.name]]));return (await planPackageImport(p,pkg,bindings)).apply(p);};

test('an edited native weapon graph survives A → B → C and each imported copy uses its own edited ammo rule',async()=>{
 const a=prefixedFixture('A-'),m=addMechanic(a,'weapon',scope('A-owner')),w=a.gameplay.weapons[0],shoot=fn(a,w.behavior,'shoot');node(shoot,'ammo-cost').value.value=2;node(shoot,'ammo-cost').comment='Первый вариант';const pkg=await exportAuthoringPackage(a,m,packageMeta),source=JSON.stringify(pkg),b=await importFor(prefixedFixture('B-'),pkg,{owner:'B-owner'}),wb=b.gameplay.weapons[0],mb=b.gameplay.mechanics.find(m=>m.type==='weapon'),bshoot=fn(b,wb.behavior,'shoot'),cost=bshoot.nodes.find(n=>n.name==='Расход патронов за выстрел');assert.equal(cost.comment,'Первый вариант');const rb=runtime();try{await rb.start(b);assert.equal(rb.game.shoot({ownerId:'B-owner',direction:[0,1,0]}),true);assert.equal(rb.game.state.weapons[wb.id].ammo,4);}finally{rb.stop();}
 cost.value.value=3;const changed=await exportAuthoringPackage(b,mb,packageMeta),c=await importFor(prefixedFixture('C-'),changed,{owner:'C-owner'}),wc=c.gameplay.weapons[0],rc=runtime();try{await rc.start(c);assert.equal(rc.game.shoot({ownerId:'C-owner',direction:[0,1,0]}),true);assert.equal(rc.game.state.weapons[wc.id].ammo,3);}finally{rc.stop();}assert.equal(node(shoot,'ammo-cost').value.value,2);assert.equal(JSON.stringify(pkg),source);assert.equal(fn(c,wc.behavior,'shoot').nodes.find(n=>n.name==='Расход патронов за выстрел').comment,'Первый вариант');
});

test('a native AI package preserves its edited BT children and conditions through two independent projects without installing health on its owner',async()=>{
 const a=prefixedFixture('A-'),m=addMechanic(a,'pursuit',scope('A-owner')),actor=a.gameplay.actors['A-owner'];Object.assign(actor.ai,{targetId:'A-target',range:5,speed:0});node(fn(a,actor.behaviors.pursuit,'attack'),'in-range').value.b=.5;node(fn(a,actor.behaviors.pursuit,'chase'),'far').value.b=.5;const pkg=await exportAuthoringPackage(a,m,packageMeta),source=JSON.stringify(pkg),base=prefixedFixture('B-');for(const id of ['B-target','B-near'])addMechanic(base,'health',scope(id));const b=await importFor(base,pkg,{owner:'B-owner',target:'B-target'}),bi=b.gameplay.actors['B-owner'],bm=b.gameplay.mechanics.find(m=>m.type==='pursuit');assert.equal(bi.maxHP,undefined);const rb=runtime();try{await rb.start(b);rb.game.tick(.05);assert.equal(rb.game.state.actors['B-target'].hp,100);assert.equal(rb.game.state.behaviors['B-owner'].status,'Running');await rb.game.execute({id:'sacura.behavior.target',version:1,parameters:{owner:reference('object','B-owner'),target:reference('object','B-near')}});rb.game.tick(.05);assert.equal(rb.game.state.actors['B-near'].hp,90);}finally{rb.stop();}
 bi.ai.targetId='B-near';fn(b,bi.behaviors.pursuit,'attack').nodes.find(n=>n.name==='in-range').value.b=.1;fn(b,bi.behaviors.pursuit,'chase').nodes.find(n=>n.name==='far').value.b=.1;const changed=await exportAuthoringPackage(b,bm,packageMeta),cbase=prefixedFixture('C-');addMechanic(cbase,'health',scope('C-near'));const c=await importFor(cbase,changed,{owner:'C-owner',near:'C-near'}),rc=runtime();try{await rc.start(c);rc.game.tick(.05);assert.equal(rc.game.state.actors['C-near'].hp,100);assert.equal(rc.game.state.behaviors['C-owner'].status,'Running');}finally{rc.stop();}assert.equal(node(fn(a,actor.behaviors.pursuit,'attack'),'in-range').value.b,.5);assert.equal(JSON.stringify(pkg),source);
});

test('a native health graph imports over independent AI without replacing it and re-exports its changed damage algorithm',async()=>{
 const a=prefixedFixture('A-'),m=addMechanic(a,'health',scope('A-owner')),behavior=a.gameplay.actors['A-owner'].behaviors.health;node(fn(a,behavior,'damage'),'change-health').value.b=50;const pkg=await exportAuthoringPackage(a,m,packageMeta),base=prefixedFixture('B-');addMechanic(base,'pursuit',scope('B-owner'));const existingAI=base.gameplay.actors['B-owner'].behaviors.pursuit.functions.tick,b=await importFor(base,pkg,{owner:'B-owner'}),actor=b.gameplay.actors['B-owner'],mh=b.gameplay.mechanics.find(m=>m.type==='health');assert.equal(actor.behaviors.pursuit.functions.tick,existingAI);const rb=runtime();try{await rb.start(b);rb.game.damage('B-owner',1);assert.equal(rb.game.state.actors['B-owner'].hp,50);}finally{rb.stop();}
 fn(b,actor.behaviors.health,'damage').nodes.find(n=>n.name==='Вычесть урон').value.b=25;const changed=await exportAuthoringPackage(b,mh,packageMeta),c=await importFor(prefixedFixture('C-'),changed,{owner:'C-owner'}),rc=runtime();assert.equal(c.gameplay.actors['C-owner'].ai,undefined);try{await rc.start(c);rc.game.damage('C-owner',1);assert.equal(rc.game.state.actors['C-owner'].hp,75);}finally{rc.stop();}assert.equal(node(fn(a,behavior,'damage'),'change-health').value.b,50);
});

test('the item graph owns presence and health conditions: medication is retained at full health and consumption/healing use live parameters',async()=>{
 const p=fixture();addMechanic(p,'inventory',scope('owner'));addMechanic(p,'health',scope('owner'));p.gameplay.items=[{...newItem(),id:'medicine',heal:20,consumable:true}];ensureGameplay(p);p.gameplay.items[0].heal=30;const rt=runtime();try{await rt.start(p);rt.game.addItem('medicine',1,{ownerId:'owner',publish:false});await rt.game.useItem('medicine',{ownerId:'owner'});assert.equal(rt.game.count('medicine','owner'),1);assert.equal(rt.game.state.actors.owner.hp,100);rt.game.damage('owner',50);await rt.game.useItem('medicine',{ownerId:'owner'});assert.equal(rt.game.count('medicine','owner'),0);assert.equal(rt.game.state.actors.owner.hp,80);await assert.rejects(rt.game.useItem('medicine',{ownerId:'owner'}),/Предмет отсутствует/);}finally{rt.stop();}
});

test('a parent timeout cancels delayed reload and equip graphs entered through registered operations and legacy commands',async()=>{
 for(const handler of ['reload','equip'])for(const registered of [true,false]){
  const p=fixture();addMechanic(p,'inventory',scope('owner'));const m=addMechanic(p,'weapon',scope('owner')),weapon=p.gameplay.weapons.find(w=>w.id===m.recordId);p.gameplay.items.push({...newItem(),id:'equipment',slot:'hand'});ensureGameplay(p);
  const item=p.gameplay.items.find(i=>i.id==='equipment'),body=fn(p,handler==='reload'?weapon.behavior:item.behavior,handler),entry=body.entry;body.nodes.push({id:'delay-'+handler,kind:'wait',duration:.05,timeSource:'real',next:entry});body.entry='delay-'+handler;
  const value=handler==='reload'?{owner:reference('object','owner')}:{owner:reference('object','owner'),item:reference('item',item.id)},call=registered?{id:'invoke',kind:'operation',call:{id:handler==='reload'?'sacura.weapon.reload':'sacura.inventory.equip',version:1,parameters:value}}:{id:'invoke',kind:'command',command:{command:handler,value:handler==='reload'?{ownerId:'owner'}:{ownerId:'owner',itemId:item.id}}};
  p.functions.push({id:'cancel-parent',name:'Отменяемый вызов',graphVersion:1,parameters:[],locals:{},entry:'timeout',nodes:[{id:'timeout',kind:'timeout',duration:.01,timeSource:'real',body:'invoke'},call]});
  const rt=runtime();try{
   await rt.start(p);rt.game.state.weapons[weapon.id].ammo=0;rt.game.addItem(item.id,1,{ownerId:'owner',publish:false});
   await assert.rejects(rt.game.callFunction('cancel-parent'),/время ожидания/);await new Promise(r=>setTimeout(r,80));
   assert.equal(rt.game.state.weapons[weapon.id].reloadUntil,0,handler+' retains its parent cancellation');assert.deepEqual(rt.game.inventoryFor('owner').equipment,{});assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.flights.size,0);
  }finally{rt.stop();}
 }
});

test('the reload scheduler starts a delayed completion graph once across ticks and completes its transfer once',async()=>{
 const p=fixture(),m=addMechanic(p,'weapon',scope('owner')),weapon=p.gameplay.weapons.find(w=>w.id===m.recordId);Object.assign(weapon,{magazine:3,reserve:4,reloadTime:.05});const completion=fn(p,weapon.behavior,'reloadComplete'),entry=completion.entry;completion.nodes.push({id:'delay-completion',kind:'wait',duration:.05,timeSource:'real',next:entry});completion.entry='delay-completion';
 const rt=runtime();try{
  await rt.start(p);rt.game.state.weapons[weapon.id].ammo=0;assert.equal(rt.game.reload('owner'),true);for(let i=0;i<25;i++)rt.game.tick(.05);
  const pending=[...rt.game.builtinRuns.values()];assert.equal(pending.length,1);assert.equal(pending[0].handler,'reloadComplete');assert.equal(pending[0].queued,1);assert.equal(rt.game.graphScopes.size,1);assert.equal(rt.game.state.weapons[weapon.id].ammo,0);
  await pending[0].promise;assert.equal(rt.game.state.weapons[weapon.id].ammo,3);assert.equal(rt.game.state.weapons[weapon.id].reserve,1);assert.equal(rt.game.state.weapons[weapon.id].reloadUntil,0);for(let i=0;i<5;i++)rt.game.tick(.05);assert.equal(rt.game.state.weapons[weapon.id].reserve,1);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.flights.size,0);
 }finally{rt.stop();}
});

test('selecting a new AI target cancels its delayed attack before damage and lets the next tick attack the new target',async()=>{
 const p=fixture(),scene=p.subscenes[0];p.objects.find(o=>o.id==='near').transforms[scene.id].position=[1,0,0];for(const id of ['owner','target','near'])addMechanic(p,'health',scope(id));addMechanic(p,'pursuit',scope('owner'));const actor=p.gameplay.actors.owner;Object.assign(actor.ai,{targetId:'target',range:5,damage:10,interval:1,speed:0});
 const attack=fn(p,actor.behaviors.pursuit,'attack'),start=node(attack,'cooldown-start'),previous=attack.nodes.find(n=>n.next===start.id||n.trueNext===start.id);attack.nodes.push({id:'delay-targeted-attack',kind:'wait',duration:.1,next:start.id});previous[previous.next===start.id?'next':'trueNext']='delay-targeted-attack';
 const rt=runtime();try{
  await rt.start(p);rt.game.tick(.05);const oldTask=rt.game.behaviorTasks.get('owner');assert.ok(oldTask);assert.equal(rt.game.state.actors.target.hp,100);
  await rt.game.execute({id:'sacura.behavior.target',version:1,parameters:{owner:reference('object','owner'),target:reference('object','near')}});await assert.rejects(oldTask,/INSTANCE_STOPPED/);
  rt.game.tick(.05);const newTask=rt.game.behaviorTasks.get('owner');assert.ok(newTask);assert.notEqual(newTask,oldTask);
  for(let i=0;i<6;i++){rt.game.tick(.05);await new Promise(r=>setTimeout(r,8));}await newTask;
  assert.equal(rt.game.state.actors.target.hp,100);assert.equal(rt.game.state.actors.near.hp,90);assert.equal(rt.game.behaviorTasks.size,0);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.flights.size,0);
 }finally{rt.stop();}
});
