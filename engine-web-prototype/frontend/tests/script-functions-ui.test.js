import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {EditorView} from 'codemirror';
import {setupCodeMirrorDom} from './helpers/codeMirrorDom.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;setupCodeMirrorDom(dom.window);globalThis.ResizeObserver=class{observe(){}unobserve(){}disconnect(){}};globalThis.requestAnimationFrame=cb=>setTimeout(cb,0);globalThis.cancelAnimationFrame=clearTimeout;
const {render,screen,fireEvent,cleanup,waitFor,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Scripts}=await server.ssrLoadModule('/src/FunctionScriptsWorkspace.jsx'),{createEmptyProject}=await server.ssrLoadModule('/src/projectLifecycle.js'),{callScriptFunction}=await server.ssrLoadModule('/src/scriptFunctions.js');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
function mount(p,props={}){let value=p;function Harness(){const [project,setProject]=React.useState(p);value=project;return React.createElement(Scripts,{...props,project,onChange:fn=>setProject(p=>{const next=structuredClone(p);fn(next);value=next;return next;})});}render(React.createElement(Harness));return ()=>value;}
function replace(text){const view=EditorView.findFromDOM(document.querySelector('.cm-editor'));act(()=>view.dispatch({changes:{from:0,to:view.state.doc.length,insert:text}}));}

test('create, edit, compile, insert and copy a standalone function without adding mechanics',async()=>{
 const p=createEmptyProject('Своя формула');let inserted;const value=mount(p,{onInsert:fn=>inserted=fn});fireEvent.click(screen.getAllByRole('button',{name:'Создать функцию',exact:true})[0]);assert.ok(screen.getByLabelText('JavaScript функции'));assert.equal(value().gameplay.mechanics.length,0);fireEvent.change(screen.getByRole('textbox',{name:'Имя функции'}),{target:{value:'Тройная награда'}});replace('function main({args}) { return args.value * 3; }');fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.ok(value().functions.find(f=>f.functionScript)?.functionScript.approved));const f=value().functions.find(f=>f.functionScript);assert.equal(callScriptFunction(value(),f.id,{value:7},{}),21);
 fireEvent.click(screen.getByRole('button',{name:'Добавить вызов в граф'}));assert.equal(inserted.id,f.id);fireEvent.click(screen.getByRole('button',{name:'Создать независимую копию функции'}));const copy=value().functions.find(x=>x.id!==f.id&&x.functionScript);assert.ok(copy);replace('function main({args}) { return args.value * 5; }');fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.equal(value().functions.find(x=>x.id===copy.id).functionScript.compiledSource,'function main({args}) { return args.value * 5; }'));assert.equal(callScriptFunction(value(),f.id,{value:7},{}),21);assert.equal(callScriptFunction(value(),copy.id,{value:7},{}),35);assert.equal(value().gameplay.mechanics.length,0);
});

test('syntax errors show source location and keep the last executable version',async()=>{
 const value=mount(createEmptyProject('Ошибка'));fireEvent.click(screen.getAllByRole('button',{name:'Создать функцию',exact:true})[0]);replace('function main({args}) { return args.value + 1; }');fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.ok(value().functions.find(f=>f.functionScript)?.functionScript.approved));const f=value().functions.find(f=>f.functionScript),compiled=f.functionScript.compiledSource;replace('function main({args}) {\n return args.value + ;\n}');fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.match(screen.getByRole('alert').textContent,/Строка 2/));assert.equal(value().functions.find(x=>x.id===f.id).functionScript.compiledSource,compiled);assert.equal(callScriptFunction(value(),f.id,{value:2},{}),3);
});
