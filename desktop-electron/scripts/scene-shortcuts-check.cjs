const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/scene-shortcuts-check-' + Date.now());
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  const Checks = [];
  async function Wait(Predicate, Description) {
    const Deadline = Date.now() + 15000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Scene shortcut check timed out: ' + Description);
      }
      await Delay(100);
    }
  }
  async function Command(Method, Params = {}) {
    const Result = await GetViewport().bridge.request(Method, Params);
    Assert.equal(Result.ok, true, JSON.stringify(Result));
    return Result.value;
  }
  async function Press(KeyCode, Modifiers = []) {
    Window.webContents.sendInputEvent({type:'keyDown', keyCode:KeyCode, modifiers:Modifiers});
    Window.webContents.sendInputEvent({type:'keyUp', keyCode:KeyCode, modifiers:Modifiers});
    await Delay(250);
  }
  async function FocusViewport() {
    await Evaluate('document.querySelector("[data-native-surface]").parentElement.focus()');
    await Delay(100);
  }
  async function Select(Identifier) {
    await Command('selection.set', {objectId:Identifier});
    await Wait(() => Evaluate(`!!document.querySelector('.native-object-inspector')`), 'Inspector');
    await Delay(350);
  }
  async function Objects() {
    return Command('scene.getHierarchy');
  }
  try {
    Window.setIgnoreMouseEvents(true);
    await FileSystem.mkdir(Output, {recursive:true});
    setTimeout(() => Application.exit(1), 120000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'), 'Editor loaded');
    await CreateNativeProject('SceneShortcutsCheck', Output, 'Empty');
    await Wait(() => GetViewport()?.state.phase === 'ready', 'Native viewport ready');
    await Wait(() => Evaluate('!!document.querySelector("[data-native-surface]")'), 'Viewport mounted');
    await FocusViewport();
    await Press('Delete');
    await Press('v', ['control']);
    Assert.equal((await Objects()).length, 0);
    Checks.push('Empty selection and empty clipboard are safe');
    const First = await Command('object.create', {name:'ShortcutFirst'});
    await Select(First.id);
    await Command('transform.set', {objectId:First.id,transform:{...(await Command('transform.get',{objectId:First.id})),scale:[1,2,3]}});
    await Wait(() => Evaluate(`document.querySelector('input[aria-label="Масштаб Y"]')?.value === '2'`), 'Scale fields');
    Assert.equal(await Evaluate(`document.querySelectorAll('.scale-axis-lock').length`),1);
    await Evaluate(`document.querySelector('.scale-axis-lock').click()`);
    await Wait(() => Evaluate(`document.querySelector('.scale-axis-lock').getAttribute('aria-pressed') === 'true'`), 'Scale locked');
    async function EditNumber(Label, Value) {
      Window.focus();
      await Delay(150);
      await Evaluate(`(() => {const Input=document.querySelector('input[aria-label="${Label}"]');Input.focus();if(!document.hasFocus()){Input.dispatchEvent(new FocusEvent('focusin',{bubbles:true}));}})()`);
      for (const Text of ['', String(Value)]) {
        await Evaluate(`(() => {const Input = document.querySelector('input[aria-label="${Label}"]');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(Text)});Input.dispatchEvent(new Event('input',{bubbles:true}));})()`);
        await Delay(80);
      }
      await Evaluate(`(() => {const Input=document.querySelector('input[aria-label="${Label}"]');const Focused=document.hasFocus();Input.blur();if(!Focused){Input.dispatchEvent(new FocusEvent('focusout',{bubbles:true}));}})()`);
    }
    await EditNumber('Масштаб X',2);
    await Wait(async () => JSON.stringify((await Command('transform.get',{objectId:First.id})).scale) === '[2,4,6]', 'Locked proportional scale');
    await FileSystem.writeFile(Path.join(Output,'ScaleLock.png'),(await Window.webContents.capturePage()).toPNG());
    const OriginalBounds = Window.getBounds();
    Window.setMinimumSize(480,400);
    Window.setSize(640,720);
    await Delay(300);
    await Evaluate(`[...document.querySelectorAll('.compact-panel-tabs button')].find(Button=>Button.textContent.trim()==='Свойства').click()`);
    await Wait(()=>Evaluate(`!!document.querySelector('.editor-workspace.compact-inspector')`),'Compact scale Inspector');
    await Delay(200);
    Assert.ok(await Evaluate(`(() => {const Row=document.querySelector('.scale-axis-lock').closest('.transform-vector');return Row.scrollWidth <= Row.clientWidth + 1;})()`));
    await FileSystem.writeFile(Path.join(Output,'ScaleLockCompact.png'),(await Window.webContents.capturePage()).toPNG());
    Window.setBounds(OriginalBounds);
    await Delay(300);
    await Command('editor.undo');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[1,2,3]);
    await Command('editor.redo');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[2,4,6]);
    await Wait(() => Evaluate(`document.querySelector('input[aria-label="Масштаб Y"]')?.value === '4'`), 'Redo scale fields');
    await EditNumber('Масштаб Y',2);
    await Wait(async () => JSON.stringify((await Command('transform.get',{objectId:First.id})).scale) === '[1,2,3]', 'Locked Y scale');
    await Evaluate(`document.querySelector('.scale-axis-lock').click()`);
    await Wait(() => Evaluate(`document.querySelector('.scale-axis-lock').getAttribute('aria-pressed') === 'false'`), 'Scale unlocked');
    await EditNumber('Масштаб Z',5);
    await Wait(async () => JSON.stringify((await Command('transform.get',{objectId:First.id})).scale) === '[1,2,5]', 'Independent scale axis');
    Checks.push('Scale-only lock preserves nonuniform ratios through clear/retype, X/Y editing and single-command Undo/Redo; unlocked axes remain independent');
    for (const ScaleValue of [0.001, -2, 100]) {
      await EditNumber('Масштаб X',ScaleValue);
      await Wait(async () => Math.abs((await Command('transform.get',{objectId:First.id})).scale[0] - ScaleValue) < 0.000001, 'Unclamped native scale');
      await Wait(() => Evaluate(`Math.abs(Number(document.querySelector('input[aria-label="Масштаб X"]').value) - ${ScaleValue}) < 0.000001`), 'Unclamped Inspector scale');
    }
    await Evaluate(`document.querySelector('.scale-axis-lock').click()`);
    await Wait(() => Evaluate(`document.querySelector('.scale-axis-lock').getAttribute('aria-pressed') === 'true'`), 'Lock unrestricted scale');
    await EditNumber('Масштаб X',0.01);
    await Wait(async () => Math.abs((await Command('transform.get',{objectId:First.id})).scale[2] - 0.0005) < 0.000001, 'Locked scale below former minimum');
    await Command('editor.undo');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[100,2,5]);
    Checks.push('Small, negative and large scale values survive native edits and Inspector refresh; locked small scale undoes in one step');

    await Command('transform.set',{objectId:First.id,transform:{...(await Command('transform.get',{objectId:First.id})),scale:[1,2,3]}});
    await Wait(()=>Evaluate(`document.querySelector('input[aria-label="Масштаб X"]').value==='1'`),'Expression baseline');
    await EditNumber('Масштаб X','(1 + 1) / 4');
    await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).scale[0]-.5)<.000001,'Math expression applied');
    await EditNumber('Масштаб X','1/0');
    await Wait(()=>Evaluate(`document.querySelector('input[aria-label="Масштаб X"]').getAttribute('aria-invalid')==='true'`),'Expression error');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[.5,1,1.5]);
    await FileSystem.writeFile(Path.join(Output,'NumericExpressionError.png'),(await Window.webContents.capturePage()).toPNG());
    await EditNumber('Масштаб X','0,5');
    async function DragScale(Cancelled, Modifiers = []) {
      Window.focus();
      await FocusViewport();
      const Point=await Evaluate(`(() => {const Input=document.querySelector('input[aria-label="Масштаб X"]');const Bounds=Input.getBoundingClientRect();return {x:Math.round(Bounds.x+Bounds.width/2),y:Math.round(Bounds.y+Bounds.height/2)};})()`);
      const Bounds=Window.getContentBounds();
      function Send(Type, Offset, Extra = {}) {
        Window.webContents.sendInputEvent({type:Type,x:Point.x+Offset,y:Point.y,globalX:Bounds.x+Point.x+Offset,globalY:Bounds.y+Point.y,...Extra});
      }
      Send('mouseMove',0);
      Send('mouseDown',0,{button:'left',clickCount:1});
      await Delay(80);
      Send('mouseMove',10,{modifiers:['leftbuttondown',...Modifiers]});
      await Delay(150);
      Send('mouseMove',20,{modifiers:['leftbuttondown',...Modifiers]});
      let Expected = .7;
      if (Modifiers.includes('shift')) {Expected = .52;}
      if (Modifiers.includes('control')) {Expected = 2.5;}
      await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).scale[0]-Expected)<.00001,'Live numeric drag preview');
      if (Cancelled) {
        await Press('Escape');
      }
      Send('mouseUp',20,{button:'left',clickCount:1});
      await Delay(250);
    }
    await DragScale(false);
    await Command('editor.undo');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[.5,1,1.5]);
    await Wait(()=>Evaluate(`document.querySelector('input[aria-label="Масштаб X"]').value==='0.5'`),'Drag undo fields');
    await DragScale(true,['shift']);
    await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).scale[0]-.5)<.00001,'Fine drag cancellation');
    await DragScale(true,['control']);
    await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).scale[0]-.5)<.00001,'Fast drag cancellation');
    await DragScale(true);
    await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).scale[0]-.5)<.00001,'Cancelled drag restores native scale');
    await Command('editor.undo');
    Assert.deepEqual((await Command('transform.get',{objectId:First.id})).scale,[1,2,3]);
    Checks.push('Arithmetic, decimal comma and division-by-zero validation; live mouse scrub, proportional scale, single-step Undo and Escape rollback without history');
    await EditNumber('Положение X','(100 + 20) / 10');
    await Wait(async()=>(await Command('transform.get',{objectId:First.id})).position[0]===12,'Position expression');
    await EditNumber('Поворот Z','90 / 2');
    await Wait(async()=>Math.abs((await Command('transform.get',{objectId:First.id})).rotation[2]-Math.sin(Math.PI/8))<.000001,'Rotation expression');
    const BeforeTransaction = await Command('transform.get',{objectId:First.id});
    const Transaction = 'selection-cancel-test';
    await Command('transform.edit',{objectId:First.id,transaction:Transaction,operation:'begin'});
    await Command('transform.edit',{objectId:First.id,transaction:Transaction,operation:'update',transform:{...BeforeTransaction,position:[10,0,0]}});
    Assert.equal((await Command('transform.get',{objectId:First.id})).position[0],10);
    await Command('selection.set',{objectId:First.id});
    Assert.deepEqual(await Command('transform.get',{objectId:First.id}),BeforeTransaction);
    const Stale = await GetViewport().bridge.request('transform.edit',{objectId:First.id,transaction:Transaction,operation:'commit',transform:BeforeTransaction});
    Assert.equal(Stale.ok,false);
    Assert.equal(Stale.error.code,'STALE_EDIT');
    Checks.push('Selection change rolls back a live transform preview and rejects its stale commit');

    const Second = await Command('object.create', {name:'ShortcutSecond'});
    await FocusViewport();
    await Select(First.id);
    await Select(Second.id);
    await Press('Delete');
    await Wait(async () => !(await Objects()).some(Object => Object.id === Second.id), 'Delete current selection');
    Assert.ok((await Objects()).some(Object => Object.id === First.id));
    Checks.push('Delete uses latest selection');
    await Press('z', ['control']);
    await Wait(async () => (await Objects()).some(Object => Object.id === Second.id), 'Undo deletion');
    await Select(First.id);
    await Press('c', ['control']);
    await Select(Second.id);
    await Press('v', ['control']);
    await Wait(async () => (await Objects()).length === 3, 'Paste copied object');
    const Pasted = (await Objects()).find(Object => Object.id !== First.id && Object.id !== Second.id);
    Assert.ok(Pasted.name.startsWith('ShortcutFirst'), JSON.stringify(Pasted));
    Checks.push('Copy snapshot survives selection change; paste creates new identity');
    await Press('z', ['control']);
    await Wait(async () => (await Objects()).length === 2, 'Undo paste');
    await Press('y', ['control']);
    await Wait(async () => (await Objects()).length === 3, 'Redo paste');
    Checks.push('Undo and redo restore scene objects');
    await Select(Second.id);
    await Press('x', ['control']);
    await Wait(async () => !(await Objects()).some(Object => Object.id === Second.id), 'Cut selected object');
    await Press('v', ['control']);
    await Wait(async () => (await Objects()).length === 3, 'Paste cut object without selection');
    Checks.push('Cut and paste without selection');
    await Select(First.id);
    await Evaluate(`document.querySelector('.native-object-inspector input').focus()`);
    await Press('Delete');
    await Press('c', ['control']);
    await Press('v', ['control']);
    Assert.equal((await Objects()).length, 3);
    Checks.push('Text fields do not delete or paste scene objects');
    await FocusViewport();
    await Command('play.start');
    await Delay(350);
    await Press('Delete');
    await Press('v', ['control']);
    await Command('play.stop');
    Assert.equal((await Objects()).length, 3);
    Checks.push('Play protects edit scene');
    await Wait(() => Evaluate('!!document.querySelector(".tree-object button[aria-label=ShortcutFirst]")'), 'Hierarchy object');
    await Evaluate('(() => {const Button = document.querySelector(".tree-object button[aria-label=ShortcutFirst]"); Button.focus(); Button.click();})()');
    await Delay(350);
    await Press('c', ['control']);
    await Press('v', ['control']);
    await Wait(async () => (await Objects()).length === 4, 'Hierarchy copy and paste');
    await Press('Delete');
    await Wait(async () => (await Objects()).length === 3, 'Hierarchy Delete after paste');
    Assert.ok((await Objects()).some(Object => Object.id === First.id));
    Checks.push('Hierarchy shortcuts use newly pasted selection');
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:true, Checks}, null, 2));
    console.log('Scene shortcuts check passed: ' + Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error(Failure);
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:false, Checks, error:Failure.stack}, null, 2));
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
