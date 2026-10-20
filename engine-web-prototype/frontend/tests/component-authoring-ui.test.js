import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {EditorView} from 'codemirror';
import {setupCodeMirrorDom} from './helpers/codeMirrorDom.js';
import {counterSource} from './fixtures/componentCounter.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;setupCodeMirrorDom(dom.window);globalThis.ResizeObserver=class{observe(){}unobserve(){}disconnect(){}};globalThis.requestAnimationFrame=callback=>setTimeout(callback,0);globalThis.cancelAnimationFrame=clearTimeout;
const {render,screen,within,fireEvent,cleanup,waitFor,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Workspace}=await server.ssrLoadModule('/src/GameplayWorkspace.jsx');
const {default:Graph}=await server.ssrLoadModule('/src/BlueprintFunctionWorkspace.jsx');
const {default:Components}=await server.ssrLoadModule('/src/ComponentWorkspace.jsx');
const {createMechanicsProject}=await server.ssrLoadModule('/src/projectLifecycle.js');
const {createComponentResource,editComponentSource,publishComponentSource}=await server.ssrLoadModule('/src/componentResources.js');
const {addMechanicInstance}=await server.ssrLoadModule('/src/mechanicDescriptions.js');
const {PreviewRuntime}=await server.ssrLoadModule('/src/runtime.js'),{SoundDesk}=await server.ssrLoadModule('/src/audio.js');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
function mount(Component,p,props={}){let value=p;function Harness(){const [project,setP]=React.useState(p);return React.createElement(Component,{...props,project,onChange:fn=>{const next=structuredClone(value);fn(next);value=next;setP(next);}});}render(React.createElement(Harness));return()=>value;}
function replaceScript(source){const editor=EditorView.findFromDOM(document.querySelector('.cm-editor'));act(()=>editor.dispatch({changes:{from:0,to:editor.state.doc.length,insert:source}}));}
async function save(){fireEvent.click(screen.getByRole('button',{name:'Сохранить компонент',exact:true}));await waitFor(()=>assert.match(document.querySelector('.component-code-editor footer').textContent,/Сохранено/));}
function openConnections(){const button=screen.getByRole('button',{name:'Подключения компонента',exact:true});if(button.getAttribute('aria-expanded')!=='true')fireEvent.click(button);}
function addGraphNode(name){fireEvent.click(screen.getByRole('button',{name:'Добавить узел',exact:true}));fireEvent.change(screen.getByRole('searchbox',{name:'Найти узел'}),{target:{value:name}});const button=[...screen.getByRole('dialog',{name:'Добавление узла'}).querySelectorAll('button')].find(b=>b.querySelector('strong')?.textContent===name);assert.ok(button);fireEvent.click(button);}

test('author creates Counter from nothing, saves actual JavaScript, adds two objects, changes source, and retains an explained syntax error',async()=>{
 const p=createMechanicsProject('Новый тип');for(const id of ['object-a','object-b'])p.objects.push({id,name:id,active:true,type:'Активный меш',subsceneId:p.subscenes[0].id});const value=mount(Workspace,p,{sceneId:p.subscenes[0].id,selectedObjectId:'object-a'});
 fireEvent.click(screen.getByRole('button',{name:'Создать компонент',exact:true}));assert.equal(screen.queryByRole('radio'),null);assert.match(document.querySelector('.component-create').textContent,/JavaScript/);fireEvent.change(screen.getByRole('textbox',{name:'Название компонента'}),{target:{value:'Counter'}});fireEvent.click(screen.getByRole('button',{name:'Создать',exact:true}));assert.ok(screen.getByRole('textbox',{name:'JavaScript компонента'}));assert.equal(value().logicResources.length,1);assert.equal(value().logicResources[0].kind,'component.code');assert.equal(value().functions.length,0);
 replaceScript(counterSource());await save();assert.equal(value().logicResources[0].source,counterSource());
 openConnections();fireEvent.change(screen.getByRole('combobox',{name:'Владелец экземпляра'}),{target:{value:'object:object-a'}});fireEvent.click(screen.getByRole('button',{name:'Добавить экземпляр'}));fireEvent.change(screen.getByRole('spinbutton',{name:'step',exact:true}),{target:{value:'2'}});
 openConnections();fireEvent.change(screen.getByRole('combobox',{name:'Владелец экземпляра'}),{target:{value:'object:object-b'}});fireEvent.click(screen.getByRole('button',{name:'Добавить экземпляр'}));fireEvent.change(screen.getByRole('spinbutton',{name:'step',exact:true}),{target:{value:'5'}});
 const [a,b]=value().gameplay.mechanics,rt=new PreviewRuntime(new SoundDesk(()=>{}));try{await rt.start(value());assert.equal(rt.snapshot.error,null);assert.equal(await rt.game.callComponent(a.id,'add',{amount:3}),6);assert.equal(await rt.game.callComponent(b.id,'add',{amount:3}),15);rt.stop();
  replaceScript(counterSource({multiply:true,formula:'args.amount * params.step * 2'}));await save();await rt.start(value());assert.equal(await rt.game.callComponent(a.id,'add',{amount:3}),12);rt.stop();
  replaceScript('const component = {\n methods: ( ;\n};');fireEvent.click(screen.getByRole('button',{name:'Сохранить компонент',exact:true}));await waitFor(()=>assert.ok(screen.getByRole('alert')));assert.match(screen.getByRole('alert').textContent,/Counter.js:2:/);assert.equal(value().logicResources[0].source,'const component = {\n methods: ( ;\n};');await rt.start(value());assert.match(rt.snapshot.error,/Counter.js/);
 }finally{rt.stop();}
});

test('a newly published method appears in the graph palette with receiver, argument and result pins; opening code and Back preserves caller',async()=>{
 const p=createMechanicsProject('Граф вызывает код'),resource=createComponentResource(p,{name:'Counter'});editComponentSource(p,resource.id,counterSource({multiply:true}));publishComponentSource(p,resource.id);const m=addMechanicInstance(p,resource.definitionId,{kind:'project',id:p.id});
 const fn={id:'caller',name:'Вызывающий граф',graphVersion:1,parameters:[],locals:{},entry:'return',nodes:[{id:'return',kind:'return'}]};p.functions.push(fn);const value=mount(Graph,p,{requestedId:fn.id});
 addGraphNode('Counter · Multiply');const call=value().functions.find(f=>f.id===fn.id).nodes.at(-1),node=within(document.querySelector('[data-id="'+call.id+'"]'));
 assert.ok(node.getByLabelText(/Вход Получатель/));assert.ok(node.getByLabelText(/Вход factor/));assert.ok(node.getByLabelText(/Выход Результат/));fireEvent.change(node.getByLabelText('Получатель',{selector:'select'}),{target:{value:m.id}});fireEvent.change(node.getByLabelText('factor',{selector:'input'}),{target:{value:4}});
 const before=JSON.stringify(value().functions.find(f=>f.id===fn.id));fireEvent.doubleClick(document.querySelector('[data-id="'+call.id+'"]'));assert.ok(screen.getByRole('textbox',{name:'JavaScript компонента'}));assert.match(document.querySelector('.component-code-editor header').textContent,/multiply/);
 fireEvent.click(screen.getByRole('button',{name:'К месту вызова'}));assert.ok(screen.getByRole('region',{name:'Граф функции'}));assert.equal(screen.getByRole('combobox',{name:'Функция',exact:true}).value,'caller');assert.equal(JSON.stringify(value().functions.find(f=>f.id===fn.id)),before);
});

test('new project types are available in Add Component and public method choices use stable definition IDs',async()=>{
 const p=createMechanicsProject('Каталог'),r=createComponentResource(p,{name:'Counter'});editComponentSource(p,r.id,counterSource({multiply:true}));publishComponentSource(p,r.id);const value=mount(Workspace,p,{sceneId:p.subscenes[0].id});
 fireEvent.click(screen.getByRole('button',{name:'Добавить компонент',exact:true}));const selector=screen.getByRole('combobox',{name:'Компонент',exact:true});assert.ok(within(selector).getByRole('option',{name:'Counter'}));fireEvent.change(selector,{target:{value:r.definitionId}});fireEvent.click(screen.getByRole('button',{name:'Добавить',exact:true}));assert.equal(value().gameplay.mechanics[0].definitionId,r.definitionId);assert.ok(screen.getByRole('textbox',{name:'JavaScript компонента'}));
});
test('a new non-template type is authored and linked through the editor, and runtime errors offer the actual receiver implementation',async()=>{
 const {relaySource,ledgerSource}=await import('./fixtures/componentAcceptance.js'),{default:Issue}=await server.ssrLoadModule('/src/ComponentRuntimeIssue.jsx');
 const p=createMechanicsProject('Новый авторский путь');p.objects.push({id:'new-owner',name:'Новый владелец',type:'Активный меш',active:true,subsceneId:p.subscenes[0].id});const value=mount(Workspace,p,{sceneId:p.subscenes[0].id,selectedObjectId:'new-owner'});
 const create=async(name,source)=>{fireEvent.click(screen.getAllByRole('button',{name:'Создать компонент',exact:true})[0]);fireEvent.change(screen.getByRole('textbox',{name:'Название компонента'}),{target:{value:name}});fireEvent.click(screen.getByRole('button',{name:'Создать',exact:true}));replaceScript(source);await save();openConnections();fireEvent.click(screen.getByRole('button',{name:'Добавить экземпляр'}));return value().gameplay.mechanics.at(-1);};
 const ledger=await create('AcceptanceLedger',ledgerSource.replace('state.total += args.amount','throw Error("Исправьте формулу журнала"); state.total += args.amount'));
 const relay=await create('ThresholdRelay',relaySource);fireEvent.change(screen.getByRole('spinbutton',{name:'Множитель'}),{target:{value:'3'}});fireEvent.change(screen.getByRole('combobox',{name:'Журнал-получатель'}),{target:{value:JSON.stringify({kind:'reference',type:'mechanic',mode:'direct',id:ledger.id})}});
 assert.equal(value().gameplay.mechanics.find(m=>m.id===relay.id).values.factor,3);const rt=new PreviewRuntime(new SoundDesk(()=>{}));let diagnostic;try{await rt.start(value());await rt.uiAction('gameplay',{command:'sacura.component.call',value:{instance:{kind:'reference',type:'mechanic',mode:'direct',id:relay.id},methodId:'apply',args:{amount:3}}});diagnostic=rt.snapshot.componentError;assert.equal(diagnostic.instanceId,ledger.id);}finally{rt.stop();}
 cleanup();let opened;render(React.createElement(Issue,{diagnostic,onOpen:issue=>opened=issue}));assert.match(screen.getByRole('alert').textContent,/Новый владелец.*AcceptanceLedger.*Записать/);fireEvent.click(screen.getByRole('button',{name:'Остановить и открыть место ошибки'}));assert.equal(opened.functionId,value().gameplay.mechanicDefinitions.find(d=>d.id===ledger.definitionId).methods.record.logic.functionId);
 cleanup();mount(Workspace,value(),{sceneId:value().subscenes[0].id,selectedObjectId:'new-owner'});fireEvent.click(screen.getByRole('button',{name:'Импортировать пакет',exact:true}));assert.ok(screen.getByLabelText('Файл авторской заготовки'));assert.ok(screen.getByRole('button',{name:'Отменить / закрыть обмен'}));
});

test('JavaScript opens its exact graph callee and returns to the unchanged source; the round trip executes both implementations',async()=>{
 const {default:Code}=await server.ssrLoadModule('/src/ComponentCodeEditor.jsx'),{componentMethodChoices}=await server.ssrLoadModule('/src/componentCatalog.js');
 const p=createMechanicsProject('Код → граф → код'),graph=createComponentResource(p,{name:'BridgeGraph',kind:'component.graph'}),gd=p.gameplay.mechanicDefinitions.find(d=>d.id===graph.definitionId),[methodId,method]=Object.entries(gd.methods)[0];method.result={type:'number'};const gi=addMechanicInstance(p,gd.id,{kind:'project',id:p.id}),worker=createComponentResource(p,{name:'BridgeWorker'});
 const source=`const component={name:'BridgeWorker',references:{type:'object',properties:{graph:{type:'reference',referenceType:'mechanic'}}},methods:{double:{name:'Double',parameters:{type:'object',properties:{amount:{type:'number'}}},result:{type:'number'},run({args}){return args.amount*2;}},start:{name:'Start',result:{type:'number'},async run({engine,refs}){return await engine.invoke(refs.graph,${JSON.stringify(methodId)},{});}}}};`;
 editComponentSource(p,worker.id,source);publishComponentSource(p,worker.id);const wi=addMechanicInstance(p,worker.definitionId,{kind:'project',id:p.id});wi.references.graph={kind:'reference',type:'mechanic',mode:'direct',id:gi.id};
 const fn=p.functions.find(f=>f.id===method.logic.functionId),call={id:'double',...componentMethodChoices(p).find(c=>c.label==='BridgeWorker · Double').make()};call.call.parameters.instance={kind:'reference',type:'mechanic',mode:'direct',id:wi.id};call.call.parameters.args.amount=4;call.next='result';fn.entry=call.id;fn.resultSchema={type:'number'};fn.nodes=[call,{id:'result',kind:'return',value:{op:'node',nodeId:call.id}}];
 const before=JSON.stringify(fn),value=mount(Code,p,{resourceId:worker.id});fireEvent.click(screen.getByText(/Вызовы компонентов/,{selector:'summary'}));fireEvent.click(screen.getByRole('button',{name:/refs.graph → BridgeGraph · Start/}));assert.ok(screen.getByRole('region',{name:'Граф функции'}));assert.equal(screen.getByLabelText('amount',{selector:'input'}).value,'4');
 fireEvent.click(screen.getByRole('button',{name:'К месту вызова',exact:true}));assert.equal(screen.getByRole('textbox',{name:'JavaScript компонента'}).textContent,source);assert.equal(JSON.stringify(value().functions.find(f=>f.id===fn.id)),before);
 const rt=new PreviewRuntime(new SoundDesk(()=>{}));try{await rt.start(value());assert.equal(rt.snapshot.error,null);assert.equal(await rt.game.callComponent(wi.id,'start'),8);}finally{rt.stop();}
});

test('components list and initial selection show JavaScript while preserving legacy resources',()=>{
 const p=createMechanicsProject('Code only');
 const tree=createComponentResource(p,{name:'Legacy tree',kind:'behavior.tree'}),graph=createComponentResource(p,{name:'Legacy graph',kind:'component.graph'}),code=createComponentResource(p,{name:'Current code'}),before=JSON.stringify(p),value=mount(Components,p);
 const nav=within(screen.getByRole('navigation',{name:'Исходные ресурсы логики'}));
 assert.ok(nav.getByRole('button',{name:/Current code/}));
 assert.equal(nav.queryByRole('button',{name:/Legacy/}),null);
 assert.ok(screen.getByRole('textbox',{name:'JavaScript компонента'}));
 assert.equal(screen.queryByRole('region',{name:'Граф функции'}),null);
 assert.equal(JSON.stringify(value()),before);
 assert.deepEqual(value().logicResources.map(r=>r.id),[tree.id,graph.id,code.id]);
});

test('old component links explain the format, allow export and return, and create code without changing the old resource',()=>{
 for(const kind of ['behavior.tree','component.graph']){
  const p=createMechanicsProject('Legacy link'),legacy=createComponentResource(p,{name:'Old resource',kind}),before=JSON.stringify(p);let exported,returned=false;
  const value=mount(Components,p,{requestedResourceId:legacy.id,onBack:()=>returned=true,onExport:(r,d)=>exported={r,d}});
  assert.match(screen.getByRole('status').textContent,/только JavaScript.*Данные сохранены/);
  assert.equal(document.querySelector('.react-flow'),null);
  assert.equal(screen.queryByRole('textbox',{name:'JavaScript компонента'}),null);
  assert.equal(screen.queryByRole('combobox',{name:'Метод компонента'}),null);
  fireEvent.click(screen.getByRole('button',{name:'Экспортировать компонент'}));
  assert.equal(exported.r.id,legacy.id);assert.equal(exported.d.logicResourceId,legacy.id);
  fireEvent.click(screen.getByRole('button',{name:'Назад к механикам'}));assert.equal(returned,true);
  assert.equal(JSON.stringify(value()),before);
  fireEvent.click(screen.getByRole('button',{name:'Создать компонент',exact:true}));
  assert.equal(screen.queryByRole('radio'),null);
  fireEvent.change(screen.getByRole('textbox',{name:'Название компонента'}),{target:{value:'New code'}});
  fireEvent.click(screen.getByRole('button',{name:'Создать',exact:true}));
  assert.ok(screen.getByRole('textbox',{name:'JavaScript компонента'}));assert.equal(value().logicResources.at(-1).kind,'component.code');
  assert.deepEqual(value().logicResources[0],legacy);
  cleanup();
 }
});

test('creating a component after switching from a project resource keeps the selected object as its attachment owner',()=>{
 const p=createMechanicsProject('Owner');p.objects.push({id:'door',name:'Выбранная дверь',type:'Активный меш',active:true,subsceneId:p.subscenes[0].id});let current=p;
 function Harness(){const [value,setValue]=React.useState(p),[scope,setScope]=React.useState({kind:'project',id:p.id}),[request,setRequest]=React.useState(0);return React.createElement(React.Fragment,null,React.createElement('button',{onClick:()=>{setScope({kind:'object',id:'door'});setRequest(1);}},'Новый у выбранного объекта'),React.createElement(Components,{project:value,scope,createRequest:request,onCreateFinished:()=>setRequest(0),onChange:fn=>{const next=structuredClone(current);fn(next);current=next;setValue(next);}}));}
 render(React.createElement(Harness));fireEvent.click(screen.getByRole('button',{name:'Новый у выбранного объекта'}));fireEvent.change(screen.getByRole('textbox',{name:'Название компонента'}),{target:{value:'Local rule'}});fireEvent.click(screen.getByRole('button',{name:'Создать',exact:true}));openConnections();assert.equal(screen.getByRole('combobox',{name:'Владелец экземпляра'}).value,'object:door');fireEvent.click(screen.getByRole('button',{name:'Добавить экземпляр'}));assert.deepEqual(current.gameplay.mechanics[0].scope,{kind:'object',id:'door'});
});

test('connections open on demand, close with Escape, retain selected instance and do not change the source',()=>{
 const p=createMechanicsProject('Compact connections'),r=createComponentResource(p,{name:'Counter'});editComponentSource(p,r.id,counterSource());publishComponentSource(p,r.id);
 const a=addMechanicInstance(p,r.definitionId,{kind:'project',id:p.id}),b=addMechanicInstance(p,r.definitionId,{kind:'scene',id:p.subscenes[0].id}),source=r.source,value=mount(Components,p,{requestedResourceId:r.id});
 assert.equal(screen.queryByRole('complementary',{name:'Свойства компонента'}),null);
 assert.equal(document.querySelectorAll('.component-code-toolbar').length,1);
 assert.equal(document.querySelector('.component-shared-scope'),null);
 openConnections();
 const panel=screen.getByRole('complementary',{name:'Свойства компонента'});
 fireEvent.change(screen.getByRole('combobox',{name:'Подключение определения'}),{target:{value:b.id}});
 fireEvent.change(screen.getByRole('spinbutton',{name:'step',exact:true}),{target:{value:'7'}});
 fireEvent.keyDown(panel,{key:'Escape'});
 assert.equal(screen.queryByRole('complementary',{name:'Свойства компонента'}),null);
 assert.equal(document.activeElement,screen.getByRole('button',{name:'Подключения компонента'}));
 openConnections();
 assert.equal(screen.getByRole('combobox',{name:'Подключение определения'}).value,b.id);
 assert.equal(screen.getByRole('spinbutton',{name:'step',exact:true}).value,'7');
 assert.equal(value().gameplay.mechanics.find(m=>m.id===a.id).values.step,1);
 assert.equal(value().logicResources[0].source,source);
});

test('component settings cannot mutate a project while Play disables editing',()=>{
 const p=createMechanicsProject('Read only'),r=createComponentResource(p,{name:'Counter'});editComponentSource(p,r.id,counterSource());publishComponentSource(p,r.id);const m=addMechanicInstance(p,r.definitionId,{kind:'project',id:p.id}),before=JSON.stringify(p),value=mount(Components,p,{requestedResourceId:r.id,requestedInstanceId:m.id,disabled:true});
 openConnections();
 assert.equal(screen.getByRole('combobox',{name:'Владелец экземпляра'}).matches(':disabled'),true);
 assert.equal(screen.getByRole('button',{name:'Добавить экземпляр'}).matches(':disabled'),true);
 assert.equal(screen.getByRole('spinbutton',{name:'step',exact:true}).closest('fieldset').disabled,true);
 assert.equal(screen.getByRole('button',{name:'Сохранить компонент'}).disabled,true);
 assert.equal(JSON.stringify(value()),before);
});
