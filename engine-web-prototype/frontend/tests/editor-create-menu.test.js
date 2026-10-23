import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';

const dom=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:EditorCreateMenu,CREATE_MENU_ITEMS}=await server.ssrLoadModule('/src/EditorCreateMenu.jsx');
afterEach(cleanup);
after(async()=>{await server.close();dom.window.close();});

test('web and native Create menu share all seven labeled icon actions in the same order',()=>{
  const calls=[];
  const labels=['Компонент','Сабсцена','Действие','Группа действий','Блок сценария','Событие','Объект'];
  const {container}=render(React.createElement(EditorCreateMenu,{onCreate:id=>calls.push(id)}));
  const buttons=screen.getAllByRole('button');
  assert.deepEqual(buttons.map(button=>button.textContent),labels);
  assert.ok(buttons.every(button=>button.querySelector('svg')));
  for(const button of buttons)fireEvent.click(button);
  assert.deepEqual(calls,CREATE_MENU_ITEMS.map(item=>item.id));
  assert.equal(container.querySelectorAll('button').length,7);
});

test('Create menu keeps unavailable native actions visible but disabled',()=>{
  const calls=[];
  render(React.createElement(EditorCreateMenu,{onCreate:id=>calls.push(id),disabled:id=>id==='event'}));
  const event=screen.getByRole('button',{name:'Событие'});
  assert.equal(event.disabled,true);
  fireEvent.click(event);
  assert.deepEqual(calls,[]);
  assert.equal(screen.getByRole('button',{name:'Объект'}).disabled,false);
});
