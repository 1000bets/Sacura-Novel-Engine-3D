import Test, {after as After, afterEach as AfterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer as CreateServer} from 'vite';
import {fileURLToPath as FileUrlToPath} from 'node:url';
import React from 'react';

const DocumentEnvironment = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'});
for (const Key of ['window','document','navigator','HTMLElement','localStorage']) {
  Object.defineProperty(globalThis, Key, {configurable:true,value:DocumentEnvironment.window[Key]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render:Render,screen:Screen,fireEvent:FireEvent,cleanup:Cleanup,waitFor:WaitFor} = await import('@testing-library/react');
const Server = await CreateServer({root:FileUrlToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Dialog} = await Server.ssrLoadModule('/src/NativeAssetCreateDialog.jsx');
AfterEach(Cleanup);
After(async () => {await Server.close();DocumentEnvironment.window.close();});

const Factory = {id:'editor.CreateInputActionAssetFactory',label:'Действие ввода',extension:'.inputaction',settings:[
  {id:'value_type',label:'Тип значения',typeId:'engine.InputValueType',value:'Boolean',enumValues:['Boolean','Axis1D','Axis2D','Axis3D']}
]};

Test('reflected enum offers named choices and submits the selected name', async () => {
  const Requests = [];
  let Closed = false;
  Render(React.createElement(Dialog,{type:Factory,folder:'Game/Input',create:async Request => {Requests.push(Request);},close:() => {Closed = true;}}));
  const Selection = Screen.getByRole('combobox',{name:'Тип значения'});
  Assert.equal(Selection.value,'Boolean');
  Assert.deepEqual(Array.from(Selection.options, Option => Option.value),Factory.settings[0].enumValues);
  Assert.equal(Screen.getByRole('button',{name:'Создать'}).disabled,true);
  FireEvent.change(Selection,{target:{value:'Axis2D'}});
  FireEvent.change(Screen.getByLabelText('Имя ассета'),{target:{value:'Movement'}});
  FireEvent.click(Screen.getByRole('button',{name:'Создать'}));
  await WaitFor(() => Assert.equal(Closed,true));
  Assert.deepEqual(Requests,[{factoryId:Factory.id,path:'Input/Movement.inputaction',settings:{value_type:'Axis2D'}}]);
});

Test('failed creation retains the enum selection for retry', async () => {
  Render(React.createElement(Dialog,{type:Factory,folder:'Game',create:async () => {throw Error('Creation failed');},close:() => {}}));
  FireEvent.change(Screen.getByLabelText('Тип значения'),{target:{value:'Axis3D'}});
  FireEvent.change(Screen.getByLabelText('Имя ассета'),{target:{value:'Movement'}});
  FireEvent.click(Screen.getByRole('button',{name:'Создать'}));
  await WaitFor(() => Assert.equal(Screen.getByRole('alert').textContent,'Creation failed'));
  Assert.equal(Screen.getByLabelText('Тип значения').value,'Axis3D');
  Assert.equal(Screen.getByRole('button',{name:'Создать'}).disabled,false);
});
