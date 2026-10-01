import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {Readable} from 'node:stream';
import {createStorage,validateMesh,MESH_LIMIT} from '../storage.js';
import {createAuth} from '../auth.js';
import {createApp} from '../app.js';

async function fixture(run){
 const dir=mkdtempSync(join(tmpdir(),'sacura-test-')),objects=new Map();let puts=0;
 const s3={async send(command){const {Key,Body}=command.input;
  if(command.constructor.name==='PutObjectCommand'){objects.set(Key,Buffer.from(Body));puts++;return {};}
  if(command.constructor.name==='GetObjectCommand')return {Body:Readable.from(objects.get(Key)),ContentLength:objects.get(Key).length};
  return {};
 }};
 const filename=join(dir,'projects.sqlite'),storage=createStorage({filename,s3,bucket:'test'});
 await createAuth(storage.db).migrateOwner('owner','1');
 const server=createApp(storage).listen(0,'127.0.0.1');
 await new Promise(resolve=>server.once('listening',resolve));
 const base=`http://127.0.0.1:${server.address().port}`;
 const registration=await fetch(base+'/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:'owner',password:'1'})});
 const cookie=registration.headers.get('set-cookie').split(';')[0],user=(await registration.json()).user;
 const request=(path,options={})=>fetch(base+path,{...options,headers:{Cookie:cookie,...options.headers}});
 try{await run({storage,request,base,filename,s3,user,puts:()=>puts});}
 finally{await new Promise(resolve=>server.close(resolve));storage.db.close();rmSync(dir,{recursive:true,force:true});}
}
const project={id:'project-test',version:2,title:'История',objects:[],chapters:[{id:'chapter',beats:[]}],events:[]};
test('API auth, CSRF, malformed input, DB persistence and optimistic concurrency',()=>fixture(async({request,base,filename,s3,user})=>{
 assert.equal((await fetch(base+'/api/projects')).status,401);
 assert.equal((await fetch(base+'/api/auth')).status,401);
 assert.equal((await request('/api/projects/project-test',{method:'PUT',headers:{Origin:'https://evil.example','Content-Type':'application/json'},body:'{}'})).status,403);
 const put=(revision,p=project)=>request('/api/projects/project-test',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({project:p,expectedRevision:revision})});
 assert.equal((await put(0)).status,200);
 assert.equal((await put(0,{...project,title:'stale'})).status,409);
 assert.equal((await (await request('/api/projects/project-test')).json()).project.title,'История');
 for(let revision=1;revision<=22;revision++)assert.equal((await put(revision)).status,200);
 const reopened=createStorage({filename,s3,bucket:'test'});
 try{assert.equal(reopened.get(user.id,project.id).revision,23);assert.equal(reopened.db.prepare('SELECT count(*) AS n FROM project_versions').get().n,20);}finally{reopened.db.close();}
 assert.equal((await request('/api/projects')).status,200);
 assert.equal((await request('/api/projects/missing')).status,404);
 assert.equal((await put(23,{...project,objects:[{model:{src:'data:bad'}}]})).status,400);
}));
test('S3 deduplicates meshes, project references round trip and private download streams exact bytes',()=>fixture(async({request,puts})=>{
 const body='v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n';
 const upload=()=>request('/api/meshes?format=obj',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body});
 const mesh=await (await upload()).json();assert.match(mesh.src,/\/api\/meshes\/[a-f0-9]{64}\.obj$/);
 assert.equal((await upload()).status,201);assert.equal(puts(),1);
 const downloaded=await request(mesh.src);assert.match(downloaded.headers.get('cache-control'),/no-store/);
 assert.equal(await downloaded.text(),body);
 const p={...project,objects:[{id:'mesh',model:{...mesh,name:'triangle.obj'}}]};
 assert.equal((await request('/api/projects/'+p.id,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({project:p,expectedRevision:0})})).status,200);
 assert.equal((await (await request('/api/projects/'+p.id)).json()).project.objects[0].model.src,mesh.src);
 assert.equal((await request('/api/meshes?format=glb',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:'bad'})).status,400);
 assert.equal((await request('/api/meshes?format=obj',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:Buffer.alloc(100*1024*1024+1)})).status,413);
}));

test('models up to 100 MiB are accepted and larger models are rejected',()=>{
 assert.equal(MESH_LIMIT,100*1024*1024);
 const body=Buffer.alloc(MESH_LIMIT,32);body.write('v 0 0 0\n');
 assert.doesNotThrow(()=>validateMesh(body,'obj'));
 assert.throws(()=>validateMesh(Buffer.alloc(MESH_LIMIT+1),'obj'),error=>error.status===413);
});
