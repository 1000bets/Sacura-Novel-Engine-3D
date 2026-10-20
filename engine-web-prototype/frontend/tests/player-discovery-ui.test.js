import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React,{useState} from 'react';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {mechanicsFor} from '../src/mechanicsModel.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Hierarchy}=await server.ssrLoadModule('/src/HierarchyTree.jsx');
const {default:Transform}=await server.ssrLoadModule('/src/TransformInspector.jsx');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});

test('player shortcut follows the controller owner after renaming, and edited start position is used in the actual runtime',async()=>{
 const p=await instantiateAuthoringTemplate(JSON.parse(await readFile('public/game-templates/shooter.sacura-template.json','utf8')),undefined,{reviewedSources:true}),id=p.gameplay.controller.ownerId,hero=p.objects.find(o=>o.id===id),scene=p.subscenes[0];hero.name='Разведчик';let selected,position;
 render(React.createElement(Hierarchy,{scene,objects:p.objects,playerId:id,query:'',expanded:{},selectedIds:[],onFrameObject:target=>{selected=target;}}));
 fireEvent.click(screen.getByRole('button',{name:'Игрок · Разведчик'}));assert.equal(selected,id);cleanup();
 function EditPosition(){const [o,setO]=useState(hero);return React.createElement(Transform,{compact:true,spawn:true,object:o,scene,onChange:value=>{position=value;p.objects.find(o=>o.id===id).transforms[scene.id]=value;setO({...o,transforms:{[scene.id]:value}});},onReset(){},onFocus(){},onDuplicate(){},onDelete(){}});}
 render(React.createElement(EditPosition));fireEvent.change(screen.getByLabelText('Положение X'),{target:{value:'2'}});fireEvent.blur(screen.getByLabelText('Положение X'));assert.equal(Number(position.position[0]),2);
 const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('This scenario does not request audio');}));try{await rt.start(p,scene.entry);assert.equal(rt.snapshot.error,null);assert.deepEqual(rt.game.position(id),[2,0,6]);const records=mechanicsFor(p).filter(m=>m.scope?.id===id);assert.equal(records.filter(m=>m.native&&m.type==='weapon').length,0);assert.ok(records.some(m=>m.native&&m.type==='movement'));assert.equal(p.gameplay.weapons.filter(w=>w.ownerId===id).length,2);const hp=p.gameplay.mechanics.find(m=>m.name==='Здоровье бойца');await rt.game.damage(id,10);assert.equal(rt.game.state.mechanicStates[hp.id].hp,90);}finally{rt.stop();}
});
