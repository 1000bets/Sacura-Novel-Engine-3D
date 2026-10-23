const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');
exports.run = async ({app:Application, window:Window, editors:Editors, windows:Windows, getViewport:GetViewport, createNativeProject:CreateNativeProject, restore:Restore = false}) => {
  const Output = Path.resolve(__dirname, '../../build/workspace-tabs-check-' + Date.now());
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  async function Wait(Predicate, Label) {
    const Deadline = Date.now() + 45000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Workspace check timed out: ' + Label);
      }
      await Delay(150);
    }
  }
  async function DragTab(Identifier, Target, Selector, Outside = false) {
    let SourceWindow;
    let Start;
    for (const Candidate of [Window,...Windows.values()]) {
      Start = await Candidate.webContents.executeJavaScript(`(() => {
        const Tab = document.getElementById('editor-tab-' + ${JSON.stringify(Identifier)});
        if (!Tab) { return null; }
        const Bounds = Tab.getBoundingClientRect();
        return {x:Math.round(Bounds.x + Bounds.width / 2),y:Math.round(Bounds.y + Bounds.height / 2),draggable:Tab.draggable};
      })()`);
      if (Start) {SourceWindow = Candidate;break;}
    }
    Assert.ok(SourceWindow, 'Drag source exists');
    Assert.equal(Start.draggable, false, 'Tabs do not enter the OS HTML drag loop');
    SourceWindow.show();
    SourceWindow.focus();
    await Delay(100);
    if (SourceWindow !== Target && !Outside) {
      SourceWindow.setPosition(-700, 200);
      Target.setPosition(400, 100);
      await Delay(200);
    }
    const Bounds = SourceWindow.getContentBounds();
    let End = {x:-60,y:-60};
    if (!Outside) {
      const TargetBounds = Target.getContentBounds();
      const Point = await Target.webContents.executeJavaScript(`(() => {
        const Element = document.querySelector(${JSON.stringify(Selector)});
        if (!Element) {throw Error('Missing drop target');}
        const Bounds = Element.getBoundingClientRect();
        return {x:Math.round(Bounds.x+Bounds.width/2),y:Math.round(Bounds.y+Bounds.height/2)};
      })()`);
      End = {x:TargetBounds.x+Point.x-Bounds.x,y:TargetBounds.y+Point.y-Bounds.y};
    }
    await SourceWindow.webContents.executeJavaScript(`window.DragEvents = []; for (const Name of ['pointerdown','pointermove','pointerup','lostpointercapture']) {document.addEventListener(Name, Event => window.DragEvents.push({type:Event.type,x:Event.clientX,y:Event.clientY,sx:Event.screenX,sy:Event.screenY,target:Event.target.id}),true);}`);
    SourceWindow.webContents.sendInputEvent({type:'mouseMove',x:Start.x,y:Start.y,globalX:Bounds.x+Start.x,globalY:Bounds.y+Start.y});
    SourceWindow.webContents.sendInputEvent({type:'mouseDown',x:Start.x,y:Start.y,globalX:Bounds.x+Start.x,globalY:Bounds.y+Start.y,button:'left',clickCount:1});
    await Delay(150);
    SourceWindow.webContents.sendInputEvent({type:'mouseMove',...End,globalX:Bounds.x+End.x,globalY:Bounds.y+End.y,modifiers:['leftbuttondown']});
    await Delay(150);
    if (!Outside) {
      const Preview = await Target.webContents.executeJavaScript('!!document.querySelector(".workspace-drop-preview")');
      if (!Preview) {
        console.error('Drag diagnostics', {Identifier,Start,End,Bounds,target:Target.getContentBounds(),events:await SourceWindow.webContents.executeJavaScript('window.DragEvents'),surfaces:await Evaluate('window.sacuraDesktop.workspaceDragSurfaces()')});
      }
      Assert.ok(Preview, 'Destination preview');
    }
    SourceWindow.webContents.sendInputEvent({type:'mouseUp',...End,globalX:Bounds.x+End.x,globalY:Bounds.y+End.y,button:'left',clickCount:1});
    await Delay(400);
  }
  async function Drop(Target, Identifier, Area) {
    await DragTab(Identifier, Target, '[data-dock-strip="' + Area + '"]');
  }
  try {
    await FileSystem.mkdir(Output, {recursive:true});
    setTimeout(() => Application.exit(1), 180000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop'), 'Renderer');
    if (Restore) {
      await Wait(() => Editors.size === 1 && Windows.size === 1, 'Restored sessions and detached window');
      const Editor = [...Editors.values()][0];
      await Wait(async () => (await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Editor.identifier)})`)).presentation.frames > 10, 'Restored preview frames');
      const Snapshot = (await Editor.viewport.bridge.request('assetDocument.execute',{...Editor.identity,operation:'get'})).value;
      Assert.ok(Snapshot.source.includes('Surface.Albedo = float3(0.2, 0.1, 0.0)'));
      const Popup = [...Windows.values()][0];
      Assert.ok(await Popup.webContents.executeJavaScript('!!document.querySelector(".workspace-split")'));
      Assert.equal(await Popup.webContents.executeJavaScript('document.querySelectorAll("[role=tab]").length'),2);
      await FileSystem.writeFile(Path.join(Output,'restored.png'),(await Popup.webContents.capturePage()).toPNG());
      await FileSystem.writeFile(Path.join(Output,'result.json'),JSON.stringify({ok:true,checks:['Restored window','Restored split groups','Restored asset identity','Saved shader source','Live restored preview']},null,2));
      console.log('Workspace restore PASS:',Output);
      await Editors.closeAll();
      await GetViewport().stop();
      Application.exit(0);
      return;
    }
    await CreateNativeProject('WorkspaceTabsCheck', Output, 'Empty');
    await Wait(() => Evaluate('!!document.querySelector("#editor-tab-scene-view")'), 'Base tabs');
    const TexturePath = Path.join(Path.dirname(GetViewport().project),'Content','DockTexture.png');
    await FileSystem.writeFile(TexturePath,Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==','base64'));
    await GetViewport().bridge.request('assets.refresh',{});
    const Catalogue = await GetViewport().bridge.request('assets.list',{});
    const Texture = Catalogue.value.find(Entry => Entry.name === 'DockTexture.png');
    Assert.ok(Texture);
    const TextureDocument = await Editors.open({assetId:Texture.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-texture-image-area canvas")'), 'Texture tab');
    const TextureWindow = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,TextureDocument.id);
    await Wait(() => TextureWindow.webContents.executeJavaScript('!!document.querySelector(".native-texture-image-area canvas")'), 'Detached texture');
    await Drop(Window,TextureDocument.id,'workspace');
    Assert.ok(await Editors.close(TextureDocument.id));
    await Wait(() => Windows.size === 0, 'Texture window closed');
    const Actions = await GetViewport().bridge.request('editor.actions',{});
    const Cube = Actions.value.find(Entry => Entry.assetId && /cube/i.test(Entry.label));
    const MeshDocument = await Editors.open({assetId:Cube.assetId,subAssetId:Cube.subAssetId});
    await Wait(async () => (await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(MeshDocument.id)})`)).presentation.frames > 5, 'Mesh preview');
    const MeshWindow = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,MeshDocument.id);
    Assert.equal(await MeshWindow.webContents.executeJavaScript('typeof window.sacuraDesktop'), 'undefined');
    await Drop(Window,MeshDocument.id,'workspace');
    Assert.ok(await Editors.close(MeshDocument.id));
    await Wait(() => Windows.size === 0, 'Mesh window closed');
    await FileSystem.writeFile(Path.join(Output,'initial.png'), (await Window.webContents.capturePage()).toPNG());
    await Evaluate(`[...document.querySelectorAll('[data-dock-strip=scene] button')].find(Element => Element.getAttribute('aria-label') === 'Открепить окно').click()`);
    await Wait(() => Windows.size === 1, 'Detached window');
    const [Name, Popup] = [...Windows][0];
    const Area = Name.slice(7);
    await Wait(() => Popup.webContents.executeJavaScript('!!document.querySelector("canvas[data-native-surface]")'), 'Detached viewport');
    await Delay(1200);
    await FileSystem.writeFile(Path.join(Output,'detached.png'), (await Popup.webContents.capturePage()).toPNG());
    await Drop(Popup, 'hierarchy-view', Area);
    Assert.equal(await Popup.webContents.executeJavaScript('document.querySelectorAll("[role=tab]").length'), 2);
    await Drop(Window, 'scene-view', 'scene');
    await Drop(Window, 'hierarchy-view', 'hierarchy');
    await Wait(() => Windows.size === 0, 'Empty window cleanup');
    const Created = await GetViewport().bridge.request('assets.create', {factoryId:'editor.CreateMaterialAssetFactory',path:'DockingMaterial.material'});
    Assert.ok(Created.ok, JSON.stringify(Created));
    const Descriptor = await Editors.open({assetId:Created.value.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-material-editor")'), 'Material tab');
    await DragTab(Descriptor.id, Window, '[data-dock-root="scene"] .dock-region-content');
    Assert.equal(await Evaluate(`document.getElementById('editor-tab-' + ${JSON.stringify(Descriptor.id)}).closest('[data-dock-root]').dataset.dockRoot`),'scene');
    await FileSystem.writeFile(Path.join(Output,'material-dropped-on-viewport.png'), (await Window.webContents.capturePage()).toPNG());
    await DragTab(Descriptor.id, Window, null, true);
    await Wait(() => Windows.size === 1, 'Pointer release outside detaches');
    await Drop(Window, Descriptor.id, 'workspace');
    await Wait(() => Windows.size === 0, 'Pointer return from detached window');
    const Document = Editors.get(Descriptor.id);
    const Session = Document.viewport.bridge.context.session;
    await Wait(async () => (await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Descriptor.id)})`)).presentation.frames > 5, 'Material frames');
    const Original = (await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'get'})).value.source;
    const Source = Original.replace('return Surface;', 'Surface.Albedo = float3(0.2, 0.1, 0.0);\n    return Surface;');
    await Evaluate(`(() => {window.WorkspaceDraft = document.querySelector('.cm-content'); window.WorkspaceDraft.focus(); document.execCommand('selectAll'); document.execCommand('insertText', false, ${JSON.stringify(Source)});})()`);
    await Wait(async () => (await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'get'})).value?.source === Source, 'Source draft');
    await Wait(async () => (await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'get'})).value?.preparing === false, 'Material ready');
    await Evaluate(`[...document.querySelectorAll('[data-dock-strip=workspace] button')].find(Element => Element.getAttribute('aria-label') === 'Открепить окно').click()`);
    await Wait(() => Windows.size === 1, 'Material detached');
    const [MaterialName, MaterialWindow] = [...Windows][0];
    await Wait(() => MaterialWindow.webContents.executeJavaScript('!!document.querySelector(".native-material-editor")'), 'Material portal');
    await Delay(1500);
    Assert.equal(await Evaluate(`window.WorkspaceDraft === window.open('', ${JSON.stringify(MaterialName)}).document.querySelector('.cm-content')`), true);
    const FirstFrame = (await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Descriptor.id)})`)).presentation.frames;
    await Delay(500);
    Assert.ok((await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Descriptor.id)})`)).presentation.frames > FirstFrame);
    Window.minimize();
    const BeforeMinimize = (await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Descriptor.id)})`)).presentation.frames;
    await Delay(1000);
    Assert.ok((await Evaluate(`window.sacuraDesktop.nativeDocumentStatus(${JSON.stringify(Descriptor.id)})`)).presentation.frames > BeforeMinimize);
    Window.restore();
    await FileSystem.writeFile(Path.join(Output,'material.png'), (await MaterialWindow.webContents.capturePage()).toPNG());
    Assert.equal(Document.viewport.bridge.context.session, Session);
    await Drop(Window, Descriptor.id, 'workspace');
    await Wait(() => Windows.size === 0, 'Material returned');
    Assert.equal(Document.viewport.bridge.context.session, Session);
    const GetDocument = async () => (await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'get'})).value;
    Assert.equal((await GetDocument()).source,Source);
    let Snapshot = await GetDocument();
    Assert.ok((await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'undo',revision:Snapshot.revision})).ok);
    Assert.equal((await GetDocument()).source, Original);
    Snapshot = await GetDocument();
    Assert.ok((await Document.viewport.bridge.request('assetDocument.execute',{assetId:Created.value.id,operation:'redo',revision:Snapshot.revision})).ok);
    Assert.equal((await GetDocument()).source, Source);
    const Dialog = require('electron').dialog;
    const Ask = Dialog.showMessageBox;
    try {
      Dialog.showMessageBox = async () => ({response:2});
      Assert.equal(await Editors.close(Descriptor.id),false);
      Assert.ok(Editors.has(Descriptor.id));
      Dialog.showMessageBox = async () => ({response:0});
      Assert.ok(await Editors.close(Descriptor.id));
    } finally {
      Dialog.showMessageBox = Ask;
    }
    Window.setMinimumSize(480,400);
    Window.setSize(640,720);
    await Delay(400);
    await FileSystem.writeFile(Path.join(Output,'compact.png'),(await Window.webContents.capturePage()).toPNG());
    Window.setSize(1440,900);
    const Reopened = await Editors.open({assetId:Created.value.id});
    await Wait(() => Evaluate('!!document.querySelector(".native-material-editor")'), 'Reopened saved material');
    const RestoredWindow = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,Reopened.id);
    const RestoredName = [...Windows].find(([,Candidate]) => Candidate === RestoredWindow)[0];
    Window.webContents.send('native-reveal-asset',{assetId:Created.value.id});
    await Wait(() => Evaluate('!!document.getElementById("editor-tab-files")'), 'Content Browser tab');
    await Drop(RestoredWindow,'files',RestoredName.slice(7));
    await RestoredWindow.webContents.executeJavaScript(`[...document.querySelectorAll('button')].find(Element => Element.textContent === 'Новая группа снизу').click()`);
    await Wait(() => RestoredWindow.webContents.executeJavaScript('!!document.querySelector(".workspace-split.bottom")'), 'Detached split');
    await FileSystem.writeFile(Path.join(Output,'split.png'),(await RestoredWindow.webContents.capturePage()).toPNG());
    await Delay(1700);
    await FileSystem.writeFile(Path.join(Output,'restore.json'),JSON.stringify({project:GetViewport().project,userData:Application.getPath('userData')},null,2));
    await Editors.closeAll(true);
    await FileSystem.writeFile(Path.join(Output,'result.json'),JSON.stringify({ok:true,checks:['Texture detach and redock','Mesh preview tab','Unprivileged detached document','Base tabs','Native detach','Cross-window grouping','Redock','Material session identity','Live GPU frames','Draft DOM identity','Source Undo/Redo','Cancel close','Save on close','Compact layout','Detached window with minimized main','Split material and Content Browser']},null,2));
    console.log('Workspace tabs PASS:',Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error('Workspace tabs FAIL:',Failure);
    const Diagnostics = await Evaluate(`(async () => Promise.all((await window.sacuraDesktop.nativeDocuments()).map(async Entry => ({...Entry,status:await window.sacuraDesktop.nativeDocumentStatus(Entry.id)}))))()`).catch(() => null);
    const Canvases = await Evaluate(`[...document.querySelectorAll('canvas[data-native-surface]')].map(Element => ({owner:Element.dataset.nativeOwner,width:Element.width,height:Element.height,visible:Element.checkVisibility()}))`);
    await FileSystem.writeFile(Path.join(Output,'result.json'),JSON.stringify({ok:false,error:Failure.stack,Diagnostics,Canvases},null,2));
    await FileSystem.writeFile(Path.join(Output,'failure.png'), (await Window.webContents.capturePage()).toPNG());
    for (const Editor of Editors.values()) {
      await Editor.viewport.stop();
    }
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
