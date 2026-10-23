const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/component-names-visual-' + Date.now());
  const Report = Message => FileSystem.appendFile(Path.join(Output, 'progress.log'), Message + '\n');
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Component caption check timed out');
      }
      await Delay(100);
    }
  }
  async function Capture(Name) {
    await Delay(250);
    await FileSystem.writeFile(Path.join(Output, Name + '.png'), (await Window.webContents.capturePage()).toPNG());
  }
  async function Search(Value) {
    await Evaluate(`(()=>{const Input=document.querySelector('input[aria-label="Поиск компонентов"]');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(Value)});Input.dispatchEvent(new Event('input',{bubbles:true}));})()`);
  }
  try {
    await FileSystem.mkdir(Output, {recursive: true});
    setTimeout(async () => { await Report('Global timeout'); await Capture('Timeout'); await GetViewport()?.stop(); Application.exit(1); }, 90000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'));
    await Report('Creating project');
    await CreateNativeProject('ComponentNamesCheck', Output, 'Empty');
    await Wait(() => GetViewport()?.bridge.connected);
    await Report('Reading component catalogue');
    const Catalogue = await GetViewport().bridge.request('reflection.getComponentTypes', {});
    await Report('Catalogue received');
    Assert.equal(Catalogue.ok, true);
    Assert.equal(Catalogue.value.find(Type => Type.typeId === 'engine.MeshRendererComponent').displayName, 'Статический меш');
    await Report('Opening placement menu');
    await Wait(() => Evaluate("Array.from(document.querySelectorAll('.editor-menubar button')).some(Button=>Button.textContent.includes('Разместить')&&!Button.disabled)"));
    await Evaluate("Array.from(document.querySelectorAll('.editor-menubar button')).find(Button=>Button.textContent.includes('Разместить')).click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"placement.basic\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"placement.basic\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"editor.CreateStaticMeshObjectAction\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"editor.CreateStaticMeshObjectAction\"]').click()");
    await Wait(() => Evaluate("document.querySelector('.component-instance-row strong')?.textContent==='Статический меш'"));
    Assert.equal(await Evaluate("document.querySelector('.component-instance-row').title"), 'engine.MeshRendererComponent');
    Assert.equal(await Evaluate("document.querySelector('option[value=\"engine.CameraComponent\"]').textContent"), 'Камера');
    await Capture('Inspector');
    for (const Value of ['Статический меш', 'engine.MeshRendererComponent', 'NoMatchingComponent']) {
      await Search(Value);
      const ExpectedCount = Number(Value !== 'NoMatchingComponent');
      await Wait(async () => await Evaluate("document.querySelectorAll('.native-component-overview button').length") === ExpectedCount);
    }
    await Capture('EmptySearch');
    await Search('');
    await Evaluate("document.querySelector('.component-instance-row').click()");
    await Wait(() => Evaluate("document.querySelector('.component-instance-row').getAttribute('aria-expanded')==='false'"));
    await Capture('Collapsed');
    await Evaluate("document.querySelector('.component-instance-row').click()");
    await Wait(() => Evaluate("document.querySelector('.component-instance-row').getAttribute('aria-expanded')==='true'"));
    await Report('PASS: captions, search, metadata and disclosure');
    console.log('PASS: component captions, metadata, search, type identity and disclosure. ' + Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    await Report(String(Failure));
    console.error(Failure);
    await Capture('Failure').catch(() => {});
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
