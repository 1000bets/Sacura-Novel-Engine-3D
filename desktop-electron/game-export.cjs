const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {setTimeout: Delay} = require('node:timers/promises');
const {randomUUID} = require('node:crypto');

async function ExportGame({Destination, Bridge, RuntimeExecutable, NativeExecutable, ApplicationDirectory}) {
  const Output = Path.resolve(Destination);
  try {
    await FileSystem.access(Output);
    throw new Error('Choose a new export directory');
  } catch (Failure) {
    if (Failure.code !== 'ENOENT') { throw Failure; }
  }
  const Staging = Path.join(Path.dirname(Output), '.sakura-player-' + randomUUID());
  try {
    const Result = await Bridge.request('project.export', {destination: Staging, runtimeDirectory: Path.dirname(RuntimeExecutable), executableName: Path.basename(RuntimeExecutable)});
    if (!Result.ok) { throw new Error(Result.error.message); }
    const Application = Path.join(Staging, 'resources', 'app');
    await FileSystem.mkdir(Application, {recursive: true});
    for (const Filename of ['player-main.cjs', 'preload.cjs', 'native-viewport.cjs']) {
      await FileSystem.copyFile(Path.join(ApplicationDirectory, Filename), Path.join(Application, Filename));
    }
    await FileSystem.mkdir(Path.join(Application, 'scripts'));
    await FileSystem.copyFile(Path.join(ApplicationDirectory, 'scripts/player-smoke.cjs'), Path.join(Application, 'scripts/player-smoke.cjs'));
    for (const Folder of ['bridge', 'dist-ui']) {
      await FileSystem.cp(Path.join(ApplicationDirectory, Folder), Path.join(Application, Folder), {recursive: true, errorOnExist: true, force: false});
    }
    await FileSystem.writeFile(Path.join(Application, 'package.json'), JSON.stringify({name: 'sakura-game', version: '1.0.0', main: 'player-main.cjs'}));
    const NativeSource = Path.dirname(NativeExecutable);
    const NativeDestination = Path.join(Staging, 'resources', 'native');
    await FileSystem.mkdir(NativeDestination);
    await FileSystem.copyFile(NativeExecutable, Path.join(NativeDestination, 'SakuraElectronHost.exe'));
    for (const Entry of await FileSystem.readdir(NativeSource, {withFileTypes: true})) {
      if (Entry.isFile() && (/\.dll$/i.test(Entry.name) && !/^Qt\d/i.test(Entry.name) || /\._pth$/i.test(Entry.name) || Entry.name === 'BridgeContract.json')) {
        await FileSystem.copyFile(Path.join(NativeSource, Entry.name), Path.join(NativeDestination, Entry.name));
      }
    }
    for (const Folder of ['Engine', 'Python']) {
      await FileSystem.cp(Path.join(NativeSource, Folder), Path.join(NativeDestination, Folder), {recursive: true,
        filter: Source => !Source.split(Path.sep).some(Part => Part === 'site-packages' || Part === '__pycache__')});
    }
    await FileSystem.access(Path.join(Application, 'dist-ui', 'index.html'));
    await FileSystem.access(Path.join(NativeDestination, 'BridgeContract.json'));
    for (let Attempt = 0; ; ++Attempt) {
      try {
        await FileSystem.rename(Staging, Output);
        break;
      } catch (Failure) {
        if (Attempt >= 10 || !['EPERM', 'EACCES', 'EBUSY'].includes(Failure.code)) {
          throw Failure;
        }
        await Delay(200);
      }
    }
    return {path: Output, executable: Path.join(Output, 'Game.exe')};
  } catch (Failure) {
    await FileSystem.rm(Staging, {recursive: true, force: true});
    throw Failure;
  }
}

module.exports = {ExportGame};
