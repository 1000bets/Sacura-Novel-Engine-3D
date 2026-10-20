import test from 'node:test';
import assert from 'node:assert/strict';
import {functionCode} from '../src/functionCode.js';
import {callBlueprint} from '../src/blueprintFunctions.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
import {registerOperation} from '../src/operationRegistry.js';
const wire=nodeId=>({op:'node',nodeId,port:'value'});
function fixture(){const p=createEmptyProject('Код без героя'),f={id:'source-function',name:'Рассчитать награду',graphVersion:1,parameters:[{name:'amount',label:'Награда',schema:{type:'integer'},default:3}],localSchema:{type:'object',properties:{factor:{type:'number',label:'Множитель',default:2}}},locals:{factor:2},entry:'return',resultSchema:{type:'number'},nodes:[{id:'multiply',kind:'value',value:{op:'mul',a:{op:'get',path:'args.amount'},b:{op:'get',path:'local.factor'}},position:{x:10,y:20}},{id:'return',kind:'return',value:wire('multiply'),position:{x:400,y:20}}]};p.functions=[f];return {p,f};}
test('text projects typed declarations and the actual formula without mutating the executable graph',async()=>{
 const {p,f}=fixture(),before=structuredClone(p),code=functionCode(p,f);assert.match(code.text,/async function Рассчитать_награду/);assert.match(code.text,/Награда = 3/);assert.match(code.text,/Награда\?: number/);assert.match(code.text,/let Множитель: number = 2;/);assert.match(code.text,/return \(Награда \* Множитель\);/);assert.ok(code.lines.some(line=>line.nodeId==='return'));assert.ok(!code.text.includes('"nodes"'));assert.deepEqual(p,before);assert.equal(await callBlueprint(p,f.id,{},p.variables,()=>{}),6);
 f.locals.factor=5;assert.match(functionCode(p,f).text,/Множитель: number = 5/);assert.equal(await callBlueprint(p,f.id,{},p.variables,()=>{}),15);
});
test('branches, shared paths, control cycles and unused nodes remain finite and visible in text',()=>{
 const {p,f}=fixture();f.entry='branch';f.nodes=[{id:'branch',kind:'branch',condition:true,trueNext:'join',falseNext:'join'},{id:'join',kind:'set',name:'Повторить',path:'local.factor',value:4,next:'branch'},{id:'unused',kind:'return',name:'Запасной результат',value:99},{id:'bad-data',kind:'value',value:wire('bad-data')}];const before=structuredClone(f),code=functionCode(p,f);assert.match(code.text,/if \(true\)/);assert.match(code.text,/else/);assert.match(code.text,/async function .*_block\(\)/);assert.match(code.text,/return await/);assert.match(code.text,/Не подключено к основному потоку/);assert.match(code.text,/return 99/);assert.match(code.text,/data cycle/);assert.ok(code.text.length<6000);assert.deepEqual(f,before);
});
test('all supported action kinds retain their parameters, work branches and effect results',()=>{
 registerOperation({id:'test.code.effect',version:1,parameters:{type:'object',properties:{amount:{type:'number',label:'Добавить'}}},result:{type:'number'},contexts:['function'],editor:{label:'Изменить счётчик'}},p=>p.amount);
 const {p,f}=fixture();p.functions.push({id:'child',name:'Проверить',parameters:[{name:'x',label:'Сколько',schema:{type:'number'},default:0}],locals:{},entry:'child-return',nodes:[{id:'child-return',kind:'return',value:1}]});p.events.push({id:'done',name:'Готово',groups:[]});f.entry='effect';f.nodes=[
  {id:'effect',kind:'operation',call:{id:'test.code.effect',version:1,parameters:{amount:7}},output:'local.factor',next:'call'},
  {id:'call',kind:'call',functionId:'child',args:{x:wire('effect')},next:'event'},
  {id:'event',kind:'event',eventId:'done',join:'FLOW_END',overrides:{message:'hello'},next:'wait'},
  {id:'wait',kind:'wait',duration:2,timeSource:'system',next:'signal'},
  {id:'signal',kind:'signal',signalId:'ready',payload:{amount:9},next:'listen'},
  {id:'listen',kind:'waitSignal',signalId:'ready',duration:4,next:'start'},
  {id:'start',kind:'start',functionId:'child',args:{x:8},next:'await'},
  {id:'await',kind:'await',handle:wire('start'),next:'cancel'},
  {id:'cancel',kind:'cancel',handle:wire('start'),next:'parallel'},
  {id:'parallel',kind:'parallel',branches:['a','b'],next:'timeout'},
  {id:'timeout',kind:'timeout',duration:3,body:'a',next:'bt'},
  {id:'bt',kind:'btSequence',children:['child'],args:{x:1},next:'selector'},
  {id:'selector',kind:'btSelector',children:[{functionId:'child'}],next:'array'},
  {id:'array',kind:'array',path:'local.items',operation:'push',value:11,next:'command'},
  {id:'command',kind:'command',command:{command:'test.code.effect',operationVersion:1,value:{amount:12}},next:'end'},
  {id:'end',kind:'return',value:wire('effect')},{id:'a',kind:'return',value:1},{id:'b',kind:'return',value:2}
 ];const code=functionCode(p,f).text;for(const pattern of [/operation\["Изменить счётчик"\]/,/Добавить: 7/,/Сколько:/,/event\["Готово"\]/,/wait\(2, "system"\)/,/signal.emit\("ready"/,/signal.wait\("ready", 4\)/,/work.start/,/work.join/,/work.cancel/,/parallel\(\[/,/timeout\(3,/,/behavior.sequence/,/behavior.selector/,/\.push\(11\)/,/Добавить: 12/])assert.match(code,pattern);assert.ok(!code.includes('unsupported'));assert.match(code,/let Изменить_счётчик_result: Value/);assert.match(code,/Множитель = Изменить_счётчик_result/);
});
test('constant structures stay literal, references use names and unsupported versions remain explicit',()=>{
 const {p,f}=fixture();f.nodes[0].value={op:'constant',value:{op:'node',nodeId:'literal-id',text:'</code><img src=x>'}};f.nodes[1].value={kind:'reference',type:'project',mode:'direct',id:p.id};f.resultSchema={type:'any'};let code=functionCode(p,f).text;assert.ok(code.includes('ref.project("Код без героя")'));assert.ok(code.includes('"nodeId":"literal-id"'));assert.ok(code.includes('</code><img src=x>'));f.graphVersion=42;const before=structuredClone(f);code=functionCode(p,f).text;assert.match(code,/Неподдерживаемая версия/);assert.ok(!code.includes('async function'));assert.deepEqual(f,before);
});
test('a function transfers A → B → C with the same readable source and independent editable defaults',async()=>{
 const {p:a,f}=fixture(),original=functionCode(a,f).text,pkg=await exportAuthoringPackage(a,f,{kind:'function',name:'Расчёт',author:'A',license:'CC0'}),b=createEmptyProject('B'),planB=await planPackageImport(b,pkg),copyB=planB.apply(b),fb=copyB.functions.find(fn=>fn.id===planB.idMap[f.id]);assert.equal(functionCode(copyB,fb).text,original);fb.locals.factor=7;assert.equal(await callBlueprint(copyB,fb.id,{},copyB.variables,()=>{}),21);assert.equal(await callBlueprint(a,f.id,{},a.variables,()=>{}),6);
 const changed=await exportAuthoringPackage(copyB,fb,{kind:'function',name:'Мой расчёт',author:'B',license:'CC0'}),c=createEmptyProject('C'),planC=await planPackageImport(c,changed),copyC=planC.apply(c),fc=copyC.functions.find(fn=>fn.id===planC.idMap[fb.id]);assert.equal(functionCode(copyC,fc).text,functionCode(copyB,fb).text);assert.equal(await callBlueprint(copyC,fc.id,{},copyC.variables,()=>{}),21);assert.equal(copyC.gameplay.playerId,'');assert.equal(functionCode(a,f).text,original);
});
