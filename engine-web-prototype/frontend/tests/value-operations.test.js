import test from 'node:test';
import assert from 'node:assert/strict';
import {readLogic,logicType,dataConnectionError,convertValue,MATH_OPERATIONS,variablePalette} from '../src/logicModel.js';
import {applyConnections} from '../src/storyConnections.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {readProject} from '../src/projectFiles.js';
import {PreviewRuntime} from '../src/runtime.js';
import {buildStoryFlow} from '../src/storyFlowModel.js';
const node=(id,kind,patch={})=>({id,kind,text:'',speaker:'Рассказчик',bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]},next:null,...patch});
const project=nodes=>{const p=createEmptyProject();p.chapters[0].beats=nodes;p.subscenes[0].entry=nodes.find(n=>!['variable','math','compare','convert','literal'].includes(n.kind))?.id||nodes[0].id;return p;};
const rt=()=>{const r=new PreviewRuntime({stopAll(){}});r.delay=async(_,token)=>r.assert(token);return r;};
const answer=(id,type,value,next)=>({id,label:id,condition:'always',result:{type,value},next});

test('new projects have no sample variables or global-answer palette entries',()=>{
 const p=createEmptyProject();assert.deepEqual(p.variables,{});assert.deepEqual(variablePalette(p),[]);assert.deepEqual(readProject(JSON.stringify(p)),p);
});

test('all math operations calculate finite numbers and reject invalid operands',()=>{
 const expected={add:6,sub:2,mul:8,div:2,mod:0,pow:16,min:2,max:4,negate:-4,abs:4,round:4,floor:4,ceil:4,sqrt:2};
 for(const operator of Object.keys(MATH_OPERATIONS)){const p=project([node('op','math',{operator,a:4,b:2})]);assert.equal(readLogic(p,'op',{}),expected[operator]);}
 const p=project([node('op','math',{operator:'div',a:4,b:0})]);assert.throws(()=>readLogic(p,'op',{}),/ноль/);
 p.chapters[0].beats[0].operator='sqrt';p.chapters[0].beats[0].a=-1;assert.throws(()=>readLogic(p,'op',{}),/конечное/);
});

test('float Get -> math -> Set updates the variable through execution and survives project reload',async()=>{
 const p=project([node('get','variable',{variable:'Доверие',valueType:'number'}),node('add','math',{operator:'add',a:0,b:1.25,inputs:{a:'get'}}),node('set','set-variable',{variable:'Доверие',valueType:'number',inputs:{value:'add'},next:'end'}),node('end','end')]);p.variables={'Доверие':2.5};
 const loaded=readProject(JSON.stringify(p)),r=rt();await r.start(loaded,'set');assert.equal(r.snapshot.error,null);assert.equal(r.snapshot.variables['Доверие'],3.75);assert.equal(r.snapshot.beatId,'end');r.stop();
});

test('conversion uses explicit boolean semantics and diagnoses malformed numbers',()=>{
 assert.equal(convertValue('Да','boolean'),true);assert.equal(convertValue('false','boolean'),false);assert.equal(convertValue('Нет','boolean'),false);assert.equal(convertValue(0,'boolean'),false);
 assert.equal(convertValue(true,'number'),1);assert.equal(convertValue('-2.75','number'),-2.75);assert.equal(convertValue(false,'string'),'false');
 assert.throws(()=>convertValue('бармен','number'),/число/);assert.throws(()=>convertValue('бармен','boolean'),/сравнение/);
 const p=project([node('text','literal',{valueType:'string',value:'true'}),node('convert','convert',{valueType:'boolean',inputs:{value:'text'}}),node('if','branch')]);
 assert.equal(applyConnections(p,[{source:'convert',sourceHandle:'value',target:'if',targetHandle:'condition'}]),null);assert.equal(readLogic(p,'convert',{}),true);
 assert.match(dataConnectionError(p,{source:'text',sourceHandle:'value',target:'if',targetHandle:'condition'}),/условия/);
});

test('earlier choice output remains available after intermediate choices and can be read directly at the end',async()=>{
 const p=project([node('identity','choice',{choices:[answer('guest','string','Гость','middle')]}),node('middle','choice',{choices:[answer('drink','string','Чай','compare')]}),node('compare','branch',{inputs:{condition:'eq'},trueNext:'guest-end',falseNext:'other-end'}),node('eq','compare',{operator:'eq',valueType:'string',b:'Гость',inputs:{a:'identity'}}),node('guest-end','end'),node('other-end','end')]);
 const r=rt();await r.start(p,'identity');await r.advance('guest');assert.equal(r.snapshot.beatId,'middle');await r.advance('drink');
 assert.equal(r.snapshot.error,null);assert.equal(r.snapshot.beatId,'guest-end');assert.equal(r.snapshot.choiceResults.identity.value,'Гость');assert.equal(r.snapshot.choiceResults.middle.value,'Чай');
 assert.equal(readLogic(p,'identity',r.snapshot.variables,new Set(),r.snapshot.choiceResults),'Гость');
 await r.start(p,'middle');assert.deepEqual(r.snapshot.choiceResults,{});r.stop();
});

test('boolean choice pin connects directly to If; mixed types require a converter',async()=>{
 const choice=node('choice','choice',{choices:[answer('yes','boolean',true,'if'),answer('no','boolean',false,'if')]}),branch=node('if','branch',{trueNext:'yes-end',falseNext:'no-end'});
 const p=project([choice,branch,node('yes-end','end'),node('no-end','end')]);assert.equal(logicType(p,choice),'boolean');
 assert.equal(applyConnections(p,[{source:'choice',sourceHandle:'value',target:'if',targetHandle:'condition'}]),null);
 const r=rt();await r.start(p,'choice');await r.advance('no');assert.equal(r.snapshot.beatId,'no-end');assert.equal(r.snapshot.error,null);r.stop();
 choice.choices[1].result={type:'string',value:'false'};assert.equal(logicType(p,choice),'any');assert.match(dataConnectionError(p,{source:'choice',sourceHandle:'value',target:'if',targetHandle:'condition'}),/разные типы/);
 p.chapters[0].beats.push(node('cast','convert',{valueType:'boolean'}));assert.equal(applyConnections(p,[{source:'choice',sourceHandle:'value',target:'cast',targetHandle:'value'},{source:'cast',sourceHandle:'value',target:'if',targetHandle:'condition'}]),null);
 await r.start(p,'choice');await r.advance('no');assert.equal(r.snapshot.beatId,'no-end');assert.equal(r.snapshot.error,null);r.stop();
});

test('unexecuted or no-result choices cannot leak a stale value; value wires are preserved on reload',()=>{
 const choice=node('choice','choice',{choices:[answer('value','number',1),answer('none','none')]});const p=project([node('entry','dialogue'),choice,node('add','math',{operator:'add',b:.5,inputs:{a:'choice'}})]);
 assert.throws(()=>readLogic(p,'choice',{}),/ещё/);assert.throws(()=>readLogic(p,'choice',{},new Set(),{choice:{type:'none'}}),/ещё/);
 const loaded=readProject(JSON.stringify(p));assert(buildStoryFlow(loaded).edges.some(e=>e.source==='choice'&&e.target==='add'&&e.sourceHandle==='value'));
});
