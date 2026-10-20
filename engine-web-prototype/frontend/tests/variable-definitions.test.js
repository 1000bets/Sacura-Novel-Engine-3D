import test from 'node:test';
import assert from 'node:assert/strict';
import {editVariable,variableType,variableValue,variableReferences} from '../src/variableModel.js';
import {readLogic,logicType,dataConnectionError} from '../src/logicModel.js';
const project=()=>({variables:{trust:2,other:false},events:[{id:'event',groups:[{actions:[{type:'variable',target:'trust'}]}]}],chapters:[{beats:[{id:'get',kind:'variable',variable:'trust'},{id:'set',kind:'set-variable',variable:'trust',inputs:{value:'get'}},{id:'choice',resultVariable:'trust',choices:[{condition:'trust',availability:{rules:[{variable:'trust'}]}}],bindings:[{condition:'trust',actionOverrides:{a:{target:'trust'}}}]}]}]});
test('renaming a definition preserves graph identities and updates all references atomically',()=>{
 const p=project();assert.equal(editVariable(p,'trust',{name:'Coins'}),null);
 assert.equal(p.variables.Coins,2);assert.equal('trust' in p.variables,false);
 assert.equal(p.chapters[0].beats[1].inputs.value,'get');
 assert.equal(variableReferences(p,'trust').length,0);assert.equal(variableReferences(p,'Coins').length,4);
 const before=structuredClone(p);assert.ok(editVariable(p,'Coins',{name:'other'}));assert.deepEqual(p,before);
});
test('types and defaults belong to the definition and survive serialization',()=>{
 const p=project();editVariable(p,'trust',{type:'integer',value:4.9});
 assert.equal(p.variables.trust,4);assert.equal(variableType(p,'trust'),'integer');assert.equal(logicType(p,p.chapters[0].beats[0]),'number');
 assert.equal(variableValue(p,'trust',9.8),9);assert.equal(readLogic(p,'get',{trust:7}),7);
 assert.equal(variableType(JSON.parse(JSON.stringify(p)),'trust'),'integer');
 editVariable(p,'trust',{type:'boolean',value:false});assert.equal(p.variables.trust,false);assert.equal(p.chapters[0].beats[1].valueType,'boolean');
});
test('Set exposes the assigned value without executing a second assignment',()=>{
 const p=project();assert.equal(readLogic(p,'set',{trust:12}),12);
 assert.equal(dataConnectionError(p,{source:'set',sourceHandle:'value',target:'set',targetHandle:'value'})!==null,true);
});
test('used definitions cannot be deleted, unused ones can; system names are protected',()=>{
 const p=project(),before=structuredClone(p);assert.ok(editVariable(p,'trust',{remove:true}));assert.deepEqual(p,before);
 assert.equal(editVariable(p,'other',{remove:true}),null);assert.equal('other' in p.variables,false);
 assert.ok(editVariable(p,'trust',{name:'$answer'}));assert.ok(editVariable(p,'$answer',{name:'renamed'}));
});

test('renaming a project declaration rewrites executable paths while preserving literal structures and defaults',()=>{
 const p=project(),literal={path:'global.trust',nested:{op:'get',path:'global.trust'}};
 p.functions=[{id:'fn',parameters:[{name:'data',default:structuredClone(literal)}],locals:{data:structuredClone(literal)},nodes:[{id:'read',kind:'value',value:{op:'get',path:'global.trust'}},{id:'write',kind:'set',path:'global.trust',value:3},{id:'constant',kind:'value',value:{op:'constant',value:structuredClone(literal)}},{id:'return',kind:'return',value:structuredClone(literal)},{id:'call',kind:'operation',output:'global.trust',call:{parameters:{data:{path:'global.trust'}}}}]}];
 assert.equal(editVariable(p,'trust',{name:'Coins'}),null);const f=p.functions[0];assert.equal(f.nodes[0].value.path,'global.Coins');assert.equal(f.nodes[1].path,'global.Coins');assert.deepEqual(f.nodes[2].value.value,literal);assert.deepEqual(f.nodes[3].value,literal);assert.deepEqual(f.parameters[0].default,literal);assert.deepEqual(f.locals.data,literal);assert.equal(f.nodes[4].output,'global.Coins');assert.deepEqual(f.nodes[4].call.parameters.data,{path:'global.trust'});
});
