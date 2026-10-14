import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {parseProjectFile,createEmptyProject} from '../src/projectLifecycle.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {inputContextsFor} from '../src/inputModel.js';
import {widgetValue} from '../src/widgetBindings.js';
import {addMechanic} from '../src/mechanicsModel.js';
import {makeAction} from '../src/model.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';

// Read shipped authoring data, never substitute a simulated gameplay executor.
const template=name=>parseProjectFile(readFileSync(new URL(`./fixtures/legacy-${name}.sacura-template.json`,import.meta.url),'utf8')).project;
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('These templates must not request audio in this headless characterization.');}));
async function settled(rt){
  const deadline=Date.now()+2500;
  while(rt.game.flights.size&&Date.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
  assert.equal(rt.game.flights.size,0,'native signal reactions finished');
  assert.equal(rt.snapshot.error,null);
}
async function combat(rt,p){await rt.start(p,p.subscenes[0].entry);await rt.advance();assert.equal(rt.snapshot.beatId,'shooter-combat');}
const metadata={name:'Characterization',description:'Audit transfer',author:'Audit',license:'CC0-1.0'};

test('characterization: shipped shooter enters dialogue before activating combat and the legacy loadout',async()=>{
  const p=template('shooter'),before=JSON.stringify(p),rt=runtime();
  try{
    await rt.start(p,p.subscenes[0].entry);
    assert.equal(rt.snapshot.beatId,'shooter-intro');assert.equal(rt.snapshot.phase,'WAITING_INPUT');assert.equal(rt.game.state.active,false);
    assert.equal(rt.game.state.actors['shooter-hero'].hp,100);assert.equal(rt.game.count('medkit'),1);assert.equal(rt.game.state.weaponId,'pistol');
    await rt.advance();
    assert.equal(rt.snapshot.phase,'PLAYING');assert.equal(rt.game.state.active,true);assert.equal(rt.snapshot.world.playerControl.characterId,'shooter-hero');
    assert.deepEqual(rt.game.state.camera,{mode:'firstPerson',targetId:'shooter-hero',pointerLock:true,crosshair:true});
    assert.ok(inputContextsFor(rt.project,rt.project.chapters[0].beats[1]).includes('game-mechanics'));
    assert.equal(rt.snapshot.variables.ammo,8);assert.equal(JSON.stringify(p),before,'playback owns a copy');
  }finally{rt.stop();}
});

test('characterization: damage emits template feedback; enemy death awards score exactly once',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);assert.equal(rt.game.command('damage',{target:'shooter-hero',amount:75}),true);await settled(rt);
    assert.equal(rt.snapshot.variables.hp,25);assert.equal(rt.snapshot.variables.lowHealth,true);assert.equal(rt.snapshot.variables.score,0);
    assert.equal(rt.game.command('damage',{target:'enemy-1',amount:50}),true);await settled(rt);
    assert.equal(rt.game.state.actors['enemy-1'].dead,true);assert.equal(rt.snapshot.world.visible['enemy-1'],false);
    assert.equal(rt.snapshot.variables.kills,1);assert.equal(rt.snapshot.variables.score,100);assert.deepEqual(rt.snapshot.variables.results.defeated,['enemy-1']);
    assert.equal(rt.game.damage('enemy-1',50),false);await settled(rt);assert.equal(rt.snapshot.variables.score,100);
  }finally{rt.stop();}
});

test('characterization: player death awaits its native Event and enters Game Over without enemy score',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);rt.game.damage('shooter-hero',100);await settled(rt);
    assert.equal(rt.snapshot.beatId,'shooter-game-over');assert.equal(rt.snapshot.phase,'ENDING');assert.equal(rt.game.state.active,false);
    assert.equal(rt.snapshot.world.playerControl,null);assert.equal(rt.snapshot.world.poses['shooter-hero'],'грусть');
    assert.equal(rt.snapshot.variables.score,0);assert.deepEqual(rt.snapshot.variables.results.defeated,[]);
  }finally{rt.stop();}
});

test('characterization: killing all shipped enemies chooses victory through the rule, not the story next link',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);
    for(const [id,actor]of Object.entries(p.gameplay.actors).filter(([,a])=>a.team==='enemy')){rt.game.damage(id,actor.maxHP);await settled(rt);}
    assert.equal(rt.snapshot.beatId,'shooter-win');assert.equal(rt.snapshot.phase,'ENDING');assert.equal(rt.snapshot.variables.kills,5);assert.equal(rt.snapshot.variables.score,500);
    assert.equal(p.chapters[0].beats.find(b=>b.id==='shooter-combat').next,null);
    assert.deepEqual(rt.snapshot.variables.results.defeated,['enemy-1','enemy-2','enemy-3','enemy-4','enemy-5']);
  }finally{rt.stop();}
});

test('characterization: medkit pickup runs its Event before the signal function and cannot repeat',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);rt.snapshot.world.positions['shooter-hero']=rt.game.position('arena-medkit');
    assert.equal(rt.game.interact('arena-medkit'),true);await settled(rt);
    assert.equal(rt.game.count('medkit'),2);assert.equal(rt.snapshot.variables.medkits,2);
    for(const id of ['arena-medkit','medkit-cross-horizontal','medkit-cross-vertical'])assert.equal(rt.snapshot.world.visible[id],false);
    assert.equal(rt.game.interact('arena-medkit'),false);assert.equal(rt.game.count('medkit'),2);
  }finally{rt.stop();}
});

test('characterization: HUD Event invokes the shipped healing function through the common command executor',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);rt.game.damage('shooter-hero',50);await settled(rt);
    await rt.uiAction('event',{id:'audit-heal',eventId:'heal-player'});
    assert.equal(rt.snapshot.variables.hp,90);assert.equal(rt.game.count('medkit'),0);assert.equal(rt.snapshot.variables.medkits,0);
    await rt.uiAction('event',{id:'audit-heal',eventId:'heal-player'});assert.equal(rt.snapshot.variables.hp,90);
    const hp=p.widgets.find(w=>w.id==='shooter-hud').elements.find(e=>e.id==='shooter-hud-hp');
    assert.equal(widgetValue({variables:rt.snapshot.variables,game:rt.game.state},hp.binding),90);
  }finally{rt.stop();}
});

test('characterization: quest goTo action crosses scenes after its preceding Event groups',async()=>{
  const p=template('point-and-click'),rt=runtime();
  try{
    await rt.start(p,p.subscenes[0].entry);await rt.advance();
    await rt.uiAction('event',{id:'audit-door',eventId:'quest-open-door'});
    assert.equal(rt.snapshot.variables.doorUnlocked,true);assert.equal(rt.snapshot.variables.hasKey,false);
    assert.equal(rt.snapshot.beatId,'quest-yard-intro');assert.equal(rt.snapshot.world.location,'quest-yard');assert.equal(rt.snapshot.phase,'WAITING_INPUT');
    assert.equal(rt.game.state.active,false);assert.equal(rt.snapshot.world.playerControl,null);
    await rt.advance();assert.equal(rt.snapshot.beatId,'quest-relic-gate');assert.equal(rt.game.state.active,true);
  }finally{rt.stop();}
});

test('characterization: stop invalidates an in-flight shipped Event; restart restores the loadout',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);const task=rt.uiAction('event',{id:'audit-equip',eventId:'equip-launcher'}),generation=rt.generation;
    assert.ok(Object.values(rt.snapshot.instances).some(i=>i.eventId==='equip-launcher'&&i.status==='running'));
    rt.stop();await task;assert.ok(rt.generation>generation);assert.equal(rt.snapshot.phase,'EDIT');assert.equal(rt.running,false);
    assert.equal(rt.game.state.weaponId,'pistol');assert.equal(rt.snapshot.world.playerControl,null);assert.deepEqual(rt.snapshot.instances,{});
    await combat(rt,p);assert.equal(rt.game.state.weaponId,'pistol');assert.equal(rt.snapshot.variables.ammo,8);assert.equal(rt.game.count('medkit'),1);
  }finally{rt.stop();}
});

test('characterization: JSON save restores selected weapon, dead actors, inventory, timers and history without replay',async()=>{
  const p=template('shooter'),rt=runtime();
  try{
    await combat(rt,p);rt.game.damage('enemy-1',50);await settled(rt);await rt.uiAction('event',{id:'audit-launcher',eventId:'equip-launcher'});
    rt.game.shoot({direction:[0,1,0]});rt.game.command('startTimer',{timerId:'audit-alarm',seconds:5});rt.snapshot.world.positions['shooter-hero']=[2,0,2];rt.togglePause();
    const clock=rt.game.state.clock;rt.playerStep([1,0,0],.05);assert.equal(rt.game.state.clock,clock);
    const saved=JSON.parse(JSON.stringify(gameSave(rt))),variables=structuredClone(saved.variables);
    await combat(rt,p);await restoreGameSave(rt,saved);
    assert.equal(rt.snapshot.beatId,'shooter-combat');assert.equal(rt.snapshot.phase,'PLAYING');assert.equal(rt.snapshot.paused,false);
    assert.equal(rt.game.state.weaponId,'launcher');assert.equal(rt.game.state.weapons.launcher.ammo,2);assert.equal(rt.game.state.actors['enemy-1'].dead,true);
    assert.equal(rt.game.state.timers['audit-alarm'].remaining,5);assert.deepEqual(rt.game.position('shooter-hero'),[2,0,2]);
    assert.deepEqual(rt.snapshot.variables,variables);assert.deepEqual(rt.snapshot.history,saved.history);assert.equal(rt.game.count('medkit'),1);
  }finally{rt.stop();}
});

test('characterization: separate quest Event transfers with typed inputs and does not copy its original world',async()=>{
  const a=template('point-and-click'),event=a.events.find(e=>e.id==='quest-open-door'),pkg=await exportAuthoringPackage(a,event,{...metadata,kind:'event'});
  assert.equal(pkg.content.objects,undefined);assert.equal(pkg.content.events.length,1);assert.equal(pkg.content.gameplay.rules.length,0);
  const b=createEmptyProject('Event destination'),scene=b.subscenes[0];b.objects.push({id:'other-door',name:'Своя дверь',type:'Меш',active:true,subsceneId:scene.id});
  b.variables={hasKey:true,doorUnlocked:false};b.variableTypes={hasKey:'boolean',doorUnlocked:'boolean'};
  const map=Object.fromEntries(pkg.inputs.map(i=>[i.id,i.type==='variable'?i.name:i.type==='object'?'other-door':scene.entry]));
  const next=(await planPackageImport(b,pkg,map)).apply(b),copy=next.events.find(e=>e.name===event.name);
  assert.notEqual(copy.id,event.id);assert.equal(copy.groups[0].actions.find(a=>a.type==='door').target,'other-door');
  assert.equal(copy.groups[1].actions[0].value.beatId,scene.entry);assert.equal(b.variables.doorUnlocked,false);
  const rt=runtime();try{await rt.start(next,scene.entry);await rt.uiAction('event',{id:'audit-import',eventId:copy.id});assert.equal(rt.snapshot.variables.doorUnlocked,true);assert.equal(rt.snapshot.beatId,scene.entry);}finally{rt.stop();}
});

test('characterization: separate quest function imports with a different variable binding and stays editable',async()=>{
  const a=template('point-and-click'),fn=a.functions.find(f=>f.id==='quest-log'),before=JSON.stringify(fn),pkg=await exportAuthoringPackage(a,fn,{...metadata,kind:'function'});
  assert.equal(pkg.content.functions.length,1);assert.equal(pkg.content.events,undefined);assert.equal(pkg.content.objects,undefined);
  const b=createEmptyProject('Function destination');b.variables.foundItems=[];b.variableTypes={foundItems:'array'};
  const next=(await planPackageImport(b,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,'foundItems'])))).apply(b),copy=next.functions.find(f=>f.name===fn.name);
  assert.equal(copy.nodes[0].path,'global.foundItems');assert.deepEqual(copy.parameters,fn.parameters);assert.notEqual(copy.id,fn.id);
  const rt=runtime();try{await rt.start(next,next.subscenes[0].entry);assert.equal(await rt.game.callFunction(copy.id,{itemId:'local-key'}),1);assert.deepEqual(rt.snapshot.variables.foundItems,['local-key']);}finally{rt.stop();}
  copy.nodes[0].position={x:37,y:91};const exported=await exportAuthoringPackage(next,copy,{...metadata,kind:'function'});
  assert.deepEqual(exported.content.functions[0].nodes[0].position,{x:37,y:91});assert.equal(exported.metadata.sources[0].id,pkg.metadata.id);assert.equal(JSON.stringify(fn),before);assert.deepEqual(b.variables.foundItems,[]);
});

// Known defects are deliberately asserted as OBSERVED behavior, not approved requirements.
// Fixes must change these assertions explicitly alongside the audit's defect record.
test('regression D1: exporting a legacy signal callback exposes its compared object and preserves the source',async()=>{
  const a=template('shooter'),fn=a.functions.find(f=>f.id==='award-kill'),pkg=await exportAuthoringPackage(a,fn,{...metadata,kind:'function'});
  assert.equal(fn.nodes[0].condition.b,'shooter-hero');assert.equal(pkg.content.functions[0].nodes[0].condition.b.op,'get');assert.equal(pkg.inputs.some(i=>i.type==='object'),true);
  const b=createEmptyProject('Function destination');b.variables={score:0,results:{defeated:[]}};b.variableTypes={score:'integer',results:'struct'};
  b.objects.push({id:'destination-hero',name:'Свой герой',type:'Меш',active:true,subsceneId:b.subscenes[0].id});
  const next=(await planPackageImport(b,pkg,Object.fromEntries(pkg.inputs.map(i=>[i.id,i.type==='object'?'destination-hero':i.name])))).apply(b),copy=next.functions.find(f=>f.name===fn.name),rt=runtime();
  try{await rt.start(next,next.subscenes[0].entry);assert.equal(await rt.game.callFunction(copy.id,{objectId:'destination-hero'}),0);assert.deepEqual(rt.snapshot.variables.results.defeated,[]);assert.equal(await rt.game.callFunction(copy.id,{objectId:'destination-enemy'}),100);assert.deepEqual(rt.snapshot.variables.results.defeated,['destination-enemy']);}finally{rt.stop();}
});

test('regression D2: interaction and pickup both react on the same object',async()=>{
  const p=createEmptyProject('Combined interaction'),s=p.subscenes[0];p.objects.push({id:'prop',name:'Предмет',type:'Активный меш',active:true,subsceneId:s.id});
  const pickup=addMechanic(p,'pickup',{kind:'object',id:'prop'});addMechanic(p,'interaction',{kind:'object',id:'prop'});
  const rt=runtime();try{await rt.start(p,s.entry);rt.playerClick(null,'prop');await settled(rt);assert.equal(rt.snapshot.world.highlights.prop,true);assert.equal(rt.game.count(pickup.recordId),1);assert.equal(rt.snapshot.world.visible.prop,false);}finally{rt.stop();}
});

test('regression D3: save rejects pending rule work and restores a completed once reaction',async()=>{
  const p=createEmptyProject('Pending signal'),timer=addMechanic(p,'timer',{kind:'project',id:p.id}),rule=p.gameplay.rules.find(r=>r.id===timer.ruleId);p.variables.finished=false;
  p.events.push({id:'pending-event',name:'Pending',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'delay',actions:[{...makeAction('wait'),duration:.05}]},{id:'complete',actions:[makeAction('variable','finished',true)]}]});rule.actions=[{kind:'event',eventId:'pending-event',join:'FLOW_END'}];
  const rt=runtime();try{
    await rt.start(p,p.subscenes[0].entry);const task=rt.game.fireRule(rule);while(!Object.values(rt.snapshot.instances).some(i=>i.eventId==='pending-event'&&i.status==='running'))await new Promise(r=>setTimeout(r,1));
    assert.ok(Object.values(rt.snapshot.instances).some(i=>i.eventId==='pending-event'&&i.status==='running'));assert.equal(rt.snapshot.ready,true);
    assert.throws(()=>gameSave(rt),/после выполнения/);for(let n=0;n<300&&!rt.game.ruleRuntime.idle;n++){rt.game.tick(.05);await new Promise(r=>setTimeout(r,2));}await task;assert.equal(rt.snapshot.variables.finished,true);
    const saved=JSON.parse(JSON.stringify(gameSave(rt)));assert.equal(saved.world.game.rules[rule.id].fired,true);
    await restoreGameSave(rt,saved);assert.equal(rt.snapshot.variables.finished,true);assert.deepEqual(rt.snapshot.instances,{});
    rt.game.tick(.05);await settled(rt);assert.equal(rt.snapshot.variables.finished,true);
  }finally{rt.stop();}
});
