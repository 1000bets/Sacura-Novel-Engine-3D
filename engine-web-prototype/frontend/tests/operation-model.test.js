import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {registerOperation,operationDefinitions,validateOperationCall,executeOperation} from '../src/operationRegistry.js';
import {reference,referenceRecords,resolveReference,validateValue,variableKey} from '../src/valueSchemas.js';
import {ensureGameplay,newRule} from '../src/gameplayModel.js';
import {createEmptyProject,copyProjectAs} from '../src/projectLifecycle.js';
import {readProject} from '../src/projectFiles.js';
import {validateStudio,upgradeProject} from '../src/studioModel.js';
import {editVariable} from '../src/variableModel.js';
import {createSignalDefinition,createMechanicDefinition,addMechanicInstance,ensureMechanicDescriptions,validateMechanicDescriptions,validateProjectOperationCalls,mechanicTransferContract} from '../src/mechanicDescriptions.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {makeAction} from '../src/model.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {exportAuthoringPackage,readAuthoringPackage} from '../src/mechanicPackages.js';
import './fixtures/registeredOperation.js';
const rt=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));
const project=()=>{const p=createEmptyProject('Без героя');p.variables={count:0,other:0,finished:false};p.variableTypes={count:'integer',other:'integer',finished:'boolean'};ensureGameplay(p);return p;};
const variable=(p,key)=>reference('variable',p.gameplay.variableBindings.find(v=>v.key===key).id);
const call=(id,parameters,version=1)=>({id,version,parameters});
const commandNode=(id,op,parameters,next='',output)=>({id,kind:'command',command:{command:op,operationVersion:1,value:parameters},next,...(output?{output}:{})});
const event=(id,actions)=>({id,name:id,owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:id+'-group',actions}]});
async function settled(runtime){for(let i=0;i<100&&runtime.game.flights.size;i++)await new Promise(r=>setTimeout(r,5));assert.equal(runtime.game.flights.size,0);assert.equal(runtime.snapshot.error,null);}

test('operation descriptors separate immutable serializable metadata from registered executors',async()=>{
 const definition={id:'test.defaults.echo',version:1,parameters:{type:'object',properties:{value:{type:'integer',default:3}}},result:{type:'integer'},contexts:['function'],editor:{label:'Вернуть число'}};
 const d=registerOperation(definition,p=>p.value);definition.parameters.properties.value.default=99;
 assert.equal(JSON.parse(JSON.stringify(d)).parameters.properties.value.default,3);
 assert.equal(await executeOperation(call(d.id,{}),{kind:'function'}),3);
 assert.throws(()=>registerOperation(d,p=>p.value),/уже зарегистрирована/);
 assert.throws(()=>registerOperation({...d,id:'Unscoped'},()=>0),/namespaced/);
 assert.throws(()=>registerOperation({...d,id:'test.closure',editor:{label:'Bad',render(){}}},()=>0),/сериализуемые/);
 assert.throws(()=>registerOperation({...d,id:'test.invalid-default',parameters:{type:'object',properties:{value:{type:'integer',default:'3'}}}},()=>0),/по умолчанию/);
 registerOperation({...d,version:2},()=>4);assert.equal(await executeOperation(call(d.id,{},2),{kind:'function'}),4);
 await assert.rejects(executeOperation(call(d.id,{}),{kind:'event'}),/контексте/);
});

test('number, boolean, string, vector and typed collection reject coercion and malformed values',()=>{
 for(const [schema,good,bad]of [[{type:'number'},2,'2'],[{type:'integer'},2,2.5],[{type:'boolean'},false,0],[{type:'string'},'2',2],[{type:'vector',size:3},[1,2,3],[1,2,Infinity]],[{type:'collection',items:{type:'integer'}},[1,2],[1,'2']]]){assert.deepEqual(validateValue(schema,good),[]);assert.ok(validateValue(schema,bad).length);}
 assert.ok(validateValue({type:'number'},NaN).length);assert.ok(validateValue({type:'vector',size:3},[1,2]).length);
 assert.ok(validateValue({type:'object',properties:{yes:{type:'boolean'}}},{yes:true,extra:1}).length);
});

test('all typed references resolve existing identities and never user labels',()=>{
 const p=project();p.objects.push({id:'object-id',name:'Имя объекта'});p.functions.push({id:'function-id',name:'Функция'});p.events.push(event('event-id',[]));p.input.actions.push({id:'input-id',name:'Кнопка'});
 createSignalDefinition(p,{id:'user.signal.example',name:'Мой сигнал'});
 for(const type of ['object','scene','variable','event','function','widget','inputAction','signal']){const record=referenceRecords(p,type)[0];assert.ok(record);assert.equal(resolveReference(reference(type,record.id),type,{project:p}),record.id);assert.throws(()=>resolveReference(reference(type,'missing'),type,{project:p}),/недоступна/);}
 assert.throws(()=>resolveReference(reference('object','Имя объекта'),'object',{project:p}),/недоступна/);
 const ref=variable(p,'count');assert.notEqual(ref.id,'count');assert.equal(editVariable(p,'count',{name:'Свой счётчик'}),null);assert.equal(variableKey(p,ref.id),'Свой счётчик');
});

test('contextual self/source/target, tags, internal and external references use project objects',async()=>{
 const p=project();p.objects.push({id:'a',tags:['switch']},{id:'b',tags:['switch']},{id:'c',tags:['other']});
 const context={project:p,scope:{kind:'object',id:'a'},sourceId:'b',targetId:'c'};
 for(const [role,id]of [['self','a'],['source','b'],['target','c']])assert.equal(resolveReference({kind:'reference',type:'object',mode:'context',role},'object',context),id);
 assert.equal(resolveReference(reference('object','a','internal'),'object',context),'a');
 assert.equal(resolveReference({kind:'reference',type:'object',mode:'external',parameter:'door'},'object',{...context,bindings:{door:reference('object','b')}}),'b');
 assert.throws(()=>resolveReference({kind:'reference',type:'object',mode:'context',role:'self'},'object',{project:p,scope:{kind:'project',id:p.id}}),/нет объекта/);
 const group={kind:'reference',type:'object',mode:'tag',tag:'switch'};assert.equal(await executeOperation(call('test.objects.count',{objects:group,factor:2}),{...context,kind:'rule'}),4);
 assert.ok(validateValue({type:'reference',referenceType:'object'},group,context).length);
 assert.throws(()=>resolveReference({kind:'reference',type:'object',mode:'external',parameter:'door'},'object',context),/Назначьте/);
});

test('the slice uses registered set/signal/compare/Event with the existing function and Event executor',async()=>{
 const p=project(),signal=createSignalDefinition(p,{id:'user.signal.progress',name:'Счётчик изменён',payloadSchema:{type:'object',properties:{amount:{type:'integer',default:1}}}});
 p.events.push(event('finish',[makeAction('variable','finished',true)]));
 p.gameplay.rules.push({...newRule(),trigger:signal.id,once:false,conditionOperation:call('sacura.condition.compare',{left:{op:'get',path:'global.count'},operator:'gt',right:1}),operation:call('sacura.event.run',{event:reference('event','finish')})});
 p.functions.push({id:'slice',name:'Срез',parameters:[],locals:{equal:false},entry:'set',nodes:[commandNode('set','sacura.variable.set',{variable:variable(p,'count'),value:2},'compare'),commandNode('compare','sacura.condition.compare',{left:{op:'get',path:'global.count'},operator:'eq',right:2},'signal','local.equal'),commandNode('signal','sacura.signal.emit',{signal:reference('signal',signal.id),payload:{amount:1}},'return'),{id:'return',kind:'return',value:{op:'get',path:'local.equal'}}]});
 ensureGameplay(p);assert.deepEqual(validateStudio(p),[]);
 const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);assert.equal(await runtime.game.callFunction('slice'),true);await settled(runtime);assert.equal(runtime.snapshot.variables.count,2);assert.equal(runtime.snapshot.variables.finished,true);assert.ok(Object.values(runtime.snapshot.instances).some(i=>i.eventId==='finish'));assert.equal(p.variables.count,0);assert.equal(p.gameplay.playerId,'');assert.deepEqual(p.objects,[]);}finally{runtime.stop();}
});

test('registered operations run from an ordinary gameplay action and a widget with the same validation',async()=>{
 const p=project();p.events.push(event('set',[{...makeAction('gameplay'),command:'sacura.variable.set',operationVersion:1,value:{variable:variable(p,'count'),value:7}}]));const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await runtime.uiAction('event',{id:'button',eventId:'set'});assert.equal(runtime.snapshot.variables.count,7);await runtime.uiAction('gameplay',{id:'widget',command:'sacura.variable.set',value:{variable:variable(p,'count'),value:8}});assert.equal(runtime.snapshot.variables.count,8);await assert.rejects(runtime.game.command('sacura.variable.set',{variable:variable(p,'count'),value:'8'}),/целое число/);assert.equal(runtime.snapshot.variables.count,8);}finally{runtime.stop();}
});

function shared(p){
 const schema={type:'object',properties:{counter:{type:'reference',referenceType:'variable',label:'Счётчик'}}};
 p.functions.push({id:'shared-function',name:'Общая функция',parameters:[{name:'counter',type:'struct',schema:schema.properties.counter}],locals:{calls:0},entry:'increment',nodes:[{id:'increment',kind:'set',path:'local.calls',value:{op:'add',a:{op:'get',path:'local.calls'},b:1},next:'write'},commandNode('write','sacura.variable.set',{variable:{op:'get',path:'args.counter'},value:{op:'get',path:'local.calls'}},'return'),{id:'return',kind:'return',value:{op:'get',path:'local.calls'}}]});
 return createMechanicDefinition(p,{id:'user.mechanic.counter',name:'Счётчик сигналов',parameters:schema,stateSchema:{type:'object',properties:{calls:{type:'integer',default:0}}},logic:{functionId:'shared-function'}});
}
test('one editable definition has independent project/object/scene instances and runtime state',async()=>{
 const p=project(),d=shared(p);p.objects.push({id:'prop',name:'Предмет'});
 const a=addMechanicInstance(p,d.id,{kind:'project',id:p.id},{counter:variable(p,'count')}),b=addMechanicInstance(p,d.id,{kind:'scene',id:p.subscenes[0].id},{counter:variable(p,'other')});
 const c=addMechanicInstance(p,d.id,{kind:'object',id:'prop'},{counter:variable(p,'count')});p.gameplay.rules.find(r=>r.id===c.ruleId).enabled=false;
 assert.deepEqual(validateStudio(p),[]);const before=JSON.stringify(d),runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await settled(runtime);assert.equal(runtime.snapshot.variables.count,1);assert.equal(runtime.snapshot.variables.other,1);assert.notEqual(runtime.game.state.mechanicStates[a.id],runtime.game.state.mechanicStates[b.id]);await runtime.game.callMechanic(a.id,{});assert.equal(runtime.game.state.mechanicStates[a.id].calls,2);assert.equal(runtime.game.state.mechanicStates[b.id].calls,1);assert.equal(JSON.stringify(d),before);assert.equal(p.functions[0].locals.calls,0);}finally{runtime.stop();}
});

test('instance state survives the existing JSON save path without replaying startup',async()=>{
 const p=project(),d=shared(p),m=addMechanicInstance(p,d.id,{kind:'project',id:p.id},{counter:variable(p,'count')}),runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await settled(runtime);const saved=JSON.parse(JSON.stringify(gameSave(runtime)));await runtime.game.callMechanic(m.id,{});await restoreGameSave(runtime,saved);assert.equal(runtime.game.state.mechanicStates[m.id].calls,1);assert.equal(runtime.snapshot.variables.count,1);await runtime.game.callMechanic(m.id,{});assert.equal(runtime.game.state.mechanicStates[m.id].calls,2);}finally{runtime.stop();}
});

test('migration is deterministic, non-destructive and idempotent for real legacy templates',()=>{
 for(const name of ['shooter','point-and-click']){const raw=JSON.parse(readFileSync(new URL('./fixtures/legacy-'+name+'.sacura-template.json',import.meta.url),'utf8')).project,before=JSON.stringify(raw),a=readProject(raw),b=readProject(raw);assert.equal(JSON.stringify(raw),before);assert.deepEqual(a,b);assert.deepEqual(upgradeProject(a),a);ensureGameplay(a);assert.deepEqual(a,b);assert.equal(a.gameplay.mechanicSchemaVersion,1);assert.equal(a.gameplay.playerId,raw.gameplay.playerId);assert.deepEqual(a.events.slice(0,raw.events.length),raw.events);}
});

test('unknown operation/definition/schema versions preserve authored data with diagnostics',()=>{
 const p=project();p.gameplay.mechanics.push({id:'unknown-instance',type:'definition',definitionId:'other.mechanic.future',definitionVersion:7,scope:{kind:'project',id:p.id},values:{message:'Сохранить'}});p.events.push(event('unknown',[{...makeAction('gameplay'),command:'other.operation.future',operationVersion:3,value:{keep:true}}]));const loaded=readProject(p);assert.deepEqual(loaded.gameplay.mechanics,p.gameplay.mechanics);assert.deepEqual(loaded.events.find(x=>x.id==='unknown'),p.events.find(x=>x.id==='unknown'));assert.ok(validateStudio(loaded).some(i=>i.detail.includes('недоступна')));assert.ok(validateMechanicDescriptions(loaded).some(i=>i.detail.includes('Неизвестное')));
 loaded.gameplay.mechanicSchemaVersion=99;const before=JSON.stringify(loaded);ensureMechanicDescriptions(loaded);assert.equal(JSON.stringify(loaded),before);assert.ok(validateMechanicDescriptions(loaded).some(i=>i.detail.includes('Версия')));
});

test('duplicate definition, instance, signal and variable reference identities are diagnosed',()=>{
 const p=project(),d=shared(p);assert.throws(()=>createMechanicDefinition(p,d),/уникальный/);createSignalDefinition(p,{id:'user.signal.unique',name:'Сигнал'});assert.throws(()=>createSignalDefinition(p,{id:'user.signal.unique',name:'Другой'}),/уникальный/);
 const m=addMechanicInstance(p,d.id,{kind:'project',id:p.id},{counter:variable(p,'count')});p.gameplay.mechanics.push(structuredClone(m));p.gameplay.variableBindings.push(structuredClone(p.gameplay.variableBindings[0]));assert.ok(validateMechanicDescriptions(p).filter(i=>i.detail.includes('ID')).length>=2);
});

test('portable contract separates internal definitions, external typed parameters and engine requirements',()=>{
 const p=project(),d=shared(p),contract=mechanicTransferContract(p,d);assert.deepEqual(contract.internal,[{type:'function',id:'shared-function'}]);assert.deepEqual(contract.external,[{parameter:'counter',type:'variable'}]);assert.deepEqual(contract.requiredOperations,[{id:'sacura.variable.set',version:1}]);assert.equal(contract.schemaVersion,1);assert.throws(()=>createMechanicDefinition(p,{...d,id:'user.mechanic.code',logic:{functionId:'shared-function',execute(){}}}),/сериализуемые/);
});

test('a single registration module runs through the central host without extending its command switch',async()=>{
 const p=project();p.objects.push({id:'a',tags:['targets']},{id:'b',tags:['targets']});const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);assert.ok(operationDefinitions().some(d=>d.id==='test.objects.count'));assert.equal(await runtime.game.command('test.objects.count',{objects:{kind:'reference',type:'object',mode:'tag',tag:'targets'},factor:3}),6);}finally{runtime.stop();}
});

test('comparison validation and function/rule/project checks share the registry schema',()=>{
 const p=project();const invalid=call('sacura.condition.compare',{left:1,right:true,operator:'eq'});assert.ok(validateOperationCall(invalid,{project:p}).length);
 p.gameplay.rules.push({...newRule(),conditionOperation:call('sacura.variable.set',{variable:variable(p,'count'),value:1})});assert.ok(validateProjectOperationCalls(p).some(i=>i.detail.includes('логическое')));
 p.functions.push({id:'bad',parameters:[],locals:{},entry:'node',nodes:[commandNode('node','sacura.condition.compare',{left:1,right:2,operator:'bad'})]});assert.ok(validateProjectOperationCalls(p).some(i=>i.detail.includes('списка')));
});

test('package preflight validates registered operations and leaves the project unchanged on failure',async()=>{
 const p=project();p.functions.push({id:'portable',name:'Сравнение',parameters:[],locals:{},entry:'node',nodes:[commandNode('node','sacura.condition.compare',{left:1,right:2,operator:'eq'})]});const before=JSON.stringify(p),pkg=await exportAuthoringPackage(p,p.functions[0],{kind:'function',name:'Сравнение',author:'Test',license:'CC0'});assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='operation.sacura.condition.compare'));await readAuthoringPackage(pkg);pkg.content.functions[0].nodes[0].command.value.operator='javascript';await assert.rejects(readAuthoringPackage(pkg),/списка/);assert.equal(JSON.stringify(p),before);
});

test('stopping an awaited registered Event cancels its caller before later commands',async()=>{
 const p=project();p.events.push(event('wait',[{...makeAction('wait'),duration:.05}]));p.functions.push({id:'await',name:'Ожидание',parameters:[],locals:{},entry:'wait',nodes:[commandNode('wait','sacura.event.run',{event:reference('event','wait')},'write'),commandNode('write','sacura.variable.set',{variable:variable(p,'count'),value:9})]});const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);const task=runtime.game.callFunction('await');await new Promise(r=>setTimeout(r,5));runtime.stop();await assert.rejects(task,/CANCELLED|INSTANCE_STOPPED/);assert.equal(runtime.snapshot.variables.count,0);}finally{runtime.stop();}
});

test('copying a project preserves existing object IDs and rebinds project placements only',()=>{
 const p=project(),d=shared(p),m=addMechanicInstance(p,d.id,{kind:'project',id:p.id},{counter:variable(p,'count')});p.objects.push({id:'existing-object'});const copy=copyProjectAs(p,'Копия');assert.notEqual(copy.id,p.id);assert.equal(copy.gameplay.mechanics.find(x=>x.id===m.id).scope.id,copy.id);assert.deepEqual(copy.objects,p.objects);assert.deepEqual(validateMechanicDescriptions(copy),[]);
});

test('typed function parameters validate vectors, defaults and rule arguments with the shared schema',async()=>{
 const p=project();p.functions.push({id:'vector',name:'Вектор',parameters:[{name:'direction',schema:{type:'vector',size:3,default:[1,0,0]}}],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:{op:'get',path:'args.direction'}}]});const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);assert.deepEqual(await runtime.game.callFunction('vector'),[1,0,0]);await assert.rejects(runtime.game.callFunction('vector',{direction:[1,'0',0]}),/вектор/);}finally{runtime.stop();}
 p.gameplay.rules.push({...newRule(),functionId:'vector',args:{direction:[1,'0',0]}});assert.ok(validateStudio(p).some(i=>i.detail.includes('вектор')));
});

test('execution checks evaluated expression types, result schemas and user signal payloads before effects',async()=>{
 const p=project(),runtime=rt(),signal=createSignalDefinition(p,{id:'user.signal.typed',name:'Типизированный сигнал',payloadSchema:{type:'object',properties:{enabled:{type:'boolean'}}}});try{await runtime.start(p,p.subscenes[0].entry);await assert.rejects(runtime.game.command('sacura.variable.set',{variable:variable(p,'count'),value:{op:'get',path:'global.finished'}}),/целое число/);assert.equal(runtime.snapshot.variables.count,0);await assert.rejects(runtime.game.command('sacura.signal.emit',{signal:reference('signal',signal.id),payload:{enabled:1}}),/тип/);registerOperation({id:'test.result.invalid',version:1,parameters:{type:'object',properties:{}},result:{type:'integer'},contexts:['event'],editor:{label:'Неправильный результат'}},()=>true);await assert.rejects(runtime.game.command('test.result.invalid',{}),/Результат/);}finally{runtime.stop();}
});

test('custom signal recursion is diagnosed without a competing scheduler or a blocked rule queue',async()=>{
 const p=project(),signal=createSignalDefinition(p,{id:'user.signal.loop',name:'Цикл'});p.gameplay.rules.push({...newRule(),trigger:signal.id,once:false,operation:call('sacura.signal.emit',{signal:reference('signal',signal.id),payload:{}})});const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await runtime.game.command('sacura.signal.emit',{signal:reference('signal',signal.id),payload:{}});await settled(runtime);assert.match(runtime.snapshot.hint,/сам себя/);}finally{runtime.stop();}
});

test('unknown operation versions including zero never execute the version-one implementation',async()=>{
 const p=project(),runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await assert.rejects(runtime.game.command('sacura.variable.set',{variable:variable(p,'count'),value:9},{version:0}),/недоступна/);assert.equal(runtime.snapshot.variables.count,0);}finally{runtime.stop();}
});

test('a shared definition owns its signal subscription and the instance keeps only its placement and values',async()=>{
 const p=project(),d=shared(p),signal=createSignalDefinition(p,{id:'user.signal.rebound',name:'Изменённый сигнал'}),m=addMechanicInstance(p,d.id,{kind:'project',id:p.id},{counter:variable(p,'count')});d.when=signal.id;const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await settled(runtime);assert.equal(runtime.snapshot.variables.count,0);await runtime.game.command('sacura.signal.emit',{signal:reference('signal',signal.id),payload:{}});assert.equal(runtime.game.state.mechanicStates[m.id].calls,1);}finally{runtime.stop();}
});

test('native variable editing cannot delete a variable used through a stable typed reference',()=>{
 const p=project();p.events.push(event('variable-user',[{...makeAction('gameplay'),command:'sacura.variable.set',value:{variable:variable(p,'count'),value:1}}]));assert.match(editVariable(p,'count',{remove:true}),/используется/);assert.equal(p.variables.count,0);
});

test('typed references in instance state remain native project references through save and restore',async()=>{
 const p=project();p.objects.push({id:'held-object'});p.functions.push({id:'state-function',name:'Состояние',parameters:[],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:0}]});const held=reference('object','held-object');const d=createMechanicDefinition(p,{name:'Ссылка в состоянии',parameters:{type:'object',properties:{}},stateSchema:{type:'object',properties:{held:{type:'reference',referenceType:'object'}}},initialState:{held},logic:{functionId:'state-function'}}),m=addMechanicInstance(p,d.id,{kind:'project',id:p.id});const runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);await settled(runtime);const saved=JSON.parse(JSON.stringify(gameSave(runtime)));await restoreGameSave(runtime,saved);assert.deepEqual(runtime.game.state.mechanicStates[m.id].held,held);}finally{runtime.stop();}
});

test('nested expressions in typed structures and collections delegate to the existing evaluator',async()=>{
 registerOperation({id:'test.values.sum',version:1,parameters:{type:'object',properties:{settings:{type:'object',properties:{amount:{type:'number'}}},values:{type:'collection',items:{type:'number'}}}},result:{type:'number'},contexts:['event'],editor:{label:'Сумма значений'}},p=>p.settings.amount+p.values.reduce((a,b)=>a+b,0));const p=project(),runtime=rt();try{await runtime.start(p,p.subscenes[0].entry);assert.equal(await runtime.game.command('test.values.sum',{settings:{amount:{op:'add',a:2,b:3}},values:[{op:'add',a:1,b:2},{op:'get',path:'global.count'}]}),8);await assert.rejects(runtime.game.command('test.values.sum',{settings:{amount:1},values:[{op:'get',path:'global.finished'}]}),/число/);}finally{runtime.stop();}
});
