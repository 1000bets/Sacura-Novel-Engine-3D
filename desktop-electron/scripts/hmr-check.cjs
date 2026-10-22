const {spawn} = require('node:child_process');
const fs = require('node:fs/promises');
const path = require('node:path');
const {setTimeout: delay} = require('node:timers/promises');

const frontend = path.resolve(__dirname, '../../engine-web-prototype/frontend');
const filename = path.join(frontend, 'src', `__desktop_hmr_${process.pid}.jsx`);
const base = 'http://127.0.0.1:5174';
(async () => {
  let vite, socket;
  try {
    await fs.writeFile(filename, 'export default function DesktopHmrProbe(){return <div>1</div>}\n', {flag: 'wx'});
    vite = spawn(process.execPath, [path.join(frontend, 'node_modules/vite/bin/vite.js'), '--host', '127.0.0.1', '--strictPort', '--port', '5174'], {cwd: frontend, stdio: 'ignore'});
    let ready = false;
    for (let i = 0; i < 100; i++) {
      try {if ((await fetch(base)).ok) {ready = true; break;}} catch {}
      await delay(100);
    }
    if (!ready) throw new Error('Vite did not start');
    socket = new WebSocket('ws://127.0.0.1:5174/', 'vite-hmr');
    await new Promise((resolve, reject) => {socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true});});
    if (!(await fetch(base + '/src/' + path.basename(filename))).ok) throw new Error('Probe module was not served');
    const update = new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('No HMR update arrived')), 5000);
      socket.addEventListener('message', event => {
        const data = JSON.parse(event.data);
        if (data.type === 'update' && data.updates?.some(item => item.path.includes(path.basename(filename)))) {clearTimeout(timer); resolve();}
      });
    });
    await fs.writeFile(filename, 'export default function DesktopHmrProbe(){return <div>2</div>}\n');
    await update;
    console.log('Vite HMR update received without rebuilding Electron');
  } finally {
    socket?.close(); vite?.kill();
    await fs.rm(filename, {force: true});
  }
})().catch(error => {console.error(error); process.exitCode = 1;});
