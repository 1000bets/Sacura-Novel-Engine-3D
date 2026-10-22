const {contextBridge, ipcRenderer} = require('electron');

// Only narrow, user-initiated project operations cross the isolated context.
contextBridge.exposeInMainWorld('sacuraDesktop', Object.freeze({
  isDesktop: true,
  listProjects: () => ipcRenderer.invoke('desktop:list-projects'),
  createProject: project => ipcRenderer.invoke('desktop:create-project', project),
  openProject: () => ipcRenderer.invoke('desktop:open-project'),
  openRecent: id => ipcRenderer.invoke('desktop:open-recent', id),
  saveProject: project => ipcRenderer.invoke('desktop:save-project', project),
  saveProjectAs: project => ipcRenderer.invoke('desktop:save-project-as', project),
  pickAssets: () => ipcRenderer.invoke('desktop:pick-assets'),
  ready: stage => ipcRenderer.invoke('desktop:renderer-ready', stage),
}));
