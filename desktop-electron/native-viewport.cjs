const {spawn} = require('node:child_process');
const {createInterface} = require('node:readline');
const path = require('node:path');
const net = require('node:net');
const {randomUUID} = require('node:crypto');
const {sharedTexture, app} = require('electron');
const {setTimeout: delay} = require('node:timers/promises');
const {BridgeTransport} = require('./bridge/transport.cjs');

// Binary frame metadata only; pixels stay on the GPU. JSON carries input/status.
class NativeViewport {
  static async create(window, project = process.env.SACURA_NATIVE_PROJECT) {
    const server = net.createServer();
    const pipe = '\\\\.\\pipe\\sacura-gpu-' + randomUUID();
    await new Promise((resolve, reject) => {server.once('error', reject); server.listen(pipe, resolve);});
    const bridge = await BridgeTransport.listen();
    return new NativeViewport(window, project, server, pipe, bridge);
  }
  constructor(window, project, server, pipe, bridge) {
    this.bridge = bridge;
    bridge.on('event', event => {if(!window.isDestroyed()&&!window.webContents.isDestroyed())window.webContents.send('native-bridge-event',event);});
    this.window = window;
    this.project = project;
    this.server = server;
    this.state = {phase: 'starting'};
    this.sequence = 0;
    this.imports = 0;
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
    this.parent = (handle.length === 8 ? handle.readBigUInt64LE() : BigInt(handle.readUInt32LE())).toString();
    this.child = spawn(executable, [this.parent, project, pipe, String(process.pid), bridge.path], {windowsHide: true, stdio: ['pipe', 'pipe', 'pipe']});
    this.closed = new Promise(resolve => this.child.once('close', code => {
      this.state = {...this.state, phase: code === 0 ? 'stopped' : 'error', exitCode: code};
      if (!this.stopping) this.state.error = `Native host exited (${code})`;
      console.log('Native host exit:', code);
      bridge.close();
      this.socket?.destroy(); this.server.close();
      resolve(code);
    }));
    this.child.on('error', error => {this.state = {...this.state, phase: 'error', error: error.message};});
    this.child.stdin.on('error', error => {if (!this.stopping) this.state = {...this.state, phase: 'error', error: error.message};});
    this.child.stderr.on('data', data => process.stderr.write(data));
    createInterface({input: this.child.stdout}).on('line', line => {
      console.log(line);
      if (!line.startsWith('@sacura ')) return;
      try {
        const message = JSON.parse(line.slice(8));
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
    let imported;
    this.imports++;
    try {
      imported = sharedTexture.importSharedTexture({textureInfo: {
        pixelFormat: 'rgba', codedSize: {width, height}, handle: {ntHandle: Buffer.from(packet.subarray(24, 32))},
        timestamp: Number(packet.readBigUInt64LE(40))
      }, allReferencesReleased: () => {this.imports--; this.state.transport.released++; acknowledge();}});
    } catch (error) {this.imports--; acknowledge(); throw error;}
    try {
      await sharedTexture.sendSharedTexture({frame: this.window.webContents.mainFrame, importedSharedTexture: imported}, packet);
      this.state.transport.frames++;
    } finally {imported.release();}
  }
  send(message) {
    if (!this.stopping && this.child?.stdin.writable) this.child.stdin.write(JSON.stringify(message) + '\n');
  }
  input(value) {
    if (!value || !['reset','key','pointer','wheel'].includes(value.kind)) throw new Error('Invalid input');
    const event = {kind:value.kind, ctrl:!!value.ctrl, alt:!!value.alt, shift:!!value.shift};
    if (value.kind === 'key') {
      if (!/^(Key[A-Z]|Escape|ShiftLeft|ShiftRight|AltLeft|AltRight|ControlLeft|ControlRight)$/.test(value.code)) throw new Error('Invalid key');
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
    this.lastTools={tool:value.tool==='move'?'translate':value.tool,space:value.space,snap:!!value.snap,grid:!!value.grid};
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
      visible: value.visible && this.window.isVisible() && !this.window.isMinimized()};
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
    if (this.child.stdin.writable) this.child.stdin.write('{"type":"shutdown"}\n');
    // Graceful Render-thread join happens before Electron destroys the parent HWND.
    const timeout = setTimeout(() => {console.error('Native host shutdown timed out'); this.child.kill();}, 10000);
    const code = await this.closed;
    clearTimeout(timeout);
    return code;
  }
}
module.exports = {NativeViewport};

