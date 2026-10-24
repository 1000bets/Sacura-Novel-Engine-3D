import Test, {after, afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const Dom = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'});
for (const Key of ['window','document','navigator','HTMLElement','MutationObserver']) {
  Object.defineProperty(globalThis, Key, {configurable:true,value:Dom.window[Key]});
}
const Popups = [];
Dom.window.open = () => {
  const Popup = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'}).window;
  Popup.focus = () => {};
  Popups.push(Popup);
  return Popup;
};
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render, screen, fireEvent, cleanup, within, act} = await import('@testing-library/react');
const Server = await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const Model = await Server.ssrLoadModule('/src/editorWindows.js');
const {setLanguage} = await Server.ssrLoadModule('/src/i18n.js');
setLanguage('ru');
const {default:Workspace, EditorWindowsMenu:Menu, EditorDockArea} = await Server.ssrLoadModule('/src/EditorWorkspace.jsx');
const {EditorDockProvider, DockPanelMount} = await Server.ssrLoadModule('/src/EditorDocking.jsx');
const {default:ProjectSettings} = await Server.ssrLoadModule('/src/ProjectSettings.jsx');
let State, Change, Mounts;
function Draft({identifier:Identifier}) {
  const [Text, SetText] = React.useState('');
  React.useEffect(() => {Mounts[Identifier] = (Mounts[Identifier] || 0) + 1;}, []);
  return React.createElement('input', {'aria-label':'Draft '+Identifier,value:Text,onChange:Event => SetText(Event.target.value)});
}
function Harness() {
  const [Windows, SetWindows] = React.useState(Model.defaultWindows());
  State = Windows;
  Change = SetWindows;
  const Select = Identifier => SetWindows(Current => Model.openWindow(Current,Identifier));
  const Close = Identifier => SetWindows(Current => Model.closeWindow(Current,Identifier));
  return React.createElement(EditorDockProvider,{windows:Windows,onChange:SetWindows,onSelect:Select,onClose:Close},
    React.createElement(Menu,{windows:Windows,onSelect:Select,onClose:Close,onReset:()=>SetWindows(Model.defaultWindows())}),
    ['scene','hierarchy','inspector'].map(Area => React.createElement(EditorDockArea,{key:Area,area:Area})),
    Object.values(Model.BaseTabs).map(Identifier => React.createElement(DockPanelMount,{key:Identifier,id:Identifier},React.createElement(Draft,{identifier:Identifier}))),
    React.createElement(Workspace,{renderPanel:Identifier => React.createElement(Draft,{identifier:Identifier})}));
}
function Show() {
  Mounts = {};
  return render(React.createElement(Harness));
}
Test('restored project settings wait for connection and reload for a new session', async () => {
  let Reads = 0;
  const Adapter = {command:async () => ({name:'Project '+(++Reads),engineVersion:'0.1',startupScene:'',startupStory:''})};
  const View = render(React.createElement(ProjectSettings,{Adapter,Connected:false,Session:null}));
  Assert.equal(Reads,0);
  View.rerender(React.createElement(ProjectSettings,{Adapter,Connected:true,Session:'first'}));
  Assert.equal((await screen.findByLabelText('Название проекта')).value,'Project 1');
  View.rerender(React.createElement(ProjectSettings,{Adapter,Connected:true,Session:'second'}));
  Assert.equal((await screen.findByLabelText('Название проекта')).value,'Project 2');
});
afterEach(() => {cleanup();for (const Popup of Popups.splice(0)) {Popup.close();}});
after(async () => {await Server.close();Dom.window.close();});
Test('base panels and tools are tabs; selection and reorder stay in their group', async () => {
  Show();
  fireEvent.click(screen.getByRole('menuitemcheckbox',{name:'Проводник',exact:true}));
  Assert.equal(screen.getByRole('tab',{name:'Проводник'}).getAttribute('aria-selected'),'true');
  fireEvent.keyDown(screen.getByRole('tab',{name:'Проводник'}),{key:'ArrowLeft',altKey:true});
  Assert.deepEqual(Model.dockTabs(State),['files','story']);
  Assert.equal(Model.activeDockWindow(State,'scene'),'scene-view');
});
Test('draft DOM and component state survive detach, grouping and return', async () => {
  Show();
  fireEvent.click(screen.getByRole('menuitemcheckbox',{name:'Проводник',exact:true}));
  const DraftElement = screen.getByRole('textbox',{name:'Draft files'});
  fireEvent.change(DraftElement,{target:{value:'unsaved query'}});
  await act(async () => {Change(Current => Model.floatWindow(Current,'files'));});
  const Area = Model.windowArea(State,'files');
  const Popup = Popups[0];
  Assert.equal(within(Popup.document.body).getByRole('textbox',{name:'Draft files'}),DraftElement);
  Assert.equal(DraftElement.value,'unsaved query');
  await act(async () => {Change(Current => Model.dockWindow(Current,'hierarchy-view',null,Area));});
  Assert.equal(within(Popup.document.body).getAllByRole('tab').length,2);
  await act(async () => {Change(Current => Model.dockWindow(Current,'files',null,'workspace'));});
  Assert.equal(screen.getByRole('textbox',{name:'Draft files'}),DraftElement);
  Assert.equal(Mounts.files,1);
});
Test('settings use ordinary tabs and preserve draft state through detach and docking', async () => {
  Show();
  for (const [Identifier, Caption] of [['project-settings','Настройки проекта'],['editor-settings','Настройки редактора']]) {
    fireEvent.click(screen.getByRole('menuitemcheckbox',{name:Caption,exact:true}));
    const Field = screen.getByRole('textbox',{name:'Draft '+Identifier});
    fireEvent.change(Field,{target:{value:'Pending settings'}});
    await act(async () => {Change(Current => Model.floatWindow(Current,Identifier));});
    Assert.equal(Field.ownerDocument,Popups.at(-1).document);
    Assert.equal(Field.value,'Pending settings');
    await act(async () => {Change(Current => Model.dockWindow(Current,Identifier,null,'workspace'));});
    Assert.equal(screen.getByRole('textbox',{name:'Draft '+Identifier}),Field);
    Assert.equal(Mounts[Identifier],1);
    await act(async () => {Change(Current => Model.closeWindow(Current,Identifier));});
    Assert.equal(Model.isWindowOpen(State,Identifier),false);
  }
});
Test('split groups keep both drafts mounted and close without closing a sibling', async () => {
  Show();
  fireEvent.click(screen.getByRole('menuitemcheckbox',{name:'Проводник',exact:true}));
  const Original = screen.getByRole('textbox',{name:'Draft files'});
  await act(async () => {Change(Current => Model.SplitDockGroup(Current,'files','workspace','right'));});
  Assert.equal(screen.getByRole('textbox',{name:'Draft files'}),Original);
  Assert.equal(Mounts.files,1);
  await act(async () => {Change(Current => Model.closeWindow(Current,'files'));});
  Assert.ok(screen.getByRole('tab',{name:'Поток истории'}));
});
Test('pointer drop on portaled content docks the tab; Escape cancels and outside release detaches', async () => {
  Show();
  fireEvent.click(screen.getByRole('menuitemcheckbox',{name:'Проводник',exact:true}));
  const DraftElement = screen.getByRole('textbox',{name:'Draft files'});
  let Tab = screen.getByRole('tab',{name:'Проводник'});
  Assert.equal(Tab.draggable,false);
  let Captured = false;
  Tab.setPointerCapture = () => {Captured = true;};
  Tab.hasPointerCapture = () => Captured;
  Tab.releasePointerCapture = () => {Captured = false;};
  const Destination = screen.getByRole('textbox',{name:'Draft scene-view'});
  const Group = Destination.closest('[data-dock-root]');
  Group.getBoundingClientRect = () => ({left:0,top:0,right:600,bottom:400,width:600,height:400});
  document.elementFromPoint = () => Destination;
  function Pointer(Type, Horizontal) {
    const Event = new window.MouseEvent(Type,{bubbles:true,cancelable:true,button:0,clientX:Horizontal,clientY:100,screenX:Horizontal,screenY:100});
    Object.defineProperty(Event,'pointerId',{value:1});
    fireEvent(Tab,Event);
  }
  Pointer('pointerdown',20);
  Pointer('pointermove',200);
  Assert.ok(Group.querySelector('.workspace-drop-preview'));
  Pointer('pointerup',200);
  Assert.equal(Model.windowArea(State,'files'),'scene');
  Assert.equal(screen.getByRole('textbox',{name:'Draft files'}),DraftElement);
  Tab = screen.getByRole('tab',{name:'Проводник'});
  Tab.setPointerCapture = () => {Captured = true;};
  Tab.hasPointerCapture = () => Captured;
  Tab.releasePointerCapture = () => {Captured = false;};
  document.elementFromPoint = () => null;
  Pointer('pointerdown',20);
  Pointer('pointermove',-100);
  fireEvent.keyDown(document,{key:'Escape'});
  Pointer('pointerup',-100);
  Assert.equal(Model.windowArea(State,'files'),'scene');
  Pointer('pointerdown',20);
  Pointer('pointermove',-100);
  Pointer('pointerup',-100);
  Assert.match(Model.windowArea(State,'files'),/^window-/);
  Assert.equal(Mounts.files,1);
  delete document.elementFromPoint;
});
