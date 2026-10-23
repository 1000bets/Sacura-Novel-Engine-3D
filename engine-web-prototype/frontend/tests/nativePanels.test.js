import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {eulerToQuaternion,quaternionToEuler,presentationObject} from '../src/editorPlatform.js';
import {nativeStoryPresentation} from '../src/nativeStoryPresentation.js';
const dom=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup,waitFor}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:HierarchyTree}=await server.ssrLoadModule('/src/HierarchyTree.jsx');
const {default:TransformInspector}=await server.ssrLoadModule('/src/TransformInspector.jsx');
const {default:Components}=await server.ssrLoadModule('/src/ComponentInstancesInspector.jsx');
const {default:AssetExplorer}=await server.ssrLoadModule('/src/AssetExplorer.jsx');
const {default:NativeViewport}=await server.ssrLoadModule('/src/NativeViewport.jsx');
const {default:NativeDialogue}=await server.ssrLoadModule('/src/NativeDialogue.jsx');
const {default:NativeStoryInspector}=await server.ssrLoadModule('/src/NativeStoryInspector.jsx');
const {default:ProjectHome}=await server.ssrLoadModule('/src/ProjectHome.jsx');
const {default:LoadingScreen}=await server.ssrLoadModule('/src/LoadingScreen.jsx');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
test('native inspector clears to the shared empty state and edits a line through story commands',()=>{
 const calls=[],command=(method,params)=>{calls.push([method,params]);return Promise.resolve({});};
 const story={document:{name:'Demo',startNodeId:'line',nodes:[{id:'line',kind:'Line',speaker:'Narrator',text:'Hello',nextNodeId:'end'},{id:'end',kind:'End'}],events:[]}};
 const view=render(React.createElement(NativeStoryInspector,{story,selection:null,command,objects:[]}));
 assert.ok(screen.getByText('Ничего не выбрано'));
 view.rerender(React.createElement(NativeStoryInspector,{story,selection:{nodeId:'line'},command,objects:[]}));
 assert.ok(view.container.querySelector('.story-inspector-heading strong')?.textContent==='Реплика');
 fireEvent.change(screen.getByLabelText('Текст выбранной реплики'),{target:{value:'Updated'}});
 fireEvent.blur(screen.getByLabelText('Текст выбранной реплики'));
 fireEvent.change(screen.getByLabelText('Продолжение реплики'),{target:{value:''}});
 assert.deepEqual(calls,[['story.updateNode',{nodeId:'line',patch:{text:'Updated'}}],['story.connect',{sourceId:'line',port:-1,targetId:null}]]);
});
test('native inspector edits choices and selected event placement with native IDs',()=>{
 const calls=[],command=(method,params)=>{calls.push([method,params]);return Promise.resolve({});};
 const node={id:'choice',kind:'Choice',prompt:'Where?',choices:[{id:'answer',label:'Left',nextNodeId:'end'}],bindings:[{id:'binding',eventId:'event',join:'EVENT_END'}],batches:{BEFORE:[{id:'batch',bindingIds:['binding']}]}};
 const story={document:{name:'Demo',startNodeId:'choice',nodes:[node,{id:'end',kind:'End'}],events:[{id:'event',name:'Open door',groups:[{id:'group',name:'Actions',actions:[{id:'action',kind:'Wait',durationSeconds:1}]}]}]}};
 const view=render(React.createElement(NativeStoryInspector,{story,selection:{nodeId:'choice'},command,objects:[]}));
 fireEvent.change(screen.getByLabelText('Продолжение ответа 1'),{target:{value:''}});
 assert.equal(calls[0][0],'story.updateNode');assert.equal(calls[0][1].patch.choices[0].nextNodeId,'');
 view.rerender(React.createElement(NativeStoryInspector,{story,selection:{nodeId:'choice',eventId:'event',bindingId:'binding'},command,objects:[]}));
 fireEvent.change(screen.getByLabelText('Когда запустить'),{target:{value:'AFTER'}});
 assert.deepEqual(calls.at(-1),['story.placeEvent',{nodeId:'choice',bindingId:'binding',phase:'AFTER',join:'EVENT_END'}]);
 fireEvent.change(screen.getByLabelText('Длительность, сек'),{target:{value:'2'}});
 assert.deepEqual(calls.at(-1),['story.setOverrides',{nodeId:'choice',bindingId:'binding',actions:[{id:'action',kind:'Wait',durationSeconds:2}]}]);
});
test('native dialogue advances, offers choices and pauses its startup cue',async()=>{
 const calls=[];const oldAudio=globalThis.Audio;
 globalThis.Audio=class{constructor(src){this.src=src;calls.push(['source',src]);}play(){calls.push(['play']);return Promise.resolve();}pause(){calls.push(['pause']);}};
 try{
  const base={play:'playing',startupAudio:true};
  const line={phase:'line',speaker:'Мира',text:'Откроем письмо?',choices:[],canAdvance:true,canChoose:false,error:''};
  const commands=[];
  const view=render(React.createElement(NativeDialogue,{state:{...base,dialogue:line},session:'session',onAdvance:()=>commands.push('next'),onChoice:i=>commands.push(i)}));
  fireEvent.click(screen.getByText('Дальше →'));assert.deepEqual(commands,['next']);
  assert.ok(calls.some(([kind,src])=>kind==='source'&&src==='sacura://startup-audio/session/cue.wav'));
  assert.ok(calls.some(([kind])=>kind==='play'));
  view.rerender(React.createElement(NativeDialogue,{state:{...base,dialogue:{...line,phase:'choice',choices:['Да','Позже'],canAdvance:false,canChoose:true}},session:'session',onAdvance:()=>{},onChoice:i=>commands.push(i)}));
  fireEvent.click(screen.getByText('Позже'));assert.deepEqual(commands,['next',1]);
  view.rerender(React.createElement(NativeDialogue,{state:{...base,play:'paused',dialogue:line},session:'session',onAdvance:()=>{},onChoice:()=>{}}));
  assert.ok(calls.some(([kind])=>kind==='pause'));
  assert.equal(screen.getByText('Дальше →').disabled,true);
 }finally{globalThis.Audio=oldAudio;}
});
test('loading overlay reports the real stage and blocks shortcuts only while mounted',()=>{
 const {unmount}=render(React.createElement(LoadingScreen,{label:'Загружаем сцену и запускаем движок…',project:'My project'}));
 assert.equal(screen.getByRole('status').textContent,'Загружаем сцену и запускаем движок…');
 assert.ok(screen.getByText('My project'));
 let calls=0;const shortcut=()=>calls++;window.addEventListener('keydown',shortcut);
 try{
   const key=new window.KeyboardEvent('keydown',{key:'z',ctrlKey:true,bubbles:true,cancelable:true});
   document.body.dispatchEvent(key);assert.equal(key.defaultPrevented,true);assert.equal(calls,0);
   unmount();document.body.dispatchEvent(new window.KeyboardEvent('keydown',{key:'z',ctrlKey:true,bubbles:true}));
   assert.equal(calls,1);
 }finally{window.removeEventListener('keydown',shortcut);}
});
test('shared Web home uses only native project actions and does not automatically open recents',async()=>{
 const calls=[],nativeAdapter={ready(){},listProjects:async()=>[{id:'recent',title:'Native project',path:'C:/Game/Game.project'}],openRecent:async id=>calls.push(['recent',id]),openProject:async()=>calls.push(['open']),createProject:async name=>{calls.push(['create',name]);return false;}};
 render(React.createElement(ProjectHome,{nativeAdapter}));
 await waitFor(()=>assert.ok(screen.getByText('Native project')));assert.deepEqual(calls,[]);
 assert.ok(screen.getByText('Место для ваших'));assert.ok(screen.getByText('Письма после дождя'));assert.ok(screen.getByText('Начните с готовой идеи'));
 fireEvent.click(screen.getByRole('button',{name:'Открыть из файла'}));await waitFor(()=>assert.deepEqual(calls,[['open']]));
 fireEvent.click(screen.getByText('Native project'));await waitFor(()=>assert.equal(calls[1][1],'recent'));
 fireEvent.click(screen.getByRole('button',{name:'Создать новый проект'}));fireEvent.change(screen.getByLabelText('Название нового проекта'),{target:{value:'Native New'}});
 fireEvent.click(screen.getByRole('button',{name:'Создать',exact:true}));await waitFor(()=>assert.deepEqual(calls.at(-1),['create','Native New']));
 assert.ok(screen.getByRole('dialog')); // Cancelled OS folder picker keeps the form.
});
const scene={id:'native',name:'Scene',kind:'3d'},object={id:'id',name:'Native object',parentId:null,transform:{position:[1,2,3],rotation:[0,0,0,1],scale:[1,1,1]}};
test('same Hierarchy component selects, deletes and reparents native IDs',()=>{
 const calls=[];render(React.createElement(HierarchyTree,{scene,objects:[object,{...object,id:'child',name:'Child',parentId:'id'}],query:'',expanded:{},selectedIds:[],onExpanded(){},onSelectObject:id=>calls.push(['select',id]),onFrameObject(){},onDelete:id=>calls.push(['delete',id]),onReparent:(id,parent)=>calls.push(['parent',id,parent])}));
 fireEvent.click(screen.getByRole('button',{name:'Native object'}));fireEvent.click(screen.getAllByTitle('Удалить')[0]);
 fireEvent.drop(screen.getByRole('button',{name:'Native object'}).parentElement.parentElement,{dataTransfer:{getData:()=>JSON.stringify({kind:'objects',id:'child'})}});
 assert.deepEqual(calls,[['select','id'],['delete','id'],['parent','child','id']]);
});
test('existing TransformInspector emits position/rotation/scale drafts; quaternion roundtrip',()=>{
 let change;render(React.createElement(TransformInspector,{object:presentationObject(object),scene,onChange:t=>change=t}));
 for(const [label,value,key,index] of [['Положение X','5','position',0],['Поворот Y','30','rotation',1],['Масштаб Z','2','scale',2]]){const input=screen.getByLabelText(label);fireEvent.change(input,{target:{value}});fireEvent.blur(input);assert.equal(Number(change[key][index]),Number(value));}
 const angles=[20,30,-40];quaternionToEuler(eulerToQuaternion(angles)).forEach((v,i)=>assert.ok(Math.abs(v-angles[i])<1e-6));
});
test('same component inspector edits reflected properties and removes components',()=>{
 const calls=[],reflection={components:[{id:'c',typeId:'engine.LightComponent',enabled:true,properties:[{id:'intensity',label:'Intensity',typeId:'engine.float',value:2,editable:true}]}],types:[{typeId:'engine.LightComponent'}],onAdd:t=>calls.push(['add',t]),onProperty:(...p)=>calls.push(['property',...p]),onRemove:id=>calls.push(['remove',id]),onEnabled:(...p)=>calls.push(['enabled',...p])};
 render(React.createElement(Components,{project:{objects:[]},reflection}));
 fireEvent.change(screen.getByLabelText('Тип компонента'),{target:{value:'engine.LightComponent'}});fireEvent.click(screen.getByText('Добавить'));
 fireEvent.click(screen.getByRole('button',{name:/engine.LightComponent/}));fireEvent.click(screen.getByLabelText('Компонент engine.LightComponent включён'));const input=screen.getByLabelText('Intensity');fireEvent.change(input,{target:{value:'3'}});fireEvent.blur(input);fireEvent.click(screen.getByText('Удалить компонент'));
 assert.deepEqual(calls,[['add','engine.LightComponent'],['enabled','c',false],['property','c','intensity',3],['remove','c']]);
});

test('Hierarchy search preserves the entire ancestor path of a native match',()=>{
 render(React.createElement(HierarchyTree,{scene,objects:[object,{...object,id:'child',name:'Child',parentId:'id'},{...object,id:'leaf',name:'Needle',parentId:'child'}],query:'needle',expanded:{},onExpanded(){},onReparent(){}}));
 for(const name of ['Native object','Child','Needle'])assert.ok(screen.getByRole('button',{name}));
});

test('existing Content Browser uses native files, selection, open and import adapters',async()=>{
 const calls=[];window.sacuraDesktop={isDesktop:true};
 try{
 const adapter={files:[{id:'asset-id',name:'Demo.story',path:'Game/Stories/Demo.story',typeId:'Story',kind:'file',users:[]}],open:a=>calls.push(['open',a.id]),import:async folder=>calls.push(['import',folder])};
 render(React.createElement(AssetExplorer,{project:{assetFolders:[]},audioAssets:[],adapter,onSelectFile:id=>calls.push(['select',id])}));
 fireEvent.change(screen.getByLabelText('Поиск файлов в папке'),{target:{value:'Demo'}});
 const tile=screen.getByText('Demo.story').closest('button');fireEvent.click(tile);fireEvent.doubleClick(tile);
 fireEvent.click(screen.getByText('Импорт файлов'));await waitFor(()=>assert.deepEqual(calls,[['select','asset-id'],['open','asset-id'],['import','']]));
 assert.equal(screen.getByText('Импорт папки').closest('button').disabled,true);
 }finally{delete window.sacuraDesktop;}
});

test('StoryFlow projection preserves native IDs, choice ports and event bindings without editing the document',()=>{
 const snapshot={document:{name:'Native story',startNodeId:'choice',nodes:[{id:'choice',kind:'Choice',prompt:'Choose',choices:[{id:'option',label:'Continue',nextNodeId:'end'}],bindings:[{id:'binding',eventId:'event',hook:'BEFORE',join:'EVENT_END'}],batches:{BEFORE:[{id:'batch',mode:'SEQUENCE',bindingIds:['binding']}]}},{id:'end',kind:'End'}],events:[{id:'event',name:'Wait',groups:[{id:'group',actions:[{id:'action',kind:'Wait',durationSeconds:.5}]}]}]}};
 const before=JSON.stringify(snapshot),view=nativeStoryPresentation(snapshot,[]);
 assert.deepEqual(view.model.edges.map(e=>[e.source,e.sourceHandle,e.target]),[['choice','choice:option','end']]);
 assert.equal(view.model.nodes[0].data.phases[0].batches[0].events[0].binding.id,'binding');
 assert.equal(view.model.nodes[1].data.ports.length,0);assert.equal(view.project.events[0].groups[0].actions[0].duration,.5);
 assert.equal(JSON.stringify(snapshot),before);
});

test('native viewport lets Ctrl+S reach editor shortcuts and sends camera keys to C++',async()=>{
 const commands=[],keys=[];globalThis.requestAnimationFrame=()=>1;globalThis.cancelAnimationFrame=()=>{};
 window.sacuraDesktop={nativeInput:async value=>commands.push(value),nativeTools:async()=>{},nativeBounds:async()=>{},nativeDetach:async()=>{},nativeStatus:async()=>({phase:'ready'}),nativeHeartbeat:async()=>{}};
 try{
  const mounted=render(React.createElement('div',{onKeyDown:e=>keys.push(e.code)},React.createElement(NativeViewport)));
  const surface=mounted.container.querySelector('[tabindex="0"]');
  await waitFor(()=>assert.ok(mounted.container.querySelector('[data-native-overlay]')));
  fireEvent.keyDown(surface,{code:'KeyS',ctrlKey:true});fireEvent.keyDown(surface,{code:'KeyW'});
  assert.deepEqual(keys,['KeyS']);assert.ok(commands.some(c=>c.kind==='key'&&c.code==='KeyW'));assert.ok(!commands.some(c=>c.code==='KeyS'));
  mounted.unmount();
 }finally{delete window.sacuraDesktop;delete globalThis.requestAnimationFrame;delete globalThis.cancelAnimationFrame;}
});

test('native viewport opens scene menu on right click, but not camera flight or drag',async()=>{
 const menus=[],commands=[];
 globalThis.requestAnimationFrame=()=>1;globalThis.cancelAnimationFrame=()=>{};
 window.sacuraDesktop={nativeInput:async value=>commands.push(value),nativeTools:async()=>{},nativeBounds:async()=>{},nativeDetach:async()=>{},
  nativeStatus:async()=>({phase:'ready',viewport:{data:{cameraPosition:[4,3,6],cameraTarget:[0,0,0]}}}),nativeHeartbeat:async()=>{}};
 try{
  const mounted=render(React.createElement(NativeViewport,{editing:true,onContext:point=>menus.push(point)}));
  const surface=mounted.container.querySelector('[tabindex="0"]');
  surface.setPointerCapture=()=>{};surface.releasePointerCapture=()=>{};surface.hasPointerCapture=()=>false;
  surface.getBoundingClientRect=()=>({left:0,top:0,width:800,height:600});
  await waitFor(()=>assert.ok(mounted.container.querySelector('[data-native-overlay]')));
  fireEvent.pointerDown(surface,{pointerId:1,button:2,buttons:2,clientX:400,clientY:300});
  fireEvent.pointerUp(surface,{pointerId:1,button:2,buttons:0,clientX:400,clientY:300});
  await waitFor(()=>assert.equal(menus.length,1));
  assert.ok(menus[0].cameraTransform&&menus[0].worldPosition);
  fireEvent.pointerDown(surface,{pointerId:2,button:2,buttons:2,clientX:400,clientY:300});
  fireEvent.pointerMove(surface,{pointerId:2,button:2,buttons:2,clientX:430,clientY:300});
  fireEvent.pointerUp(surface,{pointerId:2,button:2,buttons:0,clientX:430,clientY:300});
  fireEvent.pointerDown(surface,{pointerId:3,button:2,buttons:2,clientX:400,clientY:300});
  fireEvent.keyDown(surface,{code:'KeyW'});
  fireEvent.pointerUp(surface,{pointerId:3,button:2,buttons:0,clientX:400,clientY:300});
  assert.equal(menus.length,1);
  assert.ok(commands.some(value=>value.kind==='pointer'&&value.phase==='up'&&value.button===2));
  mounted.unmount();
 }finally{delete window.sacuraDesktop;delete globalThis.requestAnimationFrame;delete globalThis.cancelAnimationFrame;}
});
