import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {randomUUID} from 'node:crypto';
import {createEmptyProject} from '../frontend/src/projectLifecycle.js';
import {parseMeshAsset} from '../frontend/src/meshAssets.js';
import {parseCharacterGlb,disposeCharacterAsset} from '../frontend/src/characterAssets.js';

// Embedded triangle and a real translation clip; no external resources.
const data=new Float32Array([0,0,0,1,0,0,0,1,0,0,1,0,0,0,.5,0,0]);
const json={asset:{version:'2.0'},scene:0,scenes:[{nodes:[0]}],nodes:[{name:'Body',mesh:0}],meshes:[{primitives:[{attributes:{POSITION:0}}]}],buffers:[{byteLength:data.byteLength}],bufferViews:[{buffer:0,byteOffset:0,byteLength:36},{buffer:0,byteOffset:36,byteLength:8},{buffer:0,byteOffset:44,byteLength:24}],accessors:[{bufferView:0,componentType:5126,count:3,type:'VEC3',min:[0,0,0],max:[1,1,0]},{bufferView:1,componentType:5126,count:2,type:'SCALAR',min:[0],max:[1]},{bufferView:2,componentType:5126,count:2,type:'VEC3'}],animations:[{name:'Walk',samplers:[{input:1,output:2}],channels:[{sampler:0,target:{node:0,path:'translation'}}]}]};
const text=Buffer.from(JSON.stringify(json)),length=Math.ceil(text.length/4)*4;
const glb=Buffer.alloc(28+length+data.byteLength);
glb.writeUInt32LE(0x46546c67,0);glb.writeUInt32LE(2,4);glb.writeUInt32LE(glb.length,8);
glb.writeUInt32LE(length,12);glb.writeUInt32LE(0x4e4f534a,16);glb.fill(32,20,20+length);text.copy(glb,20);
glb.writeUInt32LE(data.byteLength,20+length);glb.writeUInt32LE(0x004e4942,24+length);Buffer.from(data.buffer).copy(glb,28+length);
const glbSrc='data:model/gltf-binary;base64,'+glb.toString('base64');
const obj='v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n';

export async function checkModels(base,cookie,savedProject){
 const nativeFetch=globalThis.fetch,previousStorage=globalThis.localStorage;
 const values=new Map();
 globalThis.localStorage={getItem:key=>values.get(key)||null,setItem:(key,value)=>values.set(key,value),removeItem:key=>values.delete(key)};
 globalThis.fetch=(path,options={})=>nativeFetch(typeof path==='string'&&path.startsWith('/')?base+path:path,{...options,signal:AbortSignal.timeout(20000),headers:{Cookie:cookie,...options.headers}});
 try{
  // Enable the same compile-time switch Vite sets in the production client.
  let source=(await readFile(new URL('../frontend/src/serverStorage.js',import.meta.url),'utf8')).replace("import.meta.env?.VITE_SERVER_STORAGE==='true'",'true');
  source=source.replace(/from '(\.\/[^']+)'/g,(_,path)=>`from '${new URL(path,new URL('../frontend/src/serverStorage.js',import.meta.url)).href}'`);
  const client=await import('data:text/javascript;base64,'+Buffer.from(source).toString('base64'));
  client.resetServerStorage();
  let project;
  if(savedProject){project=await client.openServerProject(savedProject.id);assert.deepEqual(project,savedProject);}
  else{
   const original=createEmptyProject('Проверка моделей');
   original.objects.push({id:'smoke-obj',name:'triangle.obj',type:'Меш',active:true,color:'#bb99aa',subsceneId:original.subscenes[0].id,model:{format:'obj',name:'triangle.obj',src:obj}});
   original.objects.push({id:'smoke-glb',name:'animated.glb',type:'Персонаж',active:true,color:'#bb99aa',subsceneId:original.subscenes[0].id,idleAnimation:'model:0',model:{format:'glb',name:'animated.glb',src:glbSrc,animations:[{id:'model:0',name:'Walk',duration:1}]}});
   original.assetFiles=[{id:'smoke-library',kind:'model',src:obj,model:{format:'obj',name:'triangle.obj',src:obj}}];
   project=await client.saveServerProject(original);
   assert.equal(project.assetFiles[0].src,project.objects[0].model.src);
   assert.equal(original.objects[0].model.src,obj,'save must not mutate active edits');
   assert.deepEqual(await client.openServerProject(project.id),project);
   assert.equal((await client.persistModel({format:'glb',src:glbSrc,name:'copy.glb'})).src,project.objects[1].model.src);
   assert.equal((await fetch('/api/meshes?format=glb',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:'broken'})).status,400);
   assert.equal((await fetch('/api/meshes?format=obj',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:Buffer.alloc(3*1024*1024+1)})).status,413);
   assert.equal((await nativeFetch(base+project.objects[0].model.src)).status,401);
   const invited=await nativeFetch(base+'/api/invites',{method:'POST',headers:{Cookie:cookie}});assert.equal(invited.status,201);
   const invite=new URL((await invited.json()).url).searchParams.get('invite');
   const other=await nativeFetch(base+'/api/auth/register',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:'other-'+randomUUID(),password:randomUUID(),invite})});
   assert.equal(other.status,201);const otherCookie=other.headers.get('set-cookie').split(';')[0];
   for(const path of ['/api/projects/'+project.id,project.objects[0].model.src,project.objects[1].model.src])assert.equal((await nativeFetch(base+path,{headers:{Cookie:otherCookie}})).status,404);
  }
  const downloadedGlb=await fetch(project.objects[1].model.src);assert.equal(downloadedGlb.status,200);
  const buffer=await downloadedGlb.arrayBuffer();assert.deepEqual(Buffer.from(buffer),glb);
  const asset=await parseCharacterGlb(buffer);
  assert.ok(asset.scene.getObjectByName('Body').isMesh);assert.equal(asset.animations[0].name,'Walk');assert.equal(asset.animations[0].duration,1);disposeCharacterAsset(asset.scene);
  const mesh=await parseMeshAsset(project.objects[0].model);let vertices=0;
  mesh.traverse(node=>{if(node.isMesh)vertices+=node.geometry.attributes.position.count;});assert.equal(vertices,3);disposeCharacterAsset(mesh);
  const portable=await client.portableProject(project);
  assert.equal(portable.objects[0].model.src,obj);assert.equal(portable.objects[1].model.src,glbSrc);assert.equal(portable.assetFiles[0].src,obj);
  console.log('Model check passed: real client save/open/export, OBJ geometry, animated GLB'+(savedProject?' after restart.':', deduplication, limits and account isolation.'));
  return project;
 }finally{globalThis.fetch=nativeFetch;globalThis.localStorage=previousStorage;}
}
