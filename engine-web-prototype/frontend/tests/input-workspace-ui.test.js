import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React,{useState} from 'react';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {readProject} from '../src/projectFiles.js';
import {InputSystem} from '../src/InputSystem.js';
import '../src/coreOperations.js';
import {validateInputSettings} from '../src/inputModel.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,within,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
await server.ssrLoadModule('/src/coreOperations.js');
const {default:InputWorkspace,InputContextSettings}=await server.ssrLoadModule('/src/InputWorkspace.jsx');
let current;
function Harness({initial=createEmptyProject(),running=false}){
 const [project,setProject]=useState(initial);current=project;
 return React.createElement(InputWorkspace,{project,beat:project.chapters[0].beats[0],running,onStop(){},onChange:change=>setProject(previous=>{const next=structuredClone(previous);change(next);return next;})});
}
const actionButton=name=>within(screen.getByRole('navigation',{name:'Действия ввода'})).getByRole('button',{name:new RegExp('^'+name)});
afterEach(()=>cleanup());
after(async()=>{await server.close();dom.window.close();});

test('selecting actions follows their context, while an explicitly chosen context is retained for editing',()=>{
 render(React.createElement(Harness));
 fireEvent.click(actionButton('Пауза'));assert.equal(screen.getByLabelText('Контекст').value,'system');
 assert.ok(screen.getByRole('button',{name:'Изменить клавишу Esc'}));
 fireEvent.change(screen.getByLabelText('Контекст'),{target:{value:'dialogue'}});
 assert.equal(screen.getByLabelText('Контекст').value,'dialogue');assert.equal(screen.queryByRole('button',{name:'Изменить клавишу Esc'}),null);
 fireEvent.click(actionButton('Дальше'));assert.equal(screen.getByLabelText('Контекст').value,'dialogue');
});

test('recording a physical key changes runtime input, Escape cancels recording and directions remain editable',()=>{
 render(React.createElement(Harness));
 const record=screen.getByRole('button',{name:'Изменить клавишу W'});
 fireEvent.click(record);fireEvent.keyDown(record,{key:'Escape',code:'Escape'});
 assert.equal(current.input.contexts.find(c=>c.id==='gameplay').bindings.find(b=>b.id==='gameplay-0').control,'KeyW');
 fireEvent.click(record);fireEvent.keyDown(record,{key:'ш',code:'KeyI'});
 const binding=current.input.contexts.find(c=>c.id==='gameplay').bindings.find(b=>b.id==='gameplay-0');assert.equal(binding.control,'KeyI');
 const row=screen.getByRole('button',{name:'Изменить клавишу I'}).closest('.input-binding');
 fireEvent.change(within(row).getByLabelText('Направление'),{target:{value:'right'}});
 assert.deepEqual(current.input.contexts.find(c=>c.id==='gameplay').bindings.find(b=>b.id==='gameplay-0').scale,[1,0]);
 const events=[],input=new InputSystem(current.input,event=>events.push(event));input.setContexts(['gameplay']);
 try{input.feed('keyboard','KeyW',1);input.tick(.02);assert.equal(events.length,0);input.feed('keyboard','KeyI',1);input.tick(.02);assert.deepEqual(events.find(event=>event.phase==='triggered'&&event.actionId==='move').value,[1,0]);}finally{input.dispose();}
});

test('opening tabs preserves custom scales, variables and adapter codes',()=>{
 const project=createEmptyProject();project.variables.customX=0;
 project.input.actions[0].variableX='customX';
 const binding=project.input.contexts.find(c=>c.id==='gameplay').bindings[0];binding.scale=[2.25,-.65];binding.deadZone=.24;binding.customMetadata={origin:'local'};
 project.input.contexts[1].bindings.push({id:'custom',actionId:'move',device:'external',control:'sensor.axis',scale:[.5,.7]});
 const before=JSON.stringify(project);render(React.createElement(Harness,{initial:project}));
 for(const name of ['Контексты','Устройства','Действия'])fireEvent.click(within(screen.getByRole('navigation',{name:'Разделы ввода'})).getByRole('button',{name,exact:true}));
 assert.equal(JSON.stringify(current),before);
 fireEvent.click(screen.getByRole('button',{name:'Параметры привязки W'}));
 const row=screen.getByRole('button',{name:'Изменить клавишу W'}).closest('.input-binding');
 assert.equal(within(row).getByLabelText('Масштаб X').value,'2.25');assert.equal(within(row).getByLabelText('Масштаб Y').value,'-0.65');
 assert.deepEqual(readProject(JSON.stringify(current)).input,current.input);
});

test('adding a gamepad binding uses named controls and serializes valid editable data',()=>{
 render(React.createElement(Harness));
 fireEvent.change(screen.getByLabelText('Устройство новой привязки'),{target:{value:'gamepad-axis'}});
 fireEvent.click(screen.getByRole('button',{name:'Добавить привязку'}));
 const added=current.input.contexts.find(c=>c.id==='gameplay').bindings.at(-1);
 assert.equal(added.device,'gamepad-axis');assert.equal(added.control,'0');assert.deepEqual(added.scale,[1,0]);assert.equal(added.deadZone,.18);
 assert.deepEqual(validateInputSettings(current),[]);assert.deepEqual(readProject(JSON.stringify(current)).input,current.input);
});

test('new action and context can be configured without a character, and deletion cleans their references',()=>{
 render(React.createElement(Harness));fireEvent.click(screen.getByRole('button',{name:'Создать действие'}));
 const name=screen.getByLabelText('Название');assert.equal(document.activeElement,name);
 fireEvent.change(name,{target:{value:'Открыть журнал'}});fireEvent.change(screen.getByLabelText('Что делает'),{target:{value:'pause'}});
 const actionId=current.input.actions.at(-1).id;
 fireEvent.click(within(screen.getByRole('navigation',{name:'Разделы ввода'})).getByRole('button',{name:'Контексты',exact:true}));
 fireEvent.click(screen.getByRole('button',{name:'Создать контекст'}));const contextId=current.input.contexts.at(-1).id;
 fireEvent.change(screen.getByLabelText('Название контекста'),{target:{value:'Журнал'}});
 fireEvent.click(screen.getByRole('button',{name:'Настроить привязки'}));fireEvent.click(screen.getByRole('button',{name:'Добавить привязку'}));
 assert.equal(current.input.contexts.at(-1).bindings[0].actionId,actionId);
 assert.equal(current.input.contexts.at(-1).id,contextId);assert.deepEqual(validateInputSettings(current),[]);
 fireEvent.click(screen.getByRole('button',{name:'Удалить действие'}));
 assert.equal(current.input.actions.some(action=>action.id===actionId),false);assert.ok(current.input.contexts.every(context=>context.bindings.every(binding=>binding.actionId!==actionId)));
 fireEvent.click(within(screen.getByRole('navigation',{name:'Разделы ввода'})).getByRole('button',{name:'Контексты',exact:true}));
 fireEvent.click(screen.getByRole('button',{name:'Удалить контекст'}));assert.equal(current.input.contexts.some(context=>context.id===contextId),false);
});

test('preview locks editing and keeps a way to stop it',()=>{
 const initial=createEmptyProject(),before=JSON.stringify(initial);render(React.createElement(Harness,{initial,running:true}));
 assert.ok(screen.getByRole('button',{name:'Остановить для редактирования'}));
 assert.equal(screen.getByRole('button',{name:'Создать действие'}).disabled,true);
 assert.equal(screen.getByLabelText('Название').closest('fieldset').disabled,true);
 fireEvent.change(screen.getByLabelText('Название'),{target:{value:'Do not change'}});assert.equal(JSON.stringify(current),before);
});


test('a shared operation remains editable on the same input action and round trips without a player',()=>{
 const project=createEmptyProject();project.input.actions.push({id:'compare-input',name:'Проверить условие',valueType:'boolean',behavior:'operation',trigger:'pressed',holdSeconds:.4,consume:true,operation:{id:'sacura.condition.compare',version:1,parameters:{left:2,operator:'gt',right:1}}});
 render(React.createElement(Harness,{initial:project}));fireEvent.click(actionButton('Проверить условие'));
 assert.equal(screen.getByLabelText('Операция').value,'sacura.condition.compare');
 fireEvent.change(screen.getByLabelText('Первое значение'),{target:{value:'4'}});
 assert.equal(current.input.actions.find(action=>action.id==='compare-input').operation.parameters.left,4);
 assert.deepEqual(validateInputSettings(current),[]);assert.deepEqual(readProject(JSON.stringify(current)).input,current.input);
});


test('beat context controls distinguish required gameplay contexts from editable selections',()=>{
 const project=createEmptyProject();project.gameplay.enabled=true;project.gameplay.storyPolicy={mode:'story',mechanics:'continuous',dialogueControl:'allow'};
 const beat=project.chapters[0].beats[0];let last;
 const {rerender}=render(React.createElement(InputContextSettings,{compact:true,project,beat,onChange:patch=>{last=patch;},onOpen(){}}));
 assert.equal(screen.getByLabelText('Ходьба и взаимодействие').disabled,true);
 assert.equal(screen.getByLabelText('Диалог').disabled,false);
 fireEvent.click(screen.getByLabelText('Диалог'));assert.deepEqual(last.inputContexts,['gameplay']);
 rerender(React.createElement(InputContextSettings,{compact:true,project,beat:{...beat,...last},onChange:patch=>{last=patch;},onOpen(){}}));
 fireEvent.click(screen.getByRole('button',{name:'Вернуть автоматический контекст'}));assert.deepEqual(last,{inputContexts:undefined});
});
