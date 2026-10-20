import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
globalThis.ResizeObserver=class{observe(){}unobserve(){}disconnect(){}};
globalThis.requestAnimationFrame=callback=>setTimeout(callback,0);globalThis.cancelAnimationFrame=clearTimeout;
const {render,screen,within,fireEvent,cleanup,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Workspace}=await server.ssrLoadModule('/src/BlueprintFunctionWorkspace.jsx');
const {createEmptyProject}=await server.ssrLoadModule('/src/projectLifecycle.js');
const {newBlueprintFunction,callBlueprint}=await server.ssrLoadModule('/src/blueprintFunctions.js');
const {migrateFunctionGraph}=await server.ssrLoadModule('/src/functionGraph.js');
const {setLanguage}=await server.ssrLoadModule('/src/i18n.js');
afterEach(()=>{cleanup();setLanguage('ru');});after(async()=>{await server.close();dom.window.close();});
function mount(project,props={}){let value=project;function Harness(){const [p,setP]=React.useState(value);return React.createElement(Workspace,{...props,project:p,onChange:fn=>setP(p=>{const next=structuredClone(p);fn(next);value=next;return next;})});}render(React.createElement(Harness));return()=>value;}
const canvasNode=id=>within(document.querySelector('[data-id="'+id+'"]'));
function fixture(){const p=createEmptyProject('Без героя'),f=newBlueprintFunction();p.functions.push(f);return {p,f};}
function addNode(name){fireEvent.click(screen.getByRole('button',{name:'Добавить узел',exact:true}));fireEvent.change(screen.getByRole('searchbox',{name:'Найти узел'}),{target:{value:name}});fireEvent.click([...screen.getByRole('dialog',{name:'Добавление узла'}).querySelectorAll('button')].find(button=>button.querySelector('strong')?.textContent===name));}


function connection(scope,label){
 const name=label.startsWith('Источник ')?new RegExp('^Вход '+label.slice(9).replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+' · '):'Исполнение '+({'Если да':'Да','Если нет':'Нет','Следующий узел':'Дальше'}[label]||label);
 fireEvent.click(scope.getByLabelText(name,{selector:'.graph-pin'}));return scope.getByLabelText(label,{selector:'select'});
}

test('requested function opens directly; palette search and Escape preserve data and restore focus',()=>{
 const {p}=fixture(),other=newBlueprintFunction();other.name='Проверка';p.functions.push(other);const value=mount(p,{requestedId:other.id});
 assert.equal(screen.getByRole('combobox',{name:'Функция',exact:true}).value,other.id);fireEvent.click(screen.getByRole('button',{name:'Показать настройки'}));
 assert.equal(screen.getByRole('combobox',{name:'Узел',exact:true}).value,other.entry);
 const before=JSON.stringify(value());fireEvent.keyDown(screen.getByRole('tab',{name:'Узел',exact:true}),{key:'ArrowRight'});assert.equal(document.activeElement,screen.getByRole('tab',{name:'Функция',exact:true}));fireEvent.keyDown(document.activeElement,{key:'ArrowLeft'});assert.equal(screen.getByRole('tab',{name:'Узел',exact:true}).getAttribute('aria-selected'),'true');fireEvent.click(screen.getByRole('button',{name:'Добавить узел',exact:true}));assert.equal(document.activeElement,screen.getByRole('searchbox',{name:'Найти узел'}));
 fireEvent.change(document.activeElement,{target:{value:'ничего такого'}});assert.ok(screen.getByText('Узлы не найдены'));
 fireEvent.keyDown(document.activeElement,{key:'Escape'});assert.equal(screen.queryByRole('dialog'),null);assert.equal(document.activeElement,screen.getByRole('button',{name:'Добавить узел',exact:true}));assert.equal(JSON.stringify(value()),before);
});

test('branch added through the palette connects through named choices and executes the same function',async()=>{
 const {p,f}=fixture(),value=mount(p);
 addNode('Условие');const branch=value().functions[0].nodes.at(-1);assert.equal(branch.kind,'branch');
 addNode('Больше');const compare=value().functions[0].nodes.at(-1);assert.equal(compare.value.op,'gt');
 fireEvent.change(canvasNode(compare.id).getByLabelText('A',{selector:'input'}),{target:{value:'3'}});fireEvent.change(canvasNode(compare.id).getByLabelText('B',{selector:'input'}),{target:{value:'2'}});
 fireEvent.change(connection(canvasNode(branch.id),'Источник Условие'),{target:{value:compare.id+'|value'}});
 fireEvent.change(connection(canvasNode(branch.id),'Если да'),{target:{value:f.nodes[1].id}});fireEvent.change(connection(canvasNode(branch.id),'Если нет'),{target:{value:''}});
 fireEvent.change(connection(canvasNode(f.entry),'Следующий узел'),{target:{value:branch.id}});
 assert.equal(await callBlueprint(value(),f.id,{amount:17},{},()=>{}),17);
 fireEvent.change(canvasNode(compare.id).getByLabelText('A',{selector:'input'}),{target:{value:'1'}});
 assert.equal(await callBlueprint(value(),f.id,{amount:17},{},()=>{}),undefined);
 assert.deepEqual(value().gameplay.actors,{});assert.equal(value().gameplay.weapons.length,0);
});

test('arrangement changes positions only, and deletion removes incoming links and updates the entry',()=>{
 const {p,f}=fixture();f.nodes[0].position={x:300,y:900};f.nodes[1].position={x:10,y:10};const value=mount(p),before=migrateFunctionGraph(f);
 fireEvent.click(screen.getByRole('button',{name:'Упорядочить',exact:true}));const arranged=value().functions[0];
 assert.deepEqual(arranged.nodes.map(({position,...n})=>n),before.nodes.map(({position,...n})=>n));assert.equal(arranged.entry,before.entry);assert.ok(arranged.nodes[0].position.x<arranged.nodes[1].position.x);fireEvent.click(screen.getByRole('button',{name:'Показать настройки'}));
 fireEvent.change(screen.getByRole('combobox',{name:'Узел',exact:true}),{target:{value:f.nodes[1].id}});fireEvent.click(screen.getByRole('button',{name:'Удалить узел'}));
 assert.equal(value().functions[0].nodes.filter(n=>n.kind!=='value').length,1);assert.equal(value().functions[0].nodes[0].next,'');
 fireEvent.click(screen.getByRole('button',{name:'Удалить узел'}));assert.equal(value().functions[0].entry,'');for(const n of [...value().functions[0].nodes]){fireEvent.change(screen.getByRole('combobox',{name:'Узел',exact:true}),{target:{value:n.id}});fireEvent.click(screen.getByRole('button',{name:'Удалить узел'}));}assert.ok(screen.getByText('Добавьте первый узел'));
 addNode('Вернуть результат');assert.equal(value().functions[0].entry,value().functions[0].nodes[0].id);
});

test('export follows the selected function; used functions stay protected and names are literal in English',async()=>{
 const {p,f}=fixture(),other=newBlueprintFunction();other.name='Функция';p.functions.push(other);p.functions[0].nodes[0]={...p.functions[0].nodes[0],kind:'call',functionId:other.id};let exported;
 await act(()=>setLanguage('en'));const value=mount(p,{onExport:fn=>exported=fn});
 fireEvent.change(screen.getByRole('combobox',{name:'Function',exact:true}),{target:{value:other.id}});assert.equal(screen.getByRole('combobox',{name:'Function',exact:true}).selectedOptions[0].text,'Функция');
 fireEvent.click(screen.getByRole('button',{name:'Export function'}));assert.equal(exported.id,other.id);
 fireEvent.click(screen.getByRole('button',{name:'Function arguments',exact:true}));fireEvent.click(screen.getByRole('tab',{name:'Function',exact:true}));fireEvent.click(screen.getByText('Delete function',{selector:'summary'}));fireEvent.click(screen.getByRole('button',{name:'Delete function',exact:true}));
 assert.equal(value().functions.length,2);assert.ok(screen.getByRole('alert'));assert.equal(value().functions[0].id,f.id);
});

test('an imported function remains editable and its changed graph re-exports independently to another project',async()=>{
 const {exportAuthoringPackage,planPackageImport}=await server.ssrLoadModule('/src/mechanicPackages.js');
 const {p:a,f}=fixture();f.nodes[0].comment='Авторская заметка';f.nodes[0].position={x:34,y:56};
 const pkg=await exportAuthoringPackage(a,f,{kind:'function',name:'Расчёт',author:'Автор A',license:'CC0'});
 const b=createEmptyProject('B'),imported=(await planPackageImport(b,pkg)).apply(b),fn=imported.functions[0],value=mount(imported,{requestedId:fn.id});
 fireEvent.click(canvasNode(fn.nodes[0].id).getByLabelText('Отсоединить Значение'));
 fireEvent.change(canvasNode(fn.nodes[0].id).getByLabelText('Значение',{selector:'input'}),{target:{value:'17'}});
 const edited=value().functions[0];assert.equal(edited.nodes[0].comment,'Авторская заметка');assert.deepEqual(edited.nodes[0].position,{x:34,y:56});
 assert.equal(await callBlueprint(value(),edited.id,{amount:3},{},()=>{}),17);assert.equal(await callBlueprint(a,f.id,{amount:3},{},()=>{}),3);
 const variant=await exportAuthoringPackage(value(),edited,{kind:'function',name:'Мой расчёт',author:'Автор B',license:'CC0'}),c=createEmptyProject('C'),next=(await planPackageImport(c,variant)).apply(c);
 assert.equal(await callBlueprint(next,next.functions[0].id,{amount:3},{},()=>{}),17);assert.equal(next.functions[0].nodes[0].comment,'Авторская заметка');assert.ok(variant.metadata.sources.some(source=>source.author==='Автор A'));
});
