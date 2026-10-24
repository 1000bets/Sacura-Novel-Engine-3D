const {app, BrowserWindow, ipcMain, dialog} = require('electron');
const Path = require('node:path');
const {NativeViewport} = require('./native-viewport.cjs');

process.env.SACURA_PLAYER = '1';
let PlayerWindow;
let Viewport;
let Closing = false;
const Smoke = process.argv.includes('--player-smoke');
for (const Output of [process.stdout, process.stderr]) {
  Output.on('error', Failure => {
    if (Failure.code !== 'EPIPE') {
      throw Failure;
    }
  });
}
if (Smoke) {
  app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
  app.commandLine.appendSwitch('disable-features', 'CalculateNativeWinOcclusion');
}
app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
const AllowedMethods = new Set(['bridge.describe', 'game.state', 'game.advance', 'game.choose', 'game.pause', 'game.action', 'audio.resolveStartup']);

app.whenReady().then(async () => {
  let Project = Path.join(process.resourcesPath, 'Game', 'Game.project');
  const ArgumentIndex = process.argv.indexOf('--project');
  if (ArgumentIndex >= 0 && process.argv[ArgumentIndex + 1]) {
    Project = Path.resolve(process.argv[ArgumentIndex + 1]);
  }
  PlayerWindow = new BrowserWindow({width: 1280, height: 800, backgroundColor: '#17171b',
    webPreferences: {preload: Path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false,
      backgroundThrottling: false, additionalArguments: ['--native-viewport', '--sakura-player', ...(Smoke ? ['--native-smoke'] : [])]}});
  PlayerWindow.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
  PlayerWindow.webContents.on('will-navigate', Event => Event.preventDefault());
  Viewport = await NativeViewport.create(PlayerWindow, Project);
  const Register = (Name, Action) => ipcMain.handle('desktop:' + Name, (Event, ...Arguments) => {
    if (PlayerWindow.isDestroyed()) { return; }
    if (Event.sender !== PlayerWindow.webContents || Event.senderFrame !== PlayerWindow.webContents.mainFrame) {
      throw new Error('Untrusted Player request');
    }
    return Action(...Arguments);
  });
  Register('native-bounds', Bounds => Viewport.bounds(Bounds));
  Register('native-status', () => Viewport.state);
  Register('native-frame-consumed', Token => Viewport.frameConsumed(Token));
  Register('native-input', Event => Viewport.input(Event));
  Register('native-heartbeat', Presentation => { Viewport.state.presentation = Presentation; });
  if (Smoke) {
    Register('bridge-test-result', Result => { Viewport.testResult = Result; });
  }
  Register('bridge-connect', () => ({connected: Viewport.bridge.connected, context: Viewport.bridge.context}));
  Register('bridge-command', Request => {
    if (!Request || !AllowedMethods.has(Request.method)) {
      throw new Error('Editor commands are unavailable in Player');
    }
    return Viewport.bridge.request(Request.method, Request.params || {}, Request.context);
  });
  Register('player-quit', () => PlayerWindow.close());
  PlayerWindow.on('close', Event => {
    if (!Closing) {
      Event.preventDefault();
      Closing = true;
      Viewport.stop().finally(() => PlayerWindow.destroy());
    }
  });
  if (Smoke) {
    require('./scripts/player-smoke.cjs').Run({Application: app, Window: PlayerWindow, Viewport});
  }
  await PlayerWindow.loadFile(Path.join(__dirname, 'dist-ui', 'index.html'));
}).catch(Failure => {
  if (Smoke) {
    const FileSystem = require('node:fs');
    FileSystem.mkdirSync(process.env.SACURA_PLAYER_TEST_OUTPUT, {recursive: true});
    FileSystem.writeFileSync(Path.join(process.env.SACURA_PLAYER_TEST_OUTPUT, 'startup-error.txt'), Failure.stack);
    app.exit(1);
    return;
  }
  dialog.showErrorBox('Game startup failed', Failure.message);
  app.quit();
});
app.on('window-all-closed', () => app.quit());
