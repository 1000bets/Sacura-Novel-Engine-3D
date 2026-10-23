const fs = require('node:fs/promises');
const path = require('node:path');
const vm = require('node:vm');
const {randomUUID} = require('node:crypto');

const starter = `// Этот скрипт запускается при старте игры. Объект выбирается рядом с кодом.\nasync function main({ engine }) {\n  await engine.self.translate(0, 1, 0);\n}\n`;
const scriptError=(error,source)=>{const found=String(error.stack||'').match(/mechanic\.js:(\d+)/)?.[1],line=found?Math.min(Number(found),source.split('\n').length):null;return Error((line?`Строка ${line}: `:'')+error.message);};
const fileFor = project => path.join(path.dirname(project), 'Scripts', 'Mechanics', 'mechanics.json');
const valid = record => record && typeof record.id === 'string' && typeof record.name === 'string'
  && typeof record.source === 'string' && typeof record.compiledSource === 'string'
  && (record.objectId === null || typeof record.objectId === 'string');
function checkSource(source) {
  if(typeof source !== 'string' || source.length > 200000) throw Error('Некорректный исходный код');
  try { new vm.Script(`${source}\n;typeof main === 'function' ? main : undefined`, {filename:'mechanic.js'}); }
  catch(error) { throw scriptError(error,source); }
  const context=vm.createContext(Object.create(null), {codeGeneration:{strings:false,wasm:false}});
  const fn=new vm.Script(`${source}\n;typeof main === 'function' ? main : undefined`, {filename:'mechanic.js'}).runInContext(context,{timeout:500});
  if(typeof fn !== 'function') throw Error('Объявите функцию main({ engine })');
}
class NativeMechanics {
  constructor(project) {this.project=project;this.file=fileFor(project);this.records=[];}
  async load() {
    let content;
    try {content=JSON.parse(await fs.readFile(this.file,'utf8'));}
    catch(error) {if(error.code==='ENOENT')return this.list();throw error;}
    if(content?.version!==1 || !Array.isArray(content.mechanics) || !content.mechanics.every(valid)) throw Error('Некорректный файл механик');
    this.records=content.mechanics;
    return this.list();
  }
  list() {return structuredClone(this.records);}
  async save() {
    await fs.mkdir(path.dirname(this.file),{recursive:true});
    const temp=this.file+'.'+process.pid+'.tmp';
    try {await fs.writeFile(temp,JSON.stringify({version:1,mechanics:this.records},null,2),'utf8');await fs.rename(temp,this.file);}
    finally {await fs.rm(temp,{force:true}).catch(()=>{});}
  }
  async create(objectId=null) {
    if(objectId!==null && (typeof objectId!=='string'||objectId.length>256)) throw Error('Некорректный объект');
    const record={id:randomUUID(),name:'Новая механика',objectId,source:starter,compiledSource:'',enabled:false};
    this.records.push(record);await this.save();return this.list();
  }
  async update(id,patch) {
    const record=this.records.find(item=>item.id===id);if(!record)throw Error('Механика не найдена');
    if(!patch || typeof patch!=='object' || Array.isArray(patch) || Object.keys(patch).some(key=>!['name','objectId','source','enabled'].includes(key)))throw Error('Некорректное изменение');
    if('name' in patch && (typeof patch.name!=='string'||!patch.name.trim()||patch.name.length>120))throw Error('Укажите название механики');
    if('source' in patch && (typeof patch.source!=='string'||patch.source.length>200000))throw Error('Некорректный исходный код');
    if('objectId' in patch && patch.objectId!==null && (typeof patch.objectId!=='string'||patch.objectId.length>256))throw Error('Некорректный объект');
    if('enabled' in patch && typeof patch.enabled!=='boolean')throw Error('Некорректное состояние');
    Object.assign(record,patch);await this.save();return this.list();
  }
  async compile(id) {
    const record=this.records.find(item=>item.id===id);if(!record)throw Error('Механика не найдена');
    checkSource(record.source);
    record.compiledSource=record.source;record.enabled=true;await this.save();return this.list();
  }
  async remove(id) {const length=this.records.length;this.records=this.records.filter(item=>item.id!==id);if(length===this.records.length)throw Error('Механика не найдена');await this.save();return this.list();}
  async export(id,output) {
    const record=this.records.find(item=>item.id===id);if(!record)throw Error('Механика не найдена');
    await fs.writeFile(output,JSON.stringify({format:'sacura.native-mechanic',version:1,name:record.name,source:record.source},null,2),'utf8');
  }
  async import(input,objectId=null) {
    const content=JSON.parse(await fs.readFile(input,'utf8'));
    if(content?.format!=='sacura.native-mechanic'||content.version!==1||typeof content.name!=='string'||typeof content.source!=='string'||content.source.length>200000)throw Error('Неподдерживаемый файл механики');
    const record={id:randomUUID(),name:content.name.slice(0,120)||'Импортированная механика',objectId,source:content.source,compiledSource:'',enabled:false};
    this.records.push(record);await this.save();return this.list();
  }
  async runStart(bridge,context) {
    const results=[];
    for(const record of this.records) {
      if(!record.enabled||!record.compiledSource)continue;
      if(!record.objectId) {results.push({id:record.id,error:'Выберите объект для механики'});continue;}
      let active=true;
      const request=async(operation,value)=>{
        if(!active)throw Error('Механика больше не выполняется');
        const result=await bridge.request('play.mechanicOperation',{objectId:record.objectId,operation,value},context);
        if(!result.ok)throw Error(result.error.message);
        context=result.context;
        return result.value;
      };
      const engine=Object.freeze({self:Object.freeze({
        setActive:active=>request('setActive',active),
        translate:(x,y,z)=>request('translate',[x,y,z]),
        position:()=>request('position',null)
      })});
      let timer;
      try {
        const sandbox=vm.createContext(Object.freeze({console:Object.freeze({log:()=>{}}),__engine:engine}),{codeGeneration:{strings:false,wasm:false}});
        const pending=new vm.Script(`${record.compiledSource}\n;main({engine:__engine})`,{filename:record.name+'.js'}).runInContext(sandbox,{timeout:500});
        await Promise.race([Promise.resolve(pending),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Время выполнения механики истекло')),5000);})]);
        results.push({id:record.id,ok:true});
      } catch(error) {results.push({id:record.id,error:error.message});}
      finally {active=false;clearTimeout(timer);}
    }
    return results;
  }
}
module.exports={NativeMechanics,checkSource,fileFor};
