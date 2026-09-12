import {upgradeProject, uid} from './studioModel.js';
import {readProject} from './projectFiles.js';

export const TEMPLATE_KEY = 'sacura-project-templates-v1';
export const BACKUP_KEY = 'sacura-studio-v2-backup-v3';
export const PROJECT_FILE_TYPES = [{description:'Проект Sacura',accept:{'application/json':['.json']}}];
export function projectName(value) {
  const name=String(value||'').trim();
  if(!name)throw new Error('Укажите название.');
  return name.slice(0,120);
}
export function projectFilename(name, template=false) {
  const safe=projectName(name).replace(/[<>:"/\\|?*\u0000-\u001f]/g,'_').replace(/[. ]+$/g,'')||'project';
  return safe+(template?'.sacura-template.json':'.sacura.json');
}
export function copyProjectAs(project,name) {
  const copy=structuredClone(project);
  copy.id=uid('project');copy.title=projectName(name);
  return copy;
}
export function createEmptyProject(name='Новый проект') {
  const sceneId=uid('scene'),entry=uid('line');
  return upgradeProject({
    version:2,id:uid('project'),title:projectName(name),revision:2,
    visualExamplesVersion:1,sceneEditingVersion:2,
    variables:{trust:0,letter:false},objects:[],events:[],audioAssets:[],actionTemplates:[],groupTemplates:[],
    scene:{location:sceneId,weather:'Ясно',time:'День',camera:'Общий план'},
    subscenes:[{id:sceneId,name:'Новая сцена',location:'Пустая сцена',kind:'living',entry,sceneId:'chapter1',weather:'Ясно',time:'День',color:'#bd94a9',stagingPoints:[]}],
    chapters:[{id:uid('chapter'),name:'Первый эпизод',subsceneId:sceneId,beats:[{id:entry,kind:'dialogue',speaker:'Рассказчик',text:'',next:null,bindings:[],mode:'SEQUENTIAL'}]}],
  });
}
export function createProjectTemplate(project,name) {
  return {format:'sacura-project-template',version:1,id:uid('template'),name:projectName(name),createdAt:new Date().toISOString(),project:structuredClone(project)};
}
export function projectFromTemplate(template,name) {
  if(template?.format!=='sacura-project-template'||template.version!==1)throw new Error('Неподдерживаемый шаблон проекта.');
  return copyProjectAs(readProject(template.project),name||template.name);
}
export function parseProjectFile(text) {
  const raw=typeof text==='string'?JSON.parse(text):text;
  return raw?.format==='sacura-project-template'?{project:projectFromTemplate(raw),template:raw}:{project:readProject(raw),template:null};
}
export function readTemplates(storage) {
  const value=JSON.parse(storage.getItem(TEMPLATE_KEY)||'[]');
  if(!Array.isArray(value))throw new Error('Не удалось прочитать библиотеку шаблонов.');
  return value.filter(t=>t?.format==='sacura-project-template'&&t.version===1&&typeof t.name==='string'&&t.project);
}
export function storeTemplate(storage,template) {
  const templates=readTemplates(storage);
  storage.setItem(TEMPLATE_KEY,JSON.stringify([...templates,template]));
}
export function backupProject(storage,project) {
  storage.setItem(BACKUP_KEY,JSON.stringify(project));
}
export async function writeProjectFile(value,{handle=null,chooseFile,download,filename}={}) {
  // Resolve the destination before doing any writes. Cancellation leaves the active project intact.
  const destination=handle||(chooseFile?await chooseFile({suggestedName:filename,types:PROJECT_FILE_TYPES}):null);
  const text=JSON.stringify(value,null,2);
  if(destination) {
    const stream=await destination.createWritable();
    try {await stream.write(text);await stream.close();}
    catch(error){try{await stream.abort?.();}catch{/* Preserve the original write error. */}throw error;}
    return {handle:destination,filename:destination.name||filename,downloaded:false};
  }
  if(!download)throw new Error('Сохранение файлов недоступно.');
  await download(text,filename);
  return {handle:null,filename,downloaded:true};
}
