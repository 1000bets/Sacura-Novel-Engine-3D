import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React,{useState} from 'react';
import {upgradeProject,allBeats} from '../src/studioModel.js';
import {setSceneEntry} from '../src/subsceneModel.js';
import {readProject} from '../src/projectFiles.js';
import {isObjectInScene} from '../src/sceneEditing.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:SubsceneInspector}=await server.ssrLoadModule('/src/SubsceneInspector.jsx');
let current;
function Harness({running=false}){
 const [project,setProject]=useState(()=>upgradeProject());current=project;
 const change=fn=>setProject(previous=>{const next=structuredClone(previous);fn(next);return next;});
 return React.createElement(SubsceneInspector,{project,scene:project.subscenes.find(s=>s.id==='garden'),running,
  onPatch:patch=>change(p=>Object.assign(p.subscenes.find(s=>s.id==='garden'),patch)),
  onEntry:(id,reroute)=>change(p=>setSceneEntry(p,'garden',id,reroute)),onOpenBeat(){},onScene(){},onCameras(){},onOpenEditor(){}});
}
const scene=()=>current.subscenes.find(s=>s.id==='garden');
afterEach(()=>cleanup());
after(async()=>{await server.close();dom.window.close();});

test('subscene inspector commits complete names, cancels Escape and retains environment through project reload',()=>{
 render(React.createElement(Harness));const before=structuredClone(current),name=screen.getByLabelText('Название сабсцены');
 fireEvent.change(name,{target:{value:'Черновик'}});assert.equal(scene().name,before.subscenes.find(s=>s.id==='garden').name);
 fireEvent.keyDown(name,{key:'Escape'});fireEvent.blur(name);assert.equal(name.value,scene().name);
 fireEvent.change(name,{target:{value:'  '}});fireEvent.blur(name);assert.equal(name.value,scene().name);
 fireEvent.change(name,{target:{value:'  Сад после дождя  '}});fireEvent.blur(name);
 fireEvent.change(screen.getByLabelText('Погода'),{target:{value:'Туман'}});
 fireEvent.change(screen.getByLabelText('Время суток'),{target:{value:'Ночь'}});
 const restored=readProject(JSON.stringify(current));assert.equal(restored.subscenes.find(s=>s.id==='garden').name,'Сад после дождя');assert.equal(restored.subscenes.find(s=>s.id==='garden').weather,'Туман');assert.equal(restored.subscenes.find(s=>s.id==='garden').time,'Ночь');
 assert.deepEqual(restored.chapters,before.chapters);assert.deepEqual(restored.objects,before.objects);
});

test('folded entry, cast and navigation controls edit the existing project model without replacing story data',()=>{
 render(React.createElement(Harness));
 const target='garden-walk',old=scene().entry,arrival=allBeats(current).find(b=>b.next===old&&b.subsceneId!=='garden');
 const start=screen.getByText('Начало сабсцены',{exact:true}).closest('details');assert.equal(start.open,false);fireEvent.click(start.querySelector('summary'));
 fireEvent.change(screen.getByLabelText('С какой реплики начинать'),{target:{value:target}});assert.equal(scene().entry,target);
 if(arrival)assert.equal(allBeats(current).find(b=>b.id===arrival.id).next,target);
 const actor=current.objects.find(o=>o.id==='alice');const cast=screen.getByText('Персонажи',{exact:true}).closest('details');fireEvent.click(cast.querySelector('summary'));
 const checkbox=screen.getByLabelText(actor.name);fireEvent.click(checkbox);assert.equal(isObjectInScene(actor,scene()),checkbox.checked);
 const nav=screen.getByText('Навигация и коллизии',{exact:true}).closest('details');assert.equal(nav.open,false);fireEvent.click(nav.querySelector('summary'));
 fireEvent.click(screen.getByLabelText('Плоский nav mesh'));
 const width=screen.getByLabelText('Ширина X · м');fireEvent.change(width,{target:{value:'24'}});fireEvent.blur(width);
 const restored=readProject(JSON.stringify(current));assert.equal(restored.subscenes.find(s=>s.id==='garden').navMesh.size[0],24);assert.equal(restored.subscenes.find(s=>s.id==='garden').entry,target);
});

test('preview keeps subscene configuration read only even if an edit event is dispatched',()=>{
 render(React.createElement(Harness,{running:true}));const before=JSON.stringify(current);
 const input=screen.getByLabelText('Название сабсцены');assert.equal(input.closest('fieldset').disabled,true);
 fireEvent.change(input,{target:{value:'Forbidden'}});fireEvent.blur(input);
 fireEvent.change(screen.getByLabelText('Погода'),{target:{value:'Снег'}});
 assert.equal(JSON.stringify(current),before);
});
