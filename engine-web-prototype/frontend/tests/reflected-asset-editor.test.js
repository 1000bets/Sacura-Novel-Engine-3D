import Test, {after, afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {ResolveAssetEditor, RegisterAssetEditor} from '../src/assetEditorRegistry.js';
const Dom = new JSDOM('<html><body></body></html>', {url:'http://localhost/'});
for (const Name of ['window','document','navigator','HTMLElement','localStorage']) {
  Object.defineProperty(globalThis, Name, {configurable:true, value:Dom.window[Name]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render:Render, screen:Screen, fireEvent:FireEvent, cleanup:Cleanup, waitFor:WaitFor} = await import('@testing-library/react');
const Server = await createServer({root:fileURLToPath(new URL('..', import.meta.url)), server:{middlewareMode:true, watch:null, hmr:false, ws:false}});
const {default:Editor} = await Server.ssrLoadModule('/src/NativeReflectedAssetEditor.jsx');
const {setLanguage:SetLanguage} = await Server.ssrLoadModule('/src/i18n.js');
SetLanguage('en');
afterEach(Cleanup);
after(async () => {await Server.close(); Dom.window.close();});
const Field = (Id, Type, Extra = {}) => ({id:Id,label:Id,typeId:Type,kind:'value',editable:true,...Extra});
const Schema = {typeId:'test.Settings',kind:'struct',fields:[Field('Name','engine.string'),Field('Enabled','engine.bool'),Field('Mode','test.Mode',{enumValues:['Pressed','Hold']}),Field('Bindings','engine.Array<test.Binding>',{kind:'array',fixedSize:0,elementDefault:{Control:''},element:{kind:'struct',typeId:'test.Binding',fields:[Field('Control','engine.string')]}})]};
const Snapshot = {type:'UnregisteredType',schema:Schema,document:{Name:'Original',Enabled:true,Mode:'Pressed',Bindings:[{Control:'KeyA'},{Control:'KeyB'}]},revision:1,readOnly:false,dirty:true,canUndo:true,canRedo:false};
Test('specialized presentation wins and unregistered types use reflected properties', () => {
  Assert.equal(ResolveAssetEditor('Material'),'material');
  Assert.equal(ResolveAssetEditor('InputAction'),'properties');
  Assert.equal(ResolveAssetEditor('UnregisteredType'),'properties');
  const CustomEditor = () => null;
  RegisterAssetEditor('CustomType', CustomEditor);
  Assert.equal(ResolveAssetEditor('CustomType'), CustomEditor);
});
Test('generic fields and collection operations use document commands', async () => {
  const Calls=[];
  Render(React.createElement(Editor,{Snapshot,Asset:{name:'Settings'},Execute:async (...Arguments)=>{Calls.push(Arguments);return Snapshot;}}));
  FireEvent.change(Screen.getByLabelText('Name'),{target:{value:'Changed'}});
  FireEvent.blur(Screen.getByLabelText('Name'));
  FireEvent.click(Screen.getByLabelText('Enabled'));
  FireEvent.change(Screen.getByLabelText('Mode'),{target:{value:'Hold'}});
  Assert.deepEqual(Calls.slice(0,3),[['property',{path:['Name'],operation:'set',value:'Changed'}],['property',{path:['Enabled'],operation:'set',value:false}],['property',{path:['Mode'],operation:'set',value:'Hold'}]]);
  FireEvent.click(Screen.getAllByRole('button',{name:'Move down',hidden:true})[0]);
  FireEvent.click(Screen.getAllByRole('button',{name:'Remove element',hidden:true})[1]);
  Assert.deepEqual(Calls.slice(-2),[['property',{path:['Bindings'],operation:'move',index:0,destination:1}],['property',{path:['Bindings'],operation:'remove',index:1}]]);
  FireEvent.click(Screen.getByRole('button',{name:'Add element',hidden:true}));
  const Control=Screen.getAllByLabelText('Control').at(-1);
  FireEvent.change(Control,{target:{value:'KeyC'}});FireEvent.blur(Control);
  FireEvent.click(Screen.getByRole('button',{name:'Add',exact:true,hidden:true}));
  await WaitFor(()=>Assert.equal(Screen.queryByText('New element'),null));
  Assert.deepEqual(Calls.at(-1),['property',{path:['Bindings'],operation:'insert',value:{Control:'KeyC'}}]);
});
Test('read-only and unavailable documents remain inspectable', () => {
  const View=Render(React.createElement(Editor,{Snapshot:{...Snapshot,readOnly:true},Execute:()=>{}}));
  Assert.ok(Screen.getByLabelText('Name').matches(':disabled'));
  Assert.ok(Screen.getByRole('button',{name:'Add element',hidden:true}).disabled);
  View.rerender(React.createElement(Editor,{Snapshot:{...Snapshot,schema:null,readOnly:true},Asset:{path:'/Game/Unknown.asset'},Execute:()=>{}}));
  Assert.ok(Screen.getByText('No reflected document is registered for this asset type.'));
});
