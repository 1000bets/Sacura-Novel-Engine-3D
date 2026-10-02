import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {DatabaseSync} from 'node:sqlite';
import {Readable} from 'node:stream';
import {createStorage} from '../storage.js';
import {createApp} from '../app.js';
import {createAuth} from '../auth.js';
const project=(title='Личный проект')=>({id:'same-id',version:2,title,objects:[],chapters:[{id:'chapter',beats:[]}],events:[]});
test('accounts, durable sessions, project and mesh isolation, logout',async t=>{
 const dir=mkdtempSync(join(tmpdir(),'sacura-auth-')),filename=join(dir,'db.sqlite');
 const storage=createStorage({filename,s3:{send:async()=>({})},bucket:'test'});
 const server=createApp(storage).listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
 t.after(async()=>{await new Promise(resolve=>server.close(resolve));storage.db.close();rmSync(dir,{recursive:true});});
 const base=`http://127.0.0.1:${server.address().port}`;
 async function request(path,{cookie,body,method=body?'POST':'GET',headers={}}={}){
  const response=await fetch(base+'/api'+path,{method,headers:{...(cookie?{Cookie:cookie}:{}),...(body?{'Content-Type':'application/json'}:{}),...headers},...(body?{body:JSON.stringify(body)}:{})});
  return {status:response.status,data:await response.json(),cookie:response.headers.get('set-cookie')?.split(';')[0]};
 }
 assert.equal((await request('/projects')).status,401);
 const a=await request('/auth/register',{body:{login:'alice',password:'1'}}),b=await request('/auth/register',{body:{login:'bob',password:'2'}});
 assert.equal(a.status,201);assert.equal(b.status,201);assert.match(a.cookie,/sacura_session=/);
 assert.equal((await request('/auth/register',{body:{login:'alice',password:'3'}})).status,409);
 assert.equal((await request('/auth/login',{body:{login:'alice',password:'wrong'}})).status,401);
 assert.equal((await request('/auth/login',{body:{login:'alice',password:'1'}})).status,200);
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
  const auth=createAuth(storage.db),newUser=await auth.register({login:'new',password:'1'});
  assert.deepEqual(storage.list(newUser.id),[]);
  await auth.migrateOwner('owner','1');
  const owner=await auth.login({login:'owner',password:'1'});assert.equal(storage.get(owner.id,'same-id').project.title,'Старый');
  storage.save(owner.id,'same-id',project('Обновлён'),1);storage.db.close();
  storage=createStorage({filename,s3:{},bucket:'test'});assert.equal(storage.get(owner.id,'same-id').revision,2);
 }finally{storage?.db.close();rmSync(dir,{recursive:true});}
});

test('only authenticated syper can list, read and edit other owners without changing ownership',async t=>{
 const dir=mkdtempSync(join(tmpdir(),'sacura-admin-'));
 const storage=createStorage({filename:join(dir,'db.sqlite'),s3:{send:async()=>({Body:Readable.from([Buffer.from('v 0 0 0\n')])})},bucket:'test'});
 const auth=createAuth(storage.db);
 const users={};
 for(const login of ['syper','alice','bob','Syper']){
  const user=await auth.register({login,password:'test'});
  users[login]={...user,cookie:'sacura_session='+auth.session(user)};
 }
 const server=createApp(storage).listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
 t.after(async()=>{await new Promise(resolve=>server.close(resolve));storage.db.close();rmSync(dir,{recursive:true});});
 const base=`http://127.0.0.1:${server.address().port}/api`;
 const request=async(path,user,body)=>{
  const response=await fetch(base+path,{method:body?'PUT':'GET',headers:{...(user?{Cookie:user.cookie}:{}),...(body?{'Content-Type':'application/json'}:{})},...(body?{body:JSON.stringify(body)}:{})});
  return {status:response.status,data:await response.json()};
 };
 storage.save(users.syper.id,'same-id',project('Свой'),0);
 const mesh=await storage.upload(users.alice.id,Buffer.from('v 0 0 0\n'),'obj');
 const aliceProject=project('Алиса');aliceProject.objects=[{model:{src:mesh.src}}];
 storage.save(users.alice.id,'same-id',aliceProject,0);
 storage.save(users.bob.id,'same-id',project('Боб'),0);
 const path='/projects/same-id?ownerId='+users.alice.id;
 assert.equal((await request('/admin/projects')).status,401);
 for(const login of ['alice','bob','Syper']){
  assert.equal((await request('/admin/projects',users[login])).status,403);
  assert.equal((await request(path,users[login])).status,login==='alice'?200:403);
  if(login!=='alice')assert.equal((await request(path,users[login],{project:project('Взлом'),expectedRevision:1})).status,403);
 }
 const others=(await request('/admin/projects',users.syper)).data;
 assert.equal(others.length,2);
 assert.deepEqual(others.map(p=>p.ownerLogin).sort(),['alice','bob']);
 assert.ok(others.every(p=>p.ownerId&&p.id==='same-id'&&!p.project));
 assert.equal((await request('/projects',users.syper)).data.length,1);
 const opened=await request(path,users.syper);assert.equal(opened.data.project.title,'Алиса');
 const meshResponse=await fetch(base+mesh.src.slice(4),{headers:{Cookie:users.syper.cookie}});
 assert.equal(meshResponse.status,200);assert.equal(await meshResponse.text(),'v 0 0 0\n');
 const uploaded=await fetch(base+'/meshes?format=obj&ownerId='+users.alice.id,{method:'POST',headers:{Cookie:users.syper.cookie,'Content-Type':'application/octet-stream'},body:'v 1 2 3\n'});
 assert.equal(uploaded.status,201);
 const uploadedMesh=await uploaded.json();
 const edited={...opened.data.project,title:'Правка syper',objects:[...opened.data.project.objects,{model:{src:uploadedMesh.src}}]};
 assert.equal((await request(path,users.syper,{project:edited,expectedRevision:1})).status,200);
 assert.equal(storage.get(users.alice.id,'same-id').project.title,'Правка syper');
 assert.equal(storage.get(users.syper.id,'same-id').project.title,'Свой');
 assert.equal(storage.get(users.bob.id,'same-id').project.title,'Боб');
 assert.equal(storage.db.prepare('SELECT COUNT(*) AS n FROM project_versions WHERE owner_id=?').get(users.alice.id).n,2);
 assert.equal((await request(path,users.syper,{project:edited,expectedRevision:1})).status,409);
 assert.equal((await request('/projects/missing?ownerId='+users.alice.id,users.syper,{project:{...project(),id:'missing'},expectedRevision:0})).status,404);
 assert.equal((await request('/projects/same-id?ownerId=unknown',users.syper)).status,404);
 const forbiddenUpload=await fetch(base+'/meshes?format=obj&ownerId='+users.alice.id,{method:'POST',headers:{Cookie:users.bob.cookie,'Content-Type':'application/octet-stream'},body:'v 1 2 3\n'});
 assert.equal(forbiddenUpload.status,403);
});
