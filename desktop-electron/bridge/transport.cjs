const net = require('node:net');
const {randomUUID} = require('node:crypto');
const {EventEmitter} = require('node:events');
const contract = require('./contract.json');
const errorResult = (code,message,context=null) => ({version:1,type:'bridge.result',id:null,ok:false,error:{code,message},context});
function validJson(value, depth=0) {
  if(depth>16)throw Error('JSON nesting limit');
  if(typeof value==='number'&&!Number.isFinite(value))throw Error('Non-finite number');
  if(typeof value==='string'&&(value.length>16384||value.includes('\0')))throw Error('Invalid string');
  if(value && typeof value==='object') {
    if(Object.keys(value).length>4096)throw Error('Collection limit');
    for(const child of Object.values(value))validJson(child,depth+1);
  } else if(!['string','number','boolean'].includes(typeof value)&&value!==null)throw Error('Not JSON');
}
function validate(method, params) {
  const spec=Object.hasOwn(contract.methods,method)&&contract.methods[method];
  if(!spec)throw Error('Unknown method');
  if(!params||typeof params!=='object'||Array.isArray(params))throw Error('params must be an object');
  validJson(params);const allowed=new Set();
  for(const [field,type] of Object.entries(spec)) {
    const optional=field.endsWith('?'),key=optional?field.slice(0,-1):field;allowed.add(key);
    if(!Object.hasOwn(params,key)){if(!optional)throw Error('Missing '+key);continue;}
    const value=params[key];
    if(type==='json')continue;
    if(type==='number'){if(typeof value!=='number'||!Number.isFinite(value)||Math.abs(value)>1e8)throw Error('Invalid '+key);continue;}
    if(type==='boolean'){if(typeof value!=='boolean')throw Error('Invalid '+key);continue;}
    if(type==='nullableId'&&value===null)continue;
    if(type==='transform') {
      if(!value||Object.keys(value).sort().join(',')!=='position,rotation,scale')throw Error('Invalid transform');
      for(const [name,n] of [['position',3],['rotation',4],['scale',3]])if(!Array.isArray(value[name])||value[name].length!==n||!value[name].every(x=>typeof x==='number'&&Number.isFinite(x)&&Math.abs(x)<=1e8))throw Error('Invalid '+name);
      continue;
    }
    if(typeof value!=='string'||!value.length||value.length>(type==='string'?1024:256))throw Error('Invalid '+key);
  }
  for(const key of Object.keys(params))if(!allowed.has(key))throw Error('Unknown parameter '+key);
}
class BridgeTransport extends EventEmitter {
  static async listen() {
    const result=new BridgeTransport();
    result.path='\\\\.\\pipe\\sacura-control-'+randomUUID();result.server=net.createServer(socket=>result.accept(socket));
    await new Promise((resolve,reject)=>{result.server.once('error',reject);result.server.listen(result.path,resolve);});return result;
  }
  constructor(){super();this.pending=new Map();this.sequence=0;this.connected=false;this.context=null;this.closed=false;}
  accept(socket) {
    if(this.socket){socket.destroy();return;}this.socket=socket;let buffer=Buffer.alloc(0);
    socket.on('error',()=>this.disconnect('Native control pipe failed'));
    socket.on('close',()=>this.disconnect('Native control pipe closed'));
    socket.on('data',data=>{
      if(this.closed)return;
      try {
        buffer=Buffer.concat([buffer,data]);
        while(buffer.length>=4) {
          const size=buffer.readUInt32LE();if(!size||size>8*1024*1024)throw Error('Invalid response size');
          if(buffer.length<4+size)break;
          const message=JSON.parse(buffer.subarray(4,4+size).toString('utf8'));buffer=buffer.subarray(4+size);
          if(message.version!==1||!['bridge.event','bridge.result'].includes(message.type)||!message.context||typeof message.context.session!=='string'||!message.context.session||!Number.isSafeInteger(message.context.epoch)||message.context.epoch<1||!Number.isSafeInteger(message.context.revision)||message.context.revision<0)throw Error('Invalid Bridge response');
          if(message.type==='bridge.result'&&(typeof message.ok!=='boolean'||(message.id!==null&&typeof message.id!=='string')||(message.ok?!Object.hasOwn(message,'value'):typeof message.error?.code!=='string'||typeof message.error?.message!=='string')))throw Error('Invalid Bridge result');
          if(message.type==='bridge.event'&&(message.event!=='state.changed'||!message.state||typeof message.state!=='object'))throw Error('Invalid Bridge event');
          if(this.context&&message.context.session!==this.context.session)throw Error('Session changed on live pipe');
          this.context=message.context;this.connected=true;
          if(message.type==='bridge.event'){this.state=message.state;this.emit('event',message);}
          else {const pending=this.pending.get(message.id);if(pending){this.pending.delete(message.id);clearTimeout(pending.timer);pending.resolve(message);}}
        }
      }catch(error){this.disconnect(error.message);socket.destroy();}
    });
  }
  request(method,params={},context=this.context) {
    try{validate(method,params);}catch(error){return Promise.resolve(errorResult('INVALID_ARGUMENT',error.message,this.context));}
    if(!this.connected||!this.socket?.writable)return Promise.resolve(errorResult('DISCONNECTED','Native Bridge is not connected',this.context));
    if(!context||typeof context.session!=='string'||!Number.isSafeInteger(context.epoch))return Promise.resolve(errorResult('INVALID_ARGUMENT','Invalid context',this.context));
    if(this.pending.size>=64)return Promise.resolve(errorResult('BUSY','Bridge queue is full',this.context));
    const id=String(++this.sequence);
    const request={type:'bridge.command',version:1,id,session:context.session,epoch:context.epoch,method,params};
    const body=Buffer.from(JSON.stringify(request));
    if(body.length>65536)return Promise.resolve(errorResult('INVALID_ARGUMENT','Request exceeds 64 KiB',this.context));
    const packet=Buffer.alloc(4+body.length);packet.writeUInt32LE(body.length);body.copy(packet,4);
    return new Promise(resolve=>{
      const timer=setTimeout(()=>{this.pending.delete(id);resolve({...errorResult('TIMEOUT','Command outcome unknown; refresh before retrying',this.context),id});},30000);
      this.pending.set(id,{resolve,timer});this.socket.write(packet);
    });
  }
  disconnect(message='Bridge disconnected') {
    if(this.closed)return;
    this.closed=true;
    this.connected=false;
    for(const [id,pending] of this.pending){clearTimeout(pending.timer);pending.resolve({...errorResult('DISCONNECTED',message,this.context),id});}
    this.pending.clear();this.emit('event',{version:1,type:'bridge.event',event:'disconnected',context:this.context,message});
  }
  close(){this.disconnect();this.socket?.destroy();this.server?.close();}
}
module.exports={BridgeTransport,validate,errorResult};
