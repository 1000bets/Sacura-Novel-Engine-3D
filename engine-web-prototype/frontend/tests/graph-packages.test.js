import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {ensureGameplay} from '../src/gameplayModel.js';
import {exportAuthoringPackage,planPackageImport,readAuthoringPackage} from '../src/mechanicPackages.js';
import {callBlueprint} from '../src/blueprintFunctions.js';
import {executeOperation,operationDefinition,registerOperation} from '../src/operationRegistry.js';
import {openOperationSource,operationSourceUrl} from '../src/operationSource.js';
import {readProject} from '../src/projectFiles.js';
import {source,fixture,bindings,conditionOf} from './fixtures/packageDoor.js';
import {duplicateInstance,createFromFunction,copyDefinition} from '../src/authoredMechanics.js';
import {descriptionFor,validateProjectOperationCalls} from '../src/mechanicDescriptions.js';
import {reference} from '../src/valueSchemas.js';
import {renameFunctionParameter} from '../src/functionSignatures.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {saveCompatibility} from '../src/saveCompatibility.js';
const meta={kind:'function',name:'Награда с расширением',author:'Тест',license:'CC0-1.0'};
const wire=nodeId=>({op:'node',nodeId,port:'value'});
function reward(p){const f={id:'reward-graph',name:'Награда с расширением',graphVersion:1,parameters:[],locals:{},resultSchema:{type:'number'},entry:'scale',nodes:[
 {id:'scale',kind:'operation',call:{id:'sacura.reward.scale',version:1,parameters:{reward:40,multiplier:3}},next:'return',position:{x:0,y:0}},
 {id:'bonus',kind:'value',value:{op:'add',a:wire('scale'),b:5},position:{x:350,y:0},comment:'Результат расширения передаётся проводом'},
 {id:'return',kind:'return',value:wire('bonus'),position:{x:700,y:0}},
 ]};p.functions.push(f);return f;}
test('installed source operation returns into a real data wire, declares code dependency and transfers without code',async()=>{
 const p=createMechanicsProject('A'),f=reward(p),before=JSON.stringify(p),pkg=await exportAuthoringPackage(p,f,meta);
 assert.deepEqual(pkg.compatibility.requiredEngineModules,[{id:'sacura.reward-extension',version:1}]);assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='function.typed-graph'));
 assert.equal(JSON.stringify(p),before);assert.ok(!JSON.stringify(pkg).includes('=>reward*multiplier'));
 const target=createMechanicsProject('B'),plan=await planPackageImport(target,pkg),b=plan.apply(target),copy=b.functions.find(x=>x.id===plan.idMap[f.id]);
 assert.equal(copy.nodes.find(n=>n.kind==='return').value.nodeId,plan.idMap.bonus);
 const result=await callBlueprint(b,copy.id,{},b.variables,call=>executeOperation({id:call.command,version:call.operationVersion,parameters:call.value},{project:b,kind:'function',evaluate:v=>v}));assert.equal(result,125);
 const bad=structuredClone(pkg);bad.compatibility.requiredEngineModules[0].version=99;const untouched=JSON.stringify(target);await assert.rejects(planPackageImport(target,bad),/недоступный модуль/);assert.equal(JSON.stringify(target),untouched);
});
test('source link uses existing development editor bridge and identifies a real installed file',async()=>{
 const d=operationDefinition('sacura.reward.scale',1),src=await readFile(d.source.path,'utf8');assert.match(src,/reward\*multiplier/);assert.match(operationSourceUrl(d),/^\/__open-in-editor\?file=src%2Fextensions%2Freward.js%3A/);
 let requested;await openOperationSource(d,{development:true,request:async url=>{requested=url;return {ok:true};}});assert.equal(requested,operationSourceUrl(d));await assert.rejects(openOperationSource(d,{development:false}),/npm run dev/);
});
test('private definition copies remap handler admission, typed data wires and Event bindings without touching other instances',()=>{
 const {p,m,d}=source();ensureGameplay(p);const before=JSON.stringify(d),second=duplicateInstance(p,m),local=duplicateInstance(p,m,{independent:true}),privateD=descriptionFor(p,local.definitionId),shared=descriptionFor(p,second.definitionId);
 assert.equal(JSON.stringify(d),before);assert.equal(shared,d);assert.notEqual(privateD.id,d.id);
 const original=conditionOf(p,d.machine.transitions[0]),copy=conditionOf(p,privateD.machine.transitions[0]);assert.notEqual(copy.id,original.id);
 const originalIds=new Set(original.nodes.map(n=>n.id));for(const n of copy.nodes){assert.ok(!originalIds.has(n.id));const refs=JSON.stringify(n);for(const id of originalIds)assert.ok(!refs.includes('"nodeId":"'+id+'"'));}
 copy.nodes.find(n=>n.kind==='return').value=false;assert.notEqual(JSON.stringify(original.nodes),JSON.stringify(copy.nodes));
});
test('unknown graph/project versions reject without altering source; migrated project reopening is deterministic',()=>{
 const p=createMechanicsProject('Новая модель');reward(p);const first=readProject(p),second=readProject(first);assert.deepEqual(first,second);const bad=structuredClone(p);bad.functions[0].graphVersion=99;const before=JSON.stringify(bad);assert.throws(()=>readProject(bad),/Версия графа/);assert.equal(JSON.stringify(bad),before);
 const future=structuredClone(first);future.authoringGraphVersion=99;assert.throws(()=>readProject(future),/Версия авторской/);
});
test('preflight rejects a broken new data link before application and preserves the whole input package',async()=>{
 const p=createMechanicsProject('Ссылка'),f=reward(p),pkg=await exportAuthoringPackage(p,f,meta),bad=structuredClone(pkg);bad.content.functions[0].nodes.find(n=>n.kind==='return').value.nodeId='missing-node';const before=JSON.stringify(bad);await assert.rejects(readAuthoringPackage(bad),/удалён|недоступен|отсутств/);assert.equal(JSON.stringify(bad),before);
});
test('renaming a function port updates value readers and callers, while literal values stay independent',async()=>{
 const p=createMechanicsProject('Имена портов'),f={id:'scale-port',name:'Расчёт',graphVersion:1,parameters:[{name:'amount',type:'number',default:2}],locals:{text:'args.amount'},entry:'return-port',nodes:[{id:'value-port',kind:'value',value:{op:'get',path:'args.amount'}},{id:'return-port',kind:'return',value:wire('value-port')}]};
 p.functions.push(f,{id:'caller',name:'Вызов',graphVersion:1,parameters:[],locals:{},entry:'invoke',nodes:[{id:'invoke',kind:'call',functionId:f.id,args:{amount:7},next:'result'},{id:'result',kind:'return',value:wire('invoke')}]});
 renameFunctionParameter(p,f.id,'amount','reward');assert.equal(f.parameters[0].name,'reward');assert.equal(f.nodes[0].value.path,'args.reward');assert.equal(f.locals.text,'args.amount');assert.deepEqual(p.functions[1].nodes[0].args,{reward:7});assert.equal(await callBlueprint(p,'caller',{},p.variables,()=>{}),7);
 assert.throws(()=>renameFunctionParameter(p,f.id,'reward','__proto__'),/уникальное/);
});
test('graph layout keeps save compatibility; changed implementation rejects before replacing a running world',async()=>{
 const p=createMechanicsProject('Сохранение графов');reward(p);const before=saveCompatibility(p);p.functions[0].viewport={x:300,y:-50,zoom:.5};p.functions[0].nodes[0].position={x:9,y:10};assert.deepEqual(saveCompatibility(p),before);
 const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('Аудио не нужно');}));try{await rt.start(p);const saved=gameSave(rt),generation=rt.generation,world=rt.snapshot.world;const changed=structuredClone(rt.project);changed.functions.find(f=>f.id==='reward-graph').nodes.find(n=>n.id==='bonus').value.b=99;
  await assert.rejects(restoreGameSave(rt,saved,changed),/Логика или состав проекта изменились/);assert.equal(rt.generation,generation);assert.equal(rt.snapshot.world,world);assert.equal(rt.running,true);
 }finally{rt.stop();}
});

test('package validation respects operation context and rejects an unavailable function operation before import',async()=>{
 for(const kind of ['function','rule'])registerOperation({id:'test.package.'+kind,version:1,parameters:{type:'object',properties:{}},result:{type:'number'},contexts:[kind],editor:{label:kind}},()=>3);
 const p=createMechanicsProject('Контексты'),f=reward(p);f.nodes[0].call={id:'test.package.function',version:1,parameters:{}};
 const pkg=await exportAuthoringPackage(p,f,meta),b=createMechanicsProject('B');assert.equal((await planPackageImport(b,pkg)).apply(b).functions.find(x=>x.name===f.name).nodes[0].call.id,'test.package.function');
 f.nodes[0].call.id='test.package.rule';assert.ok(validateProjectOperationCalls(p).some(x=>x.detail.includes('контексте')));await assert.rejects(exportAuthoringPackage(p,f,meta),/контексте/);
});

test('a private definition clones a typed Event dependency and preserves literal identities',async()=>{
 const p=createMechanicsProject('Личная логика');p.events.push({id:'private-event',name:'Эффект',groups:[]});
 const literal={id:'return-original',nodeId:'return-original',functionId:'not-a-dependency',op:'user-data'};
 const f={id:'private-function',name:'Событие по ссылке',graphVersion:1,parameters:[],locals:{literal,event:reference('event','private-event')},entry:'return-original',nodes:[{id:'literal',kind:'value',value:{op:'constant',value:{literal,event:reference('event','private-event')}}},{id:'return-original',kind:'return',value:wire('literal')}]};p.functions.push(f);
 const d=createFromFunction(p,f.id),copy=copyDefinition(p,d.id),cloned=p.functions.find(f=>f.id===copy.logic.functionId),value=cloned.nodes.find(n=>n.kind==='value').value.value;
 assert.notEqual(value.event.id,'private-event');assert.ok(p.events.some(e=>e.id===value.event.id));assert.deepEqual(value.literal,literal);
 assert.equal(cloned.locals.event.id,value.event.id);assert.deepEqual(cloned.locals.literal,literal);
 p.events.find(e=>e.id===value.event.id).name='Личный эффект';assert.equal(p.events.find(e=>e.id==='private-event').name,'Эффект');
});

test('constant data survives package transfer without being mistaken for operations or executable expressions',async()=>{
 const p=createMechanicsProject('Данные'),payload={id:'return-data',nodeId:'return-data',functionId:'not-a-function',op:'custom-format',kind:'operation',call:{id:'not.an.operation',version:99}};
 const f={id:'data-function',name:'Вернуть данные',graphVersion:1,parameters:[],locals:{},entry:'return-data',nodes:[{id:'value-data',kind:'value',value:{op:'constant',value:payload}},{id:'return-data',kind:'return',value:wire('value-data')}]};p.functions.push(f);
 const pkg=await exportAuthoringPackage(p,f,meta),b=createMechanicsProject('B'),plan=await planPackageImport(b,pkg),next=plan.apply(b);assert.equal(pkg.compatibility.requiredEngineModules.length,0);
 assert.deepEqual(await callBlueprint(next,plan.idMap[f.id],{},next.variables,()=>{}),payload);
});

test('renaming a reference parameter updates dependent value schemas but leaves literal expressions unchanged',()=>{
 const p=createMechanicsProject('Зависимые типы'),f={id:'dependent',name:'Записать значение',graphVersion:1,parameters:[{name:'variable',schema:{type:'reference',referenceType:'variable'}},{name:'value',schema:{type:'variableValue',parameter:'variable'}}],locals:{},entry:'r',nodes:[{id:'literal',kind:'value',value:{op:'constant',value:{op:'get',path:'args.variable'}}},{id:'r',kind:'return',value:wire('literal')}]};p.functions.push(f);
 const d=createFromFunction(p,f.id);renameFunctionParameter(p,f.id,'variable','destination');assert.equal(f.parameters[1].schema.parameter,'destination');assert.equal(d.parameters.properties.value.parameter,'destination');assert.deepEqual(f.nodes[0].value.value,{op:'get',path:'args.variable'});
});

test('a migrated legacy function keeps standalone scratch state separate from persistent mechanic state',async()=>{
 const p=createMechanicsProject('Старая функция'),f={id:'legacy-state',name:'Счётчик',parameters:[],locals:{count:4},localSchema:{type:'object',properties:{count:{type:'number'}}},entry:'write',nodes:[{id:'write',kind:'set',path:'local.count',value:{op:'add',a:{op:'get',path:'local.count'},b:1},next:'result'},{id:'result',kind:'return',value:{op:'get',path:'local.count'}}]};p.functions.push(f);createFromFunction(p,f.id);const migrated=readProject(p),run=options=>callBlueprint(migrated,f.id,{},migrated.variables,()=>{},options);
 assert.equal(await run(),5);assert.equal(await run(),5);const a={count:4},b={count:20};assert.equal(await run({localState:a}),5);assert.equal(await run({localState:a}),6);assert.equal(await run({localState:b}),21);assert.equal(a.count,6);assert.equal(await run(),5);
});
