for (const Output of [process.stdout, process.stderr]) {
  Output.on('error', Failure => {
    if (Failure.code !== 'EPIPE') {
      throw Failure;
    }
  });
}

const {app, BrowserWindow, dialog, ipcMain, net, protocol, clipboard, screen} = require('electron');
const path = require('node:path');
const fs = require('node:fs/promises');
const {pathToFileURL} = require('node:url');
const store = require('./project-store.cjs');
const {hostPath,projectCli,preserveDraft}=require('./native-project.cjs');
const {NativeRecents}=require('./native-recents.cjs');
const {setTimeout:delay}=require('node:timers/promises');
const mimeByExtension = {'.glb': 'model/gltf-binary', '.obj': 'text/plain', '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg', '.webp': 'image/webp', '.gif': 'image/gif', '.svg': 'image/svg+xml', '.wav': 'audio/wav', '.mp3': 'audio/mpeg', '.ogg': 'audio/ogg', '.m4a': 'audio/mp4', '.aac': 'audio/aac', '.flac': 'audio/flac', '.webm': 'audio/webm'};

const SCHEME = 'sacura';
protocol.registerSchemesAsPrivileged([{scheme: SCHEME, privileges: {standard: true, secure: true, supportFetchAPI: true, corsEnabled: true, stream: true}}]);
const devUrl = process.env.SACURA_DESKTOP_DEV_URL;
const uiRoot = path.join(__dirname, 'dist-ui');
const smoke = process.argv.includes('--desktop-smoke');
const nativePrototype = true;
const assetTransferTest=process.argv.includes('--asset-transfer-check');
const AssetReferenceCheck = process.argv.includes('--asset-reference-check');
const ReflectedAssetsCheck = process.argv.includes('--reflected-assets-check');
const ComponentNamesCheck = process.argv.includes('--component-names-check');
const WorkspaceTabsRestore = process.argv.includes('--workspace-tabs-restore-check');
const WorkspaceTabsCheck = WorkspaceTabsRestore || process.argv.includes('--workspace-tabs-check');
const LocalizationCheck = process.argv.includes('--localization-check');
const CameraSpeedCheck = process.argv.includes('--camera-speed-check');
const RenderPerformanceCheck = process.argv.includes('--render-performance-check');
const SceneShortcutsCheck = process.argv.includes('--scene-shortcuts-check');
const MaterialDropCheck = process.argv.includes('--material-drop-check');
const MaterialCapture = process.argv.find(Argument => Argument.startsWith('--screenshot-material='));
const MigrationCheck = process.argv.includes('--qt-removal-check');
const nativeSmoke = process.argv.includes('--native-smoke')||assetTransferTest||MigrationCheck;
// CLI rendering checks must keep Chromium producing frames when another app
// covers the test window. Production keeps normal background/visibility policy.
if(RenderPerformanceCheck || ReflectedAssetsCheck || LocalizationCheck || AssetReferenceCheck || WorkspaceTabsCheck || nativeSmoke || MaterialCapture || ComponentNamesCheck || CameraSpeedCheck || SceneShortcutsCheck || MaterialDropCheck){
  app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
  app.commandLine.appendSwitch('disable-features','CalculateNativeWinOcclusion');
}
if (process.env.SACURA_GPU_DIAGNOSTIC) {
  app.commandLine.appendSwitch('enable-logging', 'stderr');
  app.commandLine.appendSwitch('vmodule', '*shared_image*=2,*d3d*=2');
}
let nativeViewport;
let AssetWindows;
const WorkspaceWindows = new Map();
let releaseNativeProject = async () => {};
if (nativePrototype && process.env.SACURA_NATIVE_USERDATA) app.setPath('userData', process.env.SACURA_NATIVE_USERDATA);
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
  if (!window || window.isDestroyed() || event.sender.isDestroyed() || event.senderFrame !== event.sender.mainFrame || event.sender !== window.webContents) throw new Error('Untrusted IPC sender');
  const url = event.senderFrame.url;
  if (devUrl ? url !== devUrl : !url.startsWith('sacura://app/')) throw new Error('Untrusted IPC origin');
}
function handle(channel, action) {
  ipcMain.handle('desktop:' + channel, async (event, ...args) => {trusted(event);
    return action(...args);});
}
let LocalizationQueue = Promise.resolve();
handle('editor-localization', Values => {
  const Operation = async () => {
    let Input;
    if (Values !== undefined) {
      Input = JSON.stringify(Values);
      if (Buffer.byteLength(Input) > 4 * 1024 * 1024) {
        throw new Error('Localization preferences exceed the size limit');
      }
    }
    const Result = await projectCli(hostPath(app), ['--editor-localization'], Input);
    if (Values !== undefined) {
      for (const Target of BrowserWindow.getAllWindows()) {
        Target.webContents.send('editor-localization-changed', Result);
      }
    }
    return Result;
  };
  const Result = LocalizationQueue.then(Operation);
  LocalizationQueue = Result.catch(() => {});
  return Result;
});
async function openRoot(root) {
  const real = await fs.realpath(root);
  const project = await store.readProject(real);
  if (currentRoot && currentRoot !== real) await releaseNativeProject();
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
    let delayedBoot=false;
    protocol.handle(SCHEME, async request => {
      const url = new URL(request.url);
      if(url.host==='native-asset') {
        const [session,id]=url.pathname.split('/').filter(Boolean);
        const bridge=nativeViewport?.bridge;
        if(!bridge?.connected||session!==bridge.context?.session)return new Response('Expired asset session',{status:410});
        const result=await bridge.request('assets.resolve',{assetId:id});
        if(!result.ok)return new Response(result.error.message,{status:404});
        const file=result.value.absolutePath;
        const Extension = path.extname(file).toLowerCase();
        const IsTexture = result.value.typeId === 'Texture' && ['.png','.jpg','.jpeg','.bmp','.webp'].includes(Extension);
        const IsAudio = result.value.typeId === 'Audio' && Extension === '.wav';
        if (!IsTexture && !IsAudio) {
          return new Response('No browser preview for this native asset type', {status:415});
        }
        return net.fetch(pathToFileURL(file).toString());
      }
      if (url.host === 'native-thumbnail') {
        const [Session, Identifier, Subasset] = url.pathname.split('/').filter(Boolean);
        const Bridge = [nativeViewport, ...Array.from(AssetWindows?.values() || [], Editor => Editor.viewport)].find(Owner => Owner?.bridge.context?.session === Session)?.bridge;
        if (!Bridge?.connected || Session !== Bridge.context?.session) {
          return new Response('Expired asset session', {status: 410});
        }
        const Params = {assetId: Identifier};
        if (Subasset) { Params.subAssetId = Subasset; }
        const Result = await Bridge.request('assets.thumbnail', Params);
        if (!Result.ok) { return new Response(Result.error.message, {status: 404}); }
        if (!Result.value.ready) { return new Response('Pending preview', {status: 202}); }
        if (!Result.value.png) { return new Response('Type preview', {status: 415}); }
        return new Response(Buffer.from(Result.value.png, 'base64'), {headers: {'Content-Type': 'image/png', 'Cache-Control': 'no-store'}});
      }
      if(url.host==='startup-audio') {
        const [session,resource]=url.pathname.split('/').filter(Boolean);
        const bridge=nativeViewport?.bridge;
        if(!bridge?.connected||session!==bridge.context?.session||resource!=='cue.wav')return new Response('Expired audio session',{status:410});
        const result=await bridge.request('audio.resolveStartup',{});
        if(!result.ok)return new Response(result.error.message,{status:404});
        return net.fetch(pathToFileURL(result.value.absolutePath).toString());
      }
      if (url.host !== 'app') return new Response('Not found', {status: 404});
      const relative = decodeURIComponent(url.pathname).replace(/^\/+/, '') || 'index.html';
      const file = path.resolve(uiRoot, relative);
      if (!file.startsWith(path.resolve(uiRoot) + path.sep)) return new Response('Not found', {status: 404});
      // CLI screenshot regression only: keep the real HTML splash observable.
      if(nativeSmoke&&process.env.SACURA_TEST_BOOT_DELAY==='1'&&!delayedBoot&&/^main-.*\.js$/.test(path.basename(file))){delayedBoot=true;await delay(3000);}
      try {return await net.fetch(pathToFileURL(file).toString());} catch {return new Response('Not found', {status: 404});}
    });
  window = new BrowserWindow({width: 1500, height: 950, minWidth: 900, minHeight: 650, show: !smoke, backgroundColor:'#11121b', webPreferences: {preload: path.join(__dirname, 'preload.cjs'), contextIsolation: true, nodeIntegration: false, sandbox: true, backgroundThrottling: false, additionalArguments: ['--native-viewport', ...(nativeSmoke?['--native-smoke']:[])]}});
  if (nativePrototype) {
    window.once('ready-to-show', () => window.show());
    window.webContents.on('console-message', details => console.log('React:', details.message));
    window.setMenu(null);
    window.webContents.setZoomFactor(1);
    const {NativeViewport} = require('./native-viewport.cjs');
    nativeViewport = await NativeViewport.create(window);
    const nativeRecents=new NativeRecents(path.join(app.getPath('userData'),'native-recent-projects.json'));
    const waitForNative=async owner=>{
      const deadline=Date.now()+60000;
      while(owner.state.phase!=='ready'||!owner.bridge.connected){
        if(owner.state.phase==='error'||owner.state.phase==='stopped')throw Error(owner.state.error||'Не удалось загрузить проект.');
        if(Date.now()>deadline)throw Error('Превышено время загрузки проекта.');
        await delay(50);
      }
    };
    AssetWindows = require('./asset-windows.cjs').createAssetWindows({app, BrowserWindow, dialog, NativeViewport, waitForNative,
      getMainWindow: () => window, getViewport: () => nativeViewport, devUrl, uiRoot, nativeSmoke: nativeSmoke || Boolean(MaterialCapture)});
    handle('native-open-asset-editor', Identity => AssetWindows.open(Identity));
    handle('document-call', (Identifier, Channel, Value) => AssetWindows.call(Identifier, Channel, Value));
    handle('document-list', () => AssetWindows.list());
    handle('document-close', Identifier => AssetWindows.close(Identifier));
    handle('workspace-drag-surfaces', () => [...WorkspaceWindows.entries(), ['', window]].filter(([, Surface]) => !Surface.isDestroyed()).map(([Name, Surface]) => ({name:Name,bounds:Surface.getContentBounds(),visible:Surface.isVisible() && !Surface.isMinimized()})));
    handle('workspace-focus-window', Name => {
      let Surface = window;
      if (Name !== '') {
        Surface = WorkspaceWindows.get(Name);
      }
      if (!Surface || Surface.isDestroyed()) {
        return false;
      }
      if (Surface.isMinimized()) {
        Surface.restore();
      }
      const Bounds = Surface.getBounds();
      const Visible = screen.getAllDisplays().some(Display => {
        const Work = Display.workArea;
        return Bounds.x >= Work.x && Bounds.y >= Work.y &&
          Bounds.x + Math.min(Bounds.width, 160) <= Work.x + Work.width && Bounds.y + 40 <= Work.y + Work.height;
      });
      if (!Visible) {
        const Work = screen.getDisplayMatching(Bounds).workArea;
        Surface.setBounds({x:Work.x,y:Work.y,width:Math.min(Bounds.width,Work.width),height:Math.min(Bounds.height,Work.height)});
      }
      Surface.show();
      Surface.focus();
      return true;
    });
    handle('workspace-close-window', Name => {
      const Child = WorkspaceWindows.get(Name);
      if (Child) {
        WorkspaceWindows.delete(Name);
        Child.destroy();
      }
      return true;
    });
    handle('native-reveal-asset', Identity => {window.webContents.send('native-reveal-asset', Identity); window.show(); window.focus(); return true;});
    const rememberNative=async()=>{try{await nativeRecents.remember(nativeViewport.project,nativeViewport.bridge.state?.project||path.basename(nativeViewport.project,'.project'));}catch(error){console.error('Recent project preference:',error.message);}};
    if(nativeViewport.project)waitForNative(nativeViewport).then(rememberNative).catch(error=>console.error(error.message));
    let replacement = Promise.resolve();
    let closing = false;
    let loading = null;
    let requestedBounds, requestedTools;
    const guard=()=>preserveDraft(nativeViewport.bridge,async()=>{
      const {response}=await dialog.showMessageBox(window,{type:'question',message:'Сохранить изменения сцены и истории?',buttons:['Сохранить','Не сохранять','Отмена'],defaultId:0,cancelId:2,noLink:true});
      return ['save','discard','cancel'][response];
    });
    const replaceProject = project => replacement = replacement.catch(()=>{}).then(async () => {
      if (closing) throw new Error('Window is closing');
      try{
      loading={label:project?'Проверяем проект…':'Закрываем проект…',project:project?path.basename(project,'.project'):null};
      nativeViewport.input({kind:'reset'});
      if(project)await projectCli(hostPath(app),['--project-check',project]);
      if(!await AssetWindows.closeAll() || !await guard())return false;
      const previous = nativeViewport;
      loading={...loading,label:'Закрываем проект…'};
      await nativeViewport.stop();
      window.webContents.send('native-reset-presentation');
      loading={...loading,label:project?'Загружаем сцену и запускаем движок…':'Открываем список проектов…'};
      nativeViewport = await NativeViewport.create(window, project);
      if (requestedTools || previous.lastTools) nativeViewport.tools(requestedTools || previous.lastTools);
      if (requestedBounds || previous.lastBounds) nativeViewport.bounds(requestedBounds || previous.lastBounds);
      if(project){try{await waitForNative(nativeViewport);await rememberNative();}catch(error){await nativeViewport.stop();nativeViewport.state={...nativeViewport.state,phase:'error',error:error.message};throw error;}}
      return true;
      }finally{loading=null;}
    });
    releaseNativeProject = () => replaceProject(null);
    handle('native-export-game', async () => {
      await AssetWindows.requireSaved();
      const Choice = await dialog.showSaveDialog(window, {title: 'Export game to a new folder', buttonLabel: 'Export', defaultPath: 'Game'});
      if (Choice.canceled || !Choice.filePath) { return null; }
      return require('./game-export.cjs').ExportGame({Destination: Choice.filePath, Bridge: nativeViewport.bridge,
        RuntimeExecutable: process.execPath, NativeExecutable: hostPath(app), ApplicationDirectory: __dirname});
    });
    handle('native-bounds', value => {const result=nativeViewport.bounds(value);requestedBounds={...value};return result;});
    handle('native-status', () => nativeViewport.state);
    handle('native-frame-consumed', Token => nativeViewport.frameConsumed(Token));
    handle('native-session',()=>({phase:nativeViewport.state.phase,connected:nativeViewport.bridge.connected,project:nativeViewport.project||null,error:nativeViewport.state.error||null,loading}));
    handle('native-list-projects',()=>nativeRecents.list());
    handle('native-open-recent',async id=>replaceProject(await nativeRecents.resolve(id)));
    handle('native-forget-project',id=>nativeRecents.remove(id));
    handle('native-close-project',()=>replaceProject(null));
    handle('bridge-connect', () => ({connected:nativeViewport.bridge.connected,context:nativeViewport.bridge.context,state:nativeViewport.bridge.state}));
    if(nativeSmoke)handle('bridge-test-result',result=>{nativeViewport.bridgeTest=result;return true;});
    handle('native-copy-text', Text => {if(typeof Text !== 'string' || Text.length > 4096){throw Error('Invalid clipboard text');}clipboard.writeText(Text);return true;});
    handle('bridge-command', async request => {
      if(!request||typeof request.method!=='string'||!request.context||typeof request.context.session!=='string'||!Number.isSafeInteger(request.context.epoch))return {version:1,type:'bridge.result',id:null,context:nativeViewport.bridge.context,ok:false,error:{code:'INVALID_ARGUMENT',message:'Invalid Bridge request or missing context'}};
      if(['assets.import','assets.resolve','project.export'].includes(request.method))return {version:1,id:null,context:nativeViewport.bridge.context,ok:false,error:{code:'FORBIDDEN',message:'This operation requires the main-process asset capability'}};
      if (await AssetWindows.blocksMutation(request.method, request.params)) {
        return {version:1,id:null,context:nativeViewport.bridge.context,ok:false,error:{code:'DOCUMENT_OPEN',message:'Закройте редакторы затронутых ассетов перед переименованием, перемещением или удалением.'}};
      }
      return nativeViewport.bridge.request(request.method,request.params,request.context);
    });
    handle('native-import-assets',async destination=>{
      if(typeof destination!=='string'||destination.length>900)throw Error('Invalid import destination');
      const owner=nativeViewport,context=owner.bridge.context;
      let ImportTitle = 'Импорт в C++ AssetRegistry';
      let ImportFilters = [{name:'Native Engine assets',extensions:['glb','fbx','png','jpg','jpeg','bmp','tga','hdr','wav']}];
      if (destination === 'Audio') {
        ImportTitle = 'Импорт WAV';
        ImportFilters = [{name:'WAV audio',extensions:['wav']}];
      }
      const picked=await dialog.showOpenDialog(window,{title:ImportTitle,properties:['openFile','multiSelections'],filters:ImportFilters});
      if(picked.canceled)return [];
      if(owner!==nativeViewport||context?.session!==owner.bridge.context?.session)throw Error('Project changed during import');
      const results=[];
      for(const sourcePath of picked.filePaths){const name=path.basename(sourcePath).replace(/\.fbx$/i,'.glb').replace(/\.wav$/i,'.wav');results.push(await owner.bridge.request('assets.import',{sourcePath,destination:path.posix.join(destination||'Imports',name)},context));}
      return results;
    });
    handle('native-input', value => nativeViewport.input(value));
    handle('native-tools', value => {const result=nativeViewport.tools(value);requestedTools={...value};return result;});
    handle('native-detach', () => {nativeViewport.input({kind:'reset'}); nativeViewport.send({type:'bounds',x:0,y:0,width:0,height:0,visible:false}); return true;});
    handle('native-open-project', async () => {
      const result = await dialog.showOpenDialog(window, {title:'Открыть C++ проект',properties:['openFile'],filters:[{name:'Sacura Engine',extensions:['project']}]});
      if (result.canceled) return false;
      return replaceProject(await fs.realpath(result.filePaths[0]));
    });
    const createNativeProject=async(name,parent,templateId='Empty')=>{
      if(typeof name!=='string'||!name.trim()||name.length>120)throw Error('Укажите название проекта');
      if(!parent)return false;
      let result;
      loading={label:'Создаём проект…',project:name.trim()};
      try{result=await projectCli(hostPath(app),['--project-create',parent,name.trim(),templateId]);}
      finally{loading=null;}
      return await replaceProject(result.path)?result:false;
    };
    handle('native-create-project',async(name,templateId)=>createNativeProject(name,await chooseParent(),templateId));
    handle('native-reload-project',()=>replaceProject(nativeViewport.project));
    handle('native-animate', value => {
      if (typeof value !== 'boolean') throw new Error('Invalid animation value');
      nativeViewport.send({type: 'animate', value}); return true;
    });
    handle('native-heartbeat', presentation => {nativeViewport.heartbeat = (nativeViewport.heartbeat || 0) + 1; nativeViewport.state.presentation = presentation; return true;});

    let closed = false;
    window.on('close', event => {
      if (closed) return;
      event.preventDefault();
      if (closing) return;
      closing = true;
      replacement.catch(()=>{}).then(async()=>{
        if(!await guard() || !await AssetWindows.closeAll(true)){closing=false;return;}
        await nativeViewport.stop();
        for (const Child of WorkspaceWindows.values()) { Child.destroy(); }
        WorkspaceWindows.clear();
        closed=true;window.close();
      }).catch(error=>{closing=false;dialog.showErrorBox('Проект остаётся открытым',error.message);});
    });
    if (AssetReferenceCheck) require('./scripts/asset-reference-check.cjs').run({app,window,editors:AssetWindows,windows:WorkspaceWindows,getViewport:()=>nativeViewport,createNativeProject});
    if (ReflectedAssetsCheck) require('./scripts/reflected-assets-check.cjs').run({app,window,editors:AssetWindows,windows:WorkspaceWindows,getViewport:()=>nativeViewport,createNativeProject});
    if (ComponentNamesCheck) require('./scripts/component-names-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    if (WorkspaceTabsCheck) {
      require('./scripts/workspace-tabs-check.cjs').run({app,window,editors:AssetWindows,windows:WorkspaceWindows,getViewport:()=>nativeViewport,createNativeProject,restore:WorkspaceTabsRestore});
    }
    if (LocalizationCheck) require('./scripts/localization-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    if (CameraSpeedCheck) require('./scripts/camera-speed-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    if (RenderPerformanceCheck) require('./scripts/render-performance-check.cjs').Run({Application:app,Window:window,GetViewport:()=>nativeViewport,CreateNativeProject:createNativeProject,ReplaceProject:replaceProject});
    if (SceneShortcutsCheck) require('./scripts/scene-shortcuts-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    if (MaterialDropCheck) require('./scripts/material-drop-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    if (MigrationCheck) require('./scripts/qt-removal-check.cjs').Run({Application:app,Window:window,Editors:AssetWindows,GetViewport:()=>nativeViewport});
    else if (assetTransferTest) require('./scripts/asset-transfer-check.cjs').run({app,window,getViewport:()=>nativeViewport,createNativeProject});
    else if (nativeSmoke && !assetTransferTest && !process.argv.some(Argument => (Argument.startsWith('--screenshot-texture=') || Argument.startsWith('--widget-editor-check=') || Argument.startsWith('--sound-editor-check=')))) require('./scripts/native-smoke-main.cjs').run({app, window, getViewport:()=>nativeViewport, replaceProject});
  }
  if (smoke) {
    const deadline = setTimeout(() => {console.error('Desktop smoke: React did not become ready'); app.exit(1);}, 15000);
    window.webContents.on('did-fail-load', (_, code, description) => {console.error('Desktop smoke load failed:', code, description); clearTimeout(deadline); app.exit(1);});
    window.webContents.on('render-process-gone', (_, details) => {console.error('Desktop smoke renderer exited:', details.reason); clearTimeout(deadline); app.exit(1);});
    handle('renderer-ready', async stage => {
      if (stage !== (nativePrototype ? (process.env.SACURA_NATIVE_PROJECT?'editor':'home') : (process.env.SACURA_DESKTOP_SMOKE_PROJECT?'editor':'home'))) return true;
      clearTimeout(deadline); console.log('Desktop smoke: ' + stage + ' mounted with Electron preload'); setTimeout(() => app.quit(), 100); return true;
    });
  } else handle('renderer-ready', async () => true);
  window.webContents.setWindowOpenHandler(Details => {
    if (Details.url !== 'about:blank' || !/^sacura-window-[a-f0-9-]+$/.test(Details.frameName)) {
      return {action:'deny'};
    }
    return {action:'allow',overrideBrowserWindowOptions:{minWidth:360,minHeight:240,backgroundColor:'#191923',autoHideMenuBar:true,webPreferences:{preload:undefined,contextIsolation:true,nodeIntegration:false,sandbox:true,backgroundThrottling:false}}};
  });
  window.webContents.on('did-create-window', (Child, Details) => {
    WorkspaceWindows.set(Details.frameName, Child);
    const Bounds = Child.getBounds();
    const WorkArea = screen.getDisplayMatching(Bounds).workArea;
    if (Bounds.x + Bounds.width < WorkArea.x + 80 || Bounds.x > WorkArea.x + WorkArea.width - 80 || Bounds.y < WorkArea.y || Bounds.y > WorkArea.y + WorkArea.height - 40) {
      Child.setPosition(WorkArea.x + 40, WorkArea.y + 40);
    }
    Child.setMenu(null);
    Child.webContents.setWindowOpenHandler(() => ({action:'deny'}));
    Child.webContents.on('will-navigate', Event => Event.preventDefault());
    Child.on('close', Event => {
      Event.preventDefault();
      window.webContents.send('workspace-close-request', Details.frameName.slice('sacura-'.length));
    });
  });
  window.webContents.on('will-navigate', (event, url) => {
    if (devUrl ? url !== devUrl : !url.startsWith('sacura://app/')) event.preventDefault();
  });
  handle('list-projects', async () => recent());
  handle('create-project', async project => {
    const parent = await chooseParent(); if (!parent) return cancelled();
    const result = await store.createProject(parent, project);
    await releaseNativeProject();
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
    await releaseNativeProject();
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
  if (nativePrototype && process.env.SACURA_NATIVE_DEMO_PROJECT) await openRoot(process.env.SACURA_NATIVE_DEMO_PROJECT);
  if(nativeSmoke)console.log('Native smoke: loading React');
  if (devUrl) await window.loadURL(devUrl); else await window.loadURL('sacura://app/index.html');
  if (MaterialCapture) {
    require('./scripts/capture-material-editor.cjs').run({app, window, editors: AssetWindows, windows:WorkspaceWindows, getViewport: () => nativeViewport, output: MaterialCapture.slice('--screenshot-material='.length)});
  }
  const SoundCheck = process.argv.find(Argument => Argument.startsWith('--sound-editor-check='));
  if (SoundCheck) {
    require('./scripts/sound-editor-check.cjs').run({app,window,windows:WorkspaceWindows,getViewport:()=>nativeViewport,output:SoundCheck.slice('--sound-editor-check='.length)});
  }
  const WidgetCheck = process.argv.find(Argument => Argument.startsWith('--widget-editor-check='));
  if (WidgetCheck) {
    require('./scripts/widget-editor-check.cjs').run({app,window,editors:AssetWindows,windows:WorkspaceWindows,getViewport:()=>nativeViewport,output:WidgetCheck.slice('--widget-editor-check='.length)});
  }
  const TextureCapture = process.argv.find(Argument => Argument.startsWith('--screenshot-texture='));
  if (TextureCapture) {
    require('./scripts/capture-texture-editor.cjs').run({app, window, editors: AssetWindows, windows:WorkspaceWindows, getViewport: () => nativeViewport, output: TextureCapture.slice('--screenshot-texture='.length)});
  }
  if(nativeSmoke)console.log('Native smoke: React document loaded');
  if (nativePrototype && !window.isDestroyed()) await window.webContents.setVisualZoomLevelLimits(1, 1);
}).catch(async error => {console.error(error); await nativeViewport?.stop(); app.exit(1);});
app.on('window-all-closed', () => app.quit());
