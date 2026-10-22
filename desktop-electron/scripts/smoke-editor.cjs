const {spawn} = require('node:child_process');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {pathToFileURL} = require('node:url');
const {createProject} = require('../project-store.cjs');

(async () => {
  const temporary = await fs.mkdtemp(path.join(os.tmpdir(), 'sacura-electron-smoke-'));
  try {
    const lifecycle = path.resolve(__dirname, '../../engine-web-prototype/frontend/src/projectLifecycle.js');
    const {createEmptyProject} = await import(pathToFileURL(lifecycle).href);
    const {root} = await createProject(temporary, createEmptyProject('SmokeProject'));
    const userData = path.join(temporary, 'user-data');
    await fs.mkdir(userData);
    const executable = path.resolve(__dirname, '../release/win-unpacked/Sacura Desktop Experiment.exe');
    const child = spawn(executable, ['--desktop-smoke'], {stdio: 'inherit', env: {...process.env, SACURA_DESKTOP_SMOKE_PROJECT: root, SACURA_DESKTOP_SMOKE_USERDATA: userData}});
    const code = await new Promise((resolve, reject) => {child.on('exit', resolve); child.on('error', reject);});
    if (code !== 0) throw new Error(`Packaged editor smoke failed (${code})`);
  } finally {await fs.rm(temporary, {recursive: true, force: true});}
})().catch(error => {console.error(error); process.exitCode = 1;});
