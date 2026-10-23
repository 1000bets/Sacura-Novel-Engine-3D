const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, windows: Windows, editors: Editors, getViewport: GetViewport, output: Output}) => {
  try {
    const Deadline = Date.now() + 60000;
    while (!GetViewport()?.bridge.connected) {
      if (Date.now() > Deadline) {
        throw Error('Material capture: native host connection timed out');
      }
      await Delay(200);
    }
    const Catalogue = await GetViewport().bridge.request('assets.list', {});
    if (!Catalogue.ok) {
      throw Error(Catalogue.error.message);
    }
    const Material = Catalogue.value.find(Asset => Asset.typeId === 'Material' && Asset.path.startsWith('/Game/'))
      || Catalogue.value.find(Asset => Asset.path === '/Engine/Materials/StandardPBR.material')
      || Catalogue.value.find(Asset => Asset.typeId === 'Material');
    if (!Material) {
      throw Error('Material capture: no material assets available');
    }
    await Editors.open({assetId: Material.id});
    const Editor = Array.from(Editors.values()).find(Entry => Entry.identity.assetId === Material.id);
    const EditorWindow = await require('./workspace-capture.cjs').DetachDocument(Window, Windows, Editor.identifier);
    if (process.env.SACURA_MATERIAL_CAPTURE_SHAPE)
    {
      const Selection = await Editor.viewport.bridge.request('assetDocument.execute', {
        assetId: Material.id, operation: 'preview', value: {previewShape: process.env.SACURA_MATERIAL_CAPTURE_SHAPE}
      });
      if (!Selection.ok)
      {
        throw Error(Selection.error.message);
      }
      console.log('Material capture shape:', process.env.SACURA_MATERIAL_CAPTURE_SHAPE);
    }
    EditorWindow.setSize(1600, 1000);
    let Document;
    do
    {
      await Delay(250);
      Document = await Editor.viewport.bridge.request('assetDocument.execute', {assetId: Material.id, operation: 'get'});
      if (!Document.ok)
      {
        throw Error(Document.error.message);
      }
      if (Date.now() > Deadline)
      {
        throw Error('Material capture: shader preparation timed out');
      }
    }
    while (Document.value.preparing);
    await Delay(1000);
    console.log('Material preview frame:', JSON.stringify(Editor.viewport.state.stats));
    const Destination = Path.resolve(Output);
    await FileSystem.mkdir(Path.dirname(Destination), {recursive: true});
    await FileSystem.writeFile(Destination, (await EditorWindow.webContents.capturePage()).toPNG());
    EditorWindow.setSize(900, 700);
    await Delay(1500);
    const CompactDestination = Path.join(Path.dirname(Destination), Path.parse(Destination).name + '-compact.png');
    await FileSystem.writeFile(CompactDestination, (await EditorWindow.webContents.capturePage()).toPNG());
    console.log('Material capture saved:', Destination, CompactDestination);
    await Editors.closeAll();
    await Delay(200);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error('Material capture failed:', Failure);
    for (const Editor of Editors.values()) {
      await Editor.viewport?.stop();
    }
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
