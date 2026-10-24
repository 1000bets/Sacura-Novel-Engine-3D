const Files = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout:Delay} = require('node:timers/promises');

exports.run = async ({app:Application, window:Window, editors:Editors, windows:Windows, getViewport:GetViewport, createNativeProject:CreateProject}) => {
  const Output = Path.resolve(__dirname, '../../build/reflected-assets-visual-' + Date.now());
  await Files.mkdir(Output, {recursive:true});
  const Report = Message => Files.appendFile(Path.join(Output,'progress.log'), Message+'\n');
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  async function Wait(Check, Label) {
    const Deadline = Date.now() + 45000;
    while (!await Check()) {
      if (Date.now() > Deadline) {
        throw Error('Reflected editor timeout: ' + Label);
      }
      await Delay(100);
    }
  }
  const Watchdog = setTimeout(() => {Report('Global timeout');Application.exit(1);},150000);
  async function Capture(Target, Name) {
    await Delay(250);
    await Files.writeFile(Path.join(Output,Name+'.png'),(await Target.webContents.capturePage()).toPNG());
  }
  async function SetValue(Target, Property, Value) {
    const Source = `(() => {
      const Field = Array.from(document.querySelectorAll('[data-reflected-path]')).find(Element => Element.dataset.reflectedPath === ${JSON.stringify(JSON.stringify(Property))});
      const Input = Field.querySelector('input,select');
      Input.focus();
      const Prototype = Object.getPrototypeOf(Input);
      Object.getOwnPropertyDescriptor(Prototype,'value').set.call(Input,${JSON.stringify(Value)});
      Input.dispatchEvent(new Event('input',{bubbles:true}));
      Input.dispatchEvent(new Event('change',{bubbles:true}));
      Input.blur();
    })()`;
    await Target.webContents.executeJavaScript(Source);
  }
  try {
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'),'shell');
    await CreateProject('InputEditorCheck',Output,'Empty');
    await Wait(() => GetViewport()?.bridge.connected,'bridge');
    const Call = async (Method, Parameters = {}) => {
      const Result = await GetViewport().bridge.request(Method, Parameters);
      Assert.equal(Result.ok,true,JSON.stringify(Result));return Result.value;
    };
    const Action = await Call('assets.create',{factoryId:'editor.CreateInputActionAssetFactory',path:'VisualAction.inputaction'});
    const Context = await Call('assets.create',{factoryId:'editor.CreateInputContextAssetFactory',path:'VisualContext.inputcontext'});
    const Descriptor = await Editors.open({assetId:Action.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-reflected-asset-editor select")'),'action fields');
    const Document = Editors.get(Descriptor.id);
    const GetDocument = async () => (await Document.viewport.bridge.request('assetDocument.execute',{assetId:Action.id,operation:'get'})).value;
    await SetValue(Window,['Trigger'],'Hold');
    await Wait(async () => (await GetDocument()).document.Trigger === 'Hold','trigger edit');
    await Evaluate("document.querySelector('.native-reflected-asset-editor .native-document-toolbar>button').click()");
    await Wait(async () => !(await GetDocument()).dirty,'save');
    await SetValue(Window,['Threshold'],'-1');
    await Wait(() => Evaluate('!!document.querySelector(".reflected-document-error")'),'validation error');
    Assert.equal((await GetDocument()).document.Threshold,0.5);
    await Capture(Window,'DesktopValidation');
    await SetValue(Window,['Threshold'],'0.75');
    await Wait(async () => (await GetDocument()).document.Threshold === 0.75,'valid threshold');
    await Evaluate("document.querySelectorAll('.native-reflected-asset-editor .native-document-toolbar details button')[0].click()");
    await Wait(async () => (await GetDocument()).document.Threshold === 0.5,'undo');
    await Evaluate("document.querySelectorAll('.native-reflected-asset-editor .native-document-toolbar details button')[1].click()");
    await Wait(async () => (await GetDocument()).document.Threshold === 0.75,'redo');
    await Capture(Window,'Desktop');
    const Detached = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,Descriptor.id);
    Detached.setSize(440,780);
    await Wait(() => Detached.webContents.executeJavaScript('!!document.querySelector(".native-reflected-asset-editor")'),'detached properties');
    await SetValue(Detached,['HoldSeconds'],'0.8');
    await Wait(async () => (await GetDocument()).document.HoldSeconds > 0.79,'detached edit');
    await Capture(Detached,'NarrowDetached');
    Assert.equal((await Editors.open({assetId:Action.id})).id,Descriptor.id);
    await Detached.webContents.executeJavaScript("document.querySelector('.native-reflected-asset-editor .native-document-toolbar>button').click()");
    await Wait(async () => !(await GetDocument()).dirty,'detached save');
    await Editors.close(Descriptor.id);
    const ContextDescriptor = await Editors.open({assetId:Context.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-reflected-asset-editor .reflected-property-group")'),'context fields');
    await Capture(Window,'EmptyContext');
    await Evaluate("document.querySelector('.native-reflected-asset-editor .reflected-property-group>button').click()");
    await Wait(() => Evaluate('!!document.querySelector(".native-reflected-asset-editor .native-asset-picker")'),'binding draft');
    await Evaluate("document.querySelector('.native-reflected-asset-editor .native-asset-picker summary').click()");
    await Evaluate("Array.from(document.querySelectorAll('.native-reflected-asset-editor .native-asset-options button')).find(Button => Button.textContent.includes('VisualAction')).click()");
    await SetValue(Window,['element','Control'],'KeyF');
    await Evaluate("document.querySelector('.native-reflected-asset-editor .reflected-collection-actions>button').click()");
    const ContextDocument = Editors.get(ContextDescriptor.id);
    const GetContext = async () => (await ContextDocument.viewport.bridge.request('assetDocument.execute',{assetId:Context.id,operation:'get'})).value;
    await Wait(async () => (await GetContext()).document.Bindings.length === 1,'binding insertion');
    Assert.equal((await GetContext()).document.Bindings[0].Action,Action.id);
    Assert.equal((await GetContext()).document.Bindings[0].Control,'KeyF');
    await Capture(Window,'ContextBinding');
    await Evaluate("document.querySelector('.native-reflected-asset-editor .native-document-toolbar>button').click()");
    await Wait(async () => !(await GetContext()).dirty,'context save');
    await Call('play.start');
    await Wait(() => Evaluate("document.querySelector('.native-reflected-asset-editor input')?.matches(':disabled')"),'Play read-only');
    await Call('play.stop');
    await Wait(() => Evaluate("!document.querySelector('.native-reflected-asset-editor input')?.matches(':disabled')"),'Stop unlock');
    await Editors.close(ContextDescriptor.id);
    const Material = (await Call('assets.list')).find(Asset => Asset.typeId === 'Material');
    const MaterialDescriptor = await Editors.open({assetId:Material.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-material-editor")'),'specialized material');
    Assert.equal(await Evaluate('!!document.querySelector(".native-reflected-asset-editor")'),false);
    await Capture(Window,'SpecializedMaterial');
    await Editors.close(MaterialDescriptor.id);
    await Report('PASS: edit/save/validation/undo/redo, detached narrow layout, session reuse, empty context and specialized material');
    console.log('Reflected assets UI report: '+Output);
    clearTimeout(Watchdog);await GetViewport().stop();Application.exit(0);
  } catch (Failure) {
    await Report(String(Failure));console.error(Failure);
    await Capture(Window,'Failure').catch(()=>{});
    clearTimeout(Watchdog);await GetViewport()?.stop();Application.exit(1);
  }
};
