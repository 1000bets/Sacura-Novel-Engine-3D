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
const {default:Hierarchy} = await Server.ssrLoadModule('/src/HierarchyTree.jsx');
const {NativePrefabControls} = await Server.ssrLoadModule('/src/NativePrefab.jsx');
AfterEach(Cleanup);
After(async () => {await Server.close();DocumentEnvironment.window.close();});


const SceneObject = {id:'cube',name:'TestCube'};
function ShowHierarchy(Rename, Disabled = false) {
 return Render(React.createElement(Hierarchy,{scene:{id:'scene'},objects:[SceneObject],query:'',expanded:{},onExpanded:()=>{},selectedIds:['cube'],onSelectObject:()=>{},onRename:Rename,disabled:Disabled}));
}
Test('hierarchy pencil commits a name with Enter and cancels with Escape', () => {
 const Requests = [];
 ShowHierarchy((...Arguments) => Requests.push(Arguments));
 FireEvent.click(Screen.getByTitle('Rename'));
 FireEvent.change(Screen.getByRole('textbox'),{target:{value:'Renamed'}});
 FireEvent.keyDown(Screen.getByRole('textbox'),{key:'Enter'});
 Assert.deepEqual(Requests,[['cube','Renamed']]);
 FireEvent.click(Screen.getByTitle('Rename'));
 FireEvent.change(Screen.getByRole('textbox'),{target:{value:'Cancelled'}});
 FireEvent.keyDown(Screen.getByRole('textbox'),{key:'Escape'});
 Assert.equal(Requests.length,1);
 Assert.equal(Screen.queryByRole('textbox'),null);
});
Test('empty name is ignored and Play disables rename', () => {
 const Requests = [];
 const View = ShowHierarchy((...Arguments) => Requests.push(Arguments));
 FireEvent.click(Screen.getByTitle('Rename'));
 FireEvent.change(Screen.getByRole('textbox'),{target:{value:'  '}});
 FireEvent.blur(Screen.getByRole('textbox'));
 Assert.deepEqual(Requests,[]);
 View.unmount();
 ShowHierarchy(()=>{},true);
 Assert.equal(Screen.getByTitle('Rename').disabled,true);
});
Test('prefab button opens actions on demand for its own object', async () => {
 const Requests = [];
 Render(React.createElement(NativePrefabControls,{ObjectId:'cube',Command:async (...Arguments)=>{Requests.push(Arguments);return {instance:false};}}));
 Assert.equal(Screen.queryByRole('dialog'),null);
 Assert.equal(Requests.length,0);
 FireEvent.click(Screen.getByRole('button',{name:'Prefab'}));
 await WaitFor(()=>Assert.deepEqual(Requests,[['prefab.inspect',{objectId:'cube'}]]));
 Assert.ok(Screen.getByRole('dialog',{name:'Prefab'}));
 FireEvent.click(Screen.getByRole('button',{name:'Create prefab'}));
 await WaitFor(()=>Assert.equal(Requests.length,3));
 Assert.deepEqual(Requests[1],['prefab.create',{objectId:'cube',path:'Prefabs/NewPrefab.prefab'}]);
 FireEvent.keyDown(Screen.getByRole('dialog'),{key:'Escape'});
 Assert.equal(Screen.queryByRole('dialog'),null);
});
