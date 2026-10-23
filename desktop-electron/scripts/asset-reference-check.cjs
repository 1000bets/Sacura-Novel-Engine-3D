const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/asset-reference-check-' + Date.now());
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
    const Actions = await GetViewport().bridge.request('editor.actions', {});
    const Cube = Actions.value.find(Action => Action.assetId && /cube/i.test(Action.label));
    await GetViewport().bridge.request('assets.place', {assetId:Cube.assetId,subAssetId:Cube.subAssetId});
    await Wait(() => Evaluate("document.querySelector('.component-instance-row strong')?.textContent==='Статический меш'"));
    const Assets = await GetViewport().bridge.request('assets.list', {});
    const State = await GetViewport().bridge.request('bridge.describe', {});
    const Components = await GetViewport().bridge.request('object.getComponents', {objectId:State.value.selection});
    const Properties = await GetViewport().bridge.request('component.getProperties', {objectId:State.value.selection,componentId:Components.value[0].id});
    await FileSystem.writeFile(Path.join(Output,'snapshot.json'), JSON.stringify({Assets,Properties},null,2));
    await Delay(1500);
    await Report(await Evaluate("document.querySelector('.component-instance-properties').innerText"));
    await Capture('Inspector');
    console.log('PASS: asset reference inspection. ' + Output);
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
