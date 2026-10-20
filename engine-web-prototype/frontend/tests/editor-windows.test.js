import test from 'node:test';
import assert from 'node:assert/strict';
import {EDITOR_WINDOWS, defaultWindows, restoreWindows, openWindow, closeWindow, floatWindow, dockWindow, moveWindowTab, updateWindowRect, clampWindowRect, isWindowOpen, dockTabs, windowArea, activeDockWindow, selectDockBase} from '../src/editorWindows.js';

test('every tool can be opened, closed and reopened without duplicates or a mandatory tab', () => {
  let state = defaultWindows();
  for (const [id] of EDITOR_WINDOWS) {state = openWindow(state, id); state = openWindow(state, id);}
  assert.equal(state.tabs.length, EDITOR_WINDOWS.length);
  for (const [id] of EDITOR_WINDOWS) state = closeWindow(state, id);
  assert.deepEqual(state.tabs, []); assert.equal(state.active, null); assert.equal(state.focused, null);
  assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(state))), state);
  state = openWindow(state, 'timeline'); assert.deepEqual(state.tabs, ['story']);
  assert.equal(openWindow(state, 'unknown'), state);
});

test('two floating windows have independent geometry and survive a JSON round trip', () => {
  const viewport = {width: 1440, height: 900};
  let state = openWindow(defaultWindows(), 'widgets');
  state = floatWindow(state, 'widgets', viewport); state = floatWindow(state, 'sound', viewport);
  const soundRect = structuredClone(state.floating.sound);
  state = updateWindowRect(state, 'widgets', {x: 30, y: 50, width: 600, height: 400}, viewport);
  state = {...state, position: 'right', width: 390};
  assert.deepEqual(state.floating.sound, soundRect); assert.deepEqual(state.tabs, ['story']);
  assert.equal(state.active, 'story'); assert.equal(isWindowOpen(state, 'widgets'), true);
  assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(state))), state);
  const refocused = openWindow(state, 'widgets');
  assert.equal(Object.keys(refocused.floating).at(-1), 'widgets'); assert.deepEqual(refocused.floating.widgets, state.floating.widgets);
  const docked = dockWindow(refocused, 'widgets', 'story');
  assert.deepEqual(docked.tabs, ['widgets', 'story']); assert.equal(isWindowOpen(docked, 'sound'), true);
  assert.equal(docked.floating.widgets, undefined); assert.equal(docked.active, 'widgets');
});

test('closing the active tab selects a neighbour; reordering and closing floats do not disturb other windows', () => {
  let state = openWindow(openWindow(defaultWindows(), 'input'), 'files');
  state = moveWindowTab(state, 'files', -1); assert.deepEqual(state.tabs, ['story', 'files', 'input']);
  assert.equal(moveWindowTab(state, 'story', -1), state);
  state = closeWindow(state, 'files'); assert.equal(state.active, 'input');
  state = floatWindow(state, 'sound'); state = closeWindow(state, 'sound');
  assert.deepEqual(state.tabs, ['story', 'input']); assert.equal(state.focused, 'input');
  assert.equal(closeWindow(state, 'missing'), state);
});

test('restoring old, malformed and oversized layouts leaves controls reachable', () => {
  assert.deepEqual(restoreWindows(undefined, 'gameplay'), defaultWindows('gameplay'));
  assert.deepEqual(restoreWindows({tabs: 'invalid'}), defaultWindows());
  const state = restoreWindows({tabs: ['sound', 'sound', 'unknown', 'widgets'], active: 'unknown', focused: 'unknown', floating: {widgets: {x: -5, y: 99999, width: -10, height: 99999}, bad: {}}, position: 'bad', width: Infinity});
  assert.deepEqual(state.tabs, ['sound']); assert.equal(state.active, 'sound'); assert.equal(state.position, 'bottom');
  assert.deepEqual(Object.keys(state.floating), ['widgets']);
  const rect = clampWindowRect(state.floating.widgets, {width: 800, height: 500});
  assert.ok(rect.x >= 0 && rect.y >= 0); assert.ok(rect.x + rect.width <= 800); assert.ok(rect.y + rect.height <= 500);
  assert.equal(floatWindow(state, 'unknown'), state); assert.equal(dockWindow(state, 'unknown'), state);
});

test('moving individual tools creates independent tab groups and keeps a single local copy', () => {
  let state = openWindow(openWindow(openWindow(defaultWindows(), 'input'), 'files'), 'widgets');
  state = dockWindow(state, 'input', undefined, 'scene');
  state = dockWindow(state, 'files', undefined, 'inspector');
  assert.deepEqual(dockTabs(state), ['story', 'widgets']);
  assert.deepEqual(dockTabs(state, 'scene'), ['input']);
  assert.equal(activeDockWindow(state), 'widgets');
  assert.equal(activeDockWindow(state, 'scene'), 'input');
  assert.equal(activeDockWindow(state, 'inspector'), 'files');
  state = dockWindow(state, 'widgets', 'input', 'scene');
  assert.deepEqual(dockTabs(state, 'scene'), ['widgets', 'input']);
  assert.equal(activeDockWindow(state), 'story');
  state = moveWindowTab(state, 'widgets', 1);
  assert.deepEqual(dockTabs(state, 'scene'), ['input', 'widgets']);
  assert.deepEqual(dockTabs(state, 'inspector'), ['files']);
  assert.equal(new Set(state.tabs).size, state.tabs.length);
  assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(state))), state);
});

test('native tabs, closing, reopening and floating retain the chosen dock area', () => {
  let state = dockWindow(defaultWindows(), 'input', undefined, 'scene');
  state = selectDockBase(state, 'scene');
  assert.equal(activeDockWindow(state, 'scene'), null);
  assert.deepEqual(restoreWindows(JSON.parse(JSON.stringify(state))), state);
  state = openWindow(state, 'input'); assert.equal(activeDockWindow(state, 'scene'), 'input');
  state = dockWindow(state, 'widgets', undefined, 'scene');
  state = closeWindow(state, 'widgets'); assert.equal(activeDockWindow(state, 'scene'), 'input');
  state = floatWindow(state, 'input');
  assert.equal(activeDockWindow(state, 'scene'), null); assert.equal(windowArea(state, 'input'), 'scene');
  state = dockWindow(state, 'input', undefined, windowArea(state, 'input'));
  assert.deepEqual(dockTabs(state), ['story']); assert.equal(activeDockWindow(state, 'scene'), 'input');
  state = closeWindow(state, 'input'); state = openWindow(state, 'input');
  assert.equal(windowArea(state, 'input'), 'scene'); assert.equal(activeDockWindow(state, 'scene'), 'input');
  assert.equal(dockWindow(state, 'input', undefined, 'unknown'), state);
});

test('legacy layouts migrate to the workspace and unavailable dock locations are ignored', () => {
  const old = {tabs: ['story', 'input'], active: 'input', floating: {}, position: 'bottom', width: 480};
  const migrated = restoreWindows(old);
  assert.deepEqual(dockTabs(migrated), old.tabs); assert.equal(activeDockWindow(migrated), 'input');
  const invalid = restoreWindows({...old, placements: {input: 'missing', unknown: 'scene'}, selected: {scene: 'unknown'}});
  assert.deepEqual(dockTabs(invalid), old.tabs); assert.equal(activeDockWindow(invalid, 'scene'), null);
});
