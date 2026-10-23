const {contextBridge, ipcRenderer, sharedTexture} = require('electron');
const presentation = {frames: 0, frameIndex: 0, readyToDrawMs: 0, error: null};
let blit;
let lastDraw = 0, periodStart = 0, periodFrames = 0;
const intervals = [], latencies = [], inputLatencies = [];
function disposeBlit() {
  if (blit) {
    blit.gl.clearColor(.09,.106,.14,1);blit.gl.clear(blit.gl.COLOR_BUFFER_BIT);blit.gl.flush();
    blit.gl.deleteTexture(blit.texture); blit.gl.deleteProgram(blit.program); blit = null;
  }
  lastDraw = 0;
}
ipcRenderer.on('native-reset-presentation', () => {disposeBlit(); presentation.error=null;});
window.addEventListener('pagehide', disposeBlit);
function drawNativeFrame(canvas, frame) {
  if (blit && blit.canvas !== canvas) disposeBlit();
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
  }
  const {gl} = blit;
  gl.viewport(0, 0, canvas.width, canvas.height);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, frame);
  gl.drawArrays(gl.TRIANGLES, 0, 3); gl.flush();
  const error = gl.getError(); if (error) throw new Error('Native frame blit GL error ' + error);
}
if (process.argv.includes('--native-viewport')) sharedTexture.setSharedTextureReceiver(async ({importedSharedTexture}, bytes) => {
  let frame;
  try {
    frame = importedSharedTexture.getVideoFrame();
    const canvas = document.querySelector('canvas[data-native-surface]');
    if (canvas && !document.hidden && canvas.checkVisibility({visibilityProperty:true})) {
      if (canvas.width !== frame.codedWidth) canvas.width = frame.codedWidth;
      if (canvas.height !== frame.codedHeight) canvas.height = frame.codedHeight;
      drawNativeFrame(canvas, frame);
      const packet = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
      presentation.frames++;
      presentation.frameIndex = Number(packet.getBigUint64(32, true));
      presentation.readyToDrawMs = performance.timeOrigin + performance.now() - Number(packet.getBigUint64(40, true)) / 1000;
      const now = performance.now();
      if (lastDraw && now-lastDraw<250) {intervals.push(now-lastDraw); if(intervals.length>600) intervals.shift();}
      lastDraw=now; periodFrames++;
      if (!periodStart) periodStart=now;
      if (now-periodStart>=1000) {presentation.fps=periodFrames*1000/(now-periodStart); periodStart=now;periodFrames=0;}
      latencies.push(presentation.readyToDrawMs); if(latencies.length>600) latencies.shift();
      const inputTime=Number(packet.getBigUint64(48,true));
      if (inputTime && presentation.lastInputTime!==inputTime) {
        presentation.lastInputTime=inputTime;
        inputLatencies.push(performance.timeOrigin+now-inputTime/1000); if(inputLatencies.length>100) inputLatencies.shift();
      }
      const percentile=(a,p)=>a.length?[...a].sort((a,b)=>a-b)[Math.floor((a.length-1)*p)]:0;
      Object.assign(presentation,{frameMs:percentile(intervals,.5),frameP95Ms:percentile(intervals,.95),readyToDrawP50Ms:percentile(latencies,.5),readyToDrawP95Ms:percentile(latencies,.95),inputToDrawP50Ms:percentile(inputLatencies,.5),inputToDrawP95Ms:percentile(inputLatencies,.95)});
    }
  } catch (error) {presentation.error = error.message;}
  finally {frame?.close(); importedSharedTexture.release();}
});

// Only narrow, user-initiated project operations cross the isolated context.
contextBridge.exposeInMainWorld('sacuraDesktop', Object.freeze({
  isDesktop: true,
  bridge: Object.freeze({
    connect: () => ipcRenderer.invoke('desktop:bridge-connect'),
    request: request => ipcRenderer.invoke('desktop:bridge-command',request),
    subscribe: callback => {
      const listener = (_,event) => callback(event);ipcRenderer.on('native-bridge-event',listener);
      return () => ipcRenderer.removeListener('native-bridge-event',listener);
    }
  }),
  nativeViewport: process.argv.includes('--native-viewport'),
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
  nativeDetach: () => {disposeBlit();return ipcRenderer.invoke('desktop:native-detach');},
  nativeOpenProject: () => ipcRenderer.invoke('desktop:native-open-project'),
  nativeCreateProject: (name,templateId='Empty') => ipcRenderer.invoke('desktop:native-create-project',name,templateId),
  nativeReloadProject: () => ipcRenderer.invoke('desktop:native-reload-project'),
  nativeImportAssets: destination => ipcRenderer.invoke('desktop:native-import-assets',destination),
  nativeMechanics: (action,params) => ipcRenderer.invoke('desktop:native-mechanics',action,params),
  onNativeMechanicsResult: callback => {
    const listener=(_,result)=>callback(result);ipcRenderer.on('native-mechanics-result',listener);
    return ()=>ipcRenderer.removeListener('native-mechanics-result',listener);
  },
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
