const {contextBridge, ipcRenderer, sharedTexture} = require('electron');
const presentation = {frames: 0, frameIndex: 0, readyToDrawMs: 0, error: null};
let blit;
const WorkspaceDocuments = new Map();
const SurfaceBlits = new Map();
const DocumentPresentation = new Map();
const SurfaceTimings = new Map();
function FindCanvas(Identifier) {
  const Documents = [document, ...Array.from(WorkspaceDocuments.values()).filter(Popup => !Popup.closed).map(Popup => Popup.document)];
  for (const OwnerDocument of Documents) {
    for (const Canvas of OwnerDocument.querySelectorAll('canvas[data-native-surface]')) {
      if ((Canvas.dataset.nativeOwner || '') === Identifier) {
        return Canvas;
      }
    }
  }
  return null;
}
let lastDraw = 0, periodStart = 0, periodFrames = 0;
const intervals = [], latencies = [], inputLatencies = [];
function disposeBlit(Identifier) {
  for (const [Canvas, Surface] of SurfaceBlits) {
    if (typeof Identifier === 'string' && (Canvas.dataset.nativeOwner || '') !== Identifier) {
      continue;
    }
    Surface.gl.deleteTexture(Surface.texture);
    Surface.gl.deleteProgram(Surface.program);
    SurfaceBlits.delete(Canvas);
  }
  blit = null;
  lastDraw = 0;
}
ipcRenderer.on('native-reset-presentation', () => {disposeBlit(); presentation.error=null;});
window.addEventListener('pagehide', disposeBlit);
function drawNativeFrame(canvas, frame) {
  blit = SurfaceBlits.get(canvas);
  if (!blit || blit.canvas !== canvas) {
    const gl = canvas.getContext('webgl2', {alpha: false, antialias: false, depth: false, preserveDrawingBuffer: false});
    if (!gl) throw new Error('GPU VideoFrame presentation requires WebGL2');
    const shader = (type, code) => {
      const s = gl.createShader(type); gl.shaderSource(s, code); gl.compileShader(s);
      if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s)); return s;
    };
    const vertex = shader(gl.VERTEX_SHADER, '#version 300 es\nout vec2 uv; void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=vec2(p.x,1.0-p.y); gl_Position=vec4(p*2.0-1.0,0,1); }');
    const fragment = shader(gl.FRAGMENT_SHADER, '#version 300 es\nprecision highp float; in vec2 uv; uniform sampler2D image; out vec4 color; void main(){color=texture(image,uv);}');
    const program = gl.createProgram(); gl.attachShader(program, vertex); gl.attachShader(program, fragment); gl.linkProgram(program);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(program));
    gl.deleteShader(vertex); gl.deleteShader(fragment); gl.useProgram(program);
    const texture = gl.createTexture(); gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    blit = {canvas, gl, texture, program};
    SurfaceBlits.set(canvas, blit);
  }
  const {gl} = blit;
  gl.viewport(0, 0, canvas.width, canvas.height);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, frame);
  gl.drawArrays(gl.TRIANGLES, 0, 3); gl.finish();
  const error = gl.getError(); if (error) throw new Error('Native frame blit GL error ' + error);
}
if (process.argv.includes('--native-viewport')) sharedTexture.setSharedTextureReceiver(async ({importedSharedTexture}, bytes) => {
  let frame;
  const Identifier = new TextDecoder().decode(bytes.subarray(56));
  let Metrics = presentation;
  if (Identifier) {
    Metrics = DocumentPresentation.get(Identifier) || {frames:0,received:0};
  }
  Metrics.received = (Metrics.received || 0) + 1;
  const Timing = SurfaceTimings.get(Identifier) || {lastDraw:0,periodStart:0,periodFrames:0,intervals:[],latencies:[],inputLatencies:[]};
  SurfaceTimings.set(Identifier, Timing);
  DocumentPresentation.set(Identifier, Metrics);
  try {
    frame = importedSharedTexture.getVideoFrame();
    const canvas = FindCanvas(Identifier);
    Metrics.canvasFound = Boolean(canvas);
    Metrics.visible = Boolean(canvas?.checkVisibility({visibilityProperty:true}));
    if (canvas && !canvas.ownerDocument.hidden && canvas.checkVisibility({visibilityProperty:true})) {
      if (canvas.width !== frame.codedWidth) canvas.width = frame.codedWidth;
      if (canvas.height !== frame.codedHeight) canvas.height = frame.codedHeight;
      drawNativeFrame(canvas, frame);
      const packet = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
      Metrics.frames++;
      Metrics.frameIndex = Number(packet.getBigUint64(32, true));
      Metrics.readyToDrawMs = performance.timeOrigin + performance.now() - Number(packet.getBigUint64(40, true)) / 1000;
      const now = performance.now();
      if (Timing.lastDraw) {Timing.intervals.push(now-Timing.lastDraw); if(Timing.intervals.length>600) Timing.intervals.shift();}
      Timing.lastDraw=now; Timing.periodFrames++;
      if (!Timing.periodStart) Timing.periodStart=now;
      if (now-Timing.periodStart>=1000) {Metrics.fps=Timing.periodFrames*1000/(now-Timing.periodStart); Timing.periodStart=now;Timing.periodFrames=0;}
      Timing.latencies.push(Metrics.readyToDrawMs); if(Timing.latencies.length>600) Timing.latencies.shift();
      const inputTime=Number(packet.getBigUint64(48,true));
      if (inputTime && Metrics.lastInputTime!==inputTime) {
        Metrics.lastInputTime=inputTime;
        Timing.inputLatencies.push(performance.timeOrigin+now-inputTime/1000); if(Timing.inputLatencies.length>100) Timing.inputLatencies.shift();
      }
      const percentile=(a,p)=>a.length?[...a].sort((a,b)=>a-b)[Math.floor((a.length-1)*p)]:0;
      Object.assign(Metrics,{frameMs:percentile(Timing.intervals,.5),frameP95Ms:percentile(Timing.intervals,.95),readyToDrawP50Ms:percentile(Timing.latencies,.5),readyToDrawP95Ms:percentile(Timing.latencies,.95),inputToDrawP50Ms:percentile(Timing.inputLatencies,.5),inputToDrawP95Ms:percentile(Timing.inputLatencies,.95)});
    }
  } catch (error) {
    if (Metrics.error !== error.message) {
      console.error('Native frame presentation failed:', error.message);
    }
    Metrics.error = error.message;
  }
  finally {
    frame?.close();
    importedSharedTexture.release();
    const Packet = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    if (Identifier) {
      await ipcRenderer.invoke('desktop:document-call', Identifier, 'native-frame-consumed', Packet.getBigUint64(16, true).toString()).catch(() => {});
    } else {
      await ipcRenderer.invoke('desktop:native-frame-consumed', Packet.getBigUint64(16, true).toString());
    }
  }
});

// Only narrow, user-initiated project operations cross the isolated context.
contextBridge.exposeInMainWorld('sacuraDesktop', Object.freeze({
  isDesktop: true,
  workspaceDragSurfaces: () => ipcRenderer.invoke('desktop:workspace-drag-surfaces'),
  registerWorkspaceWindow: Name => {
    if (!/^sacura-window-[a-f0-9-]+$/.test(Name)) {
      throw Error('Invalid workspace window');
    }
    WorkspaceDocuments.set(Name, window.open('', Name));
    return true;
  },
  focusWorkspaceWindow: Name => ipcRenderer.invoke('desktop:workspace-focus-window', Name),
  closeWorkspaceWindow: Name => {
    WorkspaceDocuments.delete(Name);
    return ipcRenderer.invoke('desktop:workspace-close-window', Name);
  },
  onWorkspaceClose: Callback => {
    const Listener = (_, Area) => Callback(Area);
    ipcRenderer.on('workspace-close-request', Listener);
    return () => ipcRenderer.removeListener('workspace-close-request', Listener);
  },
  nativeDocumentCall: (Identifier, Channel, Value) => {
    if (Channel === 'native-detach') {
      disposeBlit(Identifier);
      DocumentPresentation.delete(Identifier);
      SurfaceTimings.delete(Identifier);
    }
    return ipcRenderer.invoke('desktop:document-call', Identifier, Channel, Value);
  },
  nativeDocumentStatus: async Identifier => ({...await ipcRenderer.invoke('desktop:document-call', Identifier, 'native-status'),presentation:DocumentPresentation.get(Identifier) || {frames:0}}),
  nativeDocuments: () => ipcRenderer.invoke('desktop:document-list'),
  nativeCloseDocument: Identifier => ipcRenderer.invoke('desktop:document-close', Identifier),
  onDocumentOpen: Callback => {
    const Listener = (_, Descriptor) => Callback(Descriptor);
    ipcRenderer.on('workspace-document-open', Listener);
    return () => ipcRenderer.removeListener('workspace-document-open', Listener);
  },
  onDocumentClosed: Callback => {
    const Listener = (_, Identifier) => Callback(Identifier);
    ipcRenderer.on('workspace-document-closed', Listener);
    return () => ipcRenderer.removeListener('workspace-document-closed', Listener);
  },
  onDocumentEvent: (Identifier, Callback) => {
    const Listener = (_, Event, Owner) => {
      if (Owner === Identifier) {
        Callback(Event);
      }
    };
    ipcRenderer.on('native-document-event', Listener);
    return () => ipcRenderer.removeListener('native-document-event', Listener);
  },
  editorLocalization: Values => ipcRenderer.invoke('desktop:editor-localization', Values),
  onEditorLocalization: Callback => {
    const Listener = (_, Values) => Callback(Values);
    ipcRenderer.on('editor-localization-changed', Listener);
    return () => ipcRenderer.removeListener('editor-localization-changed', Listener);
  },
  bridge: Object.freeze({
    connect: () => ipcRenderer.invoke('desktop:bridge-connect'),
    request: request => ipcRenderer.invoke('desktop:bridge-command',request),
    subscribe: callback => {
      const listener = (_,event) => callback(event);ipcRenderer.on('native-bridge-event',listener);
      return () => ipcRenderer.removeListener('native-bridge-event',listener);
    }
  }),
  assetEditorIdentity: (() => {const Argument = process.argv.find(Value => Value.startsWith('--asset-editor=')); if (Argument) {return JSON.parse(decodeURIComponent(Argument.slice(15)));} return null;})(),
  nativeCopyText: Text => ipcRenderer.invoke('desktop:native-copy-text', Text),
  nativeOpenAssetEditor: Identity => ipcRenderer.invoke('desktop:native-open-asset-editor', Identity),
  nativeRevealAsset: Identity => ipcRenderer.invoke('desktop:native-reveal-asset', Identity),
  onAssetEditorAction: (Callback, Identifier = null) => {
    const Listener = async (Event, Action, RequestId, Owner = null) => {
      if (Identifier !== Owner) {
        return;
      }
      let Completed = false;
      try {
        Completed = Boolean(await Callback(Action));
      } finally {
        if (RequestId) {
          ipcRenderer.send('asset-editor-action-result', RequestId, Completed);
        }
      }
    };
    ipcRenderer.on('asset-editor-action', Listener);
    return () => ipcRenderer.removeListener('asset-editor-action', Listener);
  },
  onRevealAsset: Callback => {const Listener = (_, Identity) => Callback(Identity); ipcRenderer.on('native-reveal-asset', Listener); return () => ipcRenderer.removeListener('native-reveal-asset', Listener);},
  nativeViewport: process.argv.includes('--native-viewport'),
  nativePlayer: process.argv.includes('--sakura-player'),
  playerQuit: () => ipcRenderer.invoke('desktop:player-quit'),
  nativeExportGame: () => ipcRenderer.invoke('desktop:native-export-game'),
  nativeSession: () => ipcRenderer.invoke('desktop:native-session'),
  nativeListProjects: () => ipcRenderer.invoke('desktop:native-list-projects'),
  nativeOpenRecent: id => ipcRenderer.invoke('desktop:native-open-recent',id),
  nativeForgetProject: id => ipcRenderer.invoke('desktop:native-forget-project',id),
  nativeCloseProject: () => ipcRenderer.invoke('desktop:native-close-project'),
  nativeBounds: bounds => ipcRenderer.invoke('desktop:native-bounds', bounds),
  nativeStatus: async () => ({...await ipcRenderer.invoke('desktop:native-status'), presentation: {...presentation}}),
  nativeAnimate: value => ipcRenderer.invoke('desktop:native-animate', value),
  nativeHeartbeat: () => ipcRenderer.invoke('desktop:native-heartbeat', {...presentation}),
  nativeInput: event => ipcRenderer.invoke('desktop:native-input', event),
  nativeTools: value => ipcRenderer.invoke('desktop:native-tools', value),
  nativeDetach: () => {disposeBlit('');return ipcRenderer.invoke('desktop:native-detach');},
  nativeOpenProject: () => ipcRenderer.invoke('desktop:native-open-project'),
  nativeCreateProject: (name,templateId='Empty') => ipcRenderer.invoke('desktop:native-create-project',name,templateId),
  nativeReloadProject: () => ipcRenderer.invoke('desktop:native-reload-project'),
  nativeImportAssets: destination => ipcRenderer.invoke('desktop:native-import-assets',destination),
  onNativeTest: process.argv.includes('--native-smoke') ? callback => {
    const listener = (_, command) => callback(command); ipcRenderer.on('native-test',listener);
    return () => ipcRenderer.removeListener('native-test',listener);
  } : undefined,
  bridgeTestResult: process.argv.includes('--native-smoke') ? result => ipcRenderer.invoke('desktop:bridge-test-result',result) : undefined,
  listProjects: () => ipcRenderer.invoke('desktop:list-projects'),
  createProject: project => ipcRenderer.invoke('desktop:create-project', project),
  openProject: () => ipcRenderer.invoke('desktop:open-project'),
  openRecent: id => ipcRenderer.invoke('desktop:open-recent', id),
  saveProject: project => ipcRenderer.invoke('desktop:save-project', project),
  saveProjectAs: project => ipcRenderer.invoke('desktop:save-project-as', project),
  pickAssets: () => ipcRenderer.invoke('desktop:pick-assets'),
  ready: stage => ipcRenderer.invoke('desktop:renderer-ready', stage),
}));
