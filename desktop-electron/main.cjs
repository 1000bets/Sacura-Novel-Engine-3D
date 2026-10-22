const {app, BrowserWindow, dialog, ipcMain, net, protocol} = require('electron');
const path = require('node:path');
const fs = require('node:fs/promises');
const {pathToFileURL} = require('node:url');
const store = require('./project-store.cjs');
const mimeByExtension = {'.glb': 'model/gltf-binary', '.obj': 'text/plain', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.svg': 'image/svg+xml', '.wav': 'audio/wav', '.mp3': 'audio/mpeg', '.ogg': 'audio/ogg', '.m4a': 'audio/mp4', '.aac': 'audio/aac', '.flac': 'audio/flac', '.webm': 'audio/webm'};

const SCHEME = 'sacura';
protocol.registerSchemesAsPrivileged([{scheme: SCHEME, privileges: {standard: true, secure: true, supportFetchAPI: true, corsEnabled: true, stream: true}}]);
const devUrl = process.env.SACURA_DESKTOP_DEV_URL;
const uiRoot = path.join(__dirname, 'dist-ui');
const smoke = process.argv.includes('--desktop-smoke');
if (smoke && process.env.SACURA_DESKTOP_SMOKE_USERDATA) app.setPath('userData', process.env.SACURA_DESKTOP_SMOKE_USERDATA);
let window;
let currentRoot = null;
const recentFile = () => path.join(app.getPath('userData'), 'recent-projects.json');
async function recent() {
  try {const list = JSON.parse(await fs.readFile(recentFile(), 'utf8')); return Array.isArray(list) ? list.filter(item => item && typeof item.path === 'string' && typeof item.id === 'string').slice(0, 30) : [];} catch {return [];}
}
async function remember(root, info) {
  const list = (await recent()).filter(item => item.path !== root);
  list.unshift({...info, path: root, updatedAt: new Date().toISOString()});
  await fs.mkdir(app.getPath('userData'), {recursive: true});
  await fs.writeFile(recentFile(), JSON.stringify(list.slice(0, 30), null, 2));
}
function trusted(event) {
  if (!window || event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame) throw new Error('Untrusted IPC sender');
  const url = event.senderFrame.url;
  if (devUrl ? url !== devUrl : !url.startsWith('sacura://app/')) throw new Error('Untrusted IPC origin');
}
function handle(channel, action) {
  ipcMain.handle('desktop:' + channel, async (event, ...args) => {trusted(event); return action(...args);});
}
async function openRoot(root) {
  const real = await fs.realpath(root);
  const project = await store.readProject(real);
  currentRoot = real;
  await remember(real, {id: project.id, title: project.title});
  return project;
}
function cancelled() {return {cancelled: true};}
async function chooseParent() {
  const result = await dialog.showOpenDialog(window, {title: 'Выберите папку для нового проекта', properties: ['openDirectory', 'createDirectory']});
  return result.canceled ? null : result.filePaths[0];
}
app.whenReady().then(async () => {
  if (!devUrl) {
    protocol.handle(SCHEME, async request => {
      const url = new URL(request.url);
      if (url.host !== 'app') return new Response('Not found', {status: 404});
      const relative = decodeURIComponent(url.pathname).replace(/^\/+/, '') || 'index.html';
      const file = path.resolve(uiRoot, relative);
      if (!file.startsWith(path.resolve(uiRoot) + path.sep)) return new Response('Not found', {status: 404});
      try {return await net.fetch(pathToFileURL(file).toString());} catch {return new Response('Not found', {status: 404});}
    });
  }
  window = new BrowserWindow({width: 1500, height: 950, minWidth: 900, minHeight: 650, show: !smoke, webPreferences: {preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false, sandbox: true}});
  if (smoke) {
    const deadline = setTimeout(() => {console.error('Desktop smoke: React did not become ready'); app.exit(1);}, 15000);
    window.webContents.on('did-fail-load', (_, code, description) => {console.error('Desktop smoke load failed:', code, description); clearTimeout(deadline); app.exit(1);});
    window.webContents.on('render-process-gone', (_, details) => {console.error('Desktop smoke renderer exited:', details.reason); clearTimeout(deadline); app.exit(1);});
    handle('renderer-ready', async stage => {
      if (stage !== (process.env.SACURA_DESKTOP_SMOKE_PROJECT ? 'editor' : 'home')) return true;
      clearTimeout(deadline); console.log('Desktop smoke: ' + stage + ' mounted with Electron preload'); setTimeout(() => app.quit(), 100); return true;
    });
  } else handle('renderer-ready', async () => true);
  window.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
  window.webContents.on('will-navigate', (event, url) => {
    if (devUrl ? url !== devUrl : !url.startsWith('sacura://app/')) event.preventDefault();
  });
  handle('list-projects', async () => recent());
  handle('create-project', async project => {
    const parent = await chooseParent(); if (!parent) return cancelled();
    const result = await store.createProject(parent, project);
    currentRoot = result.root;
    await remember(result.root, {id: result.project.id, title: result.project.title});
    return {project: result.project};
  });
  handle('open-project', async () => {
    const result = await dialog.showOpenDialog(window, {title: 'Открыть проект Sacura', properties: ['openDirectory']});
    return result.canceled ? cancelled() : {project: await openRoot(result.filePaths[0])};
  });
  handle('open-recent', async id => {
    if (typeof id !== 'string' || id.length > 256) throw new Error('Invalid project ID');
    const item = (await recent()).find(item => item.id === id);
    if (!item) throw new Error('Project is not in the recent list');
    return {project: await openRoot(item.path)};
  });
  handle('save-project', async project => {
    if (!currentRoot) throw new Error('Choose a project directory first');
    const info = await store.writeProject(currentRoot, project);
    await remember(currentRoot, info);
    return info;
  });
  handle('save-project-as', async project => {
    const parent = await chooseParent(); if (!parent) return cancelled();
    const result = await store.createProject(parent, project);
    currentRoot = result.root;
    await remember(result.root, {id: result.project.id, title: result.project.title});
    return {project: result.project};
  });
  handle('pick-assets', async () => {
    const result = await dialog.showOpenDialog(window, {title: 'Импортировать ресурсы', properties: ['openFile', 'multiSelections']});
    if (result.canceled) return [];
    if (result.filePaths.length > 20) throw new Error('Import at most 20 files at once');
    const files = [];
    let total = 0;
    for (const filename of result.filePaths) {
      const stat = await fs.stat(filename);
      if (!stat.isFile() || stat.size > 100 * 1024 * 1024 || (total += stat.size) > 200 * 1024 * 1024) throw new Error('Import size limit exceeded');
      files.push({name: path.basename(filename), type: mimeByExtension[path.extname(filename).toLowerCase()] || 'application/octet-stream', bytes: await fs.readFile(filename)});
    }
    return files;
  });
  if (smoke && process.env.SACURA_DESKTOP_SMOKE_PROJECT) await openRoot(process.env.SACURA_DESKTOP_SMOKE_PROJECT);
  if (devUrl) await window.loadURL(devUrl); else await window.loadURL('sacura://app/index.html');
}).catch(error => {console.error(error); app.exit(1);});
app.on('window-all-closed', () => app.quit());
