const fs = require('node:fs/promises');
const path = require('node:path');
const assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = path.resolve(__dirname, '../../build/asset-transfer-check-' + Date.now());
  const Errors = [];
  let AssetWindow;
  Window.webContents.on('console-message', Details => {
    if (Details.level === 'error') { Errors.push(Details.message); }
  });
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) { throw Error('Asset UI check timed out'); }
      await Delay(100);
    }
  }
  const Evaluate = async Source => {
    try { return await Window.webContents.executeJavaScript(Source); }
    catch (ErrorValue) { throw Error('Asset UI script failed: ' + Source + '\n' + ErrorValue.message); }
  };
  const Capture = async Name => {await Delay(150);await fs.writeFile(path.join(Output, Name), (await Window.webContents.capturePage()).toPNG());};
  try {
    await fs.mkdir(Output, {recursive: true});
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'));
    console.log('Asset check: creating project');
    await CreateNativeProject('AssetUiCheck', Output, 'Empty');
    await Wait(() => Evaluate("!!document.querySelector('[data-native-surface]')"));
    await Capture('loaded.png');
    console.log('Asset check: loaded', await Evaluate("document.body.innerText.slice(0,1800)"));
    await Wait(() => Evaluate("Array.from(document.querySelectorAll('.editor-menubar button')).some(Button=>Button.textContent.includes('Разместить')&&!Button.disabled)"));
    await Evaluate("Array.from(document.querySelectorAll('.editor-menubar button')).find(Button=>Button.textContent.includes('Разместить')).click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"placement.lighting\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"placement.lighting\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"editor.CreatePointLightObjectAction\"]')"));
    await Capture('placement-menu.png');
    await Evaluate("document.querySelector('[data-menu-id=\"editor.CreatePointLightObjectAction\"]').click()");
    await Wait(() => Evaluate("document.querySelector('.native-object-inspector')?.textContent.includes('Light Type')"));
    await Capture('reflected-light-inspector.png');
    const Bridge = GetViewport().bridge;
    const Catalogue = await Bridge.request('editor.actions');
    assert.equal(Catalogue.ok, true);
    for (const Identifier of ['editor.CreatePointLightObjectAction', 'editor.CreateSpotLightObjectAction', 'editor.CreateDirectionalLightObjectAction']) {
      assert.ok(Catalogue.value.some(Action => Action.id === Identifier));
    }
    await Evaluate("Array.from(document.querySelectorAll('.editor-menubar button')).find(Button=>Button.textContent.includes('Разместить')).click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"placement.basic\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"placement.basic\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"editor.CreateStaticMeshObjectAction\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"editor.CreateStaticMeshObjectAction\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('.native-object-inspector .native-asset-picker')"));
    await Evaluate("document.querySelector('.native-object-inspector .native-asset-picker').open=true");
    await Wait(() => Evaluate("Array.from(document.querySelectorAll('.native-asset-options button')).some(Button=>Button.textContent.includes('StarterBlock'))"));
    await Evaluate("Array.from(document.querySelectorAll('.native-asset-options button')).find(Button=>Button.textContent.includes('StarterBlock')).click()");
    await Wait(() => Evaluate("!!document.querySelector('.native-object-inspector .native-asset-field img')"));
    await Capture('mesh-reference.png');
    await Wait(() => Evaluate("(()=>{const Button=document.querySelector('.native-object-inspector .native-asset-field button[aria-label=\"Выделить в Проводнике\"]:not(:disabled)');if(!Button){return false;}Button.click();return true;})()"));
    await Wait(() => Evaluate("!!document.querySelector('.asset-file-tile.selected')"));
    await Wait(() => Evaluate("!!document.querySelector('.asset-file-tile.selected img')"));
    await Capture('asset-thumbnails.png');
    await Evaluate("document.querySelector('[data-asset-folder=\"Game\"] > button:last-child').click()");
    await Wait(() => Evaluate("!!document.querySelector('.asset-file-area')"));
    await Evaluate("document.querySelector('.asset-file-area').dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:500,clientY:610}))");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"asset.create.editor.CreateMaterialAssetFactory\"]')"));
    await Capture('asset-creation-menu.png');
    await Evaluate("document.querySelector('[data-menu-id=\"asset.create.editor.CreateMaterialAssetFactory\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('.asset-create-dialog')"));
    await Evaluate("(()=>{const Field=document.querySelector('.asset-create-dialog input');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Field,'ContextMenuMaterial');Field.dispatchEvent(new Event('input',{bubbles:true}));})()");
    await Capture('asset-creation-dialog.png');
    Window.setSize(1000,760);
    await Delay(300);
    assert.equal(await Evaluate('document.documentElement.scrollWidth <= window.innerWidth'),true);
    await Capture('asset-creation-dialog-compact.png');
    Window.setSize(1500,950);

    await Evaluate("document.querySelector('.asset-create-dialog').requestSubmit()");
    await Wait(() => Evaluate("!document.querySelector('.asset-create-dialog') && !!Array.from(document.querySelectorAll('.asset-file-tile.selected')).find(Entry=>Entry.textContent.includes('ContextMenuMaterial'))"));
    await Capture('created-asset-selected.png');
    await Evaluate("Array.from(document.querySelectorAll('.asset-file-tile.selected')).find(Entry=>Entry.textContent.includes('ContextMenuMaterial')).dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,clientX:500,clientY:610}))");
    await Wait(() => Evaluate("!!document.querySelector('[data-menu-id=\"asset.create.editor.CreateMaterialInstanceAssetFactory\"]')"));
    await Evaluate("document.querySelector('[data-menu-id=\"asset.create.editor.CreateMaterialInstanceAssetFactory\"]').click()");
    await Wait(() => Evaluate("!!document.querySelector('.asset-create-dialog .native-asset-field')"));
    assert.equal(await Evaluate("document.querySelector('.asset-create-dialog .native-asset-field').textContent.includes('ContextMenuMaterial')"),true);
    await Capture('asset-creation-parent.png');
    await Evaluate("document.querySelector('.asset-create-dialog button[aria-label=\"Очистить ссылку\"]').click()");
    assert.equal(await Evaluate("!!document.querySelector('.asset-create-dialog')"),true);
    assert.equal(await Evaluate("document.querySelector('.asset-create-dialog button[type=submit]').disabled"),true);
    await Evaluate("document.querySelector('.asset-create-dialog .native-asset-picker').open=true");
    await Evaluate("Array.from(document.querySelectorAll('.asset-create-dialog .native-asset-options button')).find(Entry=>Entry.textContent.includes('ContextMenuMaterial')).click()");
    await Evaluate("(()=>{const Field=document.querySelector('.asset-create-dialog input');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Field,'ContextMenuInstance');Field.dispatchEvent(new Event('input',{bubbles:true}));})()");
    await Evaluate("document.querySelector('.asset-create-dialog').requestSubmit()");
    await Wait(() => Evaluate("!document.querySelector('.asset-create-dialog')"));

    if (process.argv.includes('--asset-creation-only')) {
      assert.deepEqual(Errors.filter(Message => /Uncaught|TypeError|ReferenceError/.test(Message)), []);
      await fs.writeFile(path.join(Output, 'result.json'), JSON.stringify({passed:true,scope:'asset-creation',errors:Errors},null,2));
      console.log('Asset creation UI check passed: ' + Output);
      await GetViewport().stop();
      Application.exit(0);
      return;
    }

    assert.equal(await Evaluate("!!document.querySelector('.asset-drop-hint')"), false);
    Window.setSize(1000, 760);
    await Delay(500);
    await Capture('compact-layout.png');
    const NativeAssets = await Bridge.request('assets.list');
    const SourceModel = NativeAssets.value.find(Asset => Asset.typeId === 'Model' && Asset.subassets.some(Subasset => Subasset.typeId === 'StaticMesh'));
    const SourcePath = await Bridge.request('assets.resolve', {assetId:SourceModel.id});
    const Imported = await Bridge.request('assets.import', {sourcePath:SourcePath.value.absolutePath,destination:'Models/EditorPreview.glb'});
    assert.equal(Imported.ok,true,JSON.stringify(Imported));
    const RefreshedAssets = await Bridge.request('assets.refresh');
    const Model = RefreshedAssets.value.find(Asset => Asset.relativePath === 'Models/EditorPreview.glb');
    assert.ok(Model);
    const Identity = {assetId:Model.id,subAssetId:Model.subassets.find(Subasset => Subasset.typeId === 'StaticMesh').id};
    await Evaluate('window.sacuraDesktop.nativeOpenAssetEditor(' + JSON.stringify(Identity) + ')');
    const {BrowserWindow, dialog} = require('electron');
    AssetWindow = BrowserWindow.getAllWindows().find(Candidate => Candidate !== Window);
    assert.ok(AssetWindow);
    assert.equal(AssetWindow.isResizable(),true);
    assert.equal(AssetWindow.isMinimizable(),true);
    assert.equal(AssetWindow.isMaximizable(),true);
    const AssetEvaluate = Source => AssetWindow.webContents.executeJavaScript(Source);
    AssetWindow.webContents.on('console-message', Details => {if (Details.level === 'error') { Errors.push(Details.message); }});
    await Wait(() => AssetEvaluate("!!document.querySelector('.native-asset-workspace')"));
    await Wait(() => AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.viewport?.data?.preview && Status.presentation.frames > 5)"));
    await Evaluate('window.sacuraDesktop.nativeOpenAssetEditor(' + JSON.stringify(Identity) + ')');
    assert.equal(BrowserWindow.getAllWindows().length,2);
    await AssetEvaluate("Array.from(document.querySelectorAll('.native-asset-workspace nav button')).find(Button=>Button.textContent==='Коллизия').click()");
    await Wait(() => AssetEvaluate("!!document.querySelector('fieldset legend button[aria-pressed=true]')"));
    await Wait(() => AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.viewport?.data?.selectedCollision === 0 && Status.presentation.frames > 20)"));
    await Delay(500);
    await fs.writeFile(path.join(Output,'detached-mesh-editor.png'),(await AssetWindow.webContents.capturePage()).toPNG());
    const ColliderTarget = await AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.viewport.data.collisionTargets[0])");
    const Pointer = async (Phase, Horizontal, Vertical, Buttons) => AssetEvaluate('window.sacuraDesktop.nativeInput(' + JSON.stringify({kind:'pointer',phase:Phase,x:Horizontal,y:Vertical,button:0,buttons:Buttons}) + ')');
    await Pointer('move', ColliderTarget.x, ColliderTarget.y, 0);
    await Pointer('down', ColliderTarget.x, ColliderTarget.y, 1);
    await Delay(100);
    for (let Step=1;Step<=10;Step++) {await Pointer('move',ColliderTarget.x+Step*3,ColliderTarget.y+Step*2,1);await Delay(30);}
    await Pointer('up',ColliderTarget.x+30,ColliderTarget.y+20,0);
    await Wait(() => AssetEvaluate("!!document.querySelector('[aria-label=\"Несохранённые изменения\"]')"));
    await fs.writeFile(path.join(Output,'collision-gizmo-edited.png'),(await AssetWindow.webContents.capturePage()).toPNG());
    AssetWindow.setSize(800,650);
    await Delay(500);
    assert.equal(await AssetEvaluate('document.documentElement.scrollWidth <= window.innerWidth'),true);
    await fs.writeFile(path.join(Output,'detached-compact-layout.png'),(await AssetWindow.webContents.capturePage()).toPNG());
    AssetWindow.setSize(1200,820);
    AssetWindow.minimize();
    await Wait(() => AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.stats?.visible === false)"));
    AssetWindow.restore();
    await Wait(() => AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.stats?.visible === true)"));


    await AssetEvaluate("Array.from(document.querySelectorAll('.native-document-toolbar details button')).find(Button=>Button.textContent==='Отменить').click()");
    await Wait(() => AssetEvaluate("!document.querySelector('[aria-label=\"Несохранённые изменения\"]')"));
    await AssetEvaluate("(()=>{const Field=document.querySelector('fieldset input[type=number]'); const Setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set; Setter.call(Field,'0.25'); Field.dispatchEvent(new Event('input',{bubbles:true})); Field.dispatchEvent(new FocusEvent('focusout',{bubbles:true}));})()");
    await Wait(() => AssetEvaluate("!!document.querySelector('[aria-label=\"Несохранённые изменения\"]')"));
    const OriginalDialog = dialog.showMessageBox;
    dialog.showMessageBox = async () => ({response:2});
    AssetWindow.close();
    await Delay(800);
    assert.equal(AssetWindow.isDestroyed(),false);
    dialog.showMessageBox = async () => ({response:0});
    AssetWindow.close();
    await Wait(() => AssetWindow.isDestroyed());
    dialog.showMessageBox = OriginalDialog;
    AssetWindow = null;
    const ParentMaterial = RefreshedAssets.value.find(Asset => Asset.typeId === 'Material');
    const InstanceFile = path.join(path.dirname(GetViewport().project),'Content','Materials','PreviewInstance.materialinstance');
    await fs.mkdir(path.dirname(InstanceFile),{recursive:true});
    await fs.writeFile(InstanceFile,JSON.stringify({schemaVersion:1,parent:{assetId:ParentMaterial.id,subAssetId:null},overrides:{}}));
    const UpdatedAssets = await Bridge.request('assets.refresh');
    const Instance = UpdatedAssets.value.find(Asset => Asset.relativePath === 'Materials/PreviewInstance.materialinstance');
    await Evaluate('window.sacuraDesktop.nativeOpenAssetEditor(' + JSON.stringify({assetId:Instance.id,subAssetId:null}) + ')');
    AssetWindow = BrowserWindow.getAllWindows().find(Candidate => Candidate !== Window);
    await Wait(() => AssetEvaluate("!!document.querySelector('.native-instance-parameter')"));
    await Wait(() => AssetEvaluate("window.sacuraDesktop.nativeStatus().then(Status => Status.viewport?.data?.preview && Status.presentation.frames > 20 && Status.stats?.triangles > 0)"));
    await AssetEvaluate("(()=>{const Field=document.querySelector('.native-instance-parameter input[type=number]'); const Setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set; Setter.call(Field,'0.123'); Field.dispatchEvent(new Event('input',{bubbles:true})); Field.dispatchEvent(new FocusEvent('focusout',{bubbles:true}));})()");
    await Wait(() => AssetEvaluate("Array.from(document.querySelectorAll('.native-instance-parameter button')).some(Button=>Button.textContent==='Из родительского материала'&&!Button.disabled)"));
    await Delay(2000);
    await fs.writeFile(path.join(Output,'material-instance-editor.png'),(await AssetWindow.webContents.capturePage()).toPNG());
    await AssetEvaluate("Array.from(document.querySelectorAll('.native-instance-parameter button')).find(Button=>Button.textContent==='Из родительского материала'&&!Button.disabled).click()");
    await Wait(() => AssetEvaluate("!document.querySelector('[aria-label=\"Несохранённые изменения\"]')"));
    AssetWindow.close();
    await Wait(() => AssetWindow.isDestroyed());
    AssetWindow = null;

    const UiMaterial = UpdatedAssets.value.find(Asset => Asset.typeId === 'Material' && Asset.name === 'UIImage.material');
    assert.ok(UiMaterial);
    await Evaluate('window.sacuraDesktop.nativeOpenAssetEditor(' + JSON.stringify({assetId:UiMaterial.id,subAssetId:null}) + ')');
    AssetWindow = BrowserWindow.getAllWindows().find(Candidate => Candidate !== Window);
    await Wait(() => AssetEvaluate("!!document.querySelector('.native-ui-material-preview img')?.naturalWidth"));
    await fs.writeFile(path.join(Output,'ui-material-editor.png'),(await AssetWindow.webContents.capturePage()).toPNG());
    AssetWindow.close();
    await Wait(() => AssetWindow.isDestroyed());
    AssetWindow = null;

    assert.deepEqual(Errors.filter(Message => /Uncaught|TypeError|ReferenceError/.test(Message)), []);
    await fs.writeFile(path.join(Output, 'result.json'), JSON.stringify({passed: true, errors: Errors}, null, 2));
    console.log('Asset transfer UI check passed: ' + Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Error) {
    await fs.writeFile(path.join(Output, 'result.json'), JSON.stringify({passed: false, error: Error.stack, errors: Errors}, null, 2));
    console.error(Error);
    await Capture('failure.png').catch(() => {});
    if (AssetWindow && !AssetWindow.isDestroyed()) {AssetWindow.destroy();}
    await GetViewport().stop();
    Application.exit(1);
  }
};
