const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, editors: Editors, windows: Windows, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/asset-reference-check-' + Date.now());
  const Report = Message => FileSystem.appendFile(Path.join(Output, 'progress.log'), Message + '\n');
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Asset reference check timed out');
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
    await CreateNativeProject('AssetReferenceCheck', Output, 'Empty');
    await Wait(() => GetViewport()?.bridge.connected);
    await Report('Reading component catalogue');
    const Catalogue = await GetViewport().bridge.request('reflection.getComponentTypes', {});
    await Report('Catalogue received');
    Assert.equal(Catalogue.ok, true);
    Assert.equal(Catalogue.value.find(Type => Type.typeId === 'engine.MeshRendererComponent').displayName, 'Static Mesh');
    const Imported = await GetViewport().bridge.request('assets.import', {sourcePath:process.env.SACURA_ASSET_REFERENCE_SOURCE || Path.resolve(__dirname, '../../Samples/SampleProject/Content/Meshes/TestCube.glb'),destination:'Models/Imported.glb'});
    Assert.equal(Imported.ok,true,JSON.stringify(Imported));
    const CatalogueAssets = await GetViewport().bridge.request('assets.list', {});
    const Model = CatalogueAssets.value.find(Asset => Asset.id === Imported.value.id);
    await GetViewport().bridge.request('assets.place', {assetId:Model.id,subAssetId:Model.subassets[0].id});
    await Wait(() => Evaluate("!!document.querySelector('.component-instance-row[title=\"engine.MeshRendererComponent\"]')"));
    const Assets = await GetViewport().bridge.request('assets.list', {});
    const State = await GetViewport().bridge.request('bridge.describe', {});
    const Components = await GetViewport().bridge.request('object.getComponents', {objectId:State.value.selection});
    const Properties = await GetViewport().bridge.request('component.getProperties', {objectId:State.value.selection,componentId:Components.value[0].id});
    await FileSystem.writeFile(Path.join(Output,'snapshot.json'), JSON.stringify({Assets,Properties},null,2));
    await Wait(() => Evaluate("[...document.querySelectorAll('.native-asset-picker summary')].find(Element => Element.textContent.includes('Imported'))?.textContent.includes('Imported')"));
    const Target = {objectId:State.value.selection,componentId:Components.value[0].id};
    const Slots = await GetViewport().bridge.request('mesh.inspect', Target);
    Assert.equal(Slots.ok,true);
    Assert.ok(Slots.value.slots.every(Slot => Slot.assetId === null || Assets.value.some(Asset => Asset.id === Slot.assetId)));
    Assert.equal(await Evaluate("document.querySelector('.component-instance-properties').textContent.includes('Открыть меш')"),false);
    Assert.equal(await Evaluate("document.querySelector('.component-instance-properties').textContent.includes('Недоступный ресурс')"),false);
    await Evaluate("[...document.querySelectorAll('.native-asset-picker summary')].find(Element => Element.textContent.includes('Imported')).scrollIntoView({block:'center'})");
    await Delay(1000);
    await Evaluate("document.querySelector('.inspector-scroll').scrollTop=10000");
    await Capture('Inspector');
    await Evaluate("[...document.querySelectorAll('.native-asset-picker summary')].find(Element => Element.textContent.includes('Imported')).closest('.native-asset-field').querySelector('.lucide-folder-open').closest('button').click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-document-tab]')"));
    await Capture('OpenedMesh');
    const Identity = {assetId:Model.id,subAssetId:Model.subassets[0].id};
    const Descriptor = await Editors.open(Identity);
    const Count = Editors.size;
    await Evaluate("document.getElementById('editor-tab-story').click()");
    await Editors.open(Identity);
    await Wait(() => Evaluate(`document.getElementById(${JSON.stringify('editor-tab-' + Descriptor.id)})?.getAttribute('aria-selected') === 'true'`));
    const Popup = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,Descriptor.id);
    Popup.minimize();
    Window.focus();
    await Delay(200);
    await Editors.open(Identity);
    await Wait(() => !Popup.isMinimized() && Popup.isVisible() && Popup.isFocused());
    Popup.setPosition(-30000,-30000);
    Popup.hide();
    await Editors.open(Identity);
    await Wait(() => Popup.isVisible() && Popup.isFocused() && Popup.getBounds().x > -30000);
    Assert.equal(Editors.size,Count,'Reopening reuses the document session');
    await FileSystem.writeFile(Path.join(Output,'ReopenedDetached.png'),(await Popup.webContents.capturePage()).toPNG());
    await Report('PASS: reopening selects docked tab, restores minimized window, reveals hidden offscreen window and reuses session');
    await Editors.closeAll(true);
    await Report('PASS: imported mesh resolves, absent materials remain empty, inline Open creates an asset document');
    console.log('PASS: asset reference inspection and opening. ' + Output);
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
