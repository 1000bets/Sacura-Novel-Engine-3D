const {test}=require('node:test');
const assert=require('node:assert/strict');
const path=require('node:path');
const ts=require('typescript');
const net=require('node:net');
const {once}=require('node:events');
const {BridgeTransport,validate}=require('../bridge/transport.cjs');
const contract=require('../bridge/contract.json');

test('typed parameter contract matches the schema used by C++ and main',()=>{
  const file=path.resolve(__dirname,'../bridge/contract.d.ts');
  const program=ts.createProgram([file,path.join(__dirname,'bridge-types.ts')],{strict:true,noEmit:true,target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.NodeNext,moduleResolution:ts.ModuleResolutionKind.NodeNext,skipLibCheck:true});
  const errors=ts.getPreEmitDiagnostics(program);assert.equal(errors.length,0,ts.formatDiagnosticsWithColorAndContext(errors,{getCurrentDirectory:()=>process.cwd(),getCanonicalFileName:f=>f,getNewLine:()=> '\n'}));
  const commands=program.getSourceFile(file).statements.find(s=>ts.isInterfaceDeclaration(s)&&s.name.text==='Commands');
  assert.deepEqual(commands.members.map(m=>m.name.text).sort(),Object.keys(contract.methods).sort());
  const types={id:'string',nullableId:'string|null',string:'string',serializedJson:'string',number:'number',boolean:'boolean',json:'JsonValue',transform:'Transform'};
  for(const method of commands.members){
    const params=method.type.members.find(m=>m.name.text==='params').type;
    const actual=Object.fromEntries((params.members||[]).map(m=>[m.name.text+(m.questionToken?'?':''),m.type.getText().replace(/\bId\b/g,'string').replace(/\s/g,'')]));
    assert.deepEqual(actual,Object.fromEntries(Object.entries(contract.methods[method.name.text]).map(([k,v])=>[k,types[v]])),method.name.text);
  }
});

test('framing handles partial packets and disconnect resolves pending commands',async()=>{
  const bridge=await BridgeTransport.listen(),socket=net.connect(bridge.path);await once(socket,'connect');
  const context={session:'test-session',epoch:1,revision:0};
  function packet(message){const b=Buffer.from(JSON.stringify(message)),p=Buffer.alloc(4+b.length);p.writeUInt32LE(b.length);b.copy(p,4);return p;}
  const initial=packet({type:'bridge.event',version:1,event:'state.changed',context,state:{}});
  const ready=once(bridge,'event');socket.write(initial.subarray(0,2));socket.write(initial.subarray(2,9));socket.write(initial.subarray(9));await ready;
  const pending=bridge.request('scene.getHierarchy');socket.destroy();
  assert.equal((await pending).error.code,'DISCONNECTED');assert.equal(bridge.pending.size,0);bridge.close();
});

test('oversized/malformed native reply disconnects instead of accepting untrusted state',async()=>{
  const bridge=await BridgeTransport.listen(),socket=net.connect(bridge.path);await once(socket,'connect');
  const event=once(bridge,'event'),packet=Buffer.alloc(4);packet.writeUInt32LE(9*1024*1024);socket.write(packet);
  assert.equal((await event)[0].event,'disconnected');assert.equal(bridge.connected,false);socket.destroy();bridge.close();
});

test('clipboard subtree accepts document payloads and retains JSON limits', () => {
  const Subtree = JSON.stringify({objects:Array.from({length:500}, (_, Index) => ({id:'object-' + Index,name:'Copied object'}))});
  assert.ok(Subtree.length > 16384);
  validate('object.paste',{subtree:Subtree});
  assert.throws(() => validate('object.paste',{subtree:'{bad'}));
  assert.throws(() => validate('object.paste',{subtree:JSON.stringify({name:'x'.repeat(16385)})}));
  assert.throws(() => validate('object.paste',{subtree:' '.repeat(1024 * 1024 + 1)}));
  assert.throws(() => validate('object.rename',{objectId:'cube',name:'x'.repeat(1025)}));
});
