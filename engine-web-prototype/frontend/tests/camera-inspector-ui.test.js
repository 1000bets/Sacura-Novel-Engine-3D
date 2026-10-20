import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React,{useState} from 'react';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {mechanicsFor} from '../src/mechanicsModel.js';
import {readProject} from '../src/projectFiles.js';
import {cameraPose,cleanCamera,resolveCamera} from '../src/cameraModel.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Fields}=await server.ssrLoadModule('/src/MechanicFields.jsx');
const {default:Camera}=await server.ssrLoadModule('/src/CameraInspector.jsx');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
const shooter=async()=>instantiateAuthoringTemplate(JSON.parse(await readFile('public/game-templates/shooter.sacura-template.json','utf8')),undefined,{reviewedSources:true});
const set=(label,value)=>fireEvent.change(screen.getByLabelText(label,{exact:true}),{target:{value:String(value)}});

test('compact player controls edit canonical settings, survive reopening, and change movement, jump and camera in Play',async()=>{
 let p=await shooter();const id=p.gameplay.controller.ownerId;
 function Edit({type}){const [project,update]=useState(p);return React.createElement(Fields,{compact:true,project,mechanic:mechanicsFor(project).find(m=>m.type===type),onChange:fn=>{const next=structuredClone(project);fn(next);p=next;update(next);}});}
 render(React.createElement(Edit,{type:'movement'}));set('Движение','none');set('Движение','wasd');set('Скорость',6);
 const rare=screen.getByText('Прыжок и гравитация').closest('details');assert.equal(rare.open,false);fireEvent.click(screen.getByText('Прыжок и гравитация'));set('Сила прыжка',7);set('Гравитация',12);
 fireEvent.click(screen.getByText('Действия ввода'));fireEvent.click(screen.getByRole('button',{name:'Подключить клавиши и мышь'}));const bindings=p.input.contexts.reduce((n,c)=>n+c.bindings.length,0);fireEvent.click(screen.getByRole('button',{name:'Подключить клавиши и мышь'}));assert.equal(p.input.contexts.reduce((n,c)=>n+c.bindings.length,0),bindings);
 cleanup();render(React.createElement(Edit,{type:'camera'}));assert.equal(screen.queryByLabelText('Расстояние',{exact:true}),null);set('Режим','authored');set('Режим','firstPerson');set('Угол обзора',88);fireEvent.click(screen.getByText('Дополнительно'));set('Высота взгляда',1.8);set('Чувствительность мыши',.006);
 set('Режим','thirdPerson');set('Расстояние',4);set('Режим','firstPerson');assert.equal(p.gameplay.cameraSettings.distance,4);
 const lookActions=p.input.actions.filter(a=>a.operation?.id==='sacura.camera.look').length,lookBindings=p.input.contexts.flatMap(c=>c.bindings).filter(b=>p.input.actions.find(a=>a.id===b.actionId)?.operation?.id==='sacura.camera.look').length;fireEvent.click(screen.getByText('Действия ввода'));fireEvent.click(screen.getByRole('button',{name:'Подключить клавиши и мышь'}));assert.equal(p.input.actions.filter(a=>a.operation?.id==='sacura.camera.look').length,lookActions);assert.equal(p.input.contexts.flatMap(c=>c.bindings).filter(b=>p.input.actions.find(a=>a.id===b.actionId)?.operation?.id==='sacura.camera.look').length,lookBindings);
 p=readProject(JSON.stringify(p));const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio requested in this test');}));
 try{await rt.start(p,p.subscenes[0].entry);assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.world.playerControl.speed,6);const from=rt.game.position(id);rt.game.step([1,0,0],.05);assert.ok(Math.abs(rt.game.position(id)[0]-from[0]-.3)<1e-6);rt.game.jump(id);assert.equal(rt.game.state.bodies[id].velocity[1],7);rt.game.look([10,0],'pointer');assert.equal(rt.game.state.yaw,-.06);const view=resolveCamera(p.subscenes[0],rt.snapshot.world,p.objects);assert.equal(view.fov,88);assert.equal(view.position[1],rt.game.position(id)[1]+1.8);}finally{rt.stop();}
});

test('camera inspector preserves the follow frame when switching to fixed, and exact coordinates round-trip',async()=>{
 let p=await shooter(),scene=p.subscenes[0],camera=scene.cameras[0],world={location:scene.id,positions:{},visible:{}};const original=camera.position.slice();
 function Edit(){const [c,update]=useState(camera);return React.createElement(Camera,{camera:c,scene,objects:p.objects,state:world,onChange:(id,patch)=>{assert.equal(id,camera.id);camera=cleanCamera({...c,...patch});scene.cameras[0]=camera;update(camera);},onDefault(){},onDelete(){},onPilot(){},onView(){},onCapture(){}});}
 render(React.createElement(Edit));assert.equal(screen.getByText('Координаты').closest('details').open,false);set('Угол обзора',42);set('Режим','follow');set('Персонаж',p.gameplay.controller.ownerId);set('Расстояние',4.5);set('Высота камеры',2);fireEvent.click(screen.getByText('Дополнительно'));set('Ракурс, °',30);set('Высота взгляда',1.3);
 const expected=cameraPose(camera,world,p.objects,scene.kind);assert.notDeepEqual(expected.position,original);set('Режим','fixed');assert.deepEqual(camera.position,expected.position);assert.deepEqual(camera.target,expected.target);fireEvent.click(screen.getByText('Координаты'));set('Камера X',3);set('Точка взгляда Z',-2);
 const reopened=readProject(JSON.stringify(p)),view=cameraPose(reopened.subscenes[0].cameras[0],world,reopened.objects,scene.kind);assert.equal(view.position[0],3);assert.equal(view.target[2],-2);assert.equal(view.fov,42);
});

test('camera actions address the selected camera; Play locks edits but allows viewing and stopping',async()=>{
 const p=await shooter(),scene=p.subscenes[0],camera=scene.cameras[0],calls=[];
 const props={camera,scene,objects:p.objects,state:{location:scene.id},onChange(){throw Error('Play edits must be disabled');},onView:id=>calls.push(['view',id]),onPilot:id=>calls.push(['pilot',id]),onCapture:id=>calls.push(['capture',id]),onDefault(){},onDelete(){},onEdit:()=>calls.push(['stop'])};
 render(React.createElement(Camera,{...props,piloting:camera.id}));fireEvent.click(screen.getByRole('button',{name:'В сцене',exact:true}));fireEvent.click(screen.getByRole('button',{name:'Сохранить ракурс'}));assert.deepEqual(calls,[['pilot',camera.id],['capture',camera.id]]);cleanup();calls.length=0;
 render(React.createElement(Camera,{...props,running:true}));assert.equal(screen.getByLabelText('Название камеры').matches(':disabled'),true);assert.equal(screen.getByRole('button',{name:'В сцене',exact:true}).disabled,true);assert.equal(screen.getByRole('button',{name:'Удалить камеру'}).disabled,true);fireEvent.click(screen.getByRole('button',{name:'Посмотреть',exact:true}));fireEvent.click(screen.getByRole('button',{name:'Остановить',exact:true}));assert.deepEqual(calls,[['view',camera.id],['stop']]);
});
