const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {spawn:Spawn} = require('node:child_process');
const Assert = require('node:assert/strict');

const Repository = Path.resolve(__dirname,'../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Repository,'build/ElectronNative/Debug/SakuraElectronHost.exe');
const Electron = Path.join(Repository,'desktop-electron/node_modules/electron/dist/electron.exe');

async function Run(Executable, Arguments, Environment) {
  const Process = Spawn(Executable,Arguments,{windowsHide:true,env:{...process.env,...Environment}});
  let Output = '';
  Process.stdout.on('data',Chunk => { Output += Chunk; });
  Process.stderr.on('data',Chunk => { Output += Chunk; });
  const Timeout = setTimeout(() => Process.kill(),90000);
  const Code = await new Promise((Resolve,Reject) => {
    Process.once('error',Reject);
    Process.once('exit',Resolve);
  });
  clearTimeout(Timeout);
  Assert.equal(Code,0,Output.slice(-6000));
}

async function Verify() {
  const Directory = await FileSystem.mkdtemp(Path.join(Repository,'build/template-players-'));
  let Scenarios = ['quest','novel','shooter','horror'];
  if (process.argv.length > 2) {
    const Requested = process.argv.slice(2);
    Assert.ok(Requested.every(Scenario => Scenarios.includes(Scenario)), 'Unknown template scenario');
    Scenarios = Requested;
  }
  for (const Scenario of Scenarios) {
    await Run(Host,['--project-create',Directory,Scenario,'builtin-'+Scenario]);
    const ReportDirectory = Path.join(Directory,Scenario,'Verification');
    await Run(Electron,[Path.join(Repository,'desktop-electron/player-main.cjs'),'--project',Path.join(Directory,Scenario,Scenario+'.project'),'--player-smoke'],{
      SACURA_NATIVE_HOST:Host,SACURA_PLAYER_TEST_OUTPUT:ReportDirectory,SACURA_PLAYER_TEST_INTERACTIVE:'1'
    });
    const Report = JSON.parse(await FileSystem.readFile(Path.join(ReportDirectory,'report.json'),'utf8'));
    Assert.equal(Report.ok,true,JSON.stringify(Report));
    console.log(Scenario + ': passed, ' + ReportDirectory);
  }
}

Verify().catch(Failure => { console.error(Failure); process.exitCode = 1; });
