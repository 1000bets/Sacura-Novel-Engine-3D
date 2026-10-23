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
const {default:Workspace}=await server.ssrLoadModule('/src/NativeGameplayWorkspace.jsx');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});

test('Electron mechanics panel selects an object, saves source, compiles and starts native Play',async()=>{
  let records=[],played=0;const calls=[];
  window.sacuraDesktop={nativeMechanics:async(action,params={})=>{
    calls.push([action,params]);
    if(action==='create')records=[...records,{id:'mechanic-1',name:'Новая механика',objectId:params.objectId,source:'async function main({engine}) {}',compiledSource:'',enabled:false}];
    if(action==='update')records=records.map(item=>item.id===params.id?{...item,...params.patch}:item);
    if(action==='compile')records=records.map(item=>item.id===params.id?{...item,compiledSource:item.source,enabled:true}:item);
    return structuredClone(records);
  },onNativeMechanicsResult:()=>()=>{}};
  render(React.createElement(Workspace,{session:'session',objects:[{id:'object-1',name:'Дверь'}],selectedObjectId:'object-1',playing:false,onPlay:()=>played++,onStop:()=>{}}));
  await waitFor(()=>assert.ok(screen.getByText(/Пока нет механик/)));
  fireEvent.click(screen.getByRole('button',{name:'+ Добавить скрипт'}));
  await waitFor(()=>assert.equal(screen.getByLabelText('Объект механики').value,'object-1'));
  const view=EditorView.findFromDOM(document.querySelector('.cm-editor'));
  act(()=>view.dispatch({changes:{from:0,to:view.state.doc.length,insert:'async function main({engine}) { await engine.self.setActive(false); }'}}));
  fireEvent.click(screen.getByRole('button',{name:'Скомпилировать'}));
  await waitFor(()=>assert.equal(records[0].compiledSource,'async function main({engine}) { await engine.self.setActive(false); }'));
  fireEvent.click(screen.getByRole('button',{name:'Запустить'}));assert.equal(played,1);
  assert.ok(calls.some(([action,params])=>action==='create'&&params.objectId==='object-1'));
  assert.ok(calls.some(([action])=>action==='compile'));
});
