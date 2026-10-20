import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';

// Exercise the production-enabled client independently of Vite's compile-time env.
test('server client migrates shared models once, restores projects, exports portable assets and rejects conflicts',async()=>{
 let source=(await readFile(new URL('../src/serverStorage.js',import.meta.url),'utf8')).replace("import.meta.env?.VITE_SERVER_STORAGE==='true'",'true');
 source=source.replace(/from '(\.\/[^']+)'/g,(_,path)=>`from '${new URL(path,new URL('../src/serverStorage.js',import.meta.url)).href}'`);
 const previousFetch=globalThis.fetch,previousStorage=globalThis.localStorage;
 const values=new Map(),projects=new Map(),meshes=new Map();let uploads=0;
 globalThis.localStorage={getItem:key=>values.get(key),setItem:(key,value)=>values.set(key,value)};
 const response=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{'Content-Type':'application/json'}});
 const obj='v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n',glb='data:model/gltf-binary;base64,AQIDBA==';
 globalThis.fetch=async(path,options={})=>{
  if(path.startsWith('data:'))return previousFetch(path,options);
  if(path==='/api/projects')return response([...projects.values()].map(p=>({id:p.project.id,revision:p.revision})));
  if(path.startsWith('/api/meshes?')){
   const format=path.split('=').at(-1),src='/api/meshes/'+String(++uploads).padStart(64,'0')+'.'+format;
   meshes.set(src,options.body);return response({src,format,bytes:options.body.byteLength},201);
  }
  if(path.startsWith('/api/meshes/'))return new Response(meshes.get(path));
  const id=path.split('/').at(-1),current=projects.get(id);
  if(options.method==='PUT'){
   const next=JSON.parse(options.body);if(next.expectedRevision!==(current?.revision||0))return response({error:'conflict'},409);
   const saved={project:next.project,revision:(current?.revision||0)+1};projects.set(id,saved);return response(saved);
  }
  return current?response(current):response({error:'missing'},404);
 };
 try{
  const storage=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
  assert.equal((await storage.bootstrapServerProject()).version,2);
  const original={id:'test',objects:[{model:{format:'obj',src:obj}},{model:{format:'glb',src:glb}}],assetFiles:[{kind:'model',src:obj,model:{format:'obj',src:obj}}]};
  const saved=await storage.saveServerProject(original);
  assert.equal(uploads,2);assert.equal(saved.objects[0].model.src,saved.assetFiles[0].src);
  assert.equal(original.objects[0].model.src,obj,'migration does not mutate active edits');
  const restored=await storage.bootstrapServerProject();assert.deepEqual(restored,saved);
  assert.deepEqual(await storage.portableProject(restored),{...original,objects:[{model:{format:'obj',src:obj,bytes:new TextEncoder().encode(obj).length}},{model:{format:'glb',src:glb,bytes:4}}],assetFiles:[{kind:'model',src:obj,model:{format:'obj',src:obj,bytes:new TextEncoder().encode(obj).length}}]});
  await Promise.all([storage.saveServerProject(saved),storage.saveServerProject(saved)]);
  assert.equal(projects.get('test').revision,3,'writes are serialized against actual revisions');
  projects.get('test').revision++;
  await assert.rejects(storage.saveServerProject(saved),/conflict/);
 }finally{globalThis.fetch=previousFetch;globalThis.localStorage=previousStorage;}
});

test('owner context isolates revisions and queued saves for identical project IDs',async()=>{
 let source=(await readFile(new URL('../src/serverStorage.js',import.meta.url),'utf8')).replace("import.meta.env?.VITE_SERVER_STORAGE==='true'",'true');
 source=source.replace(/from '(\.\/[^']+)'/g,(_,path)=>`from '${new URL(path,new URL('../src/serverStorage.js',import.meta.url)).href}'`);
 const previousFetch=globalThis.fetch,previousStorage=globalThis.localStorage;
 const values=new Map(),writes=[],uploads=[];
 const projects=new Map([['own',{project:{id:'same',title:'Свой',objects:[]},revision:3}],['alice',{project:{id:'same',title:'Алиса',objects:[]},revision:7}],['bob',{project:{id:'same',title:'Боб',objects:[]},revision:2}]]);
 globalThis.localStorage={getItem:key=>values.get(key),setItem:(key,value)=>values.set(key,value)};
 const response=(value,status=200)=>new Response(JSON.stringify(value),{status});
 globalThis.fetch=async(path,options={})=>{
  const url=new URL(path,'http://localhost'),owner=url.searchParams.get('ownerId')||'own';
  if(url.pathname==='/api/admin/projects')return response([{id:'same',ownerId:'alice',ownerLogin:'alice'}]);
  if(url.pathname==='/api/meshes'){
   uploads.push(owner);return response({src:'/api/meshes/'+'a'.repeat(64)+'.obj',format:'obj'});
  }
  const current=projects.get(owner);
  if(options.method==='PUT'){
   const body=JSON.parse(options.body);assert.equal(body.expectedRevision,current.revision);
   writes.push(owner);const saved={project:body.project,revision:current.revision+1};projects.set(owner,saved);return response(saved);
  }
  return response(current);
 };
 try{
  const storage=await import('data:text/javascript;base64,'+Buffer.from(source+'\n// owner test').toString('base64'));
  assert.equal((await storage.listOtherServerProjects())[0].ownerLogin,'alice');
  const alice=await storage.openServerProject('same','alice');
  assert.equal(values.size,0,'foreign project never replaces own active project');
  const savedAlice=storage.saveServerProject({...alice,title:'Правка Алисы',objects:[{model:{src:'v 0 0 0\n',format:'obj'}}]});
  const bob=await storage.openServerProject('same','bob');
  const savedBob=storage.saveServerProject({...bob,title:'Правка Боба'});
  await Promise.all([savedAlice,savedBob]);
  const own=await storage.openServerProject('same');await storage.saveServerProject({...own,title:'Правка своего'});
  assert.deepEqual(writes,['alice','bob','own']);assert.deepEqual(uploads,['alice']);
  assert.equal(projects.get('alice').revision,8);assert.equal(projects.get('bob').revision,3);assert.equal(projects.get('own').revision,4);
  storage.resetServerStorage();
 }finally{globalThis.fetch=previousFetch;globalThis.localStorage=previousStorage;}
});
