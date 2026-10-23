import Test, {after, afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const Dom = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'});
for (const Key of ['window','document','navigator','HTMLElement']) {
  Object.defineProperty(globalThis, Key, {configurable:true,value:Dom.window[Key]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render, screen, fireEvent, cleanup, act} = await import('@testing-library/react');
const Server = await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:NumericInput} = await Server.ssrLoadModule('/src/NumericInput.jsx');
afterEach(cleanup);
after(async () => {await Server.close();Dom.window.close();});

Test('expression commit, invalid arithmetic and Escape preserve native-style edit boundaries', async () => {
  const Committed = [];
  render(React.createElement(NumericInput,{value:2,label:'Value',onCommit:Value=>Committed.push(Value)}));
  const Input = screen.getByRole('spinbutton');
  fireEvent.focus(Input);
  fireEvent.change(Input,{target:{value:'(2 + 3) * 4'}});
  await act(async()=>fireEvent.blur(Input));
  Assert.deepEqual(Committed,[20]);
  fireEvent.focus(Input);
  fireEvent.change(Input,{target:{value:'1/0'}});
  await act(async()=>fireEvent.blur(Input));
  Assert.equal(Input.getAttribute('aria-invalid'),'true');
  Assert.ok(screen.getByRole('alert'));
  Assert.deepEqual(Committed,[20]);
  Input.focus();
  fireEvent.change(Input,{target:{value:'100'}});
  await act(async()=>fireEvent.keyDown(Input,{key:'Escape'}));
  Assert.equal(Input.value,'2');
  Assert.deepEqual(Committed,[20]);
});

Test('pointer scrubbing previews, uses precision modifiers and cancels without commit', async () => {
  const Committed = [], Previewed = [];
  let Cancelled = 0;
  render(React.createElement(NumericInput,{value:1,label:'Value',step:0.01,onCommit:Value=>Committed.push(Value),onPreview:Value=>Previewed.push(Value),onCancel:()=>Cancelled++}));
  const Input = screen.getByRole('spinbutton');
  let Captured = false;
  Input.setPointerCapture = () => {Captured = true;};
  Input.hasPointerCapture = () => Captured;
  Input.releasePointerCapture = () => {Captured = false;};
  function Pointer(Type, Horizontal, Extra = {}) {
    const Event = new window.MouseEvent(Type,{bubbles:true,cancelable:true,button:0,screenX:Horizontal,...Extra});
    Object.defineProperty(Event,'pointerId',{value:1});
    fireEvent(Input,Event);
  }
  Pointer('pointerdown',0);
  Pointer('pointermove',20,{shiftKey:true});
  Assert.equal(Previewed.at(-1),1.02);
  await act(async()=>Pointer('pointerup',20));
  Assert.deepEqual(Committed,[1.02]);
  Pointer('pointerdown',0);
  Pointer('pointermove',20,{ctrlKey:true});
  Assert.equal(Previewed.at(-1),3);
  await act(async()=>fireEvent.keyDown(document,{key:'Escape'}));
  Pointer('pointerup',20);
  Assert.equal(Cancelled,1);
  Assert.deepEqual(Committed,[1.02]);
});
