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
