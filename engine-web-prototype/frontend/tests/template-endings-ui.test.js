import test from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';

test('the actual ending button restarts at the scene entry instead of treating the click event as play-from-selection',async()=>{
 const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
 for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
 globalThis.IS_REACT_ACT_ENVIRONMENT=true;globalThis.ResizeObserver=class{observe(){}disconnect(){}};
 const {render,screen,fireEvent,cleanup,act}=await import('@testing-library/react');
 const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
 let rt;
 try{
  const {GameDialogue}=await server.ssrLoadModule('/src/Editor.jsx'),{createEmptyProject}=await server.ssrLoadModule('/src/projectLifecycle.js'),{playbackStartId}=await server.ssrLoadModule('/src/storyEditing.js'),{PreviewRuntime}=await server.ssrLoadModule('/src/runtime.js');
  const p=createEmptyProject('Повторить'),entry=p.chapters[0].beats[0],ending={...structuredClone(entry),id:'ending',kind:'end',text:'Готово',ending:'Финал',next:null};p.chapters[0].beats.push(ending);
  rt=new PreviewRuntime({stopAll(){},tracks:new Map()});await rt.start(p,ending.id);await rt.advance();assert.equal(rt.snapshot.phase,'FINISHED');
  let args,restarted;render(React.createElement(GameDialogue,{project:p,beat:ending,preview:rt.snapshot,running:true,variables:rt.snapshot.variables,show:true,onStart:(...received)=>{args=received;restarted=rt.start(p,playbackStartId(p,ending.id,received[0]));}}));
  await act(async()=>{fireEvent.click(screen.getByRole('button',{name:'Сыграть ещё раз'}));await restarted;});
  assert.deepEqual(args,[]);assert.equal(rt.snapshot.beatId,entry.id);assert.equal(rt.snapshot.phase,'WAITING_INPUT');
 }finally{rt?.stop();cleanup();await server.close();dom.window.close();}
});
