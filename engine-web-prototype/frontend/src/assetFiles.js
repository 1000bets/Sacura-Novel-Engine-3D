import {uid} from './model.js';
import {importMeshFile,MAX_MESH_BYTES} from './meshAssets.js';

export function cleanAssetPath(path){return String(path||'').replaceAll('\\','/').split('/').filter(part=>part&&part!=='.'&&part!=='..').join('/');}
export function fileKind(name){const ext=String(name).split('.').at(-1).toLowerCase();return ['glb','obj'].includes(ext)?'model':['png','jpg','jpeg','webp','gif','svg'].includes(ext)?'image':['wav','mp3','ogg','m4a','aac','flac','webm'].includes(ext)?'audio':'file';}
export function collectAssetFiles(project,audio=[]){
 const files=(project.assetFiles||[]).map(file=>({...file,users:[]}));
 for(const object of project.objects||[]){
  if(!object.model?.src)continue;
  let file=files.find(file=>file.src===object.model.src);
  if(!file){file={id:`object-file:${object.id}`,name:object.model.name||'model.glb',path:`${object.type==='Персонаж'?'Персонажи':'Модели'}/${object.model.name||'model.glb'}`,kind:'model',src:object.model.src,bytes:object.model.bytes,model:object.model,users:[]};files.push(file);}
  file.users.push({id:object.id,name:object.name});
 }
 for(const asset of audio){if(files.some(file=>file.src===asset.url))continue;files.push({id:`audio-file:${asset.id}`,name:asset.file,path:`Звук/${asset.file}`,kind:'audio',src:asset.url,audioId:asset.id,builtin:true,users:[]});}
 return files.map(file=>({...file,...(file.kind==='audio'?{audioId:file.audioId||file.id}:{}),...(file.model?{model:{...file.model,src:file.src}}:{}),path:cleanAssetPath(file.path||file.name)}));
}
export function assetFolders(files,stored=[]){
 const folders=new Set(['']);
 for(const path of [...stored,...files.map(file=>file.path.split('/').slice(0,-1).join('/'))]){const parts=cleanAssetPath(path).split('/').filter(Boolean);for(let i=1;i<=parts.length;i++)folders.add(parts.slice(0,i).join('/'));}
 return [...folders].sort((a,b)=>a.localeCompare(b,'ru'));
}
export function folderContents(files,folders,path='',query=''){
 const search=query.trim().toLowerCase(),prefix=path?path+'/':'';
 const matches=file=>file.path.startsWith(prefix)&&(!search||file.path.toLowerCase().includes(search));
 return {folders:search?[]:folders.filter(folder=>folder&&folder.startsWith(prefix)&&!folder.slice(prefix.length).includes('/')),files:files.filter(file=>matches(file)&&(search||!file.path.slice(prefix.length).includes('/')))};
}
export function storeAssetFile(project,file){
 project.assetFiles||=[];
 const existing=project.assetFiles.find(item=>item.src===file.src);if(existing)return existing;
 const clean={...file,id:file.id||uid('file'),path:cleanAssetPath(file.path||file.name)};
 if(clean.model){const {src,...metadata}=clean.model;clean.model=src?.startsWith('/api/meshes/')?{...metadata,src}:metadata;}
 const original=clean.path;let count=2;
 while(project.assetFiles.some(item=>item.path===clean.path)){const dot=original.lastIndexOf('.'),slash=original.lastIndexOf('/');clean.path=dot>slash?`${original.slice(0,dot)} (${count++})${original.slice(dot)}`:`${original} (${count++})`;}
 clean.name=clean.path.split('/').at(-1);project.assetFiles.push(clean);return clean;
}
export function retainObjectModel(project,object){
 if(!object?.model?.src||project.objects?.some(other=>other.id!==object.id&&other.model?.src===object.model.src))return;
 storeAssetFile(project,{name:object.model.name,kind:'model',src:object.model.src,bytes:object.model.bytes,model:object.model,path:`${object.type==='Персонаж'?'Персонажи':'Модели'}/${object.model.name}`});
}
export async function importAssetFiles(files,path=''){
 const results=[];
 for(const file of files){
  const kind=fileKind(file.name),limit=kind==='model'?MAX_MESH_BYTES:3*1024*1024;
  if(file.size>limit)throw new Error(`${file.name}: максимальный размер файла — ${limit/1024/1024} МБ.`);
  const model=kind==='model'?await importMeshFile(file):null;
  const src=model?.src||await new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject(new Error(`Не удалось прочитать ${file.name}.`));reader.readAsDataURL(file);});
  results.push({id:uid('file'),name:file.name,path:cleanAssetPath([path,file.webkitRelativePath||file.name].filter(Boolean).join('/')),kind,src,bytes:file.size,...(model?{model}:{})});
 }
 return results;
}
