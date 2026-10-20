import test from 'node:test';
import assert from 'node:assert/strict';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {compileMechanicScript} from '../src/mechanicScriptCompiler.js';
import {applyCompiledMechanicScript} from '../src/mechanicScriptPublishing.js';
import {serializedFields,usesSerializedFields} from '../src/serializedFields.js';
import {callBlueprint} from '../src/blueprintFunctions.js';
import {createFromFunction,updateDefinitionParameters} from '../src/authoredMechanics.js';
import {addMechanic} from '../src/mechanicsModel.js';
import {ensureHandlerGraphs} from '../src/handlerGraphs.js';
import {readProject} from '../src/projectFiles.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
import {runMechanicJavaScript} from '../src/mechanicScriptRuntime.js';
const source=(n=5)=>`const amount = SerializeField(${n}, {label:"Количество", min:0});
const allowed = SerializeField(true, {label:"Начислять"});
async function main({args,global}) {if(allowed)global.score += amount;return [amount,args.amount,allowed];}`;
function fixture(){const p=createMechanicsProject('SerializeField без героя');p.variables.score=0;(p.variableTypes??={}).score='number';const f={id:'script',name:'Начислить',graphVersion:1,parameters:[],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:0}]};p.functions.push(f);const d=createFromFunction(p,f.id),a=addMechanic(p,d.id,{kind:'project',id:p.id}),b=addMechanic(p,d.id,{kind:'scene',id:p.subscenes[0].id});ensureHandlerGraphs(p);return {p,f,d,a,b};}
function compile(p,f,text){applyCompiledMechanicScript(p,f.id,compileMechanicScript(p,text,f.mechanicScript?.dependencies));}
async function idle(rt){for(let n=0;n<200;n++){if(rt.game.ruleRuntime.idle&&!rt.game.flights.size)return;await new Promise(r=>setTimeout(r,5));}throw Error('Rule work did not finish');}

test('source fields become existing instance parameters; defaults, false and independent overrides drive actual game start rules',async()=>{
 const {p,f,d,a,b}=fixture();compile(p,f,source());assert.deepEqual(d.exposed,['amount','allowed']);assert.equal(a.values.amount,undefined);a.values.amount=2;b.values.amount=9;b.values.allowed=false;ensureHandlerGraphs(p);
 const reopened=readProject(JSON.stringify(p)),rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));
 try{await rt.start(reopened);await idle(rt);assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.variables.score,2);assert.equal(reopened.gameplay.mechanics.find(m=>m.id===b.id).values.amount,9);}finally{rt.stop();}
 const result=await callBlueprint(p,f.id,{amount:0,allowed:false},p.variables,()=>{},{allowMechanicScripts:true});assert.deepEqual(result,[0,0,false]);
});

test('recompilation adds/removes source fields, retains manual parameters and overrides, and errors do not change working schema or code',async()=>{
 const {p,f,a}=fixture();f.parameters=[{name:'legacy',type:'string',default:'old'}];compile(p,f,source());a.values.amount=12;compile(p,f,source(7));assert.equal(a.values.amount,12);assert.equal(f.parameters.find(p=>p.name==='amount').default,7);const before=JSON.stringify(p);const d=p.gameplay.mechanicDefinitions.find(d=>d.logic.functionId===f.id),changed=structuredClone(d.parameters);changed.properties.amount.default=99;assert.throws(()=>updateDefinitionParameters(p,d.id,d.version,changed,{}),/исходником/);assert.equal(JSON.stringify(p),before);
 assert.throws(()=>compile(p,f,'const amount = SerializeField(doWork()); function main(){}'),e=>e.line===1&&/литералами/.test(e.message));assert.equal(JSON.stringify(p),before);
 compile(p,f,'function main(){return true;}');assert.deepEqual(f.parameters.map(p=>p.name),['legacy']);assert.equal(a.values.amount,undefined);assert.equal(p.gameplay.mechanicDefinitions[0].parameters.properties.amount,undefined);
});

test('declarations are parsed without execution; types, portable references and malformed annotations have source diagnostics',()=>{
 const p=createMechanicsProject();let executed=false;globalThis.metadataProbe=()=>{executed=true;return 1;};try{
 compileMechanicScript(p,'metadataProbe();const count=SerializeField(2,{type:"integer",min:0});function main(){}');assert.equal(executed,false);
 assert.throws(()=>compileMechanicScript(p,'function main(){\n const x=SerializeField(1);\n}'),e=>e.line===2&&/верхнем уровне/.test(e.message));
 for(const text of ['const x=SerializeField(2,{type:"boolean"});','const x=SerializeField(1,{min:3});','const x=SerializeField({kind:"reference",type:"object",mode:"direct",id:"old-world"},{type:"reference",referenceType:"object"});'])assert.throws(()=>compileMechanicScript(p,text+'function main(){}'),e=>e.line===1);
 const fields=serializedFields('const target=SerializeField(undefined,{type:"reference",referenceType:"object",label:"Цель"}); const point=SerializeField([1,2,3],{type:"vector",size:3}); const tags=SerializeField(["a"],{type:"collection",items:{type:"string"}});function main(){}');assert.equal(fields[0].schema.referenceType,'object');assert.equal(fields[1].schema.type,'vector');assert.equal(fields[2].schema.items.type,'string');
 }finally{delete globalThis.metadataProbe;}
});

test('VM uses the invocation snapshot in closures/conditions and preserves original error lines after multiline declarations',async()=>{
 const text='const amount=SerializeField(\n  5,\n  {label:"Количество"}\n);\nfunction condition(){return amount===9;}\nasync function main(){await Promise.resolve();throw Error("bad field work");}';
 assert.equal(await runMechanicJavaScript('const n=SerializeField(5);function main(){return n;}',{args:{},engine:{}},()=>{}),5);
 assert.equal(await runMechanicJavaScript(text,{args:{amount:9},engine:{}},()=>{},{phase:'condition'}),true);
 await assert.rejects(runMechanicJavaScript(text,{args:{amount:9},engine:{}},()=>{}),e=>e.line===6&&/bad field work/.test(e.message));
});

test('A → B → C retains editable field declarations, remapped dependencies and independent source; missing SDK feature refuses import atomically',async()=>{
 const {p:a,f}=fixture();compile(a,f,source());const pkg=await exportAuthoringPackage(a,f,{kind:'function',name:'Fields',author:'A',license:'CC0'});assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='mechanic.serialized-fields'));
 const b=createMechanicsProject('B');b.variables.variant=0;(b.variableTypes??={}).variant='number';const bindings=Object.fromEntries(pkg.inputs.filter(i=>i.type==='variable').map(i=>[i.id,'variant']));
 const before=JSON.stringify(b),unsupported=structuredClone(pkg);unsupported.compatibility.requiredCapabilities.find(c=>c.id==='mechanic.serialized-fields').version=99;await assert.rejects(planPackageImport(b,unsupported,bindings),/mechanic.serialized-fields/);assert.equal(JSON.stringify(b),before);
 const missing=structuredClone(pkg);missing.compatibility.requiredCapabilities=missing.compatibility.requiredCapabilities.filter(c=>c.id!=='mechanic.serialized-fields');await assert.rejects(planPackageImport(b,missing,bindings),/mechanic.serialized-fields/);assert.equal(JSON.stringify(b),before);
 const importedProject=(await planPackageImport(b,pkg,bindings)).apply(b),imported=importedProject.functions.find(f=>f.mechanicScript);assert.equal(imported.mechanicScript.approved,false);compile(importedProject,imported,imported.mechanicScript.source.replace('SerializeField(5','SerializeField(8'));assert.equal(imported.parameters[0].default,8);assert.equal(f.parameters[0].default,5);
 const variant=await exportAuthoringPackage(importedProject,imported,{kind:'function',name:'Variant',author:'B',license:'CC0'}),c=createMechanicsProject('C');c.variables.finalScore=0;(c.variableTypes??={}).finalScore='number';const cb=Object.fromEntries(variant.inputs.filter(i=>i.type==='variable').map(i=>[i.id,'finalScore'])),finalProject=(await planPackageImport(c,variant,cb)).apply(c),final=finalProject.functions.find(f=>f.mechanicScript);compile(finalProject,final,final.mechanicScript.source);assert.deepEqual(await callBlueprint(finalProject,final.id,{},finalProject.variables,()=>{},{allowMechanicScripts:true}),[8,8,true]);assert.equal(finalProject.variables.finalScore,8);assert.equal(a.variables.score,0);
});


test('typed object fields use explicit instance references and survive renaming; draft feature requirements remain explicit',async()=>{
 const {p,f,a}=fixture();p.objects.push({id:'target',name:'Дверь',type:'Активный меш',active:true,subsceneId:p.subscenes[0].id});
 compile(p,f,'const target=SerializeField(undefined,{type:"reference",referenceType:"object",label:"Цель"});function main(){return target.id;}');
 const ref={kind:'reference',type:'object',mode:'direct',id:'target'};a.values.target=ref;p.objects[0].name='Дверь склада';assert.equal(await callBlueprint(p,f.id,{target:ref},p.variables,()=>{},{allowMechanicScripts:true}),'target');
 assert.equal(usesSerializedFields({source:'const value=SerializeField( ;',compiledSource:'function main(){}'}),true);
 const pkg=await exportAuthoringPackage(p,a,{kind:'mechanic',name:'Target field',author:'A',license:'CC0'}),b=createMechanicsProject('B');b.objects.push({id:'fresh-target',name:'Новая дверь',type:'Активный меш',active:true,subsceneId:b.subscenes[0].id});const bindings=Object.fromEntries(pkg.inputs.filter(i=>i.type==='object').map(i=>[i.id,'fresh-target'])),next=(await planPackageImport(b,pkg,bindings)).apply(b),fn=next.functions.find(f=>f.mechanicScript),instance=next.gameplay.mechanics.find(m=>m.values?.target);compile(next,fn,fn.mechanicScript.source);assert.equal(await callBlueprint(next,fn.id,instance.values,next.variables,()=>{},{allowMechanicScripts:true}),'fresh-target');
});
