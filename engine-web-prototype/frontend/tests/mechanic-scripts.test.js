import test from 'node:test';
import assert from 'node:assert/strict';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {compileMechanicScript,parseMechanicScript} from '../src/mechanicScriptCompiler.js';
import {mechanicScriptSource} from '../src/mechanicScriptSource.js';
import {runMechanicJavaScript,validateMechanicJavaScript} from '../src/mechanicScriptRuntime.js';
import {callBlueprint} from '../src/blueprintFunctions.js';
import {createFromFunction} from '../src/authoredMechanics.js';
import {addMechanic} from '../src/mechanicsModel.js';
import {ensureHandlerGraphs} from '../src/handlerGraphs.js';
import {exportAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
import {readProject} from '../src/projectFiles.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';

function fixture(){const p=createMechanicsProject('JavaScript без героя');p.variables.score=0;(p.variableTypes??={}).score='number';const f={id:'js-reward',name:'Награда',graphVersion:1,parameters:[],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:7}]};p.functions.push(f);return {p,f};}
const run=(p,f,options={})=>callBlueprint(p,f.id,{},p.variables,()=>{}, {allowMechanicScripts:true,...options});
async function idle(rt){for(let i=0;i<300;i++){if(rt.game.ruleRuntime.idle&&!rt.game.flights.size)return;await new Promise(r=>setTimeout(r,5));}throw Error('Runtime did not settle');}

test('real JS supports closures, arrays, loops, async API and shared variables; other function contexts keep their original behavior',async()=>{
 const {p,f}=fixture();f.mechanicScript=compileMechanicScript(p,'async function main({global}) { const twice = n => n * 2; const values = [1,2,3].map(twice); for (const n of values) global.score += n; await Promise.resolve(); return global.score; }');
 assert.equal(await run(p,f),12);assert.equal(p.variables.score,12);assert.equal(await callBlueprint(p,f.id,{},p.variables,()=>{}),7);assert.equal(p.variables.score,12);
 const loaded=readProject(JSON.stringify(p)),saved=loaded.functions.find(x=>x.id===f.id);assert.equal(await run(loaded,saved),24);
});
test('syntax errors report a source location and never replace the last compiled script',async()=>{
 const {p,f}=fixture();f.mechanicScript=compileMechanicScript(p,'function main() { return 21; }');const active=f.mechanicScript.compiledSource;f.mechanicScript.source='function main() {\n return ( ;\n}';assert.throws(()=>compileMechanicScript(p,f.mechanicScript.source),e=>e.line===2&&e.column>0);assert.equal(f.mechanicScript.compiledSource,active);assert.equal(await run(p,f),21);
 await validateMechanicJavaScript(active);assert.throws(()=>parseMechanicScript('const amount=4;'),/main/);
});
test('JavaScript VM limits loops and memory, isolates host globals and cancels suspended invocations',async()=>{
 const input={args:{},engine:{}};
 assert.deepEqual(await runMechanicJavaScript('function main() { return [typeof window,typeof document,typeof fetch,typeof process]; }',input,()=>{}),['undefined','undefined','undefined','undefined']);
 await assert.rejects(runMechanicJavaScript('function main() { while(true) {} }',input,()=>{},{cpuMs:20}),/лимит|interrupted/);
 await assert.rejects(runMechanicJavaScript('async function main() { while(true) await Promise.resolve(); }',input,()=>{},{cpuMs:20}),/лимит|interrupted/);
 await assert.rejects(runMechanicJavaScript('function main() { let x=[]; while(true) x.push("X".repeat(10000)); }',input,()=>{},{cpuMs:100,memoryBytes:1024*1024}),/memory|лимит|interrupted/i);
 const controller=new AbortController(),pending=runMechanicJavaScript('async function main({engine}) { await engine.wait(1); return 4; }',input,()=>new Promise(()=>{}),{abortSignal:controller.signal});setTimeout(()=>controller.abort(),20);await assert.rejects(pending,/INSTANCE_STOPPED/);
});
test('generated JS preserves expressions, operation results, conditions and cached admission',async()=>{
 const {p,f}=fixture();f.nodes=[{id:'query',kind:'operation',call:{id:'sacura.variable.read',version:1,parameters:{variable:{kind:'reference',type:'variable',mode:'direct',id:'score'}}},next:'branch'},{id:'branch',kind:'branch',condition:{op:'gt',a:{op:'node',nodeId:'query'},b:0},trueNext:'yes',falseNext:'no'},{id:'yes',kind:'return',value:true},{id:'no',kind:'return',value:false}];f.entry='query';f.admission={branchId:'branch'};p.variables.score=3;
 const generated=mechanicScriptSource(p,f);parseMechanicScript(generated.source);await validateMechanicJavaScript(generated.source);
 // This test's query callback intentionally returns a value, independent of a particular capability.
 f.mechanicScript={version:1,source:generated.source,compiledSource:generated.source,dependencies:generated.dependencies,approved:true};let reads=0;
 const admitted=await callBlueprint(p,f.id,{},p.variables,()=>{reads++;return 3;},{allowMechanicScripts:true,readOnly:true,stopBefore:'branch'});assert.equal(admitted.value,true);assert.equal(reads,1);
 const result=await callBlueprint(p,f.id,{},p.variables,()=>{reads++;return 0;},{allowMechanicScripts:true,nodeResults:admitted.nodeResults,initialLocals:admitted.local,admittedBranch:{nodeId:'branch',value:admitted.value}});assert.equal(result,true);assert.equal(reads,1);
});
test('compiled mechanic script executes through the actual game start rule and changed code changes the next run',async()=>{
 const {p,f}=fixture();f.mechanicScript=compileMechanicScript(p,'function main({global}) { global.score += 9; return global.score; }');const d=createFromFunction(p,f.id);addMechanic(p,d.id,{kind:'project',id:p.id});ensureHandlerGraphs(p);
 const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await rt.start(p);await idle(rt);assert.equal(rt.snapshot.variables.score,9);assert.equal(rt.snapshot.error,null);rt.stop();f.mechanicScript=compileMechanicScript(p,'function main({global}) { global.score += 25; return global.score; }');await rt.start(p);await idle(rt);assert.equal(rt.snapshot.variables.score,25);assert.deepEqual(rt.game.state.actors,{});assert.deepEqual(rt.game.state.weapons,{});}finally{rt.stop();}
});
test('script condition and body edits are used by actual handler admission, without mutating a false branch',async()=>{
 const {p,f}=fixture();const d=createFromFunction(p,f.id);addMechanic(p,d.id,{kind:'project',id:p.id});ensureHandlerGraphs(p);const handler=p.functions.find(x=>x.id===d.rules[0].handlerFunctionId);
 handler.mechanicScript=compileMechanicScript(p,'function condition() { return false; } function main({global,engine}) { if(engine.admitted) global.score += 50; }');
 const rt=new PreviewRuntime(new SoundDesk(()=>{}));try{await rt.start(p);await idle(rt);assert.equal(rt.snapshot.variables.score,0);rt.stop();handler.mechanicScript=compileMechanicScript(p,'function condition() { return true; } function main({global}) { global.score += 50; }');await rt.start(p);await idle(rt);assert.equal(rt.snapshot.variables.score,50);}finally{rt.stop();}
});
test('script dependencies transfer A → B → C, remain editable and independent, and import never enables code',async()=>{
 const {p:a,f}=fixture();const child={...structuredClone(f),id:'child',name:'Бонус'};a.functions.push(child);f.mechanicScript=compileMechanicScript(a,'async function main({global,engine}) { global.score += await engine.call("Бонус"); return global.score; }');
 const meta={kind:'function',name:'JS награда',author:'A',license:'CC0'},pkg=await exportAuthoringPackage(a,f,meta);assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='mechanic.javascript'));assert.equal(pkg.content.functions.length,2);
 const b=createMechanicsProject('B');b.variables.total=100;(b.variableTypes??={}).total='number';const bindings=Object.fromEntries(pkg.inputs.filter(i=>i.type==='variable').map(i=>[i.id,'total'])),imported=(await planPackageImport(b,pkg,bindings)).apply(b),fb=imported.functions.find(x=>x.name===f.name);assert.equal(fb.mechanicScript.approved,false);await assert.rejects(run(imported,fb),/не включён/);
 fb.mechanicScript=compileMechanicScript(imported,fb.mechanicScript.source,fb.mechanicScript.dependencies);assert.equal(await run(imported,fb),107);fb.mechanicScript=compileMechanicScript(imported,fb.mechanicScript.source.replace('await engine.call("Бонус")','(await engine.call("Бонус")) * 2'),fb.mechanicScript.dependencies);assert.equal(await run(imported,fb),121);assert.equal(a.variables.score,0);
 const variant=await exportAuthoringPackage(imported,fb,{...meta,author:'B'}),c=createMechanicsProject('C');c.variables.points=0;(c.variableTypes??={}).points='number';const next=(await planPackageImport(c,variant,Object.fromEntries(variant.inputs.filter(i=>i.type==='variable').map(i=>[i.id,'points'])))).apply(c),fc=next.functions.find(x=>x.name===f.name);fc.mechanicScript=compileMechanicScript(next,fc.mechanicScript.source,fc.mechanicScript.dependencies);assert.equal(await run(next,fc),14);assert.equal(imported.variables.total,121);assert.equal(await run(a,f),7);
});

test('a host error keeps its diagnostic across VM rethrow and author catch can add context',async()=>{
 const original=Object.assign(Error('Inner failure'),{componentDiagnostic:{instanceId:'actual-receiver',message:'Inner failure'},componentTrace:[{instanceId:'actual-receiver'}]});
 await assert.rejects(runMechanicJavaScript('async function main({engine}) { try { await engine.operation("test"); } catch (e) { e.message = "Author context: " + e.message; throw e; } }',{args:{},engine:{}},async()=>{throw original;}),error=>error.message==='Author context: Inner failure'&&error.componentDiagnostic.instanceId==='actual-receiver');
 assert.equal(original.message,'Inner failure');
});
