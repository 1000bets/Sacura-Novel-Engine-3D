const {test: Test} = require('node:test');
const Assert = require('node:assert/strict');
const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {spawn: Spawn} = require('node:child_process');
const {setTimeout: Delay} = require('node:timers/promises');
const {BridgeTransport} = require('../bridge/transport.cjs');
const Root = Path.resolve(__dirname, '../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Root, 'build/ElectronNative/Debug/SakuraElectronHost.exe');

Test('Python widget controllers, Narrative assignments and startup Input use independent runtime instances', {timeout:120000, skip:process.platform !== 'win32'}, async () => {
  const Directory = await FileSystem.mkdtemp(Path.join(Root, 'build/widget-controller-'));
  const Create = Spawn(Host, ['--project-create', Directory, 'Game', 'builtin-novel'], {windowsHide:true});
  let Output = '';
  Create.stdout.on('data', Bytes => {Output += Bytes;});
  Create.stderr.on('data', Bytes => {Output += Bytes;});
  Assert.equal(await new Promise(Resolve => Create.once('exit', Resolve)), 0, Output);
  const ProjectRoot = Path.join(Directory, 'Game');
  const ProjectPath = Path.join(ProjectRoot, 'Game.project');
  const OriginalDescriptor = JSON.parse(await FileSystem.readFile(ProjectPath, 'utf8'));
  OriginalDescriptor.customData = {preserved:true};
  await FileSystem.writeFile(ProjectPath, JSON.stringify(OriginalDescriptor));
  await FileSystem.writeFile(Path.join(ProjectRoot, 'Content/Scripts/widget_test.py'), `import engine
from pathlib import Path

@engine.register_widget(type_id='test.Dialogue', kind='dialogue')
class TestDialogue(engine.WidgetController):
    Fields = {'Text':'', 'Count':0}

    def OnCreate(Self):
        Self.SetField('Count', 1)

    def OnUpdate(Self, Context):
        Self.SetField('Text', Context['Dialogue']['Text'])
        Self.SetVisible(True)

    def OnEvent(Self, Event, Value):
        if Event == 'Increment':
            Self.SetField('Count', Self.GetField('Count') + 1)

    def OnDestroy(Self):
        (Path(__file__).parents[2] / 'Saved/widget-destroyed.txt').write_text(str(Self.GetField('Count')))
`);
  const Bridge = await BridgeTransport.listen();
  const Process = Spawn(Host, ['--bridge-headless', ProjectPath, Bridge.path], {windowsHide:true,stdio:['pipe','pipe','pipe']});
  const Exit = new Promise(Resolve => Process.once('exit', Resolve));
  Process.stdin.on('error', () => {});
  Process.stdout.on('data', Bytes => {Output += Bytes;});
  Process.stderr.on('data', Bytes => {Output += Bytes;});
  async function Call(Method, Parameters = {}) {
    const Result = await Bridge.request(Method, Parameters);
    Assert.equal(Result.ok, true, Method + ': ' + JSON.stringify(Result) + '\n' + Output.slice(-4000));
    return Result.value;
  }
  async function Wait(Predicate) {
    const Deadline = Date.now() + 20000;
    while (!await Predicate()) {
      Assert.ok(Date.now() < Deadline, Output.slice(-6000));
      await Delay(20);
    }
  }
  async function Tap(Code) {
    Process.stdin.write(JSON.stringify({type:'input',event:{kind:'key',code:Code,down:true}}) + '\n');
    await Delay(80);
    Process.stdin.write(JSON.stringify({type:'input',event:{kind:'key',code:Code,down:false}}) + '\n');
    await Delay(80);
  }
  function Editable(Settings) {
    return {name:Settings.name,startupScene:Settings.startupScene,startupStory:Settings.startupStory,
      narrativeWidgets:Settings.narrativeWidgets,narrativeInputActions:Settings.narrativeInputActions,startupInputContexts:Settings.startupInputContexts};
  }
  try {
    await Wait(() => Bridge.connected);
    const Controllers = await Call('widget.controllers');
    Assert.ok(Controllers.some(Entry => Entry.typeId === 'test.Dialogue' && Entry.fields.Count === 0));
    let Settings = await Call('project.settings.get');
    const Assets = await Call('assets.list');
    for (const Identity of Object.values(Settings.narrativeWidgets)) {
      Assert.ok(Assets.some(Asset => Asset.id === Identity && Asset.typeId === 'Widget'));
    }
    Assert.equal(Object.keys(Settings.narrativeInputActions).length, 5);
    Assert.equal(Settings.startupInputContexts.length, 1);
    const Invalid = Editable(Settings);
    Invalid.narrativeWidgets = {...Invalid.narrativeWidgets,dialogue:Settings.narrativeWidgets.hud};
    Assert.equal((await Bridge.request('project.settings.set',{settings:Invalid})).ok,false);
    Assert.deepEqual(JSON.parse(await FileSystem.readFile(ProjectPath,'utf8')),OriginalDescriptor);
    let State = await Call('play.start');
    Assert.equal(State.widgetInstances.length,4,JSON.stringify(State.widgetInstances));
    let Dialogue = State.widgetInstances.find(Entry => Entry.kind === 'dialogue');
    Assert.ok(!Dialogue.error,JSON.stringify(Dialogue));
    Assert.equal(Dialogue.fields.Text, State.dialogue.text);
    const InitialText = Dialogue.fields.Text;
    let Advance = Dialogue.widget.elements.find(Element => Element.event === 'Advance');
    let Value = null;
    if (!Dialogue.fields.CanAdvance) {
      Advance = Dialogue.widget.elements.find(Element => Element.event === 'Choose');
      Value = Dialogue.fields.Choices.find(Choice => Choice.enabled !== false).id;
    }
    State = await Call('widget.event',{instanceId:Dialogue.instanceId,elementId:Advance.id,value:Value});
    await Wait(async () => (await Call('bridge.describe')).dialogue.text !== InitialText);
    State = await Call('bridge.describe');
    Dialogue = State.widgetInstances.find(Entry => Entry.kind === 'dialogue');
    Assert.equal(Dialogue.fields.Text, State.dialogue.text);
    Assert.equal((await Bridge.request('widget.event',{instanceId:Dialogue.instanceId,elementId:'not-an-element'})).ok,false);
    await Tap('Escape');
    Assert.equal((await Call('bridge.describe')).play,'paused');
    await Tap('Escape');
    Assert.equal((await Call('bridge.describe')).play,'playing');
    Assert.equal((await Bridge.request('project.settings.set',{settings:Editable(Settings)})).ok,false);
    await Call('play.stop');
    Assert.equal((await Call('bridge.describe')).widgetInstances.length,0);
    Assert.equal((await Bridge.request('widget.event',{instanceId:Dialogue.instanceId,elementId:Advance.id})).ok,false);
    const Factory = (await Call('assets.creationTypes')).find(Type => Type.typeId === 'Widget');
    const Created = await Call('assets.create',{factoryId:Factory.id,path:'UI/CustomDialogue.widget',settings:{}});
    let Document = await Call('assetDocument.execute',{assetId:Created.id,operation:'open'});
    const Edited = structuredClone(Document.document);
    Edited.controller = 'test.Dialogue';
    const Button = Edited.widget.elements.find(Element => Element.type === 'button');
    Button.event = 'Increment';
    delete Button.enabledField;
    Document = await Call('assetDocument.execute',{assetId:Created.id,operation:'edit',revision:Document.revision,value:Edited});
    await Call('assetDocument.execute',{assetId:Created.id,operation:'save',revision:Document.revision});
    Settings.narrativeWidgets.dialogue = Created.id;
    Settings.startupInputContexts = [];
    Settings = await Call('project.settings.set',{settings:Editable(Settings)});
    Assert.deepEqual(JSON.parse(await FileSystem.readFile(ProjectPath,'utf8')).customData,{preserved:true});
    const SavedAsset = await FileSystem.readFile(Path.join(ProjectRoot,'Content/UI/CustomDialogue.widget'),'utf8');
    State = await Call('play.start');
    Dialogue = State.widgetInstances.find(Entry => Entry.kind === 'dialogue');
    Assert.equal(Dialogue.controller,'test.Dialogue');
    Assert.equal(Dialogue.fields.Count,1);
    State = await Call('widget.event',{instanceId:Dialogue.instanceId,elementId:Button.id});
    Assert.equal(State.widgetInstances.find(Entry => Entry.kind === 'dialogue').fields.Count,2);
    await Tap('Escape');
    Assert.equal((await Call('bridge.describe')).play,'playing');
    Assert.equal(await FileSystem.readFile(Path.join(ProjectRoot,'Content/UI/CustomDialogue.widget'),'utf8'),SavedAsset);
    await Call('play.stop');
    Assert.equal(await FileSystem.readFile(Path.join(ProjectRoot,'Saved/widget-destroyed.txt'),'utf8'),'2');
    State = await Call('play.start');
    Assert.equal(State.widgetInstances.find(Entry => Entry.kind === 'dialogue').fields.Count,1);
    Assert.notEqual(State.widgetInstances.find(Entry => Entry.kind === 'dialogue').instanceId,Dialogue.instanceId);
    await Call('play.stop');
    console.log('Widget controller checks passed:',ProjectPath);
  } catch (Failure) {
    console.error(Output.slice(-8000));
    throw Failure;
  } finally {
    Process.stdin.end('{"type":"shutdown"}\n');
    const Watchdog = setTimeout(() => Process.kill(),10000);
    await Exit;
    clearTimeout(Watchdog);
    Bridge.close();
  }
});
