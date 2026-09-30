import test from 'node:test';
import assert from 'node:assert/strict';
import {ANSWER_VARIABLE,evaluateCondition,availabilityOf,choiceAvailable,newChoice,choiceValue,storyPorts} from '../src/choiceModel.js';
import {chooseNext,validateStudio,upgradeProject,normalizeBatches,newEvent} from '../src/studioModel.js';
import {PreviewRuntime} from '../src/runtime.js';
import {applyConnections} from '../src/storyConnections.js';
import {buildStoryFlow} from '../src/storyFlowModel.js';
import {buildTimelineModel} from '../src/timelineModel.js';
import {cloneStoryNodes} from '../src/editorClipboard.js';
const rule=(variable,value,type='string',operator='eq')=>({mode:'all',rules:[{variable,value,type,operator}]});
const node=(id,extra={})=>normalizeBatches({id,kind:'dialogue',speaker:'Рассказчик',text:id,bindings:[],next:null,...extra});
const project=(beats,events=[])=>({version:2,title:'Choices',variables:{permission:false,score:2,decision:''},events,objects:[],subscenes:[{id:'room',name:'Room',location:'Room',kind:'empty',entry:beats[0].id}],chapters:[{id:'c',subsceneId:'room',beats}]});
const runtime=()=>new PreviewRuntime({stopAll(){},setSidechain(){},unlock:async()=>{}});

test('conditions compare arbitrary typed variables with all/any and reject missing data',()=>{
 const vars={permission:true,score:4,name:'дом'};
 assert.equal(evaluateCondition(rule('score',3,'number','gte'),vars),true);
 assert.equal(evaluateCondition(rule('name','дом'),vars),true);
 assert.equal(evaluateCondition(rule('permission',true,'boolean'),vars),true);
 assert.equal(evaluateCondition(rule('absent',false,'boolean'),vars),false);
 assert.equal(evaluateCondition({rules:[]},vars),false);
 const rules=[...rule('permission',false,'boolean').rules,...rule('score',4,'number').rules];
 assert.equal(evaluateCondition({mode:'all',rules},vars),false);
 assert.equal(evaluateCondition({mode:'any',rules},vars),true);
 assert.equal(evaluateCondition(rule('score','bad','number','ne'),vars),false);
});

test('legacy answer conditions and destinations survive save/load without any answer catalog',()=>{
 const p=project([node('question',{kind:'choice',choices:[{id:'local',label:'Мой ответ',condition:'trust',threshold:3,next:'end'}]}),node('end',{kind:'end'})]);p.variables.trust=4;
 const restored=upgradeProject(JSON.parse(JSON.stringify(p)));
 const choice=restored.chapters[0].beats[0].choices[0];
 assert.equal(choiceAvailable(choice,restored.variables),true);assert.equal(choiceAvailable(choice,{trust:2}),false);
 assert.equal(chooseNext(restored,'question','local',restored.variables).id,'end');
 assert.equal(availabilityOf(choice).rules[0].operator,'gte');assert.equal(Object.hasOwn(restored,'choices'),false);
});

test('a local answer returns a typed value before AFTER events and automatic branching',async()=>{
 const after=newEvent('variable','score','0');Object.assign(after.groups[0].actions[0],{operation:'add',valueType:'number',value:3});
 const p=project([node('question',{kind:'choice',resultVariable:'decision',next:'check',choices:[{...newChoice('left','Остаться'),result:{type:'number',value:0}},{...newChoice('right','Уйти'),result:{type:'number',value:1}}],bindings:[{id:'after',eventId:after.id,hook:'AFTER',join:'EVENT_END',overrides:{}}]}),node('check',{kind:'branch',test:rule(ANSWER_VARIABLE,0,'number'),trueNext:'yes',falseNext:'no'}),node('yes'),node('no')],[after]);
 assert.equal(validateStudio(p).filter(i=>i.level==='error').length,0);
 const rt=runtime();await rt.start(p,'question');await rt.advance('left');
 assert.equal(rt.snapshot.beatId,'yes');assert.equal(rt.snapshot.variables.score,5);assert.equal(rt.snapshot.variables.decision,0);assert.equal(rt.snapshot.variables[ANSWER_VARIABLE],0);
 assert.deepEqual(rt.snapshot.choiceResult,{beatId:'question',choiceId:'left',type:'number',value:0});
 assert.deepEqual(rt.snapshot.history,['question','check','yes']);assert.equal(p.variables.decision,'');rt.stop();
 await rt.start(p,'question');assert.equal(rt.snapshot.choiceResult,null);await rt.advance('right');assert.equal(rt.snapshot.beatId,'no');rt.stop();
});

test('unavailable and unknown answers cannot write results or advance, including after availability changes',async()=>{
 const c={...newChoice('locked','Ответ'),availability:rule('permission',true,'boolean'),result:{type:'boolean',value:false}};
 const p=project([node('q',{kind:'choice',choices:[c,newChoice('safe','Позже')],next:'end'}),node('end')]);const rt=runtime();await rt.start(p,'q');
 await rt.advance('locked');await rt.advance('missing');assert.equal(rt.snapshot.beatId,'q');assert.equal(rt.snapshot.choiceResult,null);
 rt.snapshot.variables.permission=true;await rt.advance('locked');assert.equal(rt.snapshot.beatId,'end');assert.equal(rt.snapshot.choiceResult.value,false);rt.stop();
});

test('common and per-answer exits, check pins, deletion/copy layout form real routes',()=>{
 const q=node('q',{kind:'choice',next:'check',choices:[newChoice('one','Свой'),{...newChoice('two','Другой'),next:'end'}]});
 const check=node('check',{kind:'branch',test:rule(ANSWER_VARIABLE,'Свой'),trueNext:'end',falseNext:'q'}),p=project([q,check,node('end',{kind:'end'})]);
 assert.equal(applyConnections(p,[{source:'q',sourceHandle:'next',target:'check'},{source:'check',sourceHandle:'false',target:'end'}]),null);
 assert.equal(check.falseNext,'end');assert.equal(chooseNext(p,'q','two',{}).id,'end');
 const flow=buildStoryFlow(p);assert.ok(flow.edges.some(e=>e.sourceHandle==='next'&&e.source==='q'&&e.target==='check'));assert.ok(flow.edges.some(e=>e.sourceHandle==='true'));
 const timeline=buildTimelineModel(p);assert.ok(timeline.nodes.some(n=>n.title==='Проверка'));assert.ok(timeline.edges.some(e=>e.label==='Да'));
 assert.ok(timeline.dependencies.some(e=>e.variableId===ANSWER_VARIABLE));assert.equal(timeline.variables.find(v=>v.id===ANSWER_VARIABLE).missing,false);
 const copied=cloneStoryNodes([q,check,p.chapters[0].beats[2]]);assert.equal(copied[1].trueNext,copied[2].id);assert.equal(copied[0].next,copied[1].id);
 assert.equal(storyPorts(q).find(port=>port.common).next,'check');assert.equal(choiceValue({...newChoice('x'),result:{type:'boolean',value:false}}),false);
});

test('automatic check loops stop with a diagnostic and incomplete checks are diagnosed',async()=>{
 const p=project([node('loop',{kind:'branch',test:rule('permission',false,'boolean'),trueNext:'loop',falseNext:'end'}),node('end')]),rt=runtime();await rt.start(p,'loop');
 assert.equal(rt.snapshot.phase,'ERROR');assert.match(rt.snapshot.error,/цикл/);rt.stop();
 p.chapters[0].beats[0].test={rules:[]};assert.ok(validateStudio(p).some(i=>i.id.startsWith('empty-condition')));
});

test('conditional answers warn without blocking, while an empty choice blocks play',()=>{
 const p=project([node('q',{kind:'choice',next:'end',choices:[{...newChoice('only'),availability:rule('permission',false,'boolean')}]}),node('end')]);
 assert.equal(validateStudio(p).find(i=>i.fix==='fallback').level,'warning');
 p.chapters[0].beats[0].choices=[];assert.ok(validateStudio(p).some(i=>i.id==='empty-choice-q'&&i.level==='error'));
});

import {readLogic,migrateLogicGraph,isPureNode} from '../src/logicModel.js';
import {conditionPass} from '../src/studioModel.js';
import {deleteStoryNode} from '../src/storyEditing.js';
test('wired variables, comparisons, If and set-variable execute without displaying logic nodes',async()=>{
 const q=node('q',{kind:'choice',next:'set',choices:[{...newChoice('go','Go'),result:{type:'number',value:4},enabledSource:'compare'}]});
 const p=project([q,node('read',{kind:'variable',variable:'score',valueType:'number'}),node('compare',{kind:'compare',operator:'gte',valueType:'number',b:2,inputs:{a:'read'}}),node('answer',{kind:'variable',variable:ANSWER_VARIABLE,valueType:'number'}),node('set',{kind:'set-variable',variable:'score',inputs:{value:'answer'},next:'if'}),node('if',{kind:'branch',inputs:{condition:'compare'},trueNext:'yes',falseNext:'no'}),node('yes'),node('no')]);
 assert.equal(conditionPass(q.choices[0],{score:1},p),false);assert.equal(conditionPass(q.choices[0],p.variables,p),true);
 const rt=runtime();await rt.start(p,'q');await rt.advance('go');assert.equal(rt.snapshot.variables.score,4);assert.equal(rt.snapshot.beatId,'yes');assert.deepEqual(rt.snapshot.history,['q','set','if','yes']);rt.stop();
 const graph=buildStoryFlow(p);assert.ok(graph.edges.some(e=>e.data.valueWire&&e.targetHandle==='condition'));
 assert.ok(!buildTimelineModel(p).nodes.some(n=>n.beatIds?.includes('compare')));
});
test('no-result answers clear the transient answer without overwriting remembered variables',async()=>{
 const q=node('q',{kind:'choice',next:'end',resultVariable:'decision',choices:[newChoice('go','Go')]}),p=project([q,node('end')]),rt=runtime();
 await rt.start(p,'q');rt.snapshot.variables[ANSWER_VARIABLE]='old';await rt.advance('go');assert.equal(rt.snapshot.beatId,'end');assert.equal(rt.snapshot.choiceResult,null);assert.equal(Object.hasOwn(rt.snapshot.variables,ANSWER_VARIABLE),false);assert.equal(rt.snapshot.variables.decision,'');rt.stop();
});
test('data wires reject type confusion and cycles atomically, disconnect only their own input',()=>{
 const p=project([node('read',{kind:'variable',variable:'score'}),node('a',{kind:'compare',operator:'eq',valueType:'number'}),node('b',{kind:'compare',operator:'eq',valueType:'boolean'}),node('if',{kind:'branch'})]);
 assert.ok(applyConnections(p,[{source:'read',sourceHandle:'value',target:'if',targetHandle:'condition'}]));
 assert.ok(applyConnections(p,[{source:'a',sourceHandle:'value',target:'b',targetHandle:'a'},{source:'b',sourceHandle:'value',target:'a',targetHandle:'a'}]));assert.equal(p.chapters[0].beats[2].inputs,undefined);
 assert.equal(applyConnections(p,[{source:'read',sourceHandle:'value',target:'a',targetHandle:'a'},{source:'read',sourceHandle:'value',target:'a',targetHandle:'b'}]),null);
 assert.equal(applyConnections(p,[{source:'read',sourceHandle:'value',target:'a',targetHandle:'a',disconnect:true}]),null);assert.equal(p.chapters[0].beats[1].inputs.b,'read');
 deleteStoryNode(p,'read');assert.equal(p.chapters[0].beats.find(n=>n.id==='a').inputs.b,undefined);
});
test('legacy conditions become visible data nodes once and keep their result after save/load',()=>{
 const q=node('q',{kind:'choice',next:'end',choices:[{id:'go',label:'Go',condition:'score',availability:rule('score',2,'number','gte')}]});
 const p=migrateLogicGraph(project([q,node('end')])),count=p.chapters[0].beats.length;assert.ok(isPureNode(p.chapters[0].beats.at(-1)));assert.equal(conditionPass(q.choices[0],p.variables,p),true);assert.equal(conditionPass(q.choices[0],{score:1},p),false);migrateLogicGraph(p);assert.equal(p.chapters[0].beats.length,count);
 const saved=JSON.parse(JSON.stringify(p));assert.equal(readLogic(saved,q.choices[0].enabledSource,saved.variables),true);
 const copies=cloneStoryNodes(p.chapters[0].beats);assert.ok(copies[0].choices[0].enabledSource&&copies.some(n=>n.id===copies[0].choices[0].enabledSource));
});

import {variablePalette,initializeVariableNode} from '../src/logicModel.js';
test('variable search includes custom names, aliases and typed getters/setters without resetting values',()=>{
 const p={variables:{Монеты:10,permission:false,trust:0,message:'привет'}},items=variablePalette(p);
 assert.equal(items.filter(i=>i.variable==='Монеты').length,2);
 assert.ok(items.find(i=>i.variable==='trust').label.includes('Доверие'));
 assert.ok(items.find(i=>i.variable==='trust').searchText.includes('trust'));
 assert.equal(items.filter(i=>i.variable===ANSWER_VARIABLE).length,1);
 for(const item of items){const n={kind:item.kind};initializeVariableNode(p,n,item.variable);assert.equal(n.variable,item.variable);if(item.variable!==ANSWER_VARIABLE)assert.equal(n.valueType,typeof p.variables[item.variable]);}
 assert.deepEqual(p.variables,{Монеты:10,permission:false,trust:0,message:'привет'});
});

test('Integer definitions coerce Set and event assignments consistently',async()=>{
 const e=newEvent('variable','score','0');Object.assign(e.groups[0].actions[0],{operation:'add',valueType:'number',value:1.8});
 const p=project([node('set',{kind:'set-variable',variable:'score',value:4.9,next:'end'}),node('end',{bindings:[{id:'start',eventId:e.id,hook:'ON_START',join:'EVENT_END',overrides:{}}]})],[e]);p.variableTypes={score:'integer'};
 const rt=runtime();await rt.start(p,'set');assert.equal(rt.snapshot.variables.score,5);assert.equal(rt.snapshot.beatId,'end');rt.stop();
});
