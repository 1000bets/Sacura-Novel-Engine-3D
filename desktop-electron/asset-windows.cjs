const {ipcMain} = require('electron');
const {randomUUID} = require('node:crypto');

exports.createAssetWindows = function({dialog, NativeViewport, waitForNative, getMainWindow, getViewport}) {
  const Editors = new Map();
  const Pending = new Map();
  function Describe(Editor) {
    return {id:Editor.identifier, identity:Editor.identity, label:Editor.label, project:Editor.viewport.project};
  }
  async function Open(Identity) {
    const Catalogue = await getViewport().bridge.request('assets.list', {});
    const Asset = Catalogue.value?.find(Entry => Entry.id === Identity?.assetId);
    if (!Catalogue.ok || !Asset) {
      throw Error('Asset is unavailable');
    }
    if (Asset.typeId === 'Model' && !Identity.subAssetId) {
      Identity = {...Identity, subAssetId:Asset.subassets.find(Entry => Entry.typeId === 'StaticMesh')?.id};
    }
    if (!['Material','MaterialInstance','Texture','PythonScript'].includes(Asset.typeId) && !Asset.subassets.some(Entry => Entry.id === Identity.subAssetId && Entry.typeId === 'StaticMesh')) {
      getMainWindow().webContents.send('native-reveal-asset', {...Identity,open:true});
      return null;
    }
    const Identifier = 'asset:' + Identity.assetId + ':' + (Identity.subAssetId || '');
    if (Pending.has(Identifier)) {
      return Pending.get(Identifier);
    }
    if (Editors.has(Identifier)) {
      const Descriptor = Describe(Editors.get(Identifier));
      getMainWindow().webContents.send('workspace-document-open', Descriptor);
      return Descriptor;
    }

    const Opening = Create(Identity, Identifier, Asset.name);
    Pending.set(Identifier, Opening);
    try {
      return await Opening;
    } finally {
      Pending.delete(Identifier);
    }
  }
  async function Create(Identity, Identifier, Label) {
    const Owner = await NativeViewport.create(getMainWindow(), getViewport().project, Identifier);
    const Editor = {identifier:Identifier, identity:Identity, label:Label, viewport:Owner, closing:false};
    Editors.set(Identifier, Editor);
    try {
      await waitForNative(Owner);
      const Document = await Owner.bridge.request('assetDocument.execute', {...Identity,operation:'open'});
      if (!Document.ok) {
        throw Error(Document.error.message);
      }
      const Preview = await Owner.bridge.request('assetDocument.execute', {...Identity,operation:'preview',value:{}});
      if (!Preview.ok) {
        throw Error(Preview.error.message);
      }
      Editor.ready = true;
      const Descriptor = Describe(Editor);
      getMainWindow().webContents.send('workspace-document-open', Descriptor);
      return Descriptor;
    } catch (Failure) {
      await Owner.stop();
      Editors.delete(Identifier);
      throw Failure;
    }
  }
  function Flush(Editor) {
    return new Promise((Resolve, Reject) => {
      const RequestId = randomUUID();
      const Timeout = setTimeout(() => {
        ipcMain.removeListener('asset-editor-action-result', Completed);
        Reject(Error('Asset editor did not finish pending edits'));
      }, 10000);
      function Completed(Event, Identity, Success) {
        if (Event.sender !== getMainWindow().webContents || Identity !== RequestId) {
          return;
        }
        clearTimeout(Timeout);
        ipcMain.removeListener('asset-editor-action-result', Completed);
        if (!Success) {
          Reject(Error('Pending asset edits could not be applied; the tab remains open'));
          return;
        }
        Resolve();
      }
      ipcMain.on('asset-editor-action-result', Completed);
      getMainWindow().webContents.send('asset-editor-action', 'source', RequestId, Editor.identifier);
    });
  }
  async function Close(Editor, ConfirmOnly = false, Notify = true) {
    if (!Editor) {
      return true;
    }
    if (Editor.closing) {
      return false;
    }
    Editor.closing = true;
    try {
      await Flush(Editor);
      const Result = await Editor.viewport.bridge.request('assetDocument.execute', {...Editor.identity,operation:'get'});
      if (!Result.ok) {
        throw Error(Result.error.message);
      }
      if (Result.value.dirty) {
        const Choice = await dialog.showMessageBox(getMainWindow(), {type:'question',message:'Сохранить изменения «' + Editor.label + '»?',buttons:['Сохранить','Не сохранять','Отмена'],defaultId:0,cancelId:2,noLink:true});
        if (Choice.response === 2) {
          return false;
        }
        if (Choice.response === 0) {
          const Saved = await Editor.viewport.bridge.request('assetDocument.execute', {...Editor.identity,operation:'save',revision:Result.value.revision});
          if (!Saved.ok) {
            throw Error(Saved.error.message);
          }
          await getViewport().bridge.request('assets.refresh', {});
        }
      }
      if (ConfirmOnly) {
        return true;
      }
      await Editor.viewport.stop();
      Editors.delete(Editor.identifier);
      if (Notify) {
        getMainWindow().webContents.send('workspace-document-closed', Editor.identifier);
      }
      return true;
    } finally {
      Editor.closing = false;
    }
  }
  Editors.call = async (Identifier, Channel, Value) => {
    const Editor = Editors.get(Identifier);
    if (!Editor) {
      if (['native-detach','native-frame-consumed','native-input','native-heartbeat','native-bounds','native-tools'].includes(Channel)) {
        return false;
      }
      if (Channel === 'native-status') {
        return {phase:'stopped'};
      }
      if (Channel === 'bridge-command') {
        return {version:1,type:'bridge.result',id:null,ok:false,context:Value?.context,error:{code:'DISCONNECTED',message:'Document session is closed'}};
      }
      throw Error('Document session is closed');
    }
    const Owner = Editor.viewport;
    if (Channel === 'bridge-connect') {
      return {connected:Owner.bridge.connected,context:Owner.bridge.context,state:Owner.bridge.state};
    }
    if (Channel === 'bridge-command') {
      if (!['bridge.describe','assets.list','assets.thumbnail','assetDocument.execute'].includes(Value?.method) || !Value.context) {
        throw Error('Invalid asset editor command');
      }
      if (Value.method === 'assetDocument.execute' && (Value.params?.assetId !== Editor.identity.assetId || (Value.params?.subAssetId || null) !== (Editor.identity.subAssetId || null))) {
        throw Error('Document belongs to another tab');
      }
      const Result = await Owner.bridge.request(Value.method, Value.params, Value.context);
      if (Result.ok && Value.params?.operation === 'save') {
        await getViewport().bridge.request('assets.refresh', {});
      }
      return Result;
    }
    if (Channel === 'native-status') {
      return Owner.state;
    }
    if (Channel === 'native-bounds') {
      return Owner.bounds(Value);
    }
    if (Channel === 'native-input') {
      return Owner.input(Value);
    }
    if (Channel === 'native-tools') {
      return Owner.tools(Value);
    }
    if (Channel === 'native-frame-consumed') {
      return Owner.frameConsumed(Value);
    }
    if (Channel === 'native-heartbeat') {
      return true;
    }
    if (Channel === 'native-detach') {
      Owner.input({kind:'reset'});
      Owner.send({type:'bounds',x:0,y:0,width:0,height:0,visible:false});
      return true;
    }
    throw Error('Operation is not available in a document tab');
  };
  Editors.blocksMutation = async (Method, Params) => {
    if (!['assets.rename','assets.move','assets.delete','assets.renameFolder','assets.deleteFolder'].includes(Method) || Editors.size === 0) {
      return false;
    }
    const Catalogue = await getViewport().bridge.request('assets.list', {});
    if (!Catalogue.ok) {
      return true;
    }
    for (const Editor of Editors.values()) {
      if (Editor.identity.assetId === Params?.assetId) {
        return true;
      }
      const Asset = Catalogue.value.find(Entry => Entry.id === Editor.identity.assetId);
      if (Params?.folder && Asset && Asset.path.startsWith(Params.folder.replace(/\/$/,'')+'/')) {
        return true;
      }
    }
    return false;
  };
  Editors.open = Open;
  Editors.list = () => Array.from(Editors.values()).filter(Editor => Editor.ready).map(Describe);
  Editors.close = Identifier => Close(Editors.get(Identifier));
  Editors.closeAll = async (PreserveLayout = false) => {
    await Promise.all(Pending.values());
    for (const Editor of Editors.values()) {
      if (!await Close(Editor, true)) {
        return false;
      }
    }
    for (const Editor of Editors.values()) {
      await Editor.viewport.stop();
      Editors.delete(Editor.identifier);
      if (!PreserveLayout) {
        getMainWindow().webContents.send('workspace-document-closed', Editor.identifier);
      }
    }
    return true;
  };
  return Editors;
};
