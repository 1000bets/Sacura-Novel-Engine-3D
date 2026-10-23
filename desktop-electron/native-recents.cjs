const fs=require('node:fs/promises');
const path=require('node:path');
const {createHash}=require('node:crypto');

// Launcher preferences only. Project data is read/written exclusively by C++.
class NativeRecents {
  constructor(file){this.file=file;this.queue=Promise.resolve();}
  async read(){
    try {const value=JSON.parse(await fs.readFile(this.file,'utf8'));return Array.isArray(value)?value.filter(v=>v&&typeof v.id==='string'&&typeof v.path==='string'&&path.extname(v.path).toLowerCase()==='.project'&&typeof v.title==='string').slice(0,30):[];}
    catch(error){if(error.code==='ENOENT'||error instanceof SyntaxError)return [];throw error;}
  }
  async list(){await this.queue;return Promise.all((await this.read()).map(async item=>({...item,missing:!await fs.stat(item.path).then(s=>s.isFile(),()=>false)})));}
  update(change){const result=this.queue.catch(()=>{}).then(async()=>{
    const items=change(await this.read());await fs.mkdir(path.dirname(this.file),{recursive:true});
    const temporary=this.file+'.tmp';await fs.writeFile(temporary,JSON.stringify(items,null,2));await fs.rename(temporary,this.file);
  });this.queue=result.catch(()=>{});return result;}
  async remember(file,title){
    const real=await fs.realpath(file);
    if(path.extname(real).toLowerCase()!=='.project')throw Error('Expected .project file');
    const normalized=process.platform==='win32'?real.toLowerCase():real;
    const id=createHash('sha256').update(normalized).digest('hex');
    await this.update(items=>[{id,path:real,title,updatedAt:new Date().toISOString()},...items.filter(v=>v.id!==id)].slice(0,30));
  }
  async resolve(id){if(typeof id!=='string'||!/^[a-f0-9]{64}$/.test(id))throw Error('Invalid recent project ID');const item=(await this.list()).find(v=>v.id===id);if(!item)throw Error('Проект отсутствует в списке недавних.');if(item.missing)throw Error('Файл проекта перемещён или удалён. Нажмите «Открыть проект» и укажите его новое расположение.');return item.path;}
  remove(id){if(typeof id!=='string'||!/^[a-f0-9]{64}$/.test(id))throw Error('Invalid recent project ID');return this.update(items=>items.filter(v=>v.id!==id));}
}
module.exports={NativeRecents};
