import {setupCodeMirrorDom} from './helpers/codeMirrorDom.js';
import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;setupCodeMirrorDom(dom.window);
globalThis.ResizeObserver=class{observe(){}unobserve(){}disconnect(){}};globalThis.requestAnimationFrame=callback=>setTimeout(callback,0);globalThis.cancelAnimationFrame=clearTimeout;
const {render,screen,fireEvent,cleanup,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Fields}=await server.ssrLoadModule('/src/GameplayCommandFields.jsx');
const {default:Workspace}=await server.ssrLoadModule('/src/GameplayWorkspace.jsx');
const {createEmptyProject}=await server.ssrLoadModule('/src/projectLifecycle.js');
const {ensureGameplay}=await server.ssrLoadModule('/src/gameplayModel.js');
const {registerOperation}=await server.ssrLoadModule('/src/operationRegistry.js');
const {reference}=await server.ssrLoadModule('/src/valueSchemas.js');
const {createMechanicDefinition}=await server.ssrLoadModule('/src/mechanicDescriptions.js');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
test('a new module registration appears in an already mounted common operation library and renders schema fields',async()=>{
 const project=createEmptyProject('UI');let value={command:'runEvent',value:{}};
 function Harness(){const [data,setData]=React.useState(value);return React.createElement(Fields,{project,...data,onChange:v=>{value=v;setData(v);}});}
 render(React.createElement(Harness));
 await act(async()=>{registerOperation({id:'ui.module.scale',version:1,parameters:{type:'object',properties:{factor:{type:'integer',min:1,default:2,label:'Коэффициент'},enabled:{type:'boolean',default:true,label:'Разрешено'}}},result:{type:'integer'},contexts:['event','function'],editor:{label:'Операция из модуля'}},p=>p.factor);});
 const chooser=screen.getByRole('combobox',{name:'Команда'});assert.ok([...chooser.options].some(x=>x.text==='Операция из модуля'));
 fireEvent.change(chooser,{target:{value:'ui.module.scale'}});assert.equal(value.operationVersion,1);assert.equal(screen.getByRole('spinbutton',{name:'Коэффициент'}).value,'2');
 fireEvent.change(screen.getByRole('spinbutton',{name:'Коэффициент'}),{target:{value:'0'}});assert.equal(screen.getByRole('spinbutton',{name:'Коэффициент'}).getAttribute('aria-invalid'),'true');assert.match(screen.getByRole('alert').textContent,/диапазона/);assert.equal(screen.queryByRole('textbox',{name:/JSON/}),null);
});

test('registered set uses existing variable labels and stable references and dynamically checks the assigned type',()=>{
 const project=createEmptyProject('Variables');project.variables={score:0};project.variableTypes={score:'integer'};ensureGameplay(project);let value={command:'sacura.variable.set',value:{variable:reference('variable',project.gameplay.variableBindings[0].id),value:0}};
 function Harness(){const [data,setData]=React.useState(value);return React.createElement(Fields,{project,...data,onChange:v=>{value=v;setData(v);}});}
 render(React.createElement(Harness));const chooser=screen.getByRole('combobox',{name:'Переменная'});assert.equal(chooser.selectedOptions[0].text,'Проект · score · целое число');assert.notEqual(JSON.parse(chooser.value).id,'score');
 fireEvent.change(screen.getByRole('spinbutton',{name:'Новое значение'}),{target:{value:'2.5'}});assert.match(screen.getByRole('alert').textContent,/целое число/);assert.equal(value.value.value,2.5);
});

test('description-backed mechanic joins the existing scope catalog and edits only instance values',()=>{
 const project=createEmptyProject('UI');project.functions.push({id:'fn',name:'Существующая функция',parameters:[{name:'step',type:'number',schema:{type:'integer',default:1}}],locals:{},entry:'return',nodes:[{id:'return',kind:'return',value:{op:'get',path:'args.step'}}]});
 const d=createMechanicDefinition(project,{id:'ui.mechanic.counter',name:'Счётчик без героя',parameters:{type:'object',properties:{step:{type:'integer',default:1,label:'Шаг'}}},logic:{functionId:'fn'}});let current=project,opened=[];
 function Harness(){const [p,setP]=React.useState(project);return React.createElement(Workspace,{project:p,sceneId:p.subscenes[0].id,onChange:fn=>setP(p=>{const next=structuredClone(p);fn(next);current=next;return next;}),onEvent(){},onFunction:id=>opened.push(id)});}
 render(React.createElement(Harness));fireEvent.change(screen.getByRole('combobox',{name:'Добавить к'}),{target:{value:'project:'+project.id}});fireEvent.click(screen.getByRole('button',{name:'Добавить механику'}));fireEvent.change(screen.getByRole('combobox',{name:'Готовая механика'}),{target:{value:d.id}});fireEvent.click(screen.getByRole('button',{name:'Добавить'}));assert.ok(screen.getByRole('region',{name:'JavaScript игровых механик'}));assert.equal(screen.queryByRole('spinbutton',{name:'Шаг'}),null);fireEvent.click(screen.getByRole('button',{name:'Параметры экземпляра',exact:true}));
 fireEvent.change(screen.getByRole('spinbutton',{name:'Шаг'}),{target:{value:'3'}});assert.equal(current.gameplay.mechanics[0].values.step,3);assert.equal(current.gameplay.mechanicDefinitions[0].parameters.properties.step.default,1);assert.deepEqual(current.objects,[]);assert.equal(current.gameplay.playerId,'');fireEvent.click(screen.getByRole('button',{name:'Параметры экземпляра',exact:true}));assert.equal(screen.getByRole('combobox',{name:'Скрипт механики',exact:true}).value,'fn');assert.ok(screen.getByRole('region',{name:'JavaScript игровых механик'}));assert.equal(screen.queryByRole('complementary',{name:'Настройки графа'}),null);
});

test('an unavailable registered operation displays a diagnostic and preserves its authored values',()=>{
 const project=createEmptyProject('Future');let mutations=0;render(React.createElement(Fields,{project,command:'other.operation.future',operationVersion:9,value:{preserve:'Data'},onChange:()=>mutations++}));assert.match(screen.getByRole('alert').textContent,/сохранены/);assert.equal(mutations,0);
});

test('cross-field comparison errors come from the registry validator in the same generic form',()=>{
 const project=createEmptyProject('Compare');render(React.createElement(Fields,{project,command:'sacura.condition.compare',value:{left:true,right:2,operator:'eq'},onChange(){}}));assert.match(screen.getByRole('alert').textContent,/одного типа/);
});
