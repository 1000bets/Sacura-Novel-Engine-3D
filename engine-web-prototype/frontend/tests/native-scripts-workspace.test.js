import Test,{after,afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const Browser=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for (const Name of ['window','document','navigator','HTMLElement','localStorage']) {
 Object.defineProperty(globalThis,Name,{configurable:true,value:Browser.window[Name]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render:Render,screen:Screen,fireEvent:FireEvent,cleanup:Cleanup,waitFor:WaitFor}=await import('@testing-library/react');
const Server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Workspace}=await Server.ssrLoadModule('/src/NativeScriptsWorkspace.jsx');
afterEach(Cleanup);
after(async()=>{await Server.close();Browser.window.close();});
Test('individual script files support path and content search and open their original asset',async()=>{
 const Opened=[];
 const Adapter={command:async(Method,Parameters)=>({source:{player:'def Move():\n    return Speed',story:'def Speak():\n    return Dialogue',engine:'Builtin = True'}[Parameters.assetId]})};
 const Assets=[{id:'player',typeId:'PythonScript',path:'/Game/Scripts/player.py'},{id:'story',typeId:'PythonScript',path:'/Game/Scripts/story.py'},{id:'engine',typeId:'PythonScript',path:'/Engine/builtin.py'},{id:'mesh',typeId:'Model',path:'/Game/mesh.glb'}];
 const View=Render(React.createElement(Workspace,{Assets,Adapter,Session:'test',OpenAsset:Identity=>Opened.push(Identity)}));
 await WaitFor(()=>Assert.equal(View.container.querySelectorAll('.native-script-file').length,3));
 const Search=View.container.querySelector('input[type=search]');
 FireEvent.change(Search,{target:{value:'PLAYER.py'}});
 Assert.equal(View.container.querySelectorAll('.native-script-file').length,1);
 FireEvent.change(Search,{target:{value:'dialogue'}});
 Assert.equal(View.container.querySelectorAll('.native-script-file').length,1);
 Assert.match(View.container.querySelector('.native-script-file').textContent,/story.py/);
 Assert.match(View.container.querySelector('.native-script-matches').textContent,/2.*Dialogue/);
 FireEvent.click(View.container.querySelector('.native-script-matches button'));
 Assert.deepEqual(Opened,[{assetId:'story'}]);
 FireEvent.change(Search,{target:{value:'not-found'}});
 Assert.match(Screen.getByRole('status').textContent,/No scripts|не найдены/);
 FireEvent.change(Search,{target:{value:''}});
 Assert.equal(View.container.querySelectorAll('.native-script-file').length,3);
});
Test('refresh updates content search and failed files leave readable files available',async()=>{
 let Source='Original';
 const Adapter={command:async(Method,Parameters)=>{
  if (Parameters.assetId==='missing') {
   throw Error('Script unavailable');
  }
  return {source:Source};
 }};
 const Assets=[{id:'good',typeId:'PythonScript',path:'/Game/Scripts/good.py'},{id:'missing',typeId:'PythonScript',path:'/Game/Scripts/missing.py'}];
 const View=Render(React.createElement(Workspace,{Assets,Adapter,Session:'test',OpenAsset:()=>{}}));
 await WaitFor(()=>Assert.match(Screen.getByRole('alert').textContent,/missing.py: Script unavailable/));
 Assert.equal(View.container.querySelectorAll('.native-script-file').length,1);
 Source='Updated';
 FireEvent.click(View.container.querySelector('header button'));
 await WaitFor(()=>Assert.equal(View.container.querySelector('header button').disabled,false));
 FireEvent.change(View.container.querySelector('input'),{target:{value:'Updated'}});
 Assert.match(View.container.querySelector('.native-script-matches').textContent,/Updated/);
});
Test('empty script catalogs show a useful empty state',async()=>{
 Render(React.createElement(Workspace,{Assets:[],Adapter:{command:async()=>{}},Session:'test',OpenAsset:()=>{}}));
 await WaitFor(()=>Assert.match(Screen.getByRole('status').textContent,/No Python|нет/));
});
