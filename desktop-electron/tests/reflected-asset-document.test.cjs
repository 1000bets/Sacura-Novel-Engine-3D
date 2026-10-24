const {test:Test} = require('node:test');
const Assert = require('node:assert/strict');
const Files = require('node:fs/promises');
const Path = require('node:path');
const {spawn:Spawn} = require('node:child_process');
const {BridgeTransport} = require('../bridge/transport.cjs');
const Root = Path.resolve(__dirname, '../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Root, 'build/ElectronNative/Debug/SakuraElectronHost.exe');
const Delay = Milliseconds => new Promise(Resolve => setTimeout(Resolve, Milliseconds));

Test('reflected Input documents preserve format, validate edits and retain history', {skip:process.platform !== 'win32', timeout:120000}, async () => {
  const Output = await Files.mkdtemp(Path.join(Root, 'build/reflected-input-test-'));
  const Create = Spawn(Host, ['--project-create', Output, 'InputTest', 'Empty'], {windowsHide:true});
  let CreationOutput = '';
  Create.stdout.on('data', Data => CreationOutput += Data);
  Create.stderr.on('data', Data => CreationOutput += Data);
  Assert.equal(await new Promise(Resolve => Create.once('exit', Resolve)), 0, CreationOutput);
  const ProjectRoot = Path.join(Output, 'InputTest');
  await Files.mkdir(Path.join(ProjectRoot, 'Content/Input'), {recursive:true});
  const Bridge = await BridgeTransport.listen();
  const Child = Spawn(Host, ['--bridge-headless', Path.join(ProjectRoot, 'InputTest.project'), Bridge.path], {windowsHide:true, stdio:['pipe','pipe','pipe']});
  let Log = '';
  Child.stdout.on('data', Data => Log += Data);
  Child.stderr.on('data', Data => Log += Data);
  const Exit = new Promise(Resolve => Child.once('exit', Resolve));
  try {
    const Deadline = Date.now() + 30000;
    while (!Bridge.connected) {
      Assert.ok(Date.now() < Deadline, Log);
      await Delay(30);
    }
    const Call = async (Method, Parameters = {}) => {
      const Result = await Bridge.request(Method, Parameters);
      Assert.equal(Result.ok, true, JSON.stringify(Result));
      return Result.value;
    };
    const Action = await Call('assets.create', {factoryId:'editor.CreateInputActionAssetFactory',path:'Input/TestAction.inputaction'});
    const Context = await Call('assets.create', {factoryId:'editor.CreateInputContextAssetFactory',path:'Input/TestContext.inputcontext'});
    let ActionDocument = await Call('assetDocument.execute', {assetId:Action.id, operation:'open'});
    Assert.equal(ActionDocument.editor, 'properties');
    Assert.ok(ActionDocument.schema.fields.find(Field => Field.id === 'Trigger').enumValues.includes('Hold'));
    Assert.ok(!ActionDocument.schema.fields.some(Field => Field.id === 'Identifier'));
    Assert.deepEqual(ActionDocument.document.Modifiers.AxisOrder, ['0','1','2']);
    await Files.writeFile(Path.join(Output, 'action-snapshot.json'), JSON.stringify(ActionDocument));
    const OriginalRevision = ActionDocument.revision;
    ActionDocument = await Call('assetDocument.execute', {assetId:Action.id, operation:'property', revision:ActionDocument.revision, value:{path:['Trigger'], operation:'set', value:'Hold'}});
    Assert.equal(ActionDocument.dirty,true);
    Assert.equal((await Call('bridge.describe')).assetDocumentsDirty,true);
    for (const Change of [
      {path:['Identifier'],value:'changed'}, {path:['Threshold'],value:-1}, {path:['Trigger'],value:'Invalid'},
      {path:['Modifiers','AxisOrder',0],value:'4294967296'}, {path:['Modifiers','AxisOrder'],operation:'remove',index:0}
    ]) {
      const Rejected = await Bridge.request('assetDocument.execute', {assetId:Action.id,operation:'property',revision:ActionDocument.revision,value:Change});
      Assert.equal(Rejected.ok,false,JSON.stringify(Change));
      Assert.equal((await Call('assetDocument.execute',{assetId:Action.id,operation:'get'})).revision,ActionDocument.revision);
    }
    Assert.equal((await Bridge.request('assetDocument.execute',{assetId:Action.id,operation:'save',revision:OriginalRevision})).error.code,'STALE_DOCUMENT');
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'undo',revision:ActionDocument.revision});
    Assert.equal(ActionDocument.document.Trigger,'Pressed');
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'redo',revision:ActionDocument.revision});
    Assert.equal(ActionDocument.document.Trigger,'Hold');
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'save',revision:ActionDocument.revision});
    const ActionPath = Path.join(ProjectRoot,'Content/Input/TestAction.inputaction');
    const SavedAction = JSON.parse(await Files.readFile(ActionPath,'utf8'));
    Assert.equal(typeof SavedAction.Action.Trigger,'number');
    SavedAction.FutureRoot = {enabled:true};
    SavedAction.Action.FutureField = 42;
    SavedAction.Action.Modifiers.FutureModifier = 'preserved';
    await Files.writeFile(ActionPath, JSON.stringify(SavedAction));
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'reload',revision:ActionDocument.revision});
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'property',revision:ActionDocument.revision,value:{path:['Modifiers','DeadZone'],value:0.2}});
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'save',revision:ActionDocument.revision});
    const Preserved = JSON.parse(await Files.readFile(ActionPath,'utf8'));
    Assert.deepEqual(Preserved.FutureRoot,{enabled:true});Assert.equal(Preserved.Action.FutureField,42);Assert.equal(Preserved.Action.Modifiers.FutureModifier,'preserved');
    let ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'open'});
    const Element = ContextDocument.schema.fields.find(Field=>Field.id==='Bindings').elementDefault;
    const Insert = async Control => {
      ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'property',revision:ContextDocument.revision,value:{path:['Bindings'],operation:'insert',value:{...Element,Action:Action.id,Control}}});
    };
    await Insert('KeyA');await Insert('KeyB');
    Assert.notEqual(ContextDocument.document.Bindings[0].Identifier,ContextDocument.document.Bindings[1].Identifier);
    const First = ContextDocument.document.Bindings[0].Identifier;
    ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'property',revision:ContextDocument.revision,value:{path:['Bindings'],operation:'move',index:0,destination:1}});
    Assert.equal(ContextDocument.document.Bindings[1].Identifier,First);
    ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'property',revision:ContextDocument.revision,value:{path:['Bindings',1,'Control'],value:'KeyC'}});
    ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'save',revision:ContextDocument.revision});
    await Files.writeFile(Path.join(Output,'context-snapshot.json'),JSON.stringify(ContextDocument));
    ContextDocument = await Call('assetDocument.execute',{assetId:Context.id,operation:'reload',revision:ContextDocument.revision});
    Assert.equal(ContextDocument.document.Bindings[1].Control,'KeyC');
    await Call('play.start');
    Assert.equal((await Bridge.request('assetDocument.execute',{assetId:Action.id,operation:'property',revision:ActionDocument.revision,value:{path:['Name'],value:'Blocked'}})).error.code,'PLAY_READ_ONLY');
    await Call('play.stop');
    ActionDocument = await Call('assetDocument.execute',{assetId:Action.id,operation:'property',revision:ActionDocument.revision,value:{path:['Name'],value:'Draft'}});
    await Files.writeFile(ActionPath,JSON.stringify({...Preserved,ExternalChange:true}));
    Assert.equal((await Bridge.request('assetDocument.execute',{assetId:Action.id,operation:'save',revision:ActionDocument.revision})).ok,false);
    const Material = (await Call('assets.list')).find(Asset=>Asset.typeId==='Material');
    Assert.ok(Material);
    Assert.equal((await Call('assetDocument.execute',{assetId:Material.id,operation:'open'})).type,'Material');
    console.log('Input document report: '+Output);
  } finally {
    Child.stdin.end('{"type":"shutdown"}\n');
    const Watchdog=setTimeout(()=>Child.kill(),10000);
    await Exit;clearTimeout(Watchdog);Bridge.close();
    await Files.writeFile(Path.join(Output,'host.log'),Log);
  }
});
