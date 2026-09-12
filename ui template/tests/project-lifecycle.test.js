import test from 'node:test';
import assert from 'node:assert/strict';
import {allBeats,upgradeProject,validateStudio} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';
import {BACKUP_KEY,backupProject,copyProjectAs,createEmptyProject,createProjectTemplate,parseProjectFile,projectFilename,projectFromTemplate,readTemplates,storeTemplate,writeProjectFile} from '../src/projectLifecycle.js';

const storage=()=>{const data=new Map();return {getItem:k=>data.get(k)||null,setItem:(k,v)=>data.set(k,v)};};

test('empty project is editable, valid and stays empty after saving and migration',()=>{
  const project=createEmptyProject('С нуля');
  assert.equal(project.title,'С нуля');assert.equal(project.subscenes.length,1);
  assert.equal(project.objects.length,0);assert.equal(project.events.length,0);
  assert.equal(project.actionTemplates.length,0);assert.equal(project.groupTemplates.length,0);
  assert.equal(allBeats(project).length,1);assert.equal(allBeats(project)[0].text,'');
  assert.equal(project.subscenes[0].cameras.length,1);
  assert.deepEqual(project.subscenes[0].stagingPoints,[]);
  assert.deepEqual(validateStudio(project),[]);
  assert.deepEqual(readProject(JSON.stringify(project)),project);
  assert.notEqual(createEmptyProject().id,project.id);
});

test('Save As and templates preserve authored data while making independent projects',()=>{
  const source=upgradeProject();source.audioSettings.sidechain={attack:1.3,release:2,reductionDb:3};
  source.editor={timelinePositions:{'subscene:a1':{x:72,y:109}}};
  const snapshot=structuredClone(source),copy=copyProjectAs(source,'Копия');
  assert.equal(copy.title,'Копия');assert.notEqual(copy.id,source.id);
  assert.deepEqual(copy.editor,source.editor);assert.deepEqual(copy.audioSettings,source.audioSettings);
  const template=createProjectTemplate(source,'Постановка');
  const first=projectFromTemplate(template,'Первая'),second=projectFromTemplate(template,'Вторая');
  first.objects[0].name='Изменён';first.editor.timelinePositions['subscene:a1'].x=0;
  assert.notEqual(first.id,second.id);assert.deepEqual(source,snapshot);
  assert.deepEqual(template.project,snapshot);assert.deepEqual(second.editor,source.editor);
  assert.equal(parseProjectFile(JSON.stringify(template)).project.title,'Постановка');
});

test('template library survives reload and backup does not replace active data',()=>{
  const local=storage(),project=createEmptyProject();
  storeTemplate(local,createProjectTemplate(project,'Один'));storeTemplate(local,createProjectTemplate(project,'Два'));
  assert.deepEqual(readTemplates(local).map(t=>t.name),['Один','Два']);
  backupProject(local,project);assert.deepEqual(readProject(local.getItem(BACKUP_KEY)),project);
  assert.equal(readTemplates(local).length,2);
  assert.throws(()=>backupProject({setItem(){throw new Error('Quota exceeded');}},project),/Quota/);
});

test('file save writes and closes the selected file; subsequent save reuses the handle',async()=>{
  const writes=[],project=createEmptyProject();let picked=0,closed=0;
  const handle={name:'chosen.json',createWritable:async()=>({write:async text=>writes.push(text),close:async()=>closed++})};
  const chooseFile=async options=>{picked++;assert.equal(options.suggestedName,'suggested.json');return handle;};
  const first=await writeProjectFile(project,{chooseFile,filename:'suggested.json'});
  project.title='Обновлённый';await writeProjectFile(project,{handle:first.handle,chooseFile});
  assert.equal(picked,1);assert.equal(closed,2);assert.equal(JSON.parse(writes[1]).title,'Обновлённый');
  assert.equal(first.filename,'chosen.json');assert.equal(first.downloaded,false);
});

test('picker cancellation and failed writes never report successful saves or download copies',async()=>{
  const project=createEmptyProject();let downloads=0,aborts=0;
  await assert.rejects(writeProjectFile(project,{chooseFile:async()=>{throw Object.assign(new Error('cancel'),{name:'AbortError'});},download:()=>downloads++}),{name:'AbortError'});
  await assert.rejects(writeProjectFile(project,{handle:{createWritable:async()=>({write:async()=>{throw new Error('Disk full');},close:async()=>assert.fail('must not close'),abort:async()=>aborts++})},download:()=>downloads++}),/Disk full/);
  assert.equal(downloads,0);assert.equal(aborts,1);
});

test('browser fallback exports the complete project with safe Unicode filenames',async()=>{
  const project=createEmptyProject('Сцена: новая / копия'),files=[];
  const filename=projectFilename(project.title),result=await writeProjectFile(project,{filename,download:(text,name)=>files.push({text,name})});
  assert.equal(result.downloaded,true);assert.equal(result.handle,null);
  assert.equal(filename,'Сцена_ новая _ копия.sacura.json');
  assert.deepEqual(parseProjectFile(files[0].text).project,project);
  assert.equal(projectFilename('Мой шаблон',true),'Мой шаблон.sacura-template.json');
  assert.throws(()=>copyProjectAs(project,'  '),/название/);
});
