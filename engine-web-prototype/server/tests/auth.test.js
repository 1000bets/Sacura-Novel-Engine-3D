import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {DatabaseSync} from 'node:sqlite';
import {createStorage} from '../storage.js';
import {createApp} from '../app.js';
import {createAuth} from '../auth.js';
const project=(title='Личный проект')=>({id:'same-id',version:2,title,objects:[],chapters:[{id:'chapter',beats:[]}],events:[]});
test('accounts, durable sessions, project and mesh isolation, logout',async t=>{
 const dir=mkdtempSync(join(tmpdir(),'sacura-auth-')),filename=join(dir,'db.sqlite');
 const storage=createStorage({filename,s3:{send:async()=>({})},bucket:'test'});
 const auth=createAuth(storage.db);await auth.migrateOwner('seed','seed');
 const seed=await auth.login({login:'seed',password:'seed'});
 const invite=()=>auth.createInvite(seed.id).token;
 const server=createApp(storage).listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
 t.after(async()=>{await new Promise(resolve=>server.close(resolve));storage.db.close();rmSync(dir,{recursive:true});});
 const base=`http://127.0.0.1:${server.address().port}`;
 async function request(path,{cookie,body,method=body?'POST':'GET',headers={}}={}){
  const response=await fetch(base+'/api'+path,{method,headers:{...(cookie?{Cookie:cookie}:{}),...(body?{'Content-Type':'application/json'}:{}),...headers},...(body?{body:JSON.stringify(body)}:{})});
  return {status:response.status,data:await response.json(),cookie:response.headers.get('set-cookie')?.split(';')[0]};
 }
 assert.equal((await request('/projects')).status,401);
 const a=await request('/auth/register',{body:{login:'alice',password:'1',invite:invite()}}),b=await request('/auth/register',{body:{login:'bob',password:'2',invite:invite()}});
 assert.equal(a.status,201);assert.equal(b.status,201);assert.match(a.cookie,/sacura_session=/);
 assert.equal((await request('/auth/register',{body:{login:'alice',password:'3',invite:invite()}})).status,409);
 assert.equal((await request('/auth/login',{body:{login:'alice',password:'wrong'}})).status,401);
 assert.equal((await request('/auth/login',{body:{login:'alice',password:'1',invite:invite()}})).status,200);
 assert.equal((await request('/projects/same-id',{cookie:a.cookie,method:'PUT',body:{project:project(),expectedRevision:0}})).status,200);
 assert.deepEqual((await request('/projects',{cookie:b.cookie})).data,[]);
 assert.equal((await request('/projects/same-id',{cookie:b.cookie})).status,404);
 assert.equal((await request('/projects/same-id',{cookie:b.cookie,method:'PUT',body:{project:project('Боб'),expectedRevision:0}})).status,200);
 assert.equal((await request('/projects/same-id',{cookie:a.cookie})).data.project.title,'Личный проект');
 assert.equal((await request('/projects/same-id',{cookie:b.cookie})).data.project.title,'Боб');
 assert.equal((await request('/projects/same-id',{cookie:a.cookie,method:'PUT',body:{project:project(),expectedRevision:0}})).status,409);
 assert.equal((await request('/projects',{cookie:b.cookie,headers:{'X-Sacura-User':a.data.user.id}})).status,401);
 const mesh=await storage.upload(a.data.user.id,Buffer.from('v 0 0 0\n'),'obj');
 assert.equal((await request(mesh.src.slice(4),{cookie:b.cookie})).status,404);
 const withMesh=project();withMesh.objects=[{model:{src:mesh.src}}];
 assert.equal((await request('/projects/same-id',{cookie:b.cookie,method:'PUT',body:{project:withMesh,expectedRevision:1}})).status,400);
 const anotherDb=new DatabaseSync(filename);assert.equal(createAuth(anotherDb).user(a.cookie.slice(15)).login,'alice');anotherDb.close();
 assert.equal((await request('/auth/logout',{cookie:a.cookie,body:{}})).status,200);
 assert.equal((await request('/projects',{cookie:a.cookie})).status,401);
 assert.equal((await request('/projects',{cookie:b.cookie})).status,200);
 assert.equal((await request('/auth/register',{body:{login:'cross',password:'1'},headers:{Origin:'https://other.example'}})).status,403);
 assert.notEqual(storage.db.prepare('SELECT password_hash FROM users WHERE login=?').get('alice').password_hash,'1');
});
test('legacy projects preserved, assigned only to configured previous owner, restart works',async()=>{
 const dir=mkdtempSync(join(tmpdir(),'sacura-migration-')),filename=join(dir,'db.sqlite');
 let storage;
 try{
  const legacy=new DatabaseSync(filename);
  legacy.exec('CREATE TABLE projects(id TEXT PRIMARY KEY,title TEXT NOT NULL,json TEXT NOT NULL,revision INTEGER NOT NULL,updated_at TEXT NOT NULL); CREATE TABLE project_versions(project_id TEXT NOT NULL,revision INTEGER NOT NULL,json TEXT NOT NULL,created_at TEXT NOT NULL,PRIMARY KEY(project_id,revision));');
  legacy.prepare('INSERT INTO projects VALUES(?,?,?,?,?)').run('same-id','Старый',JSON.stringify(project('Старый')),1,'2026-01-01');
  legacy.prepare('INSERT INTO project_versions VALUES(?,?,?,?)').run('same-id',1,JSON.stringify(project('Старый')),'2026-01-01');legacy.close();
  storage=createStorage({filename,s3:{},bucket:'test'});
  const auth=createAuth(storage.db);storage.db.prepare('INSERT INTO users VALUES(?,?,?)').run('seed','seed','unused');
  const newUser=await auth.register({login:'new',password:'1',invite:auth.createInvite('seed').token});
  assert.deepEqual(storage.list(newUser.id),[]);
  await auth.migrateOwner('owner','1');
  const owner=await auth.login({login:'owner',password:'1'});assert.equal(storage.get(owner.id,'same-id').project.title,'Старый');
  storage.save(owner.id,'same-id',project('Обновлён'),1);storage.db.close();
  storage=createStorage({filename,s3:{},bucket:'test'});assert.equal(storage.get(owner.id,'same-id').revision,2);
 }finally{storage?.db.close();rmSync(dir,{recursive:true});}
});
