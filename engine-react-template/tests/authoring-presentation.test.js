import test from 'node:test';
import assert from 'node:assert/strict';
import {contextualAction,actionUnavailable,describeAction,ACTION_CATEGORIES} from '../src/authoringPresentation.js';
import {TYPES} from '../src/model.js';

const scene={id:'custom-scene',kind:'empty',stagingPoints:[{id:'custom-destination',label:'У входа',position:[1,0,1]}]};
const project={objects:[{id:'foreign',name:'Другой герой',type:'Персонаж',subsceneId:'other'}, {id:'custom-hero',name:'Мира',type:'Персонаж',subsceneId:scene.id,model:{animations:[{id:'wave',name:'Помахать'}]}}],subscenes:[scene],variables:{unlocked:false},events:[]};

test('library command picker covers every supported command exactly once',()=>{
  assert.deepEqual(ACTION_CATEGORIES.flatMap(c=>c.types).sort(),Object.keys(TYPES).sort());
});
test('new library actions select real scene targets, named points and character clips',()=>{
  const move=contextualAction('move',project,scene,'foreign');
  assert.equal(move.target,'custom-hero');assert.equal(move.value,'custom-destination');
  assert.equal(contextualAction('pose',project,scene).value,'wave');
  assert.deepEqual(describeAction(move,project,scene),{subject:'Мира',result:'У входа',detail:'2 с',icon:'MapPin'});
  assert.equal(project.objects[0].subsceneId,'other');
});
test('empty projects explain unavailable commands and never inherit demonstration IDs',()=>{
  const empty={...project,objects:[],variables:{}};
  assert.match(actionUnavailable('move',empty,scene),/объект/);
  assert.match(actionUnavailable('pose',empty,scene),/персонажа/);
  assert.match(actionUnavailable('variable',empty,scene),/переменную/);
  assert.equal(actionUnavailable('weather',empty,scene),'');
  assert.equal(contextualAction('move',empty,scene).target,'');
});
test('variable creation preserves type and music retains nonblocking event lifetime',()=>{
  const variable=contextualAction('variable',project,scene);
  assert.equal(variable.target,'unlocked');assert.equal(variable.value,false);assert.equal(variable.valueType,'boolean');assert.equal(variable.operation,'set');
  const music=contextualAction('music',project,scene);
  assert.equal(music.wait,'STARTED');assert.equal(music.scope,'EVENT');
});
test('prop commands prefer their real scene prop over the cast, including copied props',()=>{
  const living={...scene,kind:'living'};
  const p={...project,subscenes:[living],objects:[...project.objects,{id:'copied-door',builtin:'door',type:'Активный меш',subsceneId:scene.id},{id:'letter',type:'Активный меш',subsceneId:scene.id}]};
  assert.equal(contextualAction('door',p,living).target,'copied-door');
  assert.equal(contextualAction('visibility',p,living).target,'letter');
  assert.equal(contextualAction('highlight',p,living,'custom-hero').target,'custom-hero');
});
