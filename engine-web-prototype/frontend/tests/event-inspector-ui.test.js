import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React,{useState} from 'react';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {makeAction,addToBatch} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';
import {actionSummary} from '../src/authoringPresentation.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Inspector}=await server.ssrLoadModule('/src/EventSelectionInspector.jsx');
let current;
function initial(){const p=createEmptyProject();p.variables.score=0;const a={...makeAction('variable','score',1),operation:'add',valueType:'number'};p.events=[{id:'reward',name:'Награда',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'main',name:'Начислить',actions:[a]}]}];addToBatch(p,p.chapters[0].beats[0].id,'BEFORE',null,'reward');return p;}
function Harness({running=false,actionFirst=false}){
 const [p,setP]=useState(initial),[actionId,setAction]=useState(actionFirst?p.events[0].groups[0].actions[0].id:null);current=p;
 const edit=fn=>setP(previous=>{const next=structuredClone(previous);fn(next.events[0]);return next;});
 return React.createElement(Inspector,{project:p,event:p.events[0],beat:p.chapters[0].beats[0],sceneId:p.subscenes[0].id,action:p.events[0].groups.flatMap(g=>g.actions).find(a=>a.id===actionId),running,onEdit:edit,onAction:(id,patch)=>edit(e=>Object.assign(e.groups.flatMap(g=>g.actions).find(a=>a.id===id),patch)),onSelectAction:setAction,onBack:()=>setAction(null),onOpenEditor(){},onAdd(){}});
}
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});

test('event action opens its own fields, changes real execution and returns without duplicate properties',async()=>{
 render(React.createElement(Harness));assert.equal(screen.getAllByRole('textbox',{name:'Название события'}).length,1);
 const link=document.querySelector('.event-action-link');assert.ok(link);fireEvent.click(link);
 fireEvent.change(screen.getByRole('spinbutton',{name:'Значение переменной'}),{target:{value:'4'}});
 assert.equal(current.events[0].groups[0].actions[0].value,4);
 fireEvent.click(screen.getByRole('button',{name:'Награда',exact:true}));assert.ok(screen.getByRole('textbox',{name:'Название события'}));
 const restored=readProject(JSON.stringify(current)),rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('This scenario does not request audio');}));
 try{await rt.start(restored,restored.chapters[0].beats[0].id);assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.variables.score,4);}finally{rt.stop();}
});

test('execution settings and timeouts remain editable behind disclosures and survive serialization',()=>{
 render(React.createElement(Harness,{actionFirst:true}));
 const execution=screen.getByText('Выполнение действия',{exact:true}).closest('details');assert.equal(execution.open,false);fireEvent.click(execution.querySelector('summary'));
 fireEvent.change(screen.getByLabelText('Ожидание действия'),{target:{value:'STARTED'}});
 const limits=screen.getByText('Лимиты времени',{exact:true}).closest('details');assert.equal(limits.open,false);fireEvent.click(limits.querySelector('summary'));
 fireEvent.change(screen.getByLabelText('Лимит выполнения, сек'),{target:{value:'9'}});
 const a=readProject(JSON.stringify(current)).events[0].groups[0].actions[0];assert.equal(a.wait,'STARTED');assert.equal(a.executionTimeout,9);assert.equal(a.value,1);
});

test('preview cannot mutate event or action settings',()=>{
 render(React.createElement(Harness,{running:true,actionFirst:true}));const before=JSON.stringify(current);
 fireEvent.change(screen.getByRole('spinbutton',{name:'Значение переменной'}),{target:{value:'99'}});assert.equal(JSON.stringify(current),before);
});


test('a local component-call event does not claim to have a separate source template',()=>{
 const p=initial(),base={project:p,event:p.events[0],beat:p.chapters[0].beats[0],onEdit(){},onAction(){},onSelectAction(){},onOpenSource(){}};
 const binding={...p.chapters[0].beats[0].bindings[0],localEventId:'reward'};
 const {rerender}=render(React.createElement(Inspector,{...base,binding}));assert.equal(screen.queryByRole('button',{name:'Открыть исходный шаблон'}),null);
 rerender(React.createElement(Inspector,{...base,binding:{...binding,sourceEventId:'shared-reward'}}));assert.ok(screen.getByRole('button',{name:'Открыть исходный шаблон'}));
});


test('native gameplay action labels identify the recipient and never expose serialized parameters',()=>{
 const p=initial();p.objects.push({id:'owner-42',name:'Боец'});
 const action={...makeAction('gameplay'),command:'damage',value:{target:'owner-42',amount:25}};
 assert.equal(actionSummary(action,p),'Боец → Нанести урон');p.objects[0].name='Разведчик';assert.equal(actionSummary(action,p),'Разведчик → Нанести урон');assert.doesNotMatch(actionSummary(action,p),/[{}]|owner-42|amount/);
});
