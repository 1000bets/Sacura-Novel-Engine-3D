import {accountStorage,currentStorageUser} from './accountStorage.js';
import {createEmptyProject} from './projectLifecycle.js';
export const serverStorageEnabled=import.meta.env?.VITE_SERVER_STORAGE==='true';
const revisions=new Map(),uploads=new Map();
let queue=Promise.resolve();
const activeKey='sacura-server-active-project';
async function request(path,options={}){
 const response=await fetch(path,{credentials:'same-origin',...options,headers:{...options.headers,'X-Sacura-User':currentStorageUser()||''}});
 if(!response.ok){let message;try{message=(await response.json()).error;}catch{}throw new Error(message||`Хранилище: HTTP ${response.status}`);}
 return response.json();
}
export const listServerProjects=()=>request('/api/projects');
export async function openServerProject(id){
 await queue.catch(()=>{});
 const result=await request('/api/projects/'+encodeURIComponent(id));
 revisions.set(id,result.revision);accountStorage.setItem(activeKey,id);return result.project;
}
export async function bootstrapServerProject(){
 if(!serverStorageEnabled)return null;
 const projects=await listServerProjects();
 if(!projects.length)return createEmptyProject();
 const active=projects.find(p=>p.id===accountStorage.getItem(activeKey))||projects[0];
 return openServerProject(active.id);
}
export async function persistModel(model){
 if(!serverStorageEnabled||model.src.startsWith('/api/meshes/'))return model;
 const format=model.format||(/\.obj$/i.test(model.name||'')?'obj':'glb');
 const key=format+':'+model.src;
 if(!uploads.has(key))uploads.set(key,(async()=>{
  const body=format==='obj'?new TextEncoder().encode(model.src):await (await fetch(model.src)).arrayBuffer();
  return request('/api/meshes?format='+format,{method:'POST',headers:{'Content-Type':'application/octet-stream'},body});
 })().catch(error=>{uploads.delete(key);throw error;}));
 return {...model,...await uploads.get(key)};
}
export async function prepareServerProject(project){
 const copy=structuredClone(project),sources=new Map();
 async function visit(value){
  if(!value||typeof value!=='object')return;
  if(value.model?.src){const model=await persistModel(value.model);sources.set(value.model.src,model.src);value.model=model;}
  if(value.kind==='model'&&value.src&&!value.model){const model=await persistModel(value);sources.set(value.src,model.src);value.src=model.src;}
  for(const child of Object.values(value))if(child&&typeof child==='object')await visit(child);
 }
 await visit(copy);
 function replace(value){for(const [key,child]of Object.entries(value)){if(typeof child==='string'&&sources.has(child))value[key]=sources.get(child);else if(child&&typeof child==='object')replace(child);}}
 replace(copy);return copy;
}
export function saveServerProject(project){
 // Serializing writes also prevents a late autosave from overwriting Ctrl+S.
 const snapshot=structuredClone(project);
 const operation=queue.catch(()=>{}).then(async()=>{
  const prepared=await prepareServerProject(snapshot);
  const result=await request('/api/projects/'+encodeURIComponent(prepared.id),{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({project:prepared,expectedRevision:revisions.get(prepared.id)||0})});
  revisions.set(prepared.id,result.revision);accountStorage.setItem(activeKey,prepared.id);return prepared;
 });queue=operation;return operation;
}
export async function portableProject(project){
 const copy=structuredClone(project),sources=new Map();
 async function visit(value){
  for(const [key,child]of Object.entries(value)){
   if(typeof child==='string'&&/^\/api\/meshes\/[a-f0-9]{64}\.(glb|obj)$/.test(child)){
    if(!sources.has(child))sources.set(child,(async()=>{
     const response=await fetch(child,{credentials:'same-origin',headers:{'X-Sacura-User':currentStorageUser()||''}});if(!response.ok)throw new Error('Не удалось скачать модель для экспорта.');
     if(child.endsWith('.obj'))return response.text();
     const bytes=new Uint8Array(await response.arrayBuffer());let binary='';for(const byte of bytes)binary+=String.fromCharCode(byte);
     return 'data:model/gltf-binary;base64,'+btoa(binary);
    })());value[key]=await sources.get(child);
   }else if(child&&typeof child==='object')await visit(child);
  }
 }
 await visit(copy);return copy;
}

export function resetServerStorage(){revisions.clear();uploads.clear();queue=Promise.resolve();}
export async function flushServerSaves(){await queue.catch(()=>{});}
