const path = require('node:path');
const {Menu, ipcMain} = require('electron');
const {randomUUID} = require('node:crypto');

exports.createAssetWindows = function({app, BrowserWindow, dialog, NativeViewport, waitForNative, getMainWindow, getViewport, devUrl, nativeSmoke}) {
  const Editors = new Map();
  const Pending = new Map();
  async function Open(Identity) {
    const Catalogue = await getViewport().bridge.request('assets.list', {});
    const Asset = Catalogue.value?.find(Entry => Entry.id === Identity?.assetId);
    if (!Catalogue.ok || !Asset) { throw Error('Asset is unavailable'); }
    if (Asset.typeId === 'Model' && !Identity.subAssetId) { Identity = {...Identity, subAssetId: Asset.subassets.find(Entry => Entry.typeId === 'StaticMesh')?.id}; }
    if (!['Material', 'MaterialInstance', 'Texture'].includes(Asset.typeId) && !Asset.subassets.some(Entry => Entry.id === Identity.subAssetId && Entry.typeId === 'StaticMesh')) {
      getMainWindow().webContents.send('native-reveal-asset', {...Identity, open: true});
      getMainWindow().show(); getMainWindow().focus();
      return true;
    }
    const Key = JSON.stringify([getViewport().project, Identity.assetId, Identity.subAssetId || null]);
    for (const Editor of Editors.values()) {
      if (Editor.key === Key) { Editor.window.show(); Editor.window.focus(); return true; }
    }
    if (Pending.has(Key)) { return Pending.get(Key); }
    const Opening = Create(Identity, Key);
    Pending.set(Key, Opening);
    try { return await Opening; } finally { Pending.delete(Key); }
  }
  async function Create(Identity, Key) {
    const Validation = await getViewport().bridge.request('assets.list', {});
    if (!Validation.ok || !Validation.value.some(Asset => Asset.id === Identity.assetId)) { throw Error('Asset is unavailable'); }
    const EditorWindow = new BrowserWindow({width: 1200, height: 820, minWidth: 760, minHeight: 550, show: false,
      backgroundColor: '#191923', title: 'Редактор ассета', webPreferences: {preload: path.join(__dirname, 'preload.cjs'),
        contextIsolation: true, nodeIntegration: false, sandbox: true, backgroundThrottling: false,
        additionalArguments: ['--native-viewport', '--asset-editor=' + encodeURIComponent(JSON.stringify(Identity)), ...(nativeSmoke ? ['--native-smoke'] : [])]}});
    const Editor = {key: Key, identity: Identity, window: EditorWindow, actions: {}, closed: false, closing: false};
    Editors.set(EditorWindow.webContents.id, Editor);
    try {
      const Owner = await NativeViewport.create(EditorWindow, getViewport().project);
      Editor.viewport = Owner;
      await waitForNative(Owner);
      const Document = await Owner.bridge.request('assetDocument.execute', {...Identity, operation: 'open'});
      if (!Document.ok) { throw Error(Document.error.message); }
      const Preview = await Owner.bridge.request('assetDocument.execute', {...Identity, operation: 'preview', value: {}});
      if (!Preview.ok) { throw Error(Preview.error.message); }
      EditorWindow.setTitle(Validation.value.find(Asset => Asset.id === Identity.assetId).name + ' — Sacura');
      Editor.actions = {
        'bridge-connect': () => ({connected: Owner.bridge.connected, context: Owner.bridge.context, state: Owner.bridge.state}),
        'bridge-command': async Request => {
          const Allowed = ['bridge.describe', 'assets.list', 'assets.thumbnail', 'assetDocument.execute'];
          if (!Allowed.includes(Request?.method) || !Request.context) { throw Error('Invalid asset editor command'); }
          if (Request.method === 'assetDocument.execute' && (Request.params?.assetId !== Identity.assetId || (Request.params?.subAssetId || null) !== (Identity.subAssetId || null))) { throw Error('Document belongs to another editor'); }
          const Result = await Owner.bridge.request(Request.method, Request.params, Request.context);
          if (Result.ok && Request.method === 'assetDocument.execute' && Request.params.operation === 'save') {
            await getViewport().bridge.request('assets.refresh', {});
          }
          return Result;
        },
        'native-session': () => ({phase: Owner.state.phase, connected: Owner.bridge.connected, project: Owner.project}),
        'native-status': () => Owner.state,
        'native-frame-consumed': Token => Owner.frameConsumed(Token),
        'native-bounds': Bounds => Owner.bounds(Bounds),
        'native-tools': Tools => Owner.tools(Tools),
        'native-input': Input => Owner.input(Input),
        'native-heartbeat': () => true,
        'native-detach': () => {Owner.input({kind: 'reset'}); Owner.send({type: 'suspend'}); return true;},
        'native-open-asset-editor': Open,
        'native-reveal-asset': Asset => {getMainWindow().webContents.send('native-reveal-asset', Asset); getMainWindow().show(); getMainWindow().focus(); return true;},
        'renderer-ready': () => true
      };
      for (const EventName of ['minimize', 'hide']) {
        EditorWindow.on(EventName, () => Owner.send({type:'bounds',x:0,y:0,width:0,height:0,visible:false}));
      }
      for (const EventName of ['restore', 'show']) {
        EditorWindow.on(EventName, () => setTimeout(() => {
          if (!EditorWindow.isDestroyed() && Owner.lastBounds) {Owner.bounds({...Owner.lastBounds,visible:true});}
        }, 150));
      }
      const Action = Operation => EditorWindow.webContents.send('asset-editor-action', Operation);
      EditorWindow.setMenu(Menu.buildFromTemplate([
        {label: 'Файл', submenu: [{label: 'Сохранить', enabled: !Document.value.readOnly, accelerator: 'CmdOrCtrl+S', click: () => Action('save')}, {type: 'separator'}, {label: 'Закрыть', click: () => EditorWindow.close()}]},
        {label: 'Правка', submenu: [{label: 'Отменить', enabled: !Document.value.readOnly, accelerator: 'CmdOrCtrl+Z', click: () => Action('undo')}, {label: 'Повторить', enabled: !Document.value.readOnly, accelerator: 'CmdOrCtrl+Y', click: () => Action('redo')}]},
        {label: 'Вид', submenu: [{label: 'В кадр', accelerator: 'F', click: () => {if (Document.value.type === 'Texture') {Action('fit'); return;} Owner.input({kind: 'key', code: 'KeyF', down: true}); Owner.input({kind: 'key', code: 'KeyF', down: false});}}, {role: 'togglefullscreen'}]}
      ]));
      EditorWindow.webContents.setWindowOpenHandler(() => ({action: 'deny'}));
      EditorWindow.webContents.on('will-navigate', (Event, Url) => {if (devUrl ? Url !== devUrl : !Url.startsWith('sacura://app/')) { Event.preventDefault(); }});
      EditorWindow.on('close', Event => {
        if (Editor.closed) { return; }
        Event.preventDefault();
        if (!Editor.closing) { Close(Editor).catch(ErrorValue => dialog.showErrorBox('Ассет остаётся открытым', ErrorValue.message)); }
      });
      EditorWindow.on('closed', () => Editors.delete(Editor.identifier));
      Editor.identifier = EditorWindow.webContents.id;
      if (devUrl) { await EditorWindow.loadURL(devUrl); } else { await EditorWindow.loadURL('sacura://app/index.html'); }
      EditorWindow.show();
      return true;
    } catch (ErrorValue) {
      await Editor.viewport?.stop();
      Editors.delete(EditorWindow.webContents.id);
      EditorWindow.destroy();
      throw ErrorValue;
    }
  }
  async function Close(Editor) {
    Editor.closing = true;
    Editor.window.setEnabled(false);
    Editor.viewport.input({kind: 'reset'});
    try {
      await new Promise((Resolve, Reject) => {
        const RequestId = randomUUID();
        const Timeout = setTimeout(() => {
          ipcMain.removeListener('asset-editor-action-result', Completed);
          Reject(Error('Asset editor did not finish pending edits'));
        }, 10000);
        function Completed(Event, Identity, Success) {
          if (Event.sender !== Editor.window.webContents || Identity !== RequestId) {
            return;
          }
          clearTimeout(Timeout);
          ipcMain.removeListener('asset-editor-action-result', Completed);
          if (!Success) {
            Reject(Error('Pending asset edits could not be applied; the editor remains open'));
            return;
          }
          Resolve();
        }
        ipcMain.on('asset-editor-action-result', Completed);
        Editor.window.webContents.send('asset-editor-action', 'source', RequestId);
      });
      const Result = await Editor.viewport.bridge.request('assetDocument.execute', {...Editor.identity, operation: 'get'});
      if (!Result.ok) { throw Error(Result.error.message); }
      if (Result.value.dirty) {
        const Choice = await dialog.showMessageBox(Editor.window, {type: 'question', message: 'Сохранить изменения ассета?', buttons: ['Сохранить', 'Не сохранять', 'Отмена'], defaultId: 0, cancelId: 2, noLink: true});
        if (Choice.response === 2) { return false; }
        if (Choice.response === 0) {
          const Saved = await Editor.viewport.bridge.request('assetDocument.execute', {...Editor.identity, operation: 'save', revision: Result.value.revision});
          if (!Saved.ok) { throw Error(Saved.error.message); }
          await getViewport().bridge.request('assets.refresh', {});
        }
      }
      await Editor.viewport.stop();
      Editor.closed = true;
      Editor.window.close();
      return true;
    } finally { Editor.closing = false; if (!Editor.window.isDestroyed()) {Editor.window.setEnabled(true);} }
  }
  Editors.blocksMutation = async (Method, Params) => {
    if (!['assets.rename','assets.move','assets.delete','assets.renameFolder','assets.deleteFolder'].includes(Method) || Editors.size === 0) {
      return false;
    }
    const Catalogue = await getViewport().bridge.request('assets.list', {});
    if (!Catalogue.ok) {return true;}
    for (const Editor of Editors.values()) {
      if (Editor.identity.assetId === Params?.assetId) {return true;}
      const Asset = Catalogue.value.find(Entry => Entry.id === Editor.identity.assetId);
      if (Params?.folder && Asset && Asset.path.startsWith(Params.folder.replace(/\/$/,'')+'/')) {return true;}
    }
    return false;
  };
  Editors.open = Open;
  Editors.closeAll = async () => {
    await Promise.all(Pending.values());
    for (const Editor of Editors.values()) { if (!await Close(Editor)) { return false; } }
    return true;
  };
  return Editors;
};
