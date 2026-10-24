const {spawn} = require('node:child_process');
const {createInterface} = require('node:readline');
const path = require('node:path');
const net = require('node:net');
const {randomUUID} = require('node:crypto');
const FileSystem = require('node:fs');
const {sharedTexture, app} = require('electron');
const {setTimeout: delay} = require('node:timers/promises');
const {BridgeTransport} = require('./bridge/transport.cjs');

// Binary frame metadata only; pixels stay on the GPU. JSON carries input/status.
class NativeViewport {
  static async create(window, project = process.env.SACURA_NATIVE_PROJECT, DocumentIdentifier = null) {
    const server = net.createServer();
    const pipe = '\\\\.\\pipe\\sacura-gpu-' + randomUUID();
    await new Promise((resolve, reject) => {server.once('error', reject); server.listen(pipe, resolve);});
    const bridge = await BridgeTransport.listen();
    return new NativeViewport(window, project, server, pipe, bridge, DocumentIdentifier);
  }
  constructor(window, project, server, pipe, bridge, DocumentIdentifier) {
    this.documentId = DocumentIdentifier;
    this.bridge = bridge;
    bridge.on('event', event => {if(!window.isDestroyed()&&!window.webContents.isDestroyed())window.webContents.send(this.documentId ? 'native-document-event' : 'native-bridge-event',event,this.documentId);});
    this.window = window;
    this.project = project;
    this.server = server;
    this.state = {phase: 'starting'};
    this.sequence = 0;
    this.imports = 0;
    this.sharedImports = new Map();
    this.pendingFrames = new Map();
    this.state.transport = {frames: 0, released: 0};
    server.on('connection', socket => {
      if (this.socket) {socket.destroy(); return;}
      this.socket = socket;
      let pending = Buffer.alloc(0);
      socket.on('error', error => {if (!this.stopping) this.state.error = error.message;});
      socket.on('data', data => {
        pending = Buffer.concat([pending, data]);
        while (pending.length >= 56) {
          const packet = Buffer.from(pending.subarray(0, 56)); pending = pending.subarray(56);
          this.receiveFrame(packet).catch(error => {this.state.error = error.message; this.send({type: 'suspend'});});
        }
      });
    });
    const executable = process.env.SACURA_NATIVE_HOST || (app.isPackaged
      ? path.join(process.resourcesPath,'native','SakuraElectronHost.exe')
      : path.resolve(__dirname, '../build/windows-msvc-debug/ElectronNative/Debug/SakuraElectronHost.exe'));
    if (process.platform !== 'win32' || !project) {
      this.state = process.platform === 'win32'
        ? {...this.state, phase: 'idle'}
        : {...this.state, phase: 'error', error: 'Нативный viewport поддерживается только на Windows.'};
      server.close();
      bridge.close();
      return;
    }
    const handle = window.getNativeWindowHandle();
    const LogDirectory = path.join(path.dirname(project), 'Saved', 'Logs');
    const LogPath = path.join(LogDirectory, `Electron-${new Date().toISOString().replace(/[:.]/g, '-')}-${process.pid}-${randomUUID()}.log`);
    let bLogAvailable = true;
    try {
      FileSystem.mkdirSync(LogDirectory, {recursive: true});
    } catch (Failure) {
      bLogAvailable = false;
      console.error('Logging: cannot create log directory:', Failure.message);
    }
    const WriteLog = (Category, Message) => {
      if (!bLogAvailable) {
        return;
      }
      try {
        FileSystem.appendFileSync(LogPath, `${new Date().toISOString()} [${Category}] ${Message}\n`, 'utf8');
      } catch (Failure) {
        bLogAvailable = false;
        console.error('Logging: session log write failed:', Failure.message);
      }
    };
    const LogConsole = Details => {
      WriteLog('Renderer', Details.message);
    };
    const LogRendererExit = (Event, Details) => {
      WriteLog('RendererExit', JSON.stringify(Details));
    };
    window.webContents.on('console-message', LogConsole);
    window.webContents.on('render-process-gone', LogRendererExit);
    WriteLog('Session', `Project: ${project}`);
    this.parent = (handle.length === 8 ? handle.readBigUInt64LE() : BigInt(handle.readUInt32LE())).toString();
    this.child = spawn(executable, [this.parent, project, pipe, String(process.pid), bridge.path], {windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'], env: {...process.env, ...(DocumentIdentifier ? {SACURA_ASSET_WORKSPACE: '1'} : {})}});
    this.closed = new Promise(resolve => this.child.once('close', code => {
      this.state = {...this.state, phase: code === 0 ? 'stopped' : 'error', exitCode: code};
      if (!this.stopping) this.state.error = `Native host exited (${code})`;
      console.log('Native host exit:', code);
      WriteLog('HostExit', String(code));
      if (!window.isDestroyed() && !window.webContents.isDestroyed()) {
        window.webContents.removeListener('console-message', LogConsole);
        window.webContents.removeListener('render-process-gone', LogRendererExit);
      }
      bridge.close();
      this.socket?.destroy(); this.server.close();
      resolve(code);
    }));
    this.child.on('error', error => {
      this.state = {...this.state, phase: 'error', error: error.message};
      WriteLog('HostError', error.message);
    });
    this.child.stdin.on('error', error => {if (!this.stopping) this.state = {...this.state, phase: 'error', error: error.message};});
    this.child.stderr.on('data', data => {
      process.stderr.write(data);
      WriteLog('HostStderr', data.toString('utf8'));
    });
    createInterface({input: this.child.stdout}).on('line', line => {
      console.log(line);
      if (!line.startsWith('@sacura ')) {
        WriteLog('Host', line);
      }
      if (!line.startsWith('@sacura ')) return;
      try {
        const message = JSON.parse(line.slice(8));
        if (message.error) {
          WriteLog('HostError', message.error);
        }
        if (message.error) this.state = {...this.state, phase: 'error', error: message.error};
        else if (message.type === 'ready') this.state = {...this.state, phase: 'ready', ready: message};
        else this.state[message.type] = message;
      } catch (error) {this.state = {...this.state, phase: 'error', error: error.message};}
    });
  }
  async receiveFrame(packet) {
    if (packet.readUInt32LE(0) !== 0x31464753) throw new Error('Invalid GPU frame packet');
    const acknowledge = () => {
      if (this.socket?.writable) this.socket.write(packet.subarray(16, 24));
    };
    if (this.stopping || this.window.isDestroyed() || this.window.webContents.isDestroyed()) {acknowledge(); return;}
    const width = packet.readUInt32LE(4), height = packet.readUInt32LE(8);
    if (!width || !height || width > 8192 || height > 8192 || width * height > 33554432) {acknowledge(); throw new Error('Invalid GPU texture size');}
    const Identifier = packet.readBigUInt64LE(24).toString() + ':' + width + ':' + height;
    const Token = packet.readBigUInt64LE(16).toString();
    let Resource = this.sharedImports.get(Identifier);
    if (Resource) {
      // Native slots change in place; Chromium must acquire their new contents.
      Resource.imported.release();
      this.sharedImports.delete(Identifier);
      Resource = null;
    }
    if (!Resource) {
      const Imported = sharedTexture.importSharedTexture({textureInfo: {
        pixelFormat: 'rgba', codedSize: {width, height}, handle: {ntHandle: Buffer.from(packet.subarray(24, 32))},
        timestamp: Number(packet.readBigUInt64LE(40))
      }});
      Resource = {imported:Imported,width,height};
      this.sharedImports.set(Identifier,Resource);
    }
    for (const [PreviousIdentifier, PreviousResource] of this.sharedImports) {
      if (PreviousIdentifier !== Identifier && (PreviousResource.width !== width || PreviousResource.height !== height)) {
        const Pending = Array.from(this.pendingFrames.values()).some(Frame => Frame.resource === PreviousResource);
        if (!Pending) {PreviousResource.imported.release();this.sharedImports.delete(PreviousIdentifier);}
      }
    }
    this.imports++;
    this.pendingFrames.set(Token,{acknowledge,resource:Resource});
    try {
      await sharedTexture.sendSharedTexture({frame: this.window.webContents.mainFrame, importedSharedTexture: Resource.imported}, Buffer.concat([packet, Buffer.from(this.documentId || "")]));
      this.state.transport.frames++;
    } catch (ErrorValue) {
      this.frameConsumed(Token);
      throw ErrorValue;
    }
  }
  frameConsumed(Token) {
    if (typeof Token !== 'string') {throw Error('Invalid frame token');}
    const Frame = this.pendingFrames.get(Token);
    if (!Frame) {return false;}
    this.pendingFrames.delete(Token);
    this.imports--;
    this.state.transport.released++;
    Frame.acknowledge();
    return true;
  }

  send(message) {
    if (!this.stopping && this.child?.stdin.writable) this.child.stdin.write(JSON.stringify(message) + '\n');
  }
  input(value) {
    if (!value || !['reset','key','pointer','wheel','gameControl','removeSource'].includes(value.kind)) throw new Error('Invalid input');
    const event = {kind:value.kind, ctrl:!!value.ctrl, alt:!!value.alt, shift:!!value.shift};
    if (value.kind === 'gameControl' || value.kind === 'removeSource') {
      if (!/^Electron(Pointer|Gamepad[0-9]+|Touch[A-Za-z]{1,24})$/.test(value.source)) { throw new Error('Invalid input source'); }
      event.source = value.source;
      if (value.kind === 'gameControl') {
        if (!['Mouse', 'Gamepad', 'Touch'].includes(value.device) || !/^[A-Za-z]{1,24}$/.test(value.control)
          || !Array.isArray(value.value) || value.value.length !== 3 || !value.value.every(Number.isFinite)) {
          throw new Error('Invalid game control');
        }
        Object.assign(event, {device: value.device, control: value.control, value: value.value, transient: !!value.transient});
      }
    }
    if (value.kind === 'key') {
      if (!/^[A-Za-z][A-Za-z0-9]{0,47}$/.test(value.code)) throw new Error('Invalid key');
      Object.assign(event, {code:value.code, down:!!value.down, repeat:!!value.repeat});
    }
    if (value.kind === 'pointer') {
      if (!['down','move','up'].includes(value.phase) || ![value.x,value.y].every(v=>Number.isFinite(v)&&Math.abs(v)<65536) || !Number.isInteger(value.buttons) || value.buttons<0 || value.buttons>31 || !Number.isInteger(value.button) || value.button < -1 || value.button>4) throw new Error('Invalid pointer');
      Object.assign(event, {phase:value.phase,x:value.x,y:value.y,button:value.button,buttons:value.buttons});
    }
    if (value.kind === 'wheel') {
      if (!Number.isFinite(value.delta)) throw new Error('Invalid wheel');
      event.delta = Math.max(-1000,Math.min(1000,value.delta));
    }
    this.send({type:'input', event, timestampUs:Date.now()*1000});
    return true;
  }
  tools(value) {
    if (!value || !['translate','move','rotate','scale','select'].includes(value.tool) || !['world','local'].includes(value.space)) throw new Error('Invalid tools');
    if (value.cameraSpeed !== undefined && (!Number.isFinite(value.cameraSpeed) || value.cameraSpeed < 0.1 || value.cameraSpeed > 100)) {
      throw new Error('Camera speed must be between 0.1 and 100 metres per second');
    }
    this.lastTools={tool:value.tool==='move'?'translate':value.tool,space:value.space,snap:!!value.snap,grid:!!value.grid};
    if (value.cameraSpeed !== undefined) {
      this.lastTools.cameraSpeed = value.cameraSpeed;
    }
    if (typeof value.visualization === 'string') { this.lastTools.visualization = value.visualization; }
    if (Number.isFinite(value.depthRange) && value.depthRange >= 0.01 && value.depthRange <= 100000) { this.lastTools.depthRange = value.depthRange; }
    this.send({type:'tools',...this.lastTools});
    return true;
  }
  bounds(value) {
    if (!value || typeof value.visible !== 'boolean') throw new Error('Invalid viewport visibility');
    for (const key of ['x', 'y', 'width', 'height']) {
      if (!Number.isInteger(value[key]) || value[key] < 0 || value[key] > 16384) throw new Error('Invalid viewport bounds');
    }
    if (value.width>8192 || value.height>8192 || value.width*value.height>33554432) throw new Error('Viewport exceeds GPU allocation limit');
    const scale = value.scale ?? 1;
    if (!Number.isFinite(scale) || scale<0.25 || scale>8) throw new Error('Invalid DPI scale');
    const bounds = {type: 'bounds', seq: ++this.sequence, x: value.x, y: value.y, width: value.width, height: value.height,
      scale, interactive:!!value.interactive,
      visible: value.visible};
    this.lastBounds = bounds;
    this.send(bounds);
    return bounds.seq;
  }
  async stop() {
    if (!this.child || this.stopping) return this.closed;
    this.send({type: 'suspend'});
    this.stopping = true;
    this.bridge.disconnect('Native host is stopping');
    const drainUntil = Date.now() + 5000;
    while (this.imports && Date.now() < drainUntil) await delay(10);
    for (const Resource of this.sharedImports.values()) {Resource.imported.release();}
    this.sharedImports.clear();
    if (this.child.stdin.writable) this.child.stdin.write('{"type":"shutdown"}\n');
    // Graceful Render-thread join happens before Electron destroys the parent HWND.
    const timeout = setTimeout(() => {console.error('Native host shutdown timed out'); this.child.kill();}, 10000);
    const code = await this.closed;
    clearTimeout(timeout);
    return code;
  }
}
module.exports = {NativeViewport};

