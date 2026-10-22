const {spawn} = require('node:child_process');
const path = require('node:path');
const {setTimeout: delay} = require('node:timers/promises');

const root = path.resolve(__dirname, '..');
const frontend = path.resolve(root, '../engine-web-prototype/frontend');
const electron = require('electron');
const url = 'http://127.0.0.1:5173/';
let vite, app, ending = false;
function stop(code = 0) {
  if (ending) return;
  ending = true;
  if (app && !app.killed) app.kill();
  if (vite && !vite.killed) vite.kill();
  process.exitCode = code;
}
process.on('SIGINT', () => stop(0));
process.on('SIGTERM', () => stop(0));
process.on('exit', () => {
  if (vite && !vite.killed) vite.kill();
});
(async () => {
  const player = spawn(process.execPath, ['scripts/build-player.mjs'], {cwd: frontend, stdio: 'inherit'});
  const built = await new Promise(resolve => player.on('exit', resolve));
  if (built !== 0) throw new Error('Player runtime build failed');
  vite = spawn(process.execPath, [path.join(frontend, 'node_modules/vite/bin/vite.js'), '--host', '127.0.0.1', '--strictPort', '--port', '5173'], {cwd: frontend, stdio: 'inherit'});
  vite.on('exit', code => {if (!ending) stop(code || 1);});
  let ready = false;
  for (let i = 0; i < 150 && !ending; i++) {
    try {const response = await fetch(url); if (response.ok) {ready = true; break;}} catch {}
    await delay(200);
  }
  if (!ready) throw new Error('Vite did not start on 127.0.0.1:5173');
  app = spawn(electron, [root, ...(process.argv.includes('--desktop-smoke') ? ['--desktop-smoke'] : [])], {cwd: root, stdio: 'inherit', env: {...process.env, SACURA_DESKTOP_DEV_URL: url}});
  app.on('exit', code => stop(code || 0));
})().catch(error => {console.error(error); stop(1);});
