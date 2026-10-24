import Test from 'node:test';
import Assert from 'node:assert/strict';
import {EDITOR_WINDOWS, defaultWindows, restoreWindows, openWindow, closeWindow, floatWindow, dockWindow, moveWindowTab, updateWindowRect, dockTabs, windowArea, activeDockWindow, selectDockBase, SplitDockGroup, RegisterEditorTab} from '../src/editorWindows.js';

Test('all tools and base panels use the same tab lifecycle', () => {
  let State = defaultWindows();
  for (const [Identifier] of EDITOR_WINDOWS) {
    State = openWindow(openWindow(State, Identifier), Identifier);
  }
  Assert.equal(State.tabs.length, EDITOR_WINDOWS.length);
  for (const [Identifier] of EDITOR_WINDOWS) {
    State = closeWindow(State, Identifier);
  }
  Assert.deepEqual(State.tabs, []);
  Assert.equal(State.focused, null);
  Assert.equal(State.active, null);
  Assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(State))), State);
  Assert.equal(openWindow(State, 'unknown'), State);
});
Test('detached windows contain several tabs and retain independent geometry', () => {
  let State = floatWindow(defaultWindows(), 'files');
  const First = windowArea(State, 'files');
  State = dockWindow(State, 'hierarchy-view', null, First);
  State = floatWindow(State, 'inspector-view');
  const Second = windowArea(State, 'inspector-view');
  State = updateWindowRect(State, First, {x:30,y:50,width:700,height:500});
  Assert.deepEqual(dockTabs(State, First), ['files','hierarchy-view']);
  Assert.notEqual(State.containers[First].x, State.containers[Second].x);
  Assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(State))), State);
  State = dockWindow(State, 'files', null, Second);
  Assert.deepEqual(dockTabs(State, First), ['hierarchy-view']);
  Assert.deepEqual(dockTabs(State, Second), ['inspector-view','files']);
  State = dockWindow(State, 'files', 'story');
  Assert.deepEqual(dockTabs(State), ['files','story']);
});
Test('document identity survives split, transfer and layout serialization', () => {
  const Identifier = 'asset:material:test';
  const Descriptor = {id:Identifier,label:'Material',identity:{assetId:'material'},project:'M:/Game/Game.project'};
  RegisterEditorTab(Identifier, Descriptor);
  let State = openWindow({...defaultWindows(),documents:{[Identifier]:Descriptor}}, Identifier);
  State = SplitDockGroup(State, Identifier, 'workspace', 'right');
  const Group = windowArea(State, Identifier);
  Assert.equal(State.containers[Group].parent, 'workspace');
  State = dockWindow(State, 'files', null, Group);
  State = closeWindow(State, Identifier);
  Assert.equal(activeDockWindow(State, Group), 'files');
  State = openWindow(State, Identifier);
  Assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(State))), State);
  Assert.deepEqual(State.documents[Identifier], Descriptor);
});
Test('tab order and activation are scoped to each group', () => {
  let State = openWindow(openWindow(defaultWindows(), 'files'), 'sound');
  State = moveWindowTab(State, 'sound', -1);
  Assert.deepEqual(dockTabs(State), ['story','sound','files']);
  State = dockWindow(State, 'files', null, 'hierarchy');
  Assert.equal(activeDockWindow(State, 'hierarchy'), 'files');
  State = selectDockBase(State, 'hierarchy');
  Assert.equal(activeDockWindow(State, 'hierarchy'), 'hierarchy-view');
  Assert.equal(dockWindow(State, 'files', null, 'unknown'), State);
});
Test('legacy layouts migrate and corrupt container graphs cannot recurse', () => {
  const State = restoreWindows({tabs:['story','unknown'],floating:{files:{}},selected:{scene:null},position:'bad',width:Infinity});
  Assert.ok(State.tabs.includes('scene-view'));
  Assert.ok(State.tabs.includes('files'));
  Assert.ok(!State.tabs.includes('unknown'));
  Assert.equal(State.position, 'bottom');
  Assert.equal(State.width, 480);
  const Cyclic = restoreWindows({...defaultWindows(),placements:{story:'group-a'},containers:{'group-a':{kind:'group',parent:'group-b'},'group-b':{kind:'group',parent:'group-a'}}});
  Assert.equal(windowArea(Cyclic,'story'),'workspace');
  Assert.deepEqual(Cyclic.containers,{});
});

Test('removed server projects, events and gameplay panels cannot open or return from saved layouts', () => {
  const State = defaultWindows();
  for (const Identifier of ['server-projects', 'event', 'gameplay']) {
    Assert.equal(EDITOR_WINDOWS.some(([Name]) => Name === Identifier), false);
    Assert.equal(openWindow(State, Identifier), State);
  }
  const Restored = restoreWindows({...State, tabs:[...State.tabs, 'server-projects', 'event', 'gameplay'], floating:{'server-projects':{}, event:{}, gameplay:{}}});
  Assert.deepEqual(Restored.tabs, State.tabs);
  Assert.equal(activeDockWindow(Restored), 'story');
});
