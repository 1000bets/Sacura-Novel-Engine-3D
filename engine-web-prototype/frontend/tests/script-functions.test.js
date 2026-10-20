import test from 'node:test';
import assert from 'node:assert/strict';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {newScriptFunction,compileFunctionScript,callScriptFunction} from '../src/scriptFunctions.js';
import {prepareMechanicScripts} from '../src/mechanicScriptRuntime.js';
import {callBlueprint} from '../src/blueprintFunctions.js';
import {readLogic,dataConnectionError} from '../src/logicModel.js';
import {applyConnections} from '../src/storyConnections.js';
import {renameFunctionParameter} from '../src/functionSignatures.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {exportAuthoringPackage,readAuthoringPackage,planPackageImport} from '../src/mechanicPackages.js';
import {readProject} from '../src/projectFiles.js';
import {validateStudio} from '../src/studioModel.js';
await prepareMechanicScripts();
const meta={kind:'function',name:'Формула',author:'Автор A',license:'MIT'};
function fixture(type='number'){const p=createEmptyProject('Без героя'),f=newScriptFunction(p,type);p.functions.push(f);return {p,f};}
function publish(p,f,source){return f.functionScript=compileFunctionScript(p,source,f.functionScript.dependencies);}
function valueNode(f,id='formula'){return {id,kind:'script-function',functionId:f.id,args:{value:4},inputs:{},text:'Формула',next:null,bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]}};}

test('custom JavaScript computes from literals and typed wires; ordinary function calls execute the same source',async()=>{
 const {p,f}=fixture();publish(p,f,'function main({args}) { const twice = n => n * 2; return [args.value, 1].map(twice).reduce((a,b)=>a+b,0); }');
 const node=valueNode(f),literal={...node,id:'input',kind:'literal',valueType:'number',value:6};p.chapters[0].beats.push(node,literal);
 assert.equal(applyConnections(p,[{source:literal.id,sourceHandle:'value',target:node.id,targetHandle:'arg:value'}]),null);
 assert.equal(readLogic(p,node.id,p.variables),14);assert.equal(await callBlueprint(p,f.id,{value:6},p.variables,()=>{throw Error('No operations');}),14);
 assert.equal(validateStudio(p).filter(i=>i.level==='error').length,0);
 literal.valueType='boolean';assert.match(dataConnectionError(p,{source:literal.id,sourceHandle:'value',target:node.id,targetHandle:'arg:value'}),/Тип/);
});

test('story branches run custom functions without a mechanic, hero or combat entities',async()=>{
 const {p,f}=fixture('boolean');f.parameters[0].schema={type:'number'};f.parameters[0].default=0;publish(p,f,'function main({args}) { return args.value >= 3; }');
 const original=p.chapters[0].beats[0],node=valueNode(f),branch={...structuredClone(original),id:'branch',kind:'branch',inputs:{condition:node.id},trueNext:'yes',falseNext:'no'},yes={...structuredClone(original),id:'yes',text:'Да',next:null},no={...structuredClone(original),id:'no',text:'Нет',next:null};p.chapters[0].beats=[branch,node,yes,no];p.subscenes[0].entry='branch';
 const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio');}));try{await rt.start(p,'branch');assert.equal(rt.snapshot.beatId,'yes');assert.equal(rt.snapshot.error,null);rt.stop();node.args.value=1;await rt.start(p,'branch');assert.equal(rt.snapshot.beatId,'no');assert.deepEqual(rt.game.state.actors,{});assert.deepEqual(rt.game.state.weapons,{});}finally{rt.stop();}
});

test('functions read current project values, return typed data, and cannot change host state',()=>{
 const {p,f}=fixture();p.variables.score=5;publish(p,f,'function main({args,global}) { return args.value + global.score; }');assert.equal(callScriptFunction(p,f.id,{value:2},p.variables),7);p.variables.score=10;assert.equal(callScriptFunction(p,f.id,{value:2},p.variables),12);
 publish(p,f,'function main({global}) { global.score = 999; return 0; }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),/read.only|not configurable|not extensible|only a getter|not writable/i);assert.equal(p.variables.score,10);
 publish(p,f,'function main() { return "wrong"; }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),/Результат функции/);
 assert.throws(()=>compileFunctionScript(p,'async function main() { return 1; }'),/без async/);
 assert.throws(()=>compileFunctionScript(p,'function main({engine}) { return engine.operation("x"); }'),/доступен engine.call/);
});

test('loops, recursive calls, promises and runtime failures are bounded and attributed to the function',()=>{
 const {p,f}=fixture();publish(p,f,'function main() { while(true) {} }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),/лимит вычислений/);
 publish(p,f,'function main() { return Promise.resolve(1); }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),/должна вернуть результат сразу/);
 publish(p,f,'function main({engine}) { return engine.call("Новая функция"); }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),/Цикл вызовов/);
 publish(p,f,'function main() { throw Error("Ошибка автора"); }');assert.throws(()=>callScriptFunction(p,f.id,{},p.variables),e=>e.functionId===f.id&&/Ошибка автора/.test(e.message)&&e.line===1);
});

test('renaming a parameter keeps literal call arguments and wires across project save and load',()=>{
 const {p,f}=fixture(),node=valueNode(f);node.inputs={'arg:value':'source'};p.chapters[0].beats.push(node);renameFunctionParameter(p,f.id,'value','amount');assert.deepEqual(node.args,{amount:4});assert.deepEqual(node.inputs,{'arg:amount':'source'});assert.equal(readProject(JSON.stringify(p)).functions.find(x=>x.id===f.id).functionScript.source,f.functionScript.source);
});

test('A to B to C preserves editable source, nested dependencies, bindings, authorship and independent copies; imports never execute',async()=>{
 const {p:a,f}=fixture();a.variables.score=7;(a.variableTypes??={}).score='number';const helper=newScriptFunction(a);helper.name='Удвоить';a.functions.push(helper);publish(a,helper,'function main({args}) { return args.value * 2; }');publish(a,f,'function main({engine,global}) { return engine.call("Удвоить", {value:global.score}); }');
 const pkg=await exportAuthoringPackage(a,f,meta);assert.equal(pkg.formatVersion,3);assert.equal(pkg.sourceContract.version,2);assert.ok(pkg.compatibility.requiredCapabilities.some(c=>c.id==='function.javascript'));assert.equal(pkg.content.functions.length,2);assert.equal(a.variables.score,7);
 const importInto=async(pkg,name,key)=>{const p=createEmptyProject(name);p.variables[key]=10;(p.variableTypes??={})[key]='number';const before=JSON.stringify(p),bindings=Object.fromEntries(pkg.inputs.filter(i=>i.type==='variable').map(i=>[i.id,key]));const plan=await planPackageImport(p,pkg,bindings);assert.equal(JSON.stringify(p),before);return plan.apply(p);};
 const b=await importInto(pkg,'B','total'),fb=b.functions.find(x=>x.name===f.name);assert.equal(fb.functionScript.approved,false);assert.throws(()=>callScriptFunction(b,fb.id,{},b.variables),/скомпилируйте/);for(const fn of b.functions.filter(f=>f.functionScript))publish(b,fn,fn.functionScript.source);assert.equal(callScriptFunction(b,fb.id,{},b.variables),20);
 const hb=b.functions.find(x=>x.name==='Удвоить');publish(b,hb,'function main({args}) { return args.value * 3; }');assert.equal(callScriptFunction(b,fb.id,{},b.variables),30);assert.equal(callScriptFunction(a,f.id,{},a.variables),14);
 const variant=await exportAuthoringPackage(b,fb,{...meta,author:'Автор B'}),c=await importInto(variant,'C','points'),fc=c.functions.find(x=>x.name===f.name);assert.ok(variant.metadata.sources.some(s=>s.author==='Автор A'));for(const fn of c.functions.filter(f=>f.functionScript))publish(c,fn,fn.functionScript.source);assert.equal(callScriptFunction(c,fc.id,{},c.variables),30);publish(c,c.functions.find(f=>f.name==='Удвоить'),'function main({args}) { return args.value * 4; }');assert.equal(callScriptFunction(c,fc.id,{},c.variables),40);assert.equal(callScriptFunction(b,fb.id,{},b.variables),30);
 const old=structuredClone(pkg);old.formatVersion=2;delete old.sourceContract;await assert.rejects(readAuthoringPackage(old),/JavaScript функции/);const missing=structuredClone(pkg);missing.compatibility.requiredCapabilities=missing.compatibility.requiredCapabilities.filter(c=>c.id!=='function.javascript');await assert.rejects(readAuthoringPackage(missing),/function.javascript/);
});
