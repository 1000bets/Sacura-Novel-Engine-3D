const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

test('shared frames retain imports across frames and retire resized resources after consumption', async () => {
  const Imported = [];
  const Acknowledged = [];
  const Module = {exports: {}};
  const Source = fs.readFileSync(path.join(__dirname, '../native-viewport.cjs'), 'utf8');
  vm.runInNewContext(Source, {module: Module, exports: Module.exports, Buffer, process, console, setTimeout, clearTimeout,
    require: Name => {
      if (Name === 'electron') {
        return {sharedTexture: {
          importSharedTexture: () => {const Resource = {releases: 0, release() {this.releases++;}}; Imported.push(Resource); return Resource;},
          sendSharedTexture: async () => {}
        }};
      }
      if (Name === './bridge/transport.cjs') { return {}; }
      return require(Name);
    }
  });
  const Owner = Object.assign(Object.create(Module.exports.NativeViewport.prototype), {
    window: {isDestroyed: () => false, webContents: {isDestroyed: () => false, mainFrame: {}}},
    socket: {writable: true, write: Packet => Acknowledged.push(Packet.readBigUInt64LE().toString())},
    state: {transport: {frames: 0, released: 0}}, imports: 0, sharedImports: new Map(), pendingFrames: new Map()
  });
  function Packet(Token, Handle, Width = 64) {
    const Result = Buffer.alloc(56);
    Result.writeUInt32LE(0x31464753);
    Result.writeUInt32LE(Width, 4);
    Result.writeUInt32LE(64, 8);
    Result.writeBigUInt64LE(BigInt(Token), 16);
    Result.writeBigUInt64LE(BigInt(Handle), 24);
    return Result;
  }
  await Owner.receiveFrame(Packet(1, 100));
  assert.equal(Imported.length, 1);
  assert.equal(Acknowledged.length, 0);
  assert.equal(Owner.frameConsumed('1'), true);
  await Owner.receiveFrame(Packet(2, 100));
  assert.equal(Imported.length, 1);
  assert.equal(Owner.frameConsumed('1'), false);
  await Owner.receiveFrame(Packet(3, 101, 128));
  assert.equal(Imported[0].releases, 0);
  assert.equal(Owner.frameConsumed('2'), true);
  assert.equal(Owner.frameConsumed('3'), true);
  await Owner.receiveFrame(Packet(4, 100, 128));
  assert.equal(Imported.length, 3);
  assert.equal(Imported[0].releases, 1);
  assert.equal(Owner.frameConsumed('4'), true);
  assert.equal(Owner.imports, 0);
  assert.deepEqual(Acknowledged, ['1', '2', '3', '4']);
  assert.equal(Owner.state.transport.frames, 4);
  assert.equal(Owner.state.transport.released, 4);
});
