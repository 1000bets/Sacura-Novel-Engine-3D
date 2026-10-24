const {spawn} = require('node:child_process');
const fs = require('node:fs/promises');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {createProject} = require('../project-store.cjs');

(async () => {
  if (process.platform !== 'win32') throw new Error('Native child viewport currently requires Windows.');
  const root = path.resolve(__dirname, '..');
  if (!process.argv.includes('--smoke')) {
    const Child = spawn(process.env.SACURA_DESKTOP_EXECUTABLE || require('electron'),
      process.env.SACURA_DESKTOP_EXECUTABLE ? [] : [root], {cwd: root, stdio: 'inherit', env: process.env});
    const Code = await new Promise((Resolve, Reject) => { Child.once('exit', Resolve); Child.once('error', Reject); });
    process.exitCode = Code;
    return;
  }
  const TestRoot = path.resolve(root, '../build');
  if (/^C:/i.test(TestRoot)) { throw new Error('Smoke project requires a workspace outside C:'); }
  await fs.mkdir(TestRoot, {recursive: true});
  const temporary = await fs.mkdtemp(path.join(TestRoot, 'sacura-native-'));
  try {
    const executable = process.env.SACURA_NATIVE_HOST || path.resolve(root, '../build/windows-msvc-debug/ElectronNative/Debug/SakuraElectronHost.exe');
    await fs.access(executable);
    await fs.access(path.join(root, 'dist-ui/index.html'));
    const {createEmptyProject} = await import(pathToFileURL(path.resolve(root, '../engine-web-prototype/frontend/src/projectLifecycle.js')).href);
    const {root: demo} = await createProject(temporary, createEmptyProject('Native Engine viewport prototype'));
    const nativeProject = path.join(temporary, 'NativeSample');
    await fs.cp(path.resolve(root, '../Samples/SampleProject'), nativeProject, {recursive: true});
    await fs.mkdir(path.join(nativeProject,'Content/Textures'),{recursive:true});
    await fs.writeFile(path.join(nativeProject,'Content/Textures/PanelPreview.png'),Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==','base64'));
    const sceneFile = path.join(nativeProject,'Content/Scenes/Main.scene');
    const scene = JSON.parse(await fs.readFile(sceneFile,'utf8'));
    let meshIndex = 0;
    for (const object of scene.objects) if (object.components.some(c=>c.type==='engine.MeshRendererComponent')) {
      object.parent=null;
      object.transform={position:[meshIndex++===0?-1:1,0,0],rotation:[0,0,0,1],scale:[1,1,1]};
    }
    await fs.writeFile(sceneFile,JSON.stringify(scene));
    const smoke = process.argv.includes('--smoke');
    const scale = process.argv.find(arg => arg.startsWith('--force-device-scale-factor='));
    const packaged = process.env.SACURA_DESKTOP_EXECUTABLE;
    const child = spawn(packaged || require('electron'), [...(packaged?[]:[root]), '--native-viewport', ...(smoke ? ['--native-smoke'] : []), ...(scale ? [scale] : [])], {
      cwd: root, stdio: 'inherit', windowsHide: false,
      env: {...process.env, SACURA_NATIVE_HOST: executable, SACURA_NATIVE_PROJECT: path.join(nativeProject, 'SampleProject.project'),
        SACURA_NATIVE_DEMO_PROJECT: demo, SACURA_NATIVE_USERDATA: path.join(temporary, 'user-data')}
    });
    const stop = () => child.kill();
    process.once('SIGINT', stop); process.once('SIGTERM', stop);
    const code = await new Promise((resolve, reject) => {child.once('exit', resolve); child.once('error', reject);});
    process.removeListener('SIGINT', stop); process.removeListener('SIGTERM', stop);
    if (code !== 0) throw new Error(`Native prototype exited with code ${code}`);
  } finally {await fs.rm(temporary, {recursive: true, force: true, maxRetries: 10, retryDelay: 300});}
})().catch(error => {console.error(error); process.exitCode = 1;});
