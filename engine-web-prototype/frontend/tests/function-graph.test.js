import test from 'node:test';
import assert from 'node:assert/strict';
import {callBlueprint,callBlueprintSync,callBlueprintAuto} from '../src/blueprintFunctions.js';
import {migrateFunctionGraph,nodeDescription,graphConnectionError,validateFunctionGraph,graphStructureFields,graphValueSchema} from '../src/functionGraph.js';
import {registerOperation} from '../src/operationRegistry.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {createSignalDefinition} from '../src/mechanicDescriptions.js';
import {gameSave} from '../src/gameSave.js';
import {ensureGameplay} from '../src/gameplayModel.js';
import {createRule} from '../src/ruleModel.js';
import {addMechanic} from '../src/mechanicsModel.js';
import {reference} from '../src/valueSchemas.js';
import {makeAction} from '../src/model.js';
const wire=nodeId=>({op:'node',nodeId,port:'value'}),get=path=>({op:'get',path});
const fn=(id,nodes,extra={})=>({id,name:id,graphVersion:1,parameters:[],locals:{},entry:nodes[0].id,nodes,...extra});
const project=functions=>({functions,variables:{count:0,a:0,b:0},variableTypes:{count:'integer',a:'integer',b:'integer'},events:[]});
registerOperation({id:'test.graph.effect',version:1,parameters:{type:'object',properties:{amount:{type:'number'}}},result:{type:'number'},contexts:['function','rule'],editor:{label:'Эффект'}},p=>p.amount);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const wait=(seconds,{assertActive})=>sleep(seconds*1000).then(()=>assertActive());

test('structure field choices follow connected and nested parameter schemas with labels',()=>{
 const f=fn('fields',[{id:'args',kind:'value',value:get('args.settings')},{id:'field',kind:'value',value:{op:'field',a:wire('args'),field:'limits.maximum'}},{id:'return',kind:'return',value:wire('field')}],{entry:'return',parameters:[{name:'settings',schema:{type:'object',properties:{limits:{type:'object',label:'Ограничения',properties:{maximum:{type:'number',label:'Верхний предел'}}}}}}]}),p=project([f]);
 assert.deepEqual(graphStructureFields(p,f,wire('args')).map(x=>[x.path,x.label]),[['limits','Ограничения'],['limits.maximum','Ограничения · Верхний предел']]);assert.equal(graphValueSchema(p,f,wire('field')).type,'number');assert.deepEqual(graphStructureFields(p,f,get('args.unknown')),[]);
});

test('a reaction waits for a pickup child graph that starts its Event after an authored delay',async()=>{
 const p=createMechanicsProject('Отложенный подбор'),scene=p.subscenes[0];scene.kind='empty';p.objects.push({id:'pickup-object',name:'Предмет',type:'Меш',active:true,subsceneId:scene.id,transforms:{[scene.id]:{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}});
 const m=addMechanic(p,'pickup',{kind:'object',id:'pickup-object'}),item=p.gameplay.items.find(i=>i.id===m.recordId),child=p.functions.find(f=>f.id===item.pickupBehavior.functions.pickup);child.nodes.push({id:'pickup-delay',kind:'wait',duration:.03,timeSource:'native',next:child.entry});child.entry='pickup-delay';
 p.variables.afterPickup=false;(p.variableTypes??={}).afterPickup='boolean';item.pickupEventId='after-pickup-event';p.events.push({id:item.pickupEventId,name:'После подбора',owner:'GameSession',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'pickup-marker-group',actions:[{...makeAction('variable','afterPickup',true),duration:0}]}]});
 p.gameplay.rules.push(createRule({signal:'start',scope:{kind:'project',id:p.id},actions:[{kind:'operation',call:{id:'sacura.interaction.pickup',version:1,parameters:{owner:reference('project',p.id),object:reference('object','pickup-object')}}}]}));ensureGameplay(p);
 const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio');}));try{await rt.start(p);for(let i=0;i<200&&!rt.game.ruleRuntime.idle;i++)await sleep(2);assert.equal(rt.game.ruleRuntime.idle,true);assert.equal(rt.snapshot.variables.afterPickup,true);assert.equal(rt.game.count(item.id,p.id),1);assert.equal(rt.game.graphScopes.size,0);assert.equal(rt.game.builtinRuns.size,0);assert.equal(rt.game.flights.size,0);assert.equal(rt.snapshot.error,null);}finally{rt.stop();}
});

test('legacy formulas become individually editable value nodes, deterministically and without losing source data',async()=>{
 const source={id:'reward',name:'Награда',parameters:[{name:'amount',type:'number',default:20}],locals:{},entry:'r',nodes:[{id:'r',kind:'return',position:{x:15,y:25},value:{op:'mul',a:{op:'add',a:get('args.amount'),b:5},b:2}}]},before=structuredClone(source),migrated=migrateFunctionGraph(source);
 assert.deepEqual(source,before);assert.equal(migrated.graphVersion,1);assert.equal(migrated.nodes.filter(n=>n.kind==='value').length,3);assert.deepEqual(migrateFunctionGraph(migrated),migrated);assert.deepEqual(migrateFunctionGraph(source),migrated);assert.deepEqual(migrated.nodes[0].position,{x:15,y:25});
 assert.equal(await callBlueprint(project([migrated]),'reward',{amount:10},{},()=>{}),30);
 const future={...source,graphVersion:99};assert.deepEqual(migrateFunctionGraph(future),future);await assert.rejects(callBlueprint(project([future]),'reward',{amount:10},{},()=>{}),/Версия графа 99/);
});

test('ports declare named typed inputs and reject incompatible wires and pure data cycles',()=>{
 const f=fn('ports',[{id:'yes',kind:'value',value:{op:'constant',value:true}},{id:'number',kind:'value',value:{op:'constant',value:2}},{id:'mul',kind:'value',value:{op:'mul',a:wire('number'),b:3}},{id:'r',kind:'return',value:wire('mul')}],{entry:'r',resultSchema:{type:'number'}}),p=project([f]);
 assert.deepEqual(nodeDescription(p,f,f.nodes[2]).inputs.map(p=>p.name),['value.a','value.b']);assert.match(graphConnectionError(p,f,'yes','value','mul','value.a'),/не подходит/);assert.equal(graphConnectionError(p,f,'number','value','mul','value.a'),null);assert.equal(validateFunctionGraph(p,f).length,0);
 f.nodes[1].value={op:'mul',a:wire('mul'),b:1};assert.match(graphConnectionError(p,f,'mul','value','number','value.a'),/Цикл/);
});

test('effect nodes execute once on the control path and their data wires read the same result',async()=>{
 const f=fn('effect',[{id:'e',kind:'operation',call:{id:'test.graph.effect',version:1,parameters:{amount:4}},next:'r'},{id:'sum',kind:'value',value:{op:'add',a:wire('e'),b:wire('e')}},{id:'r',kind:'return',value:wire('sum')}],{resultSchema:{type:'number'}}),p=project([f]);let effects=0;
 assert.equal(validateFunctionGraph(p,f).length,0);assert.equal(await callBlueprint(p,f.id,{},p.variables,c=>{effects++;return c.value.amount;}),8);assert.equal(effects,1);
 f.entry='r';assert.ok(validateFunctionGraph(p,f).some(i=>i.nodeId==='r'&&i.message.includes('до его выполнения')));await assert.rejects(callBlueprint(p,f.id,{},p.variables,()=>{effects++;}),e=>e.nodeId==='e'&&e.message.includes('до его выполнения'));assert.equal(effects,1);
});

test('each call has fresh locals while explicit instance state is shared only within that instance',async()=>{
 const child=fn('child',[{id:'write',kind:'set',path:'state.count',value:{op:'add',a:get('state.count'),b:1},next:'r'},{id:'r',kind:'return',value:get('local.temp')}],{locals:{temp:7}}),outer=fn('outer',[{id:'local',kind:'set',path:'local.temp',value:99,next:'call'},{id:'call',kind:'call',functionId:'child',next:'r'},{id:'r',kind:'return',value:{op:'struct',fields:{child:wire('call'),parent:get('local.temp')}}}],{locals:{temp:0}}),p=project([outer,child]),a={count:0},b={count:10};
 assert.deepEqual(await callBlueprint(p,'outer',{},p.variables,()=>{},{localState:a}),{child:7,parent:99});assert.deepEqual(a,{count:1});assert.deepEqual(b,{count:10});assert.deepEqual(outer.locals,{temp:0});assert.deepEqual(child.locals,{temp:7});await callBlueprint(p,'outer',{},p.variables,()=>{},{localState:b});assert.equal(a.count,1);assert.equal(b.count,11);
});

test('the synchronous and async entry points share the graph interpreter and bounded diagnostics',async()=>{
 const f=fn('sync',[{id:'e',kind:'operation',call:{id:'test.graph.effect',version:1,parameters:{amount:6}},next:'r'},{id:'r',kind:'return',value:wire('e')}]),p=project([f]);assert.equal(callBlueprintSync(p,'sync',{},p.variables,c=>c.value.amount),6);await assert.rejects(callBlueprint(p,'sync',{},p.variables,()=>{throw Error('failure');}),e=>e.nodeId==='e'&&e.functionId==='sync');assert.throws(()=>callBlueprintSync(p,'sync',{},p.variables,()=>Promise.resolve(1)),/Асинхронная/);
 const loop=fn('loop',[{id:'loop-node',kind:'set',path:'global.count',value:1,next:'loop-node'}]);p.functions.push(loop);assert.throws(()=>callBlueprintSync(p,'loop',{},p.variables,()=>{},{budget:{remaining:5}}),e=>e.nodeId==='loop-node'&&e.message.includes('лимит шагов'));
});

test('parallel branches start together and join before the following node',async()=>{
 const f=fn('parallel',[{id:'parallel',kind:'parallel',branches:['wa','wb'],next:'r'},{id:'wa',kind:'wait',duration:1,next:'a'},{id:'a',kind:'set',path:'global.a',value:1,next:'enda'},{id:'enda',kind:'return',value:'a'},{id:'wb',kind:'wait',duration:1,next:'b'},{id:'b',kind:'set',path:'global.b',value:2,next:'endb'},{id:'endb',kind:'return',value:'b'},{id:'r',kind:'return',value:{op:'add',a:get('global.a'),b:get('global.b')}}]),p=project([f]),gates=[];
 const running=callBlueprint(p,f.id,{},p.variables,()=>{},{wait:(_,context)=>new Promise(resolve=>gates.push(()=>{context.assertActive();resolve();}))});await sleep(0);assert.equal(gates.length,2);assert.equal(p.variables.a,0);for(const release of gates)release();assert.equal(await running,3);
});

test('timeout cancels its child before a late write and propagates the owning node diagnostic',async()=>{
 const f=fn('timeout',[{id:'timeout',kind:'timeout',duration:.005,body:'wait',next:'r'},{id:'wait',kind:'wait',duration:.03,next:'late'},{id:'late',kind:'set',path:'global.count',value:9},{id:'r',kind:'return',value:true}]),p=project([f]);await assert.rejects(callBlueprint(p,f.id,{},p.variables,()=>{},{wait}),e=>e.nodeId==='timeout'&&e.message.includes('время ожидания'));await sleep(45);assert.equal(p.variables.count,0);
});

test('a launched work handle outlives its preparing call and can be awaited or cancelled',async()=>{
 const child=fn('work',[{id:'wait',kind:'wait',duration:.01,next:'write'},{id:'write',kind:'set',path:'global.count',value:3,next:'r'},{id:'r',kind:'return',value:3}]),start=fn('start',[{id:'start',kind:'start',functionId:'work',next:'r'},{id:'r',kind:'return',value:wire('start')}]),join=fn('join',[{id:'join',kind:'await',handle:get('args.work'),next:'r'},{id:'r',kind:'return',value:wire('join')}]),cancel=fn('cancel',[{id:'cancel',kind:'cancel',handle:get('args.work')}]),p=project([child,start,join,cancel]),runRegistry=new Map(),options={wait,runRegistry};
 const handle=await callBlueprint(p,'start',{},p.variables,()=>{},options);assert.equal(handle.kind,'work');assert.equal(await callBlueprint(p,'join',{work:handle},p.variables,()=>{},options),3);p.variables.count=0;const second=await callBlueprint(p,'start',{},p.variables,()=>{},options);await callBlueprint(p,'cancel',{work:second},p.variables,()=>{},options);await sleep(25);assert.equal(p.variables.count,0);assert.equal(runRegistry.get(second.id).status,'cancelled');
});

test('behavior compositions use common function calls and Running/Success/Failure results',()=>{
 const failure=fn('failure',[{id:'r',kind:'return',value:'Failure'}]),running=fn('running',[{id:'r',kind:'return',value:'Running'}]),success=fn('success',[{id:'r',kind:'return',value:'Success'}]),select=fn('select',[{id:'select',kind:'btSelector',children:['failure','running','success'],next:'r'},{id:'r',kind:'return',value:wire('select')}]),sequence=fn('sequence',[{id:'sequence',kind:'btSequence',children:['success','failure','running'],next:'r'},{id:'r',kind:'return',value:wire('sequence')}]),p=project([failure,running,success,select,sequence]);assert.equal(callBlueprintSync(p,'select',{},p.variables,()=>{}),'Running');assert.equal(callBlueprintSync(p,'sequence',{},p.variables,()=>{}),'Failure');
});

test('native game time pauses graph waits; signal subscriptions clean up on owner cancellation',async()=>{
 const p=createMechanicsProject('Граф без героя');createSignalDefinition(p,{id:'test.graph.ready',name:'Готово',payloadSchema:{type:'object',properties:{amount:{type:'integer'}}}});p.variables={...p.variables,count:0};p.variableTypes={...p.variableTypes,count:'integer'};p.functions.push(fn('timed',[{id:'wait',kind:'wait',duration:.05,next:'write'},{id:'write',kind:'set',path:'global.count',value:1}]));p.functions.push(fn('signal',[{id:'wait',kind:'waitSignal',signalId:'test.graph.ready',next:'r'},{id:'r',kind:'return',value:wire('wait')}]));
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);const timed=runtime.game.callFunction('timed');await sleep(3);runtime.togglePause();runtime.playerStep([0,0,0],.1);await sleep(10);assert.equal(runtime.snapshot.variables.count,0);runtime.togglePause();runtime.playerStep([0,0,0],.1);await timed;assert.equal(runtime.snapshot.variables.count,1);
 const signal=runtime.game.callFunction('signal');await sleep(3);assert.equal(runtime.game.ruleRuntime.signalWaiters.size,1);await runtime.game.trigger('test.graph.ready',{amount:5});assert.deepEqual(await signal,{amount:5});assert.equal(runtime.game.ruleRuntime.signalWaiters.size,0);
 const stopped=runtime.game.callFunction('signal');await sleep(3);runtime.stop();await assert.rejects(stopped,/CANCELLED|INSTANCE_STOPPED/);assert.equal(runtime.game.ruleRuntime.signalWaiters.size,0);
 }finally{runtime.stop();}
});

test('handler admission computes a snapshot guard once and a false once-rule remains available',async()=>{
 let reads=0;registerOperation({id:'test.graph.guard',version:1,pure:true,parameters:{type:'object',properties:{}},result:{type:'boolean'},contexts:['rule','function'],editor:{label:'Условие'}},(_,c)=>{reads++;return c.variables.count>0;});const p=createMechanicsProject('Условие');p.variables={...p.variables,count:0};p.variableTypes={...p.variableTypes,count:'integer'};createSignalDefinition(p,{id:'test.graph.trigger',name:'Проверка',payloadSchema:{type:'object',properties:{}}});p.gameplay.rules.push(createRule({signal:'test.graph.trigger',scope:{kind:'project',id:p.id},once:true,conditionOperation:{id:'test.graph.guard',version:1,parameters:{}},actions:[{kind:'function',functionId:'result'}]}));p.functions.push(fn('result',[{id:'r',kind:'return',value:true}]));const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('test.graph.trigger',{});assert.equal(reads,1);assert.equal(runtime.game.state.rules[p.gameplay.rules[0].id]?.fired,undefined);runtime.snapshot.variables.count=1;await runtime.game.trigger('test.graph.trigger',{});assert.equal(reads,2);assert.equal(runtime.game.state.rules[p.gameplay.rules[0].id]?.fired,true);}finally{runtime.stop();}
});


test('parallel branches inherit the launch snapshot of locals and completed node results',async()=>{
 const f=fn('branch-scope',[{id:'effect',kind:'operation',call:{id:'test.graph.effect',version:1,parameters:{amount:4}},next:'local'},{id:'local',kind:'set',path:'local.value',value:3,next:'split'},{id:'split',kind:'parallel',branches:['branch'],next:'result'},{id:'branch',kind:'return',value:{op:'add',a:get('local.value'),b:wire('effect')}},{id:'result',kind:'return',value:wire('split')}],{locals:{value:0}}),p=project([f]);
 assert.equal(validateFunctionGraph(p,f).length,0);assert.deepEqual(await callBlueprint(p,f.id,{},p.variables,c=>c.value.amount),[7]);
});

test('launched owner work survives preparation completion, gates saves and cancels on owner removal',async()=>{
 const p=createMechanicsProject('Запуск работы');p.variables={count:0};p.variableTypes={count:'integer'};p.objects.push({id:'owner',name:'Владелец',active:true,subsceneId:p.subscenes[0].id});
 p.functions.push(fn('owned-work',[{id:'wait',kind:'wait',duration:.05,next:'late'},{id:'late',kind:'set',path:'global.count',value:5}]));p.functions.push(fn('prepare',[{id:'start',kind:'start',functionId:'owned-work',next:'return'},{id:'return',kind:'return',value:wire('start')} ]));
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);const context={scope:{kind:'object',id:'owner'},sceneId:p.subscenes[0].id},handle=await runtime.game.callFunction('prepare',{},context);assert.equal(runtime.game.graphRuns.get(handle.id).status,'running');assert.throws(()=>gameSave(runtime),/после выполнения/);runtime.playerStep([0,0,0],.1);await runtime.game.graphRuns.get(handle.id).promise;assert.equal(runtime.snapshot.variables.count,5);await sleep(0);assert.equal(gameSave(runtime).variables.count,5);
 runtime.snapshot.variables.count=0;const second=await runtime.game.callFunction('prepare',{},context);runtime.project.objects=runtime.project.objects.filter(o=>o.id!=='owner');await assert.rejects(runtime.game.graphRuns.get(second.id).promise,/INSTANCE_STOPPED/);assert.equal(runtime.snapshot.variables.count,0);assert.equal(runtime.game.graphRuns.get(second.id).status,'cancelled');
 }finally{runtime.stop();}
});


test('auto execution preserves immediate native results and switches to awaiting only for real asynchronous work',async()=>{
 const immediate=fn('immediate',[{id:'op',kind:'operation',call:{id:'test.graph.effect',version:1,parameters:{amount:8}},next:'r'},{id:'r',kind:'return',value:wire('op')}]),delayed=fn('delayed',[{id:'wait',kind:'wait',duration:.001,next:'r'},{id:'r',kind:'return',value:9}]),p=project([immediate,delayed]);assert.equal(callBlueprintAuto(p,'immediate',{},p.variables,c=>c.value.amount),8);const pending=callBlueprintAuto(p,'delayed',{},p.variables,()=>{},{wait});assert.equal(typeof pending.then,'function');assert.equal(await pending,9);
});

test('timeout tears down its signal subscription immediately when its child is cancelled',async()=>{
 const p=createMechanicsProject('Отмена подписки');createSignalDefinition(p,{id:'test.graph.never',name:'Ненаступивший сигнал',payloadSchema:{type:'object',properties:{}}});p.functions.push(fn('timeout-signal',[{id:'timeout',kind:'timeout',timeSource:'real',duration:.01,body:'signal'},{id:'signal',kind:'waitSignal',signalId:'test.graph.never'}]));const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);const pending=runtime.game.callFunction('timeout-signal');await sleep(2);assert.equal(runtime.game.ruleRuntime.signalWaiters.size,1);await assert.rejects(pending,/время ожидания/);assert.equal(runtime.game.ruleRuntime.signalWaiters.size,0);assert.equal(runtime.game.graphScopes.size,0);assert.equal(runtime.game.flights.size,0);}finally{runtime.stop();}
});


test('migrated legacy object subjects still admit only their own object among two live owners',async()=>{
 const p=createMechanicsProject('Два владельца');p.variables={a:0,b:0};p.variableTypes={a:'integer',b:'integer'};for(const id of ['a','b']){p.objects.push({id,name:id,active:true,subsceneId:p.subscenes[0].id});p.functions.push(fn('record-'+id,[{id:'write',kind:'set',path:'global.'+id,value:1}]));p.gameplay.rules.push({id:'legacy-'+id,name:id,trigger:'interact',objectId:id,functionId:'record-'+id});}
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('interact',{objectId:'b'});assert.equal(runtime.snapshot.variables.a,0);assert.equal(runtime.snapshot.variables.b,1);assert.ok(runtime.project.gameplay.rules.filter(r=>r.id.startsWith('legacy-')).every(r=>r.version===1&&r.matchObjectId===true));}finally{runtime.stop();}
});

test('legacy custom-signal subject filters preserve exact payload matching including absent objectId',async()=>{
 const p=createMechanicsProject('Данные пользовательского сигнала');p.variables={count:0};p.variableTypes={count:'integer'};for(const id of ['a','b'])p.objects.push({id,name:id,active:true,subsceneId:p.subscenes[0].id});createSignalDefinition(p,{id:'test.graph.subject',name:'Субъект',payloadSchema:{type:'object',properties:{objectId:{type:'string',optional:true}}}});p.functions.push(fn('record-subject',[{id:'write',kind:'set',path:'global.count',value:{op:'add',a:get('global.count'),b:1}}]));p.gameplay.rules.push({id:'legacy-subject',name:'Только объект A',trigger:'test.graph.subject',objectId:'a',functionId:'record-subject'});
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('test.graph.subject',{});await runtime.game.trigger('test.graph.subject',{objectId:'b'});assert.equal(runtime.snapshot.variables.count,0);await runtime.game.trigger('test.graph.subject',{objectId:'a'});assert.equal(runtime.snapshot.variables.count,1);}finally{runtime.stop();}
});


test('adding effects to an initially empty canonical handler executes the graph and keeps an empty false branch available',async()=>{
 const p=createMechanicsProject('Пустой граф');p.variables={count:0,allowed:false};p.variableTypes={count:'integer',allowed:'boolean'};createSignalDefinition(p,{id:'test.graph.empty',name:'Запустить пустую реакцию',payloadSchema:{type:'object',properties:{}}});const rule=createRule({signal:'test.graph.empty',scope:{kind:'project',id:p.id},once:true,condition:get('global.allowed'),actions:[]});p.gameplay.rules.push(rule);ensureGameplay(p);const handler=p.functions.find(f=>f.id===rule.handlerFunctionId),branch=handler.nodes.find(n=>n.id===handler.admission.branchId),variable=p.gameplay.variableBindings.find(v=>v.key==='count');handler.nodes.push({id:handler.id+'-new-effect',kind:'operation',call:{id:'sacura.variable.set',version:1,parameters:{variable:{kind:'reference',type:'variable',mode:'direct',id:variable.id},value:3}},next:branch.trueNext});branch.trueNext=handler.id+'-new-effect';assert.deepEqual(rule.actions,[]);
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('test.graph.empty',{});assert.equal(runtime.game.state.rules[rule.id]?.fired,undefined);assert.equal(runtime.snapshot.variables.count,0);runtime.snapshot.variables.allowed=true;await runtime.game.trigger('test.graph.empty',{});assert.equal(runtime.snapshot.variables.count,3);assert.equal(runtime.game.state.rules[rule.id].fired,true);}finally{runtime.stop();}
});

test('adding an otherwise effect uses the edited handler branch even when the rule marker list remains empty',async()=>{
 const p=createMechanicsProject('Ветка иначе');p.variables={count:0};p.variableTypes={count:'integer'};createSignalDefinition(p,{id:'test.graph.otherwise',name:'Проверка иначе',payloadSchema:{type:'object',properties:{}}});const rule=createRule({signal:'test.graph.otherwise',scope:{kind:'project',id:p.id},condition:false,actions:[],otherwise:[]});p.gameplay.rules.push(rule);ensureGameplay(p);const handler=p.functions.find(f=>f.id===rule.handlerFunctionId),branch=handler.nodes.find(n=>n.id===handler.admission.branchId);handler.nodes.push({id:handler.id+'-otherwise-effect',kind:'set',path:'global.count',value:7,next:branch.falseNext});branch.falseNext=handler.id+'-otherwise-effect';assert.deepEqual(rule.otherwise,[]);
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('test.graph.otherwise',{});assert.equal(runtime.snapshot.variables.count,7);}finally{runtime.stop();}
});


test('handler admission follows the visible guard call arguments and selected function',async()=>{
 const p=createMechanicsProject('Аргументы условия');p.variables={count:0};p.variableTypes={count:'integer'};createSignalDefinition(p,{id:'test.graph.guard-input',name:'Вход условия',payloadSchema:{type:'object',properties:{amount:{type:'number'}}}});p.functions.push(fn('accept-any',[{id:'r',kind:'return',value:true}]));p.functions.push(fn('count-effect',[{id:'write',kind:'set',path:'global.count',value:1}]));const r=createRule({signal:'test.graph.guard-input',scope:{kind:'project',id:p.id},condition:{op:'gt',a:get('args.amount'),b:0},actions:[{kind:'function',functionId:'count-effect'}]});p.gameplay.rules.push(r);ensureGameplay(p);const handler=p.functions.find(f=>f.id===r.handlerFunctionId),call=handler.nodes.find(n=>n.id===handler.admission.callNodeId);call.args.amount=-2;
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);await runtime.game.trigger('test.graph.guard-input',{amount:3});assert.equal(runtime.snapshot.variables.count,0);const live=runtime.project.functions.find(f=>f.id===handler.id).nodes.find(n=>n.id===call.id);live.functionId='accept-any';await runtime.game.trigger('test.graph.guard-input',{amount:3});assert.equal(runtime.snapshot.variables.count,1);}finally{runtime.stop();}
});

test('handler admission preserves local prelude results and the sampled branch value until execution',async()=>{
 const p=createMechanicsProject('Контекст условия');p.variables={count:0,allowed:true};p.variableTypes={count:'integer',allowed:'boolean'};createSignalDefinition(p,{id:'test.graph.prelude',name:'Подготовка условия',payloadSchema:{type:'object',properties:{}}});const r=createRule({signal:'test.graph.prelude',scope:{kind:'project',id:p.id},condition:true,actions:[]});p.gameplay.rules.push(r);ensureGameplay(p);const handler=p.functions.find(f=>f.id===r.handlerFunctionId),branch=handler.nodes.find(n=>n.id===handler.admission.branchId);handler.locals={computed:0};handler.nodes.push({id:handler.id+'-prelude',kind:'set',path:'local.computed',value:5,next:handler.entry},{id:handler.id+'-effect',kind:'set',path:'global.count',value:get('local.computed'),next:branch.trueNext});handler.entry=handler.id+'-prelude';branch.condition=get('global.allowed');branch.trueNext=handler.id+'-effect';
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);const pending=runtime.game.trigger('test.graph.prelude',{});runtime.snapshot.variables.allowed=false;await pending;assert.equal(runtime.snapshot.variables.count,5);assert.equal(runtime.snapshot.variables.allowed,false);assert.equal(handler.locals.computed,0);}finally{runtime.stop();}
});


test('cancelling an awaiting owner releases its caller while independent project work keeps running',async()=>{
 const p=createMechanicsProject('Отмена ожидания работы');p.variables={count:0};p.variableTypes={count:'integer'};p.objects.push({id:'waiter',name:'Ожидающий владелец',active:true,subsceneId:p.subscenes[0].id});createSignalDefinition(p,{id:'test.graph.release',name:'Завершить работу',payloadSchema:{type:'object',properties:{}}});p.functions.push(fn('project-work',[{id:'signal',kind:'waitSignal',signalId:'test.graph.release',next:'r'},{id:'r',kind:'return',value:true}]));p.functions.push(fn('launch-project-work',[{id:'start',kind:'start',functionId:'project-work',next:'r'},{id:'r',kind:'return',value:wire('start')} ]));p.functions.push(fn('await-project-work',[{id:'await',kind:'await',handle:get('args.work'),next:'late'},{id:'late',kind:'set',path:'global.count',value:9}]));
 const runtime=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await runtime.start(p);const handle=await runtime.game.callFunction('launch-project-work',{}, {scope:{kind:'project',id:p.id}}),work=runtime.game.graphRuns.get(handle.id),waiting=runtime.game.callFunction('await-project-work',{work:handle},{scope:{kind:'object',id:'waiter'},sceneId:p.subscenes[0].id});runtime.project.objects=runtime.project.objects.filter(o=>o.id!=='waiter');runtime.game.cleanupOwners();await assert.rejects(waiting,/INSTANCE_STOPPED/);assert.equal(runtime.game.graphScopes.size,0);assert.equal(work.status,'running');assert.equal(runtime.snapshot.variables.count,0);await runtime.game.trigger('test.graph.release',{});await work.promise;await sleep(0);assert.equal(runtime.game.flights.size,0);assert.equal(runtime.snapshot.variables.count,0);}finally{runtime.stop();}
});
