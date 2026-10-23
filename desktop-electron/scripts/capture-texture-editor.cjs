const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, windows: Windows, editors: Editors, getViewport: GetViewport, output: Output}) => {
  let Editor;
  let EditorWindow;
  const Destination = Path.resolve(Output);
  const Errors = [];
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Texture editor check timed out');
      }
      await Delay(100);
    }
  }
  try {
    await FileSystem.mkdir(Path.dirname(Destination), {recursive: true});
    await Wait(() => GetViewport()?.bridge.connected);
    const Catalogue = await GetViewport().bridge.request('assets.list', {});
    Assert.equal(Catalogue.ok, true);
    const Texture = Catalogue.value.find(Asset => Asset.typeId === 'Texture' && Asset.name === 'TextureEditorCheck.png')
      || Catalogue.value.find(Asset => Asset.typeId === 'Texture');
    Assert.ok(Texture, 'Texture asset required');
    await Wait(() => !Window.webContents.isLoading());
    Window.webContents.send('native-reveal-asset', {assetId: Texture.id});
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".asset-file-tile.selected")'));
    await Window.webContents.executeJavaScript('document.querySelector(".asset-file-tile.selected").dispatchEvent(new MouseEvent("dblclick",{bubbles:true}))');
    await Wait(() => Array.from(Editors.values()).some(Entry => Entry.identity.assetId === Texture.id && Entry.ready));
    Editor = Array.from(Editors.values()).find(Entry => Entry.identity.assetId === Texture.id);
    EditorWindow = await require('./workspace-capture.cjs').DetachDocument(Window, Windows, Editor.identifier);
    EditorWindow.webContents.on('console-message', Details => {
      if (Details.level === 'error') {
        Errors.push(Details.message);
      }
    });
    const Evaluate = Source => EditorWindow.webContents.executeJavaScript(Source);
    await Wait(() => Evaluate('!!document.querySelector(".native-texture-image-area canvas")?.width'));
    await Editors.open({assetId: Texture.id});
    Assert.equal(Array.from(Editors.values()).filter(Entry => Entry.identity.assetId === Texture.id).length, 1);
    const Snapshot = await Editor.viewport.bridge.request('assetDocument.execute', {assetId: Texture.id, operation: 'get'});
    Assert.equal(Snapshot.ok, true);
    Assert.equal(Snapshot.value.type, 'Texture');
    Assert.equal(Snapshot.value.readOnly, true);
    Assert.equal(Snapshot.value.dirty, false);
    Assert.ok(Snapshot.value.document.width > 0);
    Assert.ok(Snapshot.value.document.previewWidth <= 1024);
    const Rejected = await Editor.viewport.bridge.request('assetDocument.execute', {assetId: Texture.id, operation: 'edit', revision: Snapshot.value.revision, value: {}});
    Assert.equal(Rejected.ok, false);
    Assert.equal(Rejected.error.code, 'READ_ONLY');
    const Pixel = await Evaluate('Array.from(document.querySelector("canvas").getContext("2d").getImageData(0,0,1,1).data)');
    await Evaluate('document.querySelector("select[aria-label=Каналы]").value="A";document.querySelector("select[aria-label=Каналы]").dispatchEvent(new Event("change",{bubbles:true}))');
    await Wait(async () => {
      const Alpha = await Evaluate('Array.from(document.querySelector("canvas").getContext("2d").getImageData(0,0,1,1).data)');
      return Alpha[0] === Pixel[3] && Alpha[1] === Pixel[3] && Alpha[2] === Pixel[3] && Alpha[3] === 255;
    });
    await Evaluate('Array.from(document.querySelectorAll("button")).find(Button=>Button.textContent==="1:1").click()');
    await Wait(() => Evaluate('document.querySelector("input[type=number]").value==="100"'));
    await Evaluate('document.querySelector("select[aria-label=Фон]").value="light";document.querySelector("select[aria-label=Фон]").dispatchEvent(new Event("change",{bubbles:true}))');
    await Wait(() => Evaluate('!!document.querySelector(".native-texture-surface.light")'));
    await Evaluate('document.querySelector("select[aria-label=Каналы]").value="RGBA";document.querySelector("select[aria-label=Каналы]").dispatchEvent(new Event("change",{bubbles:true}));document.querySelector("select[aria-label=Фон]").value="checker";document.querySelector("select[aria-label=Фон]").dispatchEvent(new Event("change",{bubbles:true}));Array.from(document.querySelectorAll("button")).find(Button=>Button.textContent==="Вписать").click()');
    EditorWindow.setSize(1500, 950);
    await Delay(500);
    await FileSystem.writeFile(Destination, (await EditorWindow.webContents.capturePage()).toPNG());
    EditorWindow.setSize(800, 650);
    await Delay(500);
    Assert.equal(await Evaluate('document.documentElement.scrollWidth <= innerWidth'), true);
    const Compact = Path.join(Path.dirname(Destination), Path.parse(Destination).name + '-compact.png');
    await FileSystem.writeFile(Compact, (await EditorWindow.webContents.capturePage()).toPNG());
    Assert.deepEqual(Errors.filter(Message => /Uncaught|TypeError|ReferenceError/.test(Message)), []);
    Assert.equal(await Editors.closeAll(), true);
    await Wait(() => Editors.size === 0);
    const Broken = Catalogue.value.find(Asset => Asset.name === 'TextureEditorBroken.png');
    if (Broken) {
      await Editors.open({assetId: Broken.id});
      const BrokenEditor = Array.from(Editors.values()).find(Entry => Entry.identity.assetId === Broken.id);
      const BrokenWindow = await require('./workspace-capture.cjs').DetachDocument(Window, Windows, BrokenEditor.identifier);
      await Wait(() => BrokenWindow.webContents.executeJavaScript('!!document.querySelector("[role=alert]")?.textContent'));
      Assert.equal(await BrokenWindow.webContents.executeJavaScript('!!document.querySelector("canvas")'), false);
      Assert.equal(await Editors.closeAll(), true);
      await Wait(() => Editors.size === 0);
    }
    await Assert.rejects(Editors.open({assetId: '00000000-0000-0000-0000-000000000000'}), /unavailable/);
    console.log('Texture editor check passed:', Destination, Compact);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error('Texture editor check failed:', Failure);
    if (EditorWindow && !EditorWindow.isDestroyed()) {
      await FileSystem.writeFile(Path.join(Path.dirname(Destination), 'TextureEditor-failure.png'), (await EditorWindow.webContents.capturePage()).toPNG()).catch(() => {});
    }
    for (const Entry of Editors.values()) {
      await Entry.viewport?.stop();
    }
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
