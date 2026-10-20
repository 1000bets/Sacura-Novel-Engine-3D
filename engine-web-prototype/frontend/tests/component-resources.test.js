import test from 'node:test';
import assert from 'node:assert/strict';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {createComponentResource,editComponentSource,publishComponentSource,addGraphComponentMethod} from '../src/componentResources.js';
import {componentMethodChoices} from '../src/componentCatalog.js';
import {parseComponentSource} from '../src/componentSources.js';
import {addMechanicInstance} from '../src/mechanicDescriptions.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {readProject} from '../src/projectFiles.js';
import {reference} from '../src/valueSchemas.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
import {collectAssetFiles} from '../src/assetFiles.js';
import {counterSource} from './fixtures/componentCounter.js';
import {duplicateInstance,planAuthoringChange} from '../src/authoredMechanics.js';
import {gameSave,restoreGameSave,validateGameSave} from '../src/gameSave.js';

const runtime=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio');}));
async function start(p){const rt=runtime();await rt.start(p);assert.equal(rt.snapshot.error,null);return rt;}
function fixture(options){const p=createMechanicsProject('Авторский Counter'),r=createComponentResource(p,{name:'Counter'});editComponentSource(p,r.id,counterSource(options));publishComponentSource(p,r.id);const a=addMechanicInstance(p,r.definitionId,{kind:'project',id:p.id},{step:2}),b=addMechanicInstance(p,r.definitionId,{kind:'scene',id:p.subscenes[0].id},{step:5});return {p,r,a,b};}
test('blank code/graph resources need no preset; Counter executes its source with independent state, collections, inspector parameters and signals',async()=>{
 const {p,r,a,b}=fixture();assert.ok(collectAssetFiles(p).some(f=>f.logicResourceId===r.id&&f.path==='Компоненты/Counter.js'));
 const rt=await start(p);try{const signals=[];rt.game.ruleRuntime.subscribeSignal(p.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId).events.changed.signalId,(value,context)=>signals.push({value,emitter:context.emitterInstanceId}));
  assert.equal(await rt.game.callComponent(a.id,'add',{amount:3}),6);assert.equal(await rt.game.callComponent(b.id,'add',{amount:3}),15);assert.deepEqual(rt.game.state.mechanicStates[a.id],{value:6,history:[6]});assert.deepEqual(rt.game.state.mechanicStates[b.id],{value:15,history:[15]});assert.equal(signals[0].emitter,a.id);
 }finally{rt.stop();}
 const loaded=readProject(JSON.stringify(p));assert.equal(loaded.logicResources[0].source,r.source);assert.equal(loaded.functions[0].nodes.length,0);
 const blank=createComponentResource(p,{name:'Без пресета',kind:'component.graph'});assert.equal(p.gameplay.mechanicDefinitions.find(d=>d.id===blank.definitionId).methods!==undefined,true);
});
test('publishing Multiply updates method choices automatically; graph-to-code and code-to-graph share the dispatcher',async()=>{
 const {p,r,a}=fixture();const before=p.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId).methods.add.logic.functionId;
 editComponentSource(p,r.id,counterSource({multiply:true}));publishComponentSource(p,r.id);assert.equal(p.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId).methods.add.logic.functionId,before);
 const graph=createComponentResource(p,{name:'Calculations',kind:'component.graph'}),methodId=addGraphComponentMethod(p,graph.definitionId,{name:'Calculate',result:{type:'number'}}),d=p.gameplay.mechanicDefinitions.find(d=>d.id===graph.definitionId),fn=p.functions.find(f=>f.id===d.methods[methodId].logic.functionId),choice=componentMethodChoices(p,fn).find(c=>c.label==='Counter · Multiply');assert.ok(choice);
 const node={id:'multiply-call',...choice.make()};node.call.parameters.instance=reference('mechanic',a.id);node.call.parameters.args.factor=4;node.next='result';fn.entry=node.id;fn.nodes=[node,{id:'result',kind:'return',value:{op:'node',nodeId:node.id}}];
 const instance=addMechanicInstance(p,d.id,{kind:'project',id:p.id});
 const rt=await start(p);try{await rt.game.callComponent(a.id,'add',{amount:2});assert.equal(await rt.game.callComponent(instance.id,methodId),16);}finally{rt.stop();}
 // A JS method calls the graph method using an ordinary named instance slot.
 const source=counterSource({multiply:true}).replace('args, params, state, engine','args, params, state, engine, refs').replace('  methods: {','  initialState: {value: 3},\n  methods: {').replace('const delta = args.amount * params.step;',`const delta = await engine.invoke(refs.other, ${JSON.stringify(methodId)}, {});`);editComponentSource(p,r.id,source);publishComponentSource(p,r.id);p.gameplay.mechanics.find(m=>m.id===a.id).references.other=reference('mechanic',instance.id);
 const next=await start(p);try{assert.equal(await next.game.callComponent(a.id,'add',{amount:1}),24);}finally{next.stop();}
});
test('next Play uses the updated formula, while draft/syntax error blocks old code and keeps all source text',async()=>{
 const {p,r,a}=fixture(),rt=await start(p);try{assert.equal(await rt.game.callComponent(a.id,'add',{amount:2}),4);const save=gameSave(rt),renamed=structuredClone(p);renamed.logicResources[0].path='Scripts/Renamed.js';assert.equal(validateGameSave(renamed,save),save);
  editComponentSource(p,r.id,counterSource({formula:'args.amount * params.step * 3'}));publishComponentSource(p,r.id);const current=rt.game;await assert.rejects(restoreGameSave(rt,save,p),/Логика.*изменились/);assert.equal(rt.game,current);assert.equal(rt.game.state.mechanicStates[a.id].value,4);
  rt.stop();editComponentSource(p,r.id,counterSource({formula:'args.amount * params.step * 3'}));await rt.start(p);assert.match(rt.snapshot.error,/изменён|сохраните|Сохраните/);publishComponentSource(p,r.id);await rt.start(p);assert.equal(rt.snapshot.error,null);assert.equal(await rt.game.callComponent(a.id,'add',{amount:2}),12);
  editComponentSource(p,r.id,'const component = {\n methods: { nope: ( ;\n};');assert.throws(()=>publishComponentSource(p,r.id));const saved=readProject(JSON.stringify(p));assert.equal(saved.logicResources[0].source,p.logicResources[0].source);assert.equal(saved.logicResources[0].status,'error');await rt.start(saved);assert.match(rt.snapshot.error,/Counter.js/);assert.equal(rt.snapshot.phase,'ERROR');
 }finally{rt.stop();}
});
test('renaming display names preserves identity, removed method calls fail and runtime errors name source, component and method',async()=>{
 const {p,r,a}=fixture({multiply:true});editComponentSource(p,r.id,counterSource({multiply:true}).replace('name: "Multiply"','name: "Масштабировать"'));publishComponentSource(p,r.id);assert.ok(componentMethodChoices(p).some(c=>c.label==='Counter · Масштабировать'));
 editComponentSource(p,r.id,counterSource());publishComponentSource(p,r.id);let rt=await start(p);try{await assert.rejects(rt.game.callComponent(a.id,'multiply',{factor:2}),/недоступен/);}finally{rt.stop();}
 editComponentSource(p,r.id,counterSource({formula:'(() => { throw Error("Моя ошибка"); })()'}));publishComponentSource(p,r.id);rt=await start(p);try{await assert.rejects(rt.game.callComponent(a.id,'add',{amount:1}),error=>{assert.match(error.message,/Counter.*Add.*Counter.js.*Моя ошибка/);assert.ok(error.line>0);return true;});}finally{rt.stop();}
});
test('resource packages A → B → C preserve one source, methods and dependencies, and imported code remains disabled until reviewed',async()=>{
 const {p,r,a}=fixture({multiply:true}),meta={name:'Counter',author:'A',license:'CC0'},pkg=await exportAuthoringPackage(p,a,meta);assert.equal(pkg.content.logicResources.length,1);assert.equal(pkg.content.functions.some(f=>f.mechanicScript),false);
 const b=createMechanicsProject('B'),before=JSON.stringify(b),plan=await planPackageImport(b,pkg,{});assert.equal(JSON.stringify(b),before);const imported=plan.apply(b),rb=imported.logicResources[0],mb=imported.gameplay.mechanics[0],rt=runtime();try{await rt.start(imported);assert.match(rt.snapshot.error,/импортирован|сохраните|Сохраните/);publishComponentSource(imported,rb.id);await rt.start(imported);assert.equal(await rt.game.callComponent(mb.id,'add',{amount:2}),4);rt.stop();editComponentSource(imported,rb.id,rb.source.replace('args.amount * params.step','args.amount * params.step * 2'));publishComponentSource(imported,rb.id);
  const variant=await exportAuthoringPackage(imported,mb,{...meta,author:'B'}),c=createMechanicsProject('C'),pc=(await planPackageImport(c,variant,{})).apply(c);publishComponentSource(pc,pc.logicResources[0].id);await rt.start(pc);assert.equal(await rt.game.callComponent(pc.gameplay.mechanics[0].id,'add',{amount:2}),8);assert.equal(p.logicResources[0].source,counterSource({multiply:true}));
 }finally{rt.stop();}
});
test('static metadata extraction never executes top-level author JavaScript',()=>{
 globalThis.__sacuraExecuted=false;parseComponentSource('globalThis.__sacuraExecuted=true; const component={methods:{},name:"No execution"};');assert.equal(globalThis.__sacuraExecuted,false);delete globalThis.__sacuraExecuted;
 assert.throws(()=>parseComponentSource('const component={methods:{}, methods:{}};'),/уникальные/);
 assert.throws(()=>parseComponentSource('const component={methods:{}, references:{type:"object",properties:{door:{type:"reference",referenceType:"object",default:{kind:"reference",mode:"direct",type:"object",id:"door"}}}}};'),/непереносимы/);
});
test('a method node refuses an unrelated receiver with the same method key instead of changing the opened implementation',async()=>{
 const {p,r,a}=fixture({multiply:true}),other=createComponentResource(p,{name:'Other'});editComponentSource(p,other.id,counterSource({multiply:true}).replace('name: "Counter"','name: "Other"'));publishComponentSource(p,other.id);const b=addMechanicInstance(p,other.definitionId,{kind:'project',id:p.id});
 const node={id:'wrong-receiver',...componentMethodChoices(p).find(c=>c.label==='Counter · Multiply').make()};node.call.parameters.instance=reference('mechanic',b.id);node.call.parameters.args.factor=9;
 const graph=createComponentResource(p,{name:'Caller',kind:'component.graph'}),d=p.gameplay.mechanicDefinitions.find(d=>d.id===graph.definitionId),fn=p.functions.find(f=>f.id===Object.values(d.methods)[0].logic.functionId);fn.nodes=[node];fn.entry=node.id;const caller=addMechanicInstance(p,d.id,{kind:'project',id:p.id}),rt=await start(p);
 try{await assert.rejects(rt.game.callFunction(p.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId).methods.multiply.logic.functionId,{factor:9}),/через экземпляр компонента/);await rt.game.callComponent(a.id,'add',{amount:2});await assert.rejects(rt.game.callComponent(caller.id,Object.keys(d.methods)[0]),/Получатель не соответствует.*wrong-receiver/);assert.equal(rt.game.state.mechanicStates[a.id].value,4);assert.equal(rt.game.state.mechanicStates[b.id].value,0);}finally{rt.stop();}
});
test('an independent local component copy owns a different source and signal while shared instances keep their original implementation',async()=>{
 const {p,r,a}=fixture();p.logicResources.find(x=>x.id===r.id).path='Компоненты/Counter';const plan=planAuthoringChange(p,next=>duplicateInstance(next,next.gameplay.mechanics.find(m=>m.id===a.id),{independent:true}));plan.apply(p);const copy=p.gameplay.mechanics.find(m=>m.id===plan.record.id),definition=p.gameplay.mechanicDefinitions.find(d=>d.id===copy.definitionId),source=p.logicResources.find(r=>r.id===definition.logicResourceId);assert.notEqual(source.id,r.id);assert.equal(source.path,'Компоненты/Counter копия');
 editComponentSource(p,source.id,source.source.replace('args.amount * params.step','args.amount * params.step * 10'));publishComponentSource(p,source.id);
 const rt=await start(p);try{assert.equal(await rt.game.callComponent(a.id,'add',{amount:1}),2);assert.equal(await rt.game.callComponent(copy.id,'add',{amount:1}),20);assert.notEqual(rt.game.config.mechanicDefinitions.find(d=>d.id===a.definitionId).events.changed.signalId,definition.events.changed.signalId);}finally{rt.stop();}
 const pkg=await exportAuthoringPackage(p,a,{name:'Counter',author:'A',license:'CC0'}),imported=(await planPackageImport(p,pkg,{})).project;assert.ok(imported.logicResources.some(r=>r.path==='Компоненты/Counter 2'));
});
