const {test: Test} = require('node:test');
const Assert = require('node:assert/strict');
const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {spawn: Spawn} = require('node:child_process');
const {setTimeout: Delay} = require('node:timers/promises');
const {BridgeTransport} = require('../bridge/transport.cjs');
const Root = Path.resolve(__dirname, '../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Root, 'build/ElectronNative/Debug/SakuraElectronHost.exe');

Test('individual widget assets preserve authored layouts, validate history, save explicitly and feed native gameplay', {timeout:120000,skip:process.platform !== 'win32'}, async () => {
  const Parent = Path.join(Root, 'build/widget-asset-tests');
  await FileSystem.mkdir(Parent, {recursive:true});
  const Directory = await FileSystem.mkdtemp(Path.join(Parent, 'run-'));
  const Create = Spawn(Host, ['--project-create',Directory,'WidgetProject','builtin-novel'], {windowsHide:true});
  let Output = '';
  Create.stdout.on('data', Bytes => {Output += Bytes;});
  Create.stderr.on('data', Bytes => {Output += Bytes;});
  Assert.equal(await new Promise(Resolve => Create.once('exit', Resolve)), 0, Output);
  const ProjectDirectory = Path.join(Directory, 'WidgetProject');
  const Project = Path.join(ProjectDirectory, 'WidgetProject.project');
  await FileSystem.mkdir(Path.join(ProjectDirectory, 'Content/Textures'), {recursive:true});
  await FileSystem.copyFile(Path.join(Root, 'Samples/RenderValidation/Content/Textures/CutoutMask.png'), Path.join(ProjectDirectory, 'Content/Textures/WidgetImage.png'));
  let Bridge;
  let Process;
  let Exit;
  async function Start() {
    Bridge = await BridgeTransport.listen();
    Output = '';
    Process = Spawn(Host, ['--bridge-headless',Project,Bridge.path], {windowsHide:true,stdio:['pipe','pipe','pipe']});
    Exit = new Promise(Resolve => Process.once('exit', Resolve));
    Process.stdout.on('data', Bytes => {Output += Bytes;});
    Process.stderr.on('data', Bytes => {Output += Bytes;});
    const Deadline = Date.now() + 30000;
    while (!Bridge.connected && Process.exitCode === null && Date.now() < Deadline) {
      await Delay(25);
    }
    Assert.equal(Bridge.connected, true, Output);
  }
  async function Stop() {
    if (Process) {
      Process.stdin.end('{"type":"shutdown"}\n');
      const Watchdog = setTimeout(() => Process.kill(), 10000);
      const Code = await Exit;
      clearTimeout(Watchdog);
      Bridge.close();
      Process = null;
      Assert.equal(Code, 0, Output);
    }
  }
  async function Call(Method, Parameters = {}) {
    const Result = await Bridge.request(Method, Parameters);
    Assert.equal(Result.ok, true, Method + ': ' + JSON.stringify(Result));
    return Result.value;
  }
  try {
    await Start();
    const Factory = (await Call('assets.creationTypes')).find(Type => Type.typeId === 'Widget');
    Assert.ok(Factory);
    await Call('assets.createFolder', {folder:'Game',name:'UI'});
    const Created = await Call('assets.create', {factoryId:Factory.id,path:'UI/Interface.widget',settings:{}});
    const Identity = {assetId:Created.id};
    const File = Path.join(ProjectDirectory, 'Content/UI/Interface.widget');
    const Original = await FileSystem.readFile(File, 'utf8');
    let Snapshot = await Call('assetDocument.execute', {...Identity,operation:'open'});
    Assert.equal(Snapshot.document.format, 'sakura.widget');
    Assert.equal(Snapshot.dirty, false);
    const Edited = structuredClone(Snapshot.document);
    Edited.widget.elements[1].style.color = '#123456';
    Edited.widget.elements[1].layout.x = 123;
    Edited.customProjectData = {retained:true};
    const Texture = (await Call('assets.list')).find(Asset => Asset.typeId === 'Texture');
    Assert.ok(Texture);
    Edited.widget.elements.push({...structuredClone(Edited.widget.elements[1]),id:'widget-image-check',type:'image',role:'none',parentId:null,imageAssetId:Texture.id});
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'edit',revision:Snapshot.revision,value:Edited});
    Assert.equal(Snapshot.dirty, true);
    Assert.equal((await Call('bridge.describe')).assetDocumentsDirty, true);
    Assert.equal(await FileSystem.readFile(File, 'utf8'), Original);
    const Stale = await Bridge.request('assetDocument.execute', {...Identity,operation:'edit',revision:1,value:Edited});
    Assert.equal(Stale.error.code, 'STALE_DOCUMENT');
    const Invalid = structuredClone(Edited);
    Invalid.widget.elements[0].parentId = Invalid.widget.elements[0].id;
    Assert.equal((await Bridge.request('assetDocument.execute', {...Identity,operation:'edit',revision:Snapshot.revision,value:Invalid})).ok, false);
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'undo',revision:Snapshot.revision});
    Assert.equal(Snapshot.dirty, false);
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'redo',revision:Snapshot.revision});
    Assert.deepEqual(Snapshot.document, Edited);
    Assert.equal((await Call('project.save')).assetDocumentsDirty, false);
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'save',revision:Snapshot.revision});
    Assert.equal(Snapshot.dirty, false);
    Assert.deepEqual(JSON.parse(await FileSystem.readFile(File, 'utf8')), Edited);
    await Call('play.start');
    Assert.equal((await Bridge.request('assetDocument.execute', {...Identity,operation:'edit',revision:Snapshot.revision,value:Edited})).error.code, 'PLAY_READ_ONLY');
    await Call('play.stop');
    await FileSystem.appendFile(File, '\n');
    Assert.equal((await Bridge.request('assetDocument.execute', {...Identity,operation:'save',revision:Snapshot.revision})).ok, false);
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'reload',revision:Snapshot.revision});
    Assert.deepEqual(Snapshot.document, Edited);
    const Second = await Call('assets.create', {factoryId:Factory.id,path:'UI/Hud.widget',settings:{}});
    Assert.notEqual(Second.id, Created.id);
    let Hud = await Call('assetDocument.execute', {assetId:Second.id,operation:'open'});
    const HudDocument = structuredClone(Hud.document);
    HudDocument.widget.kind = 'hud';
    HudDocument.controller = 'engine.ui.Hud';
    Hud = await Call('assetDocument.execute', {assetId:Second.id,operation:'edit',revision:Hud.revision,value:HudDocument});
    await Call('assetDocument.execute', {assetId:Second.id,operation:'save',revision:Hud.revision});
    Assert.deepEqual((await Call('assetDocument.execute', {...Identity,operation:'get'})).document, Edited);
    const LibraryShape = {...Edited,widget:undefined,widgets:[Edited.widget]};
    Assert.equal((await Bridge.request('assetDocument.execute', {...Identity,operation:'edit',revision:Snapshot.revision,value:LibraryShape})).ok,false);
    const Settings = await Call('project.settings.get');
    await Call('project.settings.set', {settings:{name:Settings.name,startupScene:Settings.startupScene,startupStory:Settings.startupStory,
      narrativeWidgets:{...Settings.narrativeWidgets,dialogue:Created.id,hud:Second.id}}});
    await Stop();
    const ScriptPath = Path.join(ProjectDirectory, 'Content/Scripts/template_game.py');
    let Script = await FileSystem.readFile(ScriptPath, 'utf8');
    Assert.ok(Script.includes("View['projectTitle'] ="));
    Script = Script.replace("        View['projectTitle'] =", "        View['widgetAssets'] = {'dialogue':'" + Created.id + "','hud':'" + Second.id + "'}\n        View['variables'] = {'hp':73}\n        View['projectTitle'] =");
    await FileSystem.writeFile(ScriptPath, Script);
    await Start();
    Snapshot = await Call('assetDocument.execute', {...Identity,operation:'open'});
    Assert.deepEqual(Snapshot.document, Edited);
    const State = await Call('play.start');
    Assert.equal(State.widgetInstances.find(Entry => Entry.kind === 'dialogue').widget.elements[1].style.color, '#123456');
    Assert.equal(State.widgetInstances.find(Entry => Entry.kind === 'hud').widget.kind, 'hud');
    Assert.equal(State.gameViews[0].variables.hp, 73);
    let ImageSource = '';
    for (let Attempt = 0; Attempt < 200; ++Attempt) {
      const Current = await Call('bridge.describe');
      ImageSource = Current.widgetInstances.find(Entry => Entry.kind === 'dialogue').widget.elements.find(Element => Element.id === 'widget-image-check').src || '';
      if (ImageSource) {
        break;
      }
      await Delay(10);
    }
    Assert.ok(ImageSource.startsWith('data:image/png;base64,'));
    await Call('play.stop');
    console.log('Widget asset checks passed:', Project);
  } finally {
    await Stop();
  }
});
