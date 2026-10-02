import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {InputSystem} from '../src/InputSystem.js';
import {defaultInputSettings,inputContextsFor,validateInputSettings} from '../src/inputModel.js';
import {createGameControls} from '../src/GameControls.js';
import {createEmptyProject,createStandardProject} from '../src/projectLifecycle.js';
import {readProject} from '../src/projectFiles.js';
import {upgradeProject,allBeats,makeAction} from '../src/studioModel.js';
import {editVariable,variableReferences} from '../src/variableModel.js';
import {PreviewRuntime} from '../src/runtime.js';
const setup=(settings=defaultInputSettings(),contexts=['gameplay'])=>{const events=[],input=new InputSystem(settings,e=>events.push(e));input.setContexts(contexts);return {input,events};};
const triggered=(events,id)=>events.filter(e=>e.actionId===id&&e.phase==='triggered');
function one(trigger='pressed',valueType='boolean'){
 return {version:1,touchMode:'auto',actions:[{id:'test',name:'Test',valueType,behavior:'event',trigger,holdSeconds:.4,consume:true}],contexts:[{id:'test',name:'Test',priority:1,bindings:[{id:'b',actionId:'test',device:'external',control:'fire',scale:valueType==='axis2d'?[1,1]:1,deadZone:0}]}]};
}
for(const [device,control]of [['keyboard','KeyE'],['gamepad-button','0'],['touch','interact']])test(device+' routes to the same interact action, without repeats',()=>{
 const {input,events}=setup();input.feed(device,control,1);input.tick(.02);input.tick(.02);assert.equal(triggered(events,'interact').length,1);
 input.feed(device,control,0);input.tick(.02);assert.equal(events.filter(e=>e.actionId==='interact').at(-1).phase,'completed');
});
test('analog magnitude, dead zone, inversion and keyboard diagonals are preserved',()=>{
 const {input,events}=setup();input.feed('gamepad-axis','0',.59);input.feed('gamepad-axis','1',-.59);input.tick(.02);
 const axis=triggered(events,'move').at(-1).value;assert.ok(Math.abs(axis[0]-.5)<1e-9);assert.ok(Math.abs(axis[1]-.5)<1e-9);
 input.feed('gamepad-axis','0',.1);input.feed('gamepad-axis','1',-.1);input.tick(.02);assert.equal(events.at(-1).phase,'completed');
 input.feed('keyboard','KeyW',1);input.feed('keyboard','KeyD',1);input.tick(.02);assert.ok(Math.abs(Math.hypot(...triggered(events,'move').at(-1).value)-1)<1e-9);
 input.feed('keyboard','KeyS',1);input.feed('keyboard','KeyA',1);input.tick(.02);assert.equal(events.at(-1).phase,'completed');
});
test('hold fires once after its threshold and early release is canceled',()=>{
 const {input,events}=setup(one('hold'),['test']);input.feed('external','fire',1);input.tick(.2);input.feed('external','fire',0);input.tick(0);assert.equal(triggered(events,'test').length,0);assert.equal(events.at(-1).phase,'canceled');
 input.feed('external','fire',1);input.tick(.2);input.tick(.2);input.tick(3);assert.equal(triggered(events,'test').length,1);input.feed('external','fire',0);input.tick(0);assert.equal(events.at(-1).phase,'completed');
});
test('release trigger does not execute on context removal, focus cancellation or unplug',()=>{
 const {input,events}=setup(one('released'),['test']);input.feed('external','fire',1,{sourceId:'pad'});input.tick(0);input.removeSource('pad',{cancel:true});input.tick(0);assert.equal(triggered(events,'test').length,0);assert.equal(events.at(-1).phase,'canceled');
 input.feed('external','fire',1);input.tick(0);input.clear();assert.equal(triggered(events,'test').length,0);
 input.feed('external','fire',1);input.tick(0);input.feed('external','fire',0);input.tick(0);assert.equal(triggered(events,'test').length,1);
 input.feed('external','fire',1);input.tick(0);input.setContexts([]);assert.equal(triggered(events,'test').length,1);
});
test('higher priority context consumes a channel, and held input must return to neutral when switching',()=>{
 const settings=one();settings.actions.push({...settings.actions[0],id:'menu',name:'Menu'});settings.contexts.push({id:'menu',name:'Menu',priority:10,bindings:[{...settings.contexts[0].bindings[0],id:'m',actionId:'menu'}]});
 const {input,events}=setup(settings,['test','menu']);input.feed('external','fire',1);input.tick(0);assert.equal(triggered(events,'test').length,0);assert.equal(triggered(events,'menu').length,1);
 input.setContexts(['test']);input.tick(.02);assert.equal(triggered(events,'test').length,0);input.feed('external','fire',0);input.tick(0);input.feed('external','fire',1);input.tick(0);assert.equal(triggered(events,'test').length,1);
 settings.actions.find(a=>a.id==='menu').consume=false;input.clear();input.setContexts(['test','menu']);input.feed('external','fire',1);input.tick(0);assert.equal(triggered(events,'test').length,2);
});
test('arbitrary adapters poll or push values, unplug cleanly, and faulty hardware cannot leave held values',()=>{
 const {input,events}=setup(one('continuous','axis2d'),['test']);let emit,stops=0,broken=false;
 const unplug=input.registerAdapter({id:'sensor',start:send=>{emit=send;},poll:send=>{if(broken)throw new Error('offline');send('external','fire',[.25,.5]);},stop:()=>stops++});
 input.tick(.02);assert.deepEqual(triggered(events,'test').at(-1).value,[.25,.5]);broken=true;input.tick(.02);assert.equal(events.at(-1).phase,'canceled');assert.equal(input.adapterErrors.get('sensor'),'offline');
 broken=false;emit('external','fire',[NaN]);input.tick(.02);assert.ok(triggered(events,'test').at(-1).value.every(Number.isFinite));unplug();unplug();assert.equal(stops,1);assert.equal(events.at(-1).phase,'canceled');input.dispose();
});
test('pointer payload survives action routing and touch can hold an axis and button simultaneously',()=>{
 const {input,events}=setup();input.feed('touch','move',[.2,.7],{sourceId:'finger1'});input.feed('touch','interact',1,{sourceId:'finger2'});input.tick(.02);assert.deepEqual(triggered(events,'move').at(-1).value,[.2,.7]);assert.equal(triggered(events,'interact').length,1);
 input.pulse('pointer','primary',{point:[1,0,2],id:'prop'});assert.deepEqual(triggered(events,'point')[0].payload,{point:[1,0,2],id:'prop'});assert.ok(input.sources.size>=2);
});
test('legacy projects gain defaults; edited mappings, context choices and custom actions survive export',()=>{
 const p=createStandardProject();assert.deepEqual(validateInputSettings(p),[]);assert.deepEqual(upgradeProject(p),p);const old=structuredClone(p);delete old.input;assert.deepEqual(readProject(old).input,defaultInputSettings());
 const beat=allBeats(p).find(b=>b.id==='rain-explore');assert.deepEqual(inputContextsFor(p,beat),['system','gameplay']);beat.inputContexts=[];assert.deepEqual(inputContextsFor(p,beat),['system']);
 p.input.contexts.find(c=>c.id==='gameplay').bindings.find(b=>b.control==='KeyW').control='KeyI';p.input.touchMode='always';assert.deepEqual(readProject(JSON.stringify(p)),p);
 const invalid=structuredClone(p);invalid.input.contexts[0].bindings[0].actionId='missing';assert.throws(()=>readProject(invalid),/действие/);
 const missing=structuredClone(p);allBeats(missing)[0].inputContexts=['gone'];assert.throws(()=>readProject(missing),/Контекст ввода удалён/);
});
test('runtime custom input runs a real event once at a time and respects readiness, pause and context scope',async t=>{
 const p=createEmptyProject(),beat=allBeats(p)[0];p.variables.shots=0;
 const event={id:'fire-event',name:'Fire',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'fire-group',actions:[{...makeAction('wait'),duration:.02}]},{id:'count-group',actions:[{...makeAction('variable','shots',1),operation:'add',valueType:'number'}]}]};p.events.push(event);
 const action={...one().actions[0],id:'fire',eventId:event.id};p.input.actions.push(action);p.input.contexts.push({id:'combat',name:'Combat',priority:30,bindings:[{id:'fire-bind',actionId:'fire',device:'external',control:'trigger',scale:1,deadZone:0}]});beat.inputContexts=['combat'];
 const rt=new PreviewRuntime({stopAll(){},tracks:new Map()});t.after(()=>rt.stop());await rt.start(p,beat.id);
 const input={actionId:'fire',phase:'triggered',value:1,device:'external'};const first=rt.inputAction(input);await rt.inputAction(input);await first;assert.equal(rt.snapshot.variables.shots,1);assert.equal(Object.keys(rt.snapshot.instances).length,1);
 rt.togglePause();await rt.inputAction(input);assert.equal(rt.snapshot.variables.shots,1);rt.togglePause();rt.snapshot.ready=false;await rt.inputAction(input);assert.equal(rt.snapshot.variables.shots,1);rt.snapshot.ready=true;
 rt.project.chapters[0].beats[0].inputContexts=[];await rt.inputAction(input);assert.equal(rt.snapshot.variables.shots,1);rt.stop();await rt.inputAction(input);assert.equal(rt.snapshot.variables.shots,1);
});
test('gamepad navigation skips disabled choices and confirms the highlighted response',async t=>{
 const p=createStandardProject(),beat=allBeats(p).find(b=>b.id==='choice1');beat.bindings=[];beat.batches={};const rt=new PreviewRuntime({stopAll(){},tracks:new Map()});t.after(()=>rt.stop());await rt.start(p,beat.id);const first=rt.snapshot.inputChoiceId;
 await rt.inputAction({actionId:'choiceNext',phase:'triggered',value:1,device:'gamepad-button'});assert.notEqual(rt.snapshot.inputChoiceId,first);const selected=rt.snapshot.inputChoiceId,next=beat.choices.find(c=>c.id===selected).next;
 await rt.inputAction({actionId:'advance',phase:'triggered',value:1,device:'gamepad-button'});assert.equal(rt.snapshot.beatId,next);
});
test('browser adapter polls analog gamepad input, releases on disconnect, and supports remapping keyboard codes',()=>{
 const previousWindow=globalThis.window,navDescriptor=Object.getOwnPropertyDescriptor(globalThis,'navigator');globalThis.window=new EventTarget();
 let pads=[{index:0,id:'Test pad',mapping:'standard',connected:true,axes:[.59,0],buttons:[{value:0,pressed:false}]}];Object.defineProperty(globalThis,'navigator',{configurable:true,value:{getGamepads:()=>pads}});
 const viewport=new EventTarget(),camera=new THREE.PerspectiveCamera();camera.lookAt(0,0,-1);camera.updateMatrixWorld();const steps=[],events=[],settings=defaultInputSettings();settings.contexts.find(c=>c.id==='gameplay').bindings.find(b=>b.control==='KeyW').control='KeyI';
 const live={mode:'game',state:{playerControl:{mode:'both'},inputActive:true,inputReady:true,inputContexts:['system','gameplay']},inputSettings:settings,onPlayerStep:v=>steps.push(v),onInputAction:e=>events.push(e)};
 const bridge=createGameControls(camera,{closest:()=>viewport},()=>live);
 try{bridge.tick(.02);assert.ok(Math.abs(steps.at(-1)[0]-.5)<1e-9);pads[0].buttons[0]={value:1,pressed:true};bridge.tick(.02);bridge.tick(.02);assert.equal(triggered(events,'interact').length,1);
  pads=[];bridge.tick(.02);assert.ok(steps.at(-1).every(v=>v===0));assert.equal(events.filter(e=>e.actionId==='interact').at(-1).phase,'canceled');
  const press=code=>{const e=new Event('keydown',{cancelable:true});e.code=code;viewport.dispatchEvent(e);return e;};assert.equal(press('KeyW').defaultPrevented,false);assert.equal(press('KeyI').defaultPrevented,true);bridge.tick(.02);assert.ok(steps.at(-1)[2]<0);
  window.dispatchEvent(new Event('blur'));bridge.tick(.02);assert.ok(steps.at(-1).every(v=>v===0));
 }finally{bridge.dispose();globalThis.window=previousWindow;if(navDescriptor)Object.defineProperty(globalThis,'navigator',navDescriptor);else delete globalThis.navigator;}
});

test('custom axes and buttons write typed variables, survive rename, and reset on pause, context cancellation and stop',async t=>{
 const p=createEmptyProject(),beat=allBeats(p)[0];p.variables.aimX=0;p.variables.aimY=0;p.variables.down=false;
 p.input.actions.push({...one('continuous','axis2d').actions[0],id:'aim',variableX:'aimX',variableY:'aimY'}, {...one().actions[0],id:'button',variable:'down'});
 p.input.contexts.push({id:'custom',name:'Custom',priority:30,bindings:[{id:'aim-bind',actionId:'aim',device:'external',control:'aim',scale:[1,1]},{id:'button-bind',actionId:'button',device:'external',control:'button',scale:1}]});beat.inputContexts=['custom'];
 assert.deepEqual(validateInputSettings(p),[]);assert.ok(variableReferences(p,'aimX').includes('aim'));assert.match(editVariable(p,'aimX',{remove:true}),/используется/);assert.equal(editVariable(p,'aimX',{name:'directionX'}),null);assert.equal(p.input.actions.find(a=>a.id==='aim').variableX,'directionX');
 const rt=new PreviewRuntime({stopAll(){},tracks:new Map()});t.after(()=>rt.stop());await rt.start(p,beat.id);
 await rt.inputAction({actionId:'aim',phase:'triggered',value:[.3,-.8],device:'external'});await rt.inputAction({actionId:'button',phase:'started',value:1,device:'external'});
 assert.equal(rt.snapshot.variables.directionX,.3);assert.equal(rt.snapshot.variables.aimY,-.8);assert.equal(rt.snapshot.variables.down,true);
 rt.togglePause();assert.equal(rt.snapshot.variables.directionX,0);assert.equal(rt.snapshot.variables.down,false);rt.togglePause();
 await rt.inputAction({actionId:'aim',phase:'triggered',value:[.7,.2],device:'external'});rt.project.chapters[0].beats[0].inputContexts=[];
 await rt.inputAction({actionId:'aim',phase:'canceled',value:[0,0],device:'external'});assert.equal(rt.snapshot.variables.directionX,0);
 rt.project.chapters[0].beats[0].inputContexts=['custom'];await rt.inputAction({actionId:'button',phase:'triggered',value:1,device:'external'});rt.stop();assert.equal(rt.snapshot.variables.down,false);
 const invalid=structuredClone(p);invalid.input.actions.find(a=>a.id==='button').variable='directionX';assert.ok(validateInputSettings(invalid).length);await rt.start(invalid,beat.id);assert.equal(rt.snapshot.phase,'ERROR');
});
test('malformed adapter values cannot produce non-finite axes',()=>{
 const {input,events}=setup(one('continuous','axis2d'),['test']);input.feed('external','fire',[NaN, .5]);input.tick(.01);assert.deepEqual(triggered(events,'test').at(-1).value,[0,.5]);
 input.feed('external','fire',[.2]);input.tick(.01);assert.deepEqual(triggered(events,'test').at(-1).value,[.2,0]);
});
