const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/material-drop-check-' + Date.now());
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  const Checks = [];
  async function Wait(Predicate, Description) {
    const Deadline = Date.now() + 20000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Material drop check timed out: ' + Description);
      }
      await Delay(100);
    }
  }
  async function Command(Method, Params = {}) {
    const Result = await GetViewport().bridge.request(Method, Params);
    Assert.equal(Result.ok, true, JSON.stringify(Result));
    return Result.value;
  }
  async function Drop(AssetId, Horizontal = 0.5, Vertical = 0.5) {
    await Wait(() => Evaluate('document.querySelector("[data-native-drop-enabled]")?.dataset.nativeDropEnabled === "true"'), 'Viewport accepts asset drops');
    await Evaluate(`(() => {
      const Tile = document.querySelector('[data-asset-id="${AssetId}"]');
      if (!Tile?.draggable) { throw Error('Asset tile is not draggable'); }
      const Transfer = new DataTransfer();
      Tile.dispatchEvent(new DragEvent('dragstart', {bubbles:true, cancelable:true, dataTransfer:Transfer}));
      if (!Transfer.getData('application/x-sacura-native-asset')) { throw Error('Missing asset payload'); }
      const Surface = document.querySelector('[data-native-surface]').parentElement;
      const Bounds = Surface.getBoundingClientRect();
      Surface.dispatchEvent(new DragEvent('drop', {bubbles:true, cancelable:true, dataTransfer:Transfer,
        clientX:Bounds.left + Bounds.width * ${Horizontal}, clientY:Bounds.top + Bounds.height * ${Vertical}}));
    })()`);
    await Delay(400);
  }
  try {
    Window.setIgnoreMouseEvents(true);
    await FileSystem.mkdir(Output, {recursive:true});
    setTimeout(() => Application.exit(1), 120000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'), 'Editor loaded');
    await CreateNativeProject('MaterialDropCheck', Output, 'Empty');
    await Wait(() => GetViewport()?.state.phase === 'ready', 'Native viewport');
    const Actions = await Command('editor.actions');
    const Cube = Actions.find(Action => Action.assetId && /cube/i.test(Action.label));
    Assert.ok(Cube, 'Builtin cube action');
    const Object = await Command('assets.place', {assetId:Cube.assetId,subAssetId:Cube.subAssetId});
    const Components = await Command('object.getComponents', {objectId:Object.id});
    const Mesh = Components.find(Component => Component.typeId === 'engine.MeshRendererComponent');
    const Target = {objectId:Object.id,componentId:Mesh.id};
    await Wait(async () => (await Command('mesh.inspect', Target)).slots.length > 0, 'Loaded mesh slots');
    const Original = await Command('mesh.inspect', Target);
    const Material = await Command('assets.create', {factoryId:'editor.CreateMaterialAssetFactory',path:'DropMaterial.material'});
    const Instance = await Command('assets.create', {factoryId:'editor.CreateMaterialInstanceAssetFactory',path:'DropInstance.materialinstance',settings:{parent:{assetId:Material.id}}});
    await Wait(() => Evaluate(`!!document.querySelector('.native-object-inspector button[aria-label="Выделить в Проводнике"]:not(:disabled)')`), 'Mesh reference');
    Assert.equal(await Evaluate('document.querySelectorAll(".native-component-overview, .component-instances-inspector > input").length'), 0);
    async function OpenComponentMenu() {
      await Evaluate(`(() => {const Button=document.querySelector('.component-add-button');Button.scrollIntoView({block:'nearest'});Button.click();})()`);
      await Wait(() => Evaluate('!!document.querySelector(".component-add-menu input")'), 'Component popup');
    }
    async function SearchComponents(Text) {
      await Evaluate(`(() => {const Input=document.querySelector('.component-add-menu input');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(Text)});Input.dispatchEvent(new Event('input',{bubbles:true}));})()`);
      await Delay(150);
    }
    await OpenComponentMenu();
    await FileSystem.writeFile(Path.join(Output, 'ComponentAdd.png'), (await Window.webContents.capturePage()).toPNG());
    await SearchComponents('NoSuchComponent123');
    Assert.equal(await Evaluate('document.querySelectorAll(".component-add-menu [role=menuitem]").length'),0);
    Assert.ok(await Evaluate('document.querySelector(".component-add-menu").textContent.includes("Ничего не найдено")'));
    await SearchComponents('engine.LightComponent');
    Assert.equal(await Evaluate('document.querySelectorAll(".component-add-menu [role=menuitem]").length'),1);
    await Evaluate(`document.querySelector('.component-add-menu input').dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}))`);
    await Wait(async () => (await Command('object.getComponents',{objectId:Object.id})).some(Component=>Component.typeId==='engine.LightComponent'), 'Light added from popup');
    Assert.equal(await Evaluate('!!document.querySelector(".component-add-menu")'),false);
    await Command('editor.undo');
    Assert.equal((await Command('object.getComponents',{objectId:Object.id})).length,Components.length);
    await Command('editor.redo');
    Assert.equal((await Command('object.getComponents',{objectId:Object.id})).length,Components.length+1);
    await Command('editor.undo');
    await OpenComponentMenu();
    await Evaluate(`document.querySelector('.component-add-menu input').dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))`);
    await Wait(() => Evaluate('!document.querySelector(".component-add-menu")'), 'Escape dismisses popup');
    Checks.push('Component popup search, empty results, Enter addition, Escape dismissal and Undo/Redo');
    await Evaluate(`document.querySelector('.native-component-card > .component-instance-row').click()`);
    await Wait(() => Evaluate('!document.querySelector(".native-component-card:first-child > .component-instance-properties")'), "Collapsed component");
    await Evaluate(`document.querySelector('.native-component-card > .component-instance-row').click()`);
    await Wait(() => Evaluate('!!document.querySelector(".native-component-card:first-child > .component-instance-properties")'), "Expanded component");
    await FileSystem.writeFile(Path.join(Output, 'ComponentGrouping.png'), (await Window.webContents.capturePage()).toPNG());
    const OriginalBounds = Window.getBounds();
    Window.setMinimumSize(480,400);
    Window.setSize(640,720);
    await Delay(300);
    await Evaluate(`[...document.querySelectorAll('button')].find(Button => Button.textContent.trim() === 'Свойства').click()`);
    await Wait(() => Evaluate(`!!document.querySelector('.editor-workspace.compact-inspector') && document.querySelector('.native-component-card').getBoundingClientRect().width > 0`), 'Compact Inspector');
    await Delay(300);
    await OpenComponentMenu();
    await FileSystem.writeFile(Path.join(Output, 'ComponentAddCompact.png'), (await Window.webContents.capturePage()).toPNG());
    Assert.ok(await Evaluate(`(() => {const Bounds=document.querySelector('.component-add-menu').getBoundingClientRect();return Bounds.left>=0&&Bounds.top>=0&&Bounds.right<=innerWidth&&Bounds.bottom<=innerHeight;})()`));
    await Evaluate(`document.body.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}))`);
    await Wait(() => Evaluate('!document.querySelector(".component-add-menu")'), 'Outside click dismisses popup');
    await FileSystem.writeFile(Path.join(Output, 'ComponentGroupingCompact.png'), (await Window.webContents.capturePage()).toPNG());
    Assert.ok(await Evaluate(`(() => {const Card = document.querySelector('.native-component-card');return Card.scrollWidth <= Card.clientWidth + 1;})()`));
    Window.setBounds(OriginalBounds);
    await Delay(300);
    Checks.push('Component card groups properties, collapses and expands without duplicate filters');
    await Evaluate(`document.querySelector('.native-object-inspector button[aria-label="Выделить в Проводнике"]:not(:disabled)').click()`);
    await Wait(() => Evaluate('!!document.querySelector(".asset-breadcrumb button")'), 'Content Browser');
    await Evaluate('document.querySelector(".asset-breadcrumb button").click()');
    await Wait(() => Evaluate('!!document.querySelector(".asset-file-tile[data-asset-folder=Game]")'), 'Game folder');
    await Evaluate('document.querySelector(".asset-file-tile[data-asset-folder=Game]").dispatchEvent(new MouseEvent("dblclick", {bubbles:true}))');
    await Wait(() => Evaluate(`!!document.querySelector('[data-asset-id="${Material.id}"]')`), 'Material tile');
    await Wait(async () => (await Command('scene.pickMesh', {horizontal:0.5,vertical:0.5}))?.objectId === Object.id, 'Mesh picking');
    const Other = await Command('object.create', {name:'UnrelatedSelection'});
    await Command('selection.set', {objectId:Other.id});
    await Drop(Material.id);
    await Wait(async () => (await Command('mesh.inspect', Target)).slots.every(Slot => Slot.assetId === Material.id && !Slot.inherited), 'Material assigned');
    Checks.push('Content Browser material drag targets mesh under cursor, not selection');
    await Command('editor.undo');
    Assert.deepEqual((await Command('mesh.inspect', Target)).slots, Original.slots);
    await Command('editor.redo');
    Assert.ok((await Command('mesh.inspect', Target)).slots.every(Slot => Slot.assetId === Material.id));
    Checks.push('Single-step Undo and Redo preserve original slot inheritance');
    await Drop(Instance.id);
    await Wait(async () => (await Command('mesh.inspect', Target)).slots.every(Slot => Slot.assetId === Instance.id), 'Material instance assigned');
    Checks.push('Material Instance drag');
    Assert.equal(await Command('scene.pickMesh', {horizontal:0.01,vertical:0.01}), null);
    await Drop(Material.id, 0.01, 0.01);
    Assert.ok((await Command('mesh.inspect', Target)).slots.every(Slot => Slot.assetId === Instance.id));
    Checks.push('Empty-space drop leaves materials unchanged');
    await Command('play.start');
    const Rejected = await GetViewport().bridge.request('scene.pickMesh', {horizontal:0.5,vertical:0.5});
    Assert.equal(Rejected.ok, false);
    Assert.equal(Rejected.error.code, 'PLAY_READ_ONLY');
    await Command('play.stop');
    Checks.push('Play rejects material drop targeting');
    await FileSystem.writeFile(Path.join(Output, 'Editor.png'), (await Window.webContents.capturePage()).toPNG());
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:true,Checks}, null, 2));
    console.log('Material drop check passed: ' + Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error(Failure);
    await FileSystem.writeFile(Path.join(Output,'Failure.png'), (await Window.webContents.capturePage()).toPNG());
    console.error('Material drop UI:', await Evaluate(`document.querySelector('.editor-status')?.textContent`));
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:false,Checks,error:Failure.stack}, null, 2));
    await GetViewport()?.stop();
    Application.exit(1);
  }
};

