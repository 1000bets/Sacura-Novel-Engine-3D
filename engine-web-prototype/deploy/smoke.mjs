// Run against an isolated deployment: creates a disposable account and project.
import assert from 'node:assert/strict';
import {randomUUID} from 'node:crypto';
import {readFile,writeFile} from 'node:fs/promises';
import {checkModels} from './model-smoke.mjs';
import {createEmptyProject} from '../frontend/src/projectLifecycle.js';
const base=process.env.SACURA_SMOKE_URL||'http://127.0.0.1:8080';
if(process.env.SACURA_SMOKE_STATE){
 let state;try{state=JSON.parse(await readFile(process.env.SACURA_SMOKE_STATE,'utf8'));}catch(error){if(error.code!=='ENOENT')throw error;}
 if(state){
  const opened=await fetch(base+'/api/projects/'+state.project.id,{headers:{Cookie:state.cookie}});
  assert.equal(opened.status,200);assert.deepEqual((await opened.json()).project,state.project);
  const mesh=await fetch(base+state.project.objects[0].model.src,{headers:{Cookie:state.cookie}});
  assert.equal(mesh.status,200);assert.equal(await mesh.text(),state.body);
  await checkModels(base,state.cookie,state.models);
  console.log('Persistence passed: session, SQLite project and S3 mesh survive container restart.');process.exit(0);
 }
}
const ownerLogin=process.env.SACURA_SMOKE_LOGIN||process.env.APP_USER,ownerPassword=process.env.SACURA_SMOKE_PASSWORD||process.env.APP_PASSWORD;
assert.ok(ownerLogin&&ownerPassword,'Set SACURA_SMOKE_LOGIN/PASSWORD to an existing account on this isolated deployment.');
const ownerResponse=await fetch(base+'/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:ownerLogin,password:ownerPassword})});
assert.equal(ownerResponse.status,200);
const ownerCookie=ownerResponse.headers.get('set-cookie').split(';')[0];
assert.equal((await fetch(base+'/api/auth/register',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:'blocked',password:'blocked'})})).status,403);
const invited=await fetch(base+'/api/invites',{method:'POST',headers:{Cookie:ownerCookie}});
assert.equal(invited.status,201);
const invite=new URL((await invited.json()).url).searchParams.get('invite');
const validation=await fetch(base+'/api/invites/validate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({invite})});
assert.equal(validation.status,200);
const registration=await fetch(base+'/api/auth/register',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:'smoke-'+randomUUID(),password:randomUUID(),invite})});
assert.equal(registration.status,201);
assert.equal((await fetch(base+'/api/invites/validate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({invite})})).status,403);
const cookie=registration.headers.get('set-cookie').split(';')[0];
const request=(path,options={})=>fetch(base+path,{...options,headers:{Cookie:cookie,...options.headers}});
assert.equal((await fetch(base+'/api/projects')).status,401);
const body='v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n';
const uploaded=await request('/api/meshes?format=obj',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body});
assert.equal(uploaded.status,201);const model=await uploaded.json();
const project=createEmptyProject('Проверка контейнеров');
project.objects.push({id:'smoke-mesh',name:'triangle.obj',type:'Меш',model:{...model,name:'triangle.obj'}});
const saved=await request('/api/projects/'+project.id,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({project,expectedRevision:0})});
assert.equal(saved.status,200);
const opened=await request('/api/projects/'+project.id);
assert.deepEqual((await opened.json()).project,project);
assert.equal(await (await request(model.src)).text(),body);
const login=await fetch(base+'/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({login:(await registration.json()).user.login,password:'wrong'})});
assert.equal(login.status,401);
const models=await checkModels(base,cookie);
console.log('Container smoke passed: accounts, SQLite project round trip, S3 upload and download.');

if(process.env.SACURA_SMOKE_STATE)await writeFile(process.env.SACURA_SMOKE_STATE,JSON.stringify({cookie,project,body,models}),{mode:0o600});
