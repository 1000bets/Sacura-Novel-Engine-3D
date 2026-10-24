const {test: Test} = require('node:test');
const Assert = require('node:assert/strict');
const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {spawn: Spawn} = require('node:child_process');
const {setTimeout: Delay} = require('node:timers/promises');
const {BridgeTransport} = require('../bridge/transport.cjs');
const Root = Path.resolve(__dirname, '../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Root, 'build/ElectronNative/Debug/SakuraElectronHost.exe');

Test('WAV import, startup persistence, relocation, conflict and Play protection', {timeout:120000,skip:process.platform !== 'win32'}, async () => {
  const Parent = Path.join(Root, 'build/sound-native-tests');
  await FileSystem.mkdir(Parent, {recursive:true});
  const Directory = await FileSystem.mkdtemp(Path.join(Parent, 'run-'));
  const Create = Spawn(Host, ['--project-create',Directory,'AudioProject','Empty'], {windowsHide:true});
  let Output = '';
  Create.stdout.on('data', Bytes => {Output += Bytes;});
  Create.stderr.on('data', Bytes => {Output += Bytes;});
  Assert.equal(await new Promise(Resolve => Create.once('exit', Resolve)), 0, Output);
  const ProjectDirectory = Path.join(Directory, 'AudioProject');
  const Project = Path.join(ProjectDirectory, 'AudioProject.project');
  const Descriptor = JSON.parse(await FileSystem.readFile(Project, 'utf8'));
  Descriptor.audioAcceptance = {preserved:true};
  await FileSystem.writeFile(Project, JSON.stringify(Descriptor));
  const Audio = Buffer.alloc(44 + 44100 * 2 * 8);
  Audio.write('RIFF'); Audio.writeUInt32LE(Audio.length - 8, 4); Audio.write('WAVEfmt ', 8);
  Audio.writeUInt32LE(16, 16); Audio.writeUInt16LE(1, 20); Audio.writeUInt16LE(1, 22);
  Audio.writeUInt32LE(44100, 24); Audio.writeUInt32LE(88200, 28);
  Audio.writeUInt16LE(2, 32); Audio.writeUInt16LE(16, 34); Audio.write('data', 36); Audio.writeUInt32LE(Audio.length - 44, 40);
  for (let Sample = 0; Sample < 44100 * 8; Sample++) {
    Audio.writeInt16LE(Math.round(Math.sin(Sample * 440 * Math.PI * 2 / 44100) * 1800), 44 + Sample * 2);
  }
  const Source = Path.join(Directory, 'Preview.wav');
  const Invalid = Path.join(Directory, 'Invalid.wav');
  await FileSystem.writeFile(Source, Audio);
  await FileSystem.writeFile(Invalid, 'invalid audio');
  let Bridge;
  let Process;
  let Exit;
  async function Start() {
    Bridge = await BridgeTransport.listen();
    Output = '';
    Process = Spawn(Host, ['--bridge-headless',Project,Bridge.path], {windowsHide:true});
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
    Assert.equal((await Bridge.request('assets.import',{sourcePath:Invalid,destination:'Audio/Invalid.wav'})).ok,false);
    await Call('assets.import',{sourcePath:Source,destination:'Audio/Preview.wav'});
    const Asset = (await Call('assets.list')).find(Entry => Entry.relativePath === 'Audio/Preview.wav');
    Assert.equal(Asset.typeId,'Audio');
    Assert.deepEqual(await FileSystem.readFile(Path.join(ProjectDirectory,'Content/Audio/Preview.wav')),Audio);
    const Metadata = JSON.parse(await FileSystem.readFile(Path.join(ProjectDirectory,'Content/Audio/Preview.wav.meta'),'utf8'));
    Assert.ok(Metadata);
    Assert.equal((await Call('audio.setStartup',{assetId:Asset.id})).startupAudioPath,'Audio/Preview.wav');
    Assert.equal(JSON.parse(await FileSystem.readFile(Project,'utf8')).startupAudio,'Content/Audio/Preview.wav');
    Assert.equal(JSON.parse(await FileSystem.readFile(Project,'utf8')).audioAcceptance.preserved,true);
    Assert.match((await Call('audio.resolveStartup')).absolutePath,/Preview\.wav$/);
    await Call('assets.rename',{assetId:Asset.id,name:'Renamed'});
    Assert.equal((await Call('bridge.describe')).startupAudioPath,'Audio/Renamed.wav');
    await Stop();
    await Start();
    Assert.equal((await Call('bridge.describe')).startupAudioPath,'Audio/Renamed.wav');
    const Scene = (await Call('assets.list')).find(Entry => Entry.typeId === 'Scene');
    Assert.equal((await Bridge.request('audio.setStartup',{assetId:Scene.id})).error.code,'INVALID_ARGUMENT');
    await Call('play.start');
    Assert.equal((await Bridge.request('audio.setStartup',{})).error.code,'PLAY_READ_ONLY');
    await Call('play.stop');
    Assert.equal((await Call('audio.setStartup')).startupAudioPath,null);
    Assert.equal((await Bridge.request('audio.resolveStartup',{})).ok,false);
    const Before = await FileSystem.readFile(Project,'utf8');
    await FileSystem.writeFile(Project,Before + '\n');
    Assert.equal((await Bridge.request('audio.setStartup',{assetId:Asset.id})).error.code,'EXTERNAL_CHANGES');
    Assert.equal(await FileSystem.readFile(Project,'utf8'),Before + '\n');
    console.log('Audio acceptance project:', Project);
  } finally {
    await Stop();
  }
});

