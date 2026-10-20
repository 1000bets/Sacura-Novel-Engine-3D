import test, {after, afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {defaultWindows, restoreWindows, openWindow, closeWindow, selectDockBase, activeDockWindow} from '../src/editorWindows.js';

const dom = new JSDOM('<!doctype html><html><body><button data-windows-menu>Окна</button></body></html>', {url: 'http://localhost/'});
dom.window.PointerEvent = dom.window.MouseEvent;
dom.window.HTMLElement.prototype.setPointerCapture = () => {};
for (const key of ['window', 'document', 'navigator', 'HTMLElement', 'localStorage']) Object.defineProperty(globalThis, key, {configurable: true, value: dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render, screen, fireEvent, cleanup, within, act} = await import('@testing-library/react');
const server = await createServer({root: fileURLToPath(new URL('..', import.meta.url)), server: {middlewareMode: true, watch: null, hmr: false, ws: false}});
const {default: Workspace, EditorWindowsMenu: Menu, EditorDockTabs} = await server.ssrLoadModule('/src/EditorWorkspace.jsx');
const {EditorDockProvider, EditorDockStrip, EditorDockBody, DockDropTarget} = await server.ssrLoadModule('/src/EditorDocking.jsx');
let value, mounts;
function Draft({id}) {
  const [text, setText] = React.useState('');
  React.useEffect(() => {mounts[id] = (mounts[id] || 0) + 1;}, []);
  return React.createElement('input', {'aria-label': 'Черновик ' + id, value: text, onChange: e => setText(e.target.value)});
}
function Harness({regions = false}) {
  const [windows, setWindows] = React.useState(() => restoreWindows(JSON.parse(localStorage.getItem('test-layout') || 'null')));
  value = windows;
  React.useEffect(() => {localStorage.setItem('test-layout', JSON.stringify(windows));}, [windows]);
  const onSelect = id => setWindows(s => openWindow(s, id));
  const content = React.createElement(React.Fragment, null,
    React.createElement(Menu, {windows, onChange: setWindows, onSelect, eventAvailable: false, issueCount: 2, onReset: () => setWindows(defaultWindows())}),
    React.createElement(Workspace, {windows, onChange: setWindows, onSelect, onClose: id => setWindows(s => closeWindow(s, id)), renderPanel: id => React.createElement(Draft, {id})}),
  );
  if (!regions) return content;
  const region = area => React.createElement('section', {key: area, 'aria-label': 'Область ' + area},
      React.createElement(EditorDockStrip, {area}, React.createElement('button', {'data-dock-base': area, onClick: () => setWindows(s => selectDockBase(s, area))}, 'Основа ' + area), React.createElement(EditorDockTabs, {area})),
      React.createElement(EditorDockBody, {area}), React.createElement(DockDropTarget, {area}));
  return React.createElement(EditorDockProvider, {windows, onChange: setWindows, onSelect, onClose: id => setWindows(s => closeWindow(s, id))},
    region('scene'), region('hierarchy'), content, region('inspector'));
}
function show(regions = false) {mounts = {}; return render(React.createElement(Harness, {regions}));}
function open(label) {fireEvent.click(screen.getByRole('menuitemcheckbox', {name: label, exact: true}));}
function detach() {fireEvent.click(screen.getByRole('button', {name: 'Открепить окно'}));}
afterEach(() => {cleanup(); localStorage.clear();});
after(async () => {await server.close(); dom.window.close();});

test('Windows toggles real tools, shows checked state, and can close the last tab', async () => {
  show(); assert.equal(screen.queryByRole('button', {name: 'Ещё'}), null);
  assert.equal(screen.queryByRole('button', {name: 'Настройки вкладки'}), null);
  assert.deepEqual(screen.getAllByRole('tab').map(el => el.textContent), ['Поток истории']);
  assert.equal(screen.getByRole('menuitemcheckbox', {name: 'Событие', exact: true}).disabled, true);
  open('Ввод'); open('Звук'); assert.equal(screen.getAllByRole('tab').length, 3);
  assert.equal(screen.getByRole('menuitemcheckbox', {name: 'Звук', exact: true}).getAttribute('aria-checked'), 'true');
  fireEvent.click(screen.getByRole('button', {name: 'Закрыть окно «Звук»'}));
  await act(async () => {}); assert.equal(screen.getByRole('tab', {name: 'Ввод'}).getAttribute('aria-selected'), 'true');
  open('Ввод'); open('Поток истории'); assert.equal(screen.queryByRole('tab'), null);
  open('Проводник'); assert.equal(screen.getByRole('tab', {name: 'Проводник'}).getAttribute('aria-selected'), 'true');
});

test('switching, hiding, floating and redocking preserve the same unsaved draft', async () => {
  show(); open('Создание');
  fireEvent.change(screen.getByRole('textbox', {name: 'Черновик create'}), {target: {value: 'Несохранённое событие'}});
  fireEvent.click(screen.getByRole('tab', {name: 'Поток истории'}));
  fireEvent.click(screen.getByRole('tab', {name: 'Создание'})); detach();
  const floating = screen.getByRole('dialog', {name: 'Создание'});
  assert.equal(within(floating).getByRole('textbox').value, 'Несохранённое событие'); assert.equal(mounts.create, 1);
  const before = structuredClone(value.floating.create);
  fireEvent.keyDown(within(floating).getByRole('banner'), {key: 'ArrowRight'});
  assert.equal(value.floating.create.x, before.x + 16);
  fireEvent.keyDown(within(floating).getByRole('button', {name: 'Изменить размер окна «Создание»'}), {key: 'ArrowLeft'});
  assert.equal(value.floating.create.width, before.width - 16);
  fireEvent.click(within(floating).getByRole('button', {name: 'Закрепить во вкладках'}));
  assert.equal(screen.queryByRole('dialog', {name: 'Создание'}), null); assert.equal(screen.getByRole('textbox', {name: 'Черновик create'}).value, 'Несохранённое событие');
  open('Создание'); open('Создание'); await act(async () => {});
  assert.equal(screen.getByRole('textbox', {name: 'Черновик create'}).value, 'Несохранённое событие'); assert.equal(mounts.create, 1);
});

test('independent floating windows, keyboard reorder, saved positions and reset', async () => {
  const rendered = show(); open('Ввод'); open('Звук');
  fireEvent.keyDown(screen.getByRole('tab', {name: 'Звук'}), {key: 'ArrowLeft', altKey: true});
  assert.deepEqual(value.tabs, ['story', 'sound', 'input']);
  detach(); open('Виджеты UI'); detach();
  assert.equal(screen.getAllByRole('dialog').length, 2); assert.deepEqual(value.tabs, ['story', 'input']);
  fireEvent.change(screen.getByRole('combobox', {name: 'Расположение рабочей области'}), {target: {value: 'right'}});
  const saved = JSON.parse(JSON.stringify(value)); rendered.unmount();
  render(React.createElement(Harness)); assert.deepEqual(value, saved); assert.equal(screen.getAllByRole('dialog').length, 2);
  fireEvent.click(screen.getByRole('menuitem', {name: 'Восстановить раскладку'})); await act(async () => {});
  assert.equal(screen.queryByRole('dialog'), null); assert.deepEqual(value, defaultWindows());
});

test('menu search and keyboard navigation work without dragging', () => {
  show(); const search = screen.getByRole('searchbox', {name: 'Найти окно'});
  assert.equal(document.activeElement, search);
  fireEvent.change(search, {target: {value: 'камер'}}); assert.equal(screen.getAllByRole('menuitemcheckbox').length, 1);
  fireEvent.keyDown(search, {key: 'ArrowDown'}); assert.equal(document.activeElement, screen.getByRole('menuitemcheckbox', {name: 'Камеры'}));
  fireEvent.click(document.activeElement); assert.equal(screen.getByRole('tab', {name: 'Камеры'}).getAttribute('aria-selected'), 'true');
});

function transfer() {
  const data = new Map();
  return {get types() {return [...data.keys()];}, setData: (type, value) => data.set(type, value), getData: type => data.get(type) || '', effectAllowed: '', dropEffect: ''};
}

test('drag/drop moves a tool between dock areas without remounting its draft', () => {
  show(true); open('Ввод');
  const draft = screen.getByRole('textbox', {name: 'Черновик input'});
  fireEvent.change(draft, {target: {value: 'Мои привязки'}});
  const dataTransfer = transfer(); fireEvent.dragStart(screen.getByRole('tab', {name: 'Ввод'}), {dataTransfer});
  const target = screen.getByRole('region', {name: 'Закрепить в области «Локация и игра»'});
  assert.equal(fireEvent.dragOver(target, {dataTransfer}), false); fireEvent.drop(target, {dataTransfer});
  const scene = screen.getByRole('region', {name: 'Область scene'});
  assert.equal(within(scene).getByRole('tab', {name: 'Ввод'}).getAttribute('aria-selected'), 'true');
  assert.equal(screen.getByRole('tab', {name: 'Поток истории'}).getAttribute('aria-selected'), 'true');
  assert.equal(within(scene).getByRole('textbox'), draft); assert.equal(draft.value, 'Мои привязки'); assert.equal(mounts.input, 1);
  fireEvent.click(screen.getByRole('button', {name: 'Основа scene'}));
  assert.equal(screen.queryByRole('textbox', {name: 'Черновик input'}), null); assert.equal(activeDockWindow(value, 'scene'), null);
  fireEvent.click(screen.getByRole('tab', {name: 'Ввод'}));
  const again = transfer(); fireEvent.dragStart(screen.getByRole('tab', {name: 'Ввод'}), {dataTransfer: again});
  fireEvent.drop(screen.getByRole('region', {name: 'Закрепить в области «Инспектор»'}), {dataTransfer: again});
  assert.equal(within(screen.getByRole('region', {name: 'Область inspector'})).getByRole('textbox'), draft);
  assert.equal(mounts.input, 1); assert.equal(screen.getAllByRole('tab', {name: 'Ввод'}).length, 1);
});

test('tab menus move between areas with the keyboard and layouts survive reload', () => {
  const rendered = show(true); open('Создание');
  fireEvent.keyDown(screen.getByRole('tab', {name: 'Создание'}), {key: 'F10', shiftKey: true});
  fireEvent.click(screen.getByRole('menuitem', {name: 'Перенести в область «Иерархия»'}));
  const hierarchy = screen.getByRole('region', {name: 'Область hierarchy'});
  assert.ok(within(hierarchy).getByRole('tab', {name: 'Создание'}));
  fireEvent.click(within(hierarchy).getByRole('button', {name: 'Открепить окно'}));
  fireEvent.click(within(screen.getByRole('dialog', {name: 'Создание'})).getByRole('button', {name: 'Закрепить во вкладках'}));
  assert.ok(within(hierarchy).getByRole('textbox', {name: 'Черновик create'}));
  const saved = JSON.parse(JSON.stringify(value)); rendered.unmount(); render(React.createElement(Harness, {regions: true}));
  assert.deepEqual(value, saved); assert.ok(within(screen.getByRole('region', {name: 'Область hierarchy'})).getByRole('tab', {name: 'Создание'}));
});

test('canceling a drag or dropping unrelated payloads leaves the layout and draft intact', () => {
  show(true); open('Ввод'); const before = JSON.stringify(value), dataTransfer = transfer();
  fireEvent.dragStart(screen.getByRole('tab', {name: 'Ввод'}), {dataTransfer});
  fireEvent.keyDown(window, {key: 'Escape'});
  assert.equal(screen.queryByRole('region', {name: 'Закрепить в области «Локация и игра»'}), null); assert.equal(JSON.stringify(value), before);
  const foreign = transfer(); foreign.setData('application/sacura-asset', '{"kind":"objects","id":"input"}');
  fireEvent.drop(screen.getByRole('button', {name: 'Основа scene'}), {dataTransfer: foreign}); assert.equal(JSON.stringify(value), before);
});

test('pointer capture transfers the same editor to a later DOM region and Escape cancels until release', async () => {
  show(true); open('Ввод');
  const editor = screen.getByRole('textbox', {name: 'Черновик input'}), source = screen.getByRole('tab', {name: 'Ввод'});
  const target = screen.getByRole('button', {name: 'Основа inspector'}), previousHitTest = document.elementFromPoint;
  document.elementFromPoint = () => target;
  try {
    fireEvent.pointerDown(source, {button: 0, clientX: 20, clientY: 400});
    fireEvent.pointerMove(source, {clientX: 600, clientY: 60});
    assert.ok(screen.getByRole('region', {name: 'Закрепить в области «Инспектор»'}));
    fireEvent.pointerUp(source, {clientX: 600, clientY: 60});
    await act(async () => {});
    const inspector = screen.getByRole('region', {name: 'Область inspector'});
    assert.equal(within(inspector).getByRole('textbox'), editor); assert.equal(mounts.input, 1);
    const relocated = within(inspector).getByRole('tab', {name: 'Ввод'}), before = JSON.stringify(value);
    document.elementFromPoint = () => screen.getByRole('button', {name: 'Основа scene'});
    fireEvent.pointerDown(relocated, {button: 0, clientX: 600, clientY: 60});
    fireEvent.pointerMove(relocated, {clientX: 100, clientY: 60});
    fireEvent.keyDown(window, {key: 'Escape'});
    fireEvent.pointerMove(relocated, {clientX: 130, clientY: 60});
    fireEvent.pointerUp(relocated, {clientX: 130, clientY: 60});
    assert.equal(JSON.stringify(value), before); assert.equal(within(inspector).getByRole('textbox'), editor);
    const saved = JSON.parse(JSON.stringify(value)); cleanup(); render(React.createElement(Harness, {regions: true}));
    await act(async () => {});
    assert.deepEqual(value, saved);
    assert.ok(within(screen.getByRole('region', {name: 'Область inspector'})).getByRole('textbox', {name: 'Черновик input'}));
  } finally {document.elementFromPoint = previousHitTest;}
});
