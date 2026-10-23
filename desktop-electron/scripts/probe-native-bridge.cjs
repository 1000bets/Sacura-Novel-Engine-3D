// Run with Electron, not Node. Capability probe only: does not initialize Engine.
const {app, BrowserWindow, ipcMain, sharedTexture} = require('electron');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
let directory, window, offscreen, timeout;
let finishing = false;
let received;
async function finish(code) {
  if (finishing) return;
  finishing = true;
  clearTimeout(timeout);
  window?.destroy();
  offscreen?.destroy();
  if (directory) await fs.rm(directory, {recursive: true, force: true});
  app.exit(code);
}
app.whenReady().then(async () => {
  directory = await fs.mkdtemp(path.join(os.tmpdir(), 'sacura-native-probe-'));
  const preload = path.join(directory, 'preload.cjs');
  await fs.writeFile(preload, `
    const {ipcRenderer, sharedTexture} = require('electron');
    sharedTexture.setSharedTextureReceiver(async ({importedSharedTexture}) => {
      const frame = importedSharedTexture.getVideoFrame();
      const size = {width: frame.codedWidth, height: frame.codedHeight};
      frame.close();
      importedSharedTexture.release();
      ipcRenderer.send('native-frame-probe', size);
    });
    ipcRenderer.send('native-capability-probe', {
      receiver: typeof sharedTexture?.setSharedTextureReceiver === 'function',
      sandboxed: process.sandboxed === true,
      contextIsolated: process.contextIsolated === true,
      videoFrame: typeof VideoFrame === 'function'
    });
  `);
  window = new BrowserWindow({show: false, webPreferences: {
    preload, sandbox: true, contextIsolation: true, nodeIntegration: false
  }});
  ipcMain.once('native-capability-probe', (event, renderer) => {
    if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame) return finish(1);
    const result = {electron: process.versions.electron, napi: process.versions.napi,
      mainImport: typeof sharedTexture?.importSharedTexture === 'function',
      mainTransfer: typeof sharedTexture?.sendSharedTexture === 'function', renderer};
    received = result;
  });
  timeout = setTimeout(() => {console.error('Capability probe timed out'); finish(1);}, 10000);
  await window.loadURL('data:text/html,<title>Native bridge capability probe</title>');
  if (!received) throw new Error('Preload did not report capabilities');
  // Exercise official OSR → import → VideoFrame → release using Chromium's own
  // texture. This proves Electron transport only, not Diligent/D3D12 export.
  offscreen = new BrowserWindow({show: false, width: 64, height: 64, webPreferences: {
    sandbox: true, contextIsolation: true, offscreen: {useSharedTexture: true}
  }});
  let first = true;
  const painted = new Promise(resolve => offscreen.webContents.on('paint', event => {
    if (!event.texture) return;
    if (first) {first = false; resolve(event.texture);} else event.texture.release();
  }));
  await offscreen.loadURL('data:text/html,<body style="background:rgb(40,80,120)">API probe</body>');
  const source = await painted;
  let released;
  const allReleased = new Promise(resolve => {released = resolve;});
  const frameResult = new Promise(resolve => ipcMain.once('native-frame-probe', (event, size) => {
    if (event.sender !== window.webContents || event.senderFrame !== window.webContents.mainFrame) return finish(1);
    resolve(size);
  }));
  const importedSharedTexture = sharedTexture.importSharedTexture({textureInfo: source.textureInfo,
    allReferencesReleased: () => {source.release(); released();}});
  await sharedTexture.sendSharedTexture({frame: window.webContents.mainFrame, importedSharedTexture});
  importedSharedTexture.release();
  const [size] = await Promise.all([frameResult, allReleased]);
  received.offscreenSharedTexture = {pixelFormat: source.textureInfo.pixelFormat, ...size};
  received.importTransferVideoFrameRelease = true;
  console.log(JSON.stringify(received));
  await finish(received.mainImport && received.mainTransfer && received.renderer.receiver && received.renderer.videoFrame && received.renderer.sandboxed && received.renderer.contextIsolated ? 0 : 1);
}).catch(error => {console.error(error); finish(1);});
