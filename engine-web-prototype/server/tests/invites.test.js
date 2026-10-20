import test from 'node:test';
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {createHash} from 'node:crypto';
import {mkdtempSync,rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {createStorage} from '../storage.js';
import {createAuth} from '../auth.js';
import {createApp} from '../app.js';
const digest=token=>createHash('sha256').update(token).digest('hex');
async function fixture(t,options={}){
 const dir=mkdtempSync(join(tmpdir(),'sacura-invites-')),filename=join(dir,'db.sqlite');
 const storage=createStorage({filename,s3:{send:async()=>({})},bucket:'test'}),auth=createAuth(storage.db);
 await auth.migrateOwner('creator','secret');
 const user=await auth.login({login:'creator',password:'secret'}),cookie='sacura_session='+auth.session(user);
 const server=createApp(storage,options).listen(0,'127.0.0.1');await new Promise(resolve=>server.once('listening',resolve));
 t.after(async()=>{await new Promise(resolve=>server.close(resolve));storage.db.close();rmSync(dir,{recursive:true});});
 const base=`http://127.0.0.1:${server.address().port}`;
 async function request(path,{body,cookie:session,headers={},method=body?'POST':'GET'}={}){
  const response=await fetch(base+'/api'+path,{method,headers:{...(session?{Cookie:session}:{}),...(body?{'Content-Type':'application/json'}:{}),...headers},...(body?{body:JSON.stringify(body)}:{})});
  return {status:response.status,data:await response.json(),cookie:response.headers.get('set-cookie'),retry:response.headers.get('retry-after')};
 }
 return {auth,storage,user,cookie,request,base,filename};
}
test('registration rejects missing, malformed, unknown, expired and used invites',async t=>{
 const {auth,storage,user}=await fixture(t),credentials={login:'new',password:'secret'};
 for(const invite of [undefined,'bad','f'.repeat(64),{},['a'.repeat(64)]])await assert.rejects(auth.register({...credentials,invite}),{status:403});
 const expired=auth.createInvite(user.id).token;
 storage.db.prepare('UPDATE invites SET expires_at=? WHERE token_hash=?').run(Date.now()-1,digest(expired));
 await assert.rejects(auth.register({...credentials,invite:expired}),/истёк/);
 const used=auth.createInvite(user.id).token;
 storage.db.prepare('UPDATE invites SET used_at=? WHERE token_hash=?').run(Date.now(),digest(used));
 await assert.rejects(auth.register({...credentials,invite:used}),/использовано/);
 assert.equal(storage.db.prepare('SELECT count(*) AS n FROM users').get().n,1);
});
test('invite API requires authentication, returns safe URL and metadata, registers and auto logs in once',async t=>{
 const {request,cookie,base,storage,user}=await fixture(t);
 assert.equal((await request('/invites',{body:{}})).status,401);
 assert.equal((await request('/auth/register',{body:{login:'blocked',password:'secret'}})).status,403);
 const created=await request('/invites',{cookie,body:{}});assert.equal(created.status,201);
 const url=new URL(created.data.url),token=url.searchParams.get('invite');
 assert.equal(url.origin,base);assert.equal(url.pathname,'/register');assert.match(token,/^[a-f0-9]{64}$/);
 const row=storage.db.prepare('SELECT * FROM invites').get();
 assert.equal(row.created_by_user_id,user.id);assert.equal(row.token_hash,digest(token));assert.equal(row.expires_at-row.created_at,72*60*60*1000);assert.equal(row.used_at,null);
 const valid=await request('/invites/validate',{body:{invite:token}});assert.equal(valid.status,200);assert.deepEqual(Object.keys(valid.data),['expiresAt']);
 const registered=await request('/auth/register',{body:{login:'new',password:'secret',invite:token}});assert.equal(registered.status,201);assert.match(registered.cookie,/HttpOnly/);
 assert.equal((await request('/auth',{cookie:registered.cookie.split(';')[0]})).data.user.login,'new');
 const used=storage.db.prepare('SELECT * FROM invites').get();assert.ok(used.used_at);assert.equal(used.used_by_user_id,registered.data.user.id);
 assert.equal((await request('/invites/validate',{body:{invite:token}})).status,403);
 await assert.rejects(createAuth(storage.db).register({login:'another',password:'secret',invite:token}),{status:403});
 assert.equal((await request('/auth/register',{body:{login:'another',password:'secret',invite:token}})).status,403);
});
test('logged in clients cannot create a second account; configured frontend origin takes precedence',async t=>{
 const {request,cookie,storage}=await fixture(t,{frontendBaseUrl:'https://editor.example.test',secureCookies:true});
 const created=await request('/invites',{cookie,body:{}});assert.equal(new URL(created.data.url).origin,'https://editor.example.test');
 const invite=new URL(created.data.url).searchParams.get('invite');
 assert.equal((await request('/auth/register',{cookie,body:{login:'accidental',password:'secret',invite}})).status,409);
 assert.equal(storage.db.prepare('SELECT used_at FROM invites').get().used_at,null);
});
test('duplicate login rolls back consumption; concurrent registration across DB connections consumes exactly once',async t=>{
 const {auth,storage,user,filename}=await fixture(t),invite=auth.createInvite(user.id).token;
 await assert.rejects(auth.register({login:'creator',password:'secret',invite}),{status:409});
 assert.equal(storage.db.prepare('SELECT used_at FROM invites').get().used_at,null);
 const db=new DatabaseSync(filename);db.exec('PRAGMA foreign_keys=ON; PRAGMA busy_timeout=5000;');t.after(()=>db.close());
 const other=createAuth(db);
 const results=await Promise.allSettled([auth.register({login:'a',password:'secret',invite}),other.register({login:'b',password:'secret',invite})]);
 assert.equal(results.filter(r=>r.status==='fulfilled').length,1);assert.equal(results.find(r=>r.status==='rejected').reason.status,403);
 assert.equal(storage.db.prepare('SELECT count(*) AS n FROM users').get().n,2);
});
test('parallel HTTP requests using one invite produce only one user',async t=>{
 const {request,auth,user,storage}=await fixture(t),invite=auth.createInvite(user.id).token;
 const results=await Promise.all(['one','two'].map(login=>request('/auth/register',{body:{login,password:'secret',invite}})));
 assert.deepEqual(results.map(r=>r.status).sort(),[201,403]);assert.equal(storage.db.prepare('SELECT count(*) AS n FROM users').get().n,2);
});
test('IP limits ignore spoofed X-Forwarded-For by default and reset after their window; creation uses user budget',async t=>{
 const {request,cookie,storage,auth}=await fixture(t);
 for(let i=0;i<5;i++)assert.equal((await request('/auth/login',{body:{login:'creator',password:'wrong'},headers:{'X-Forwarded-For':`203.0.113.${i}`}})).status,401);
 const denied=await request('/auth/login',{body:{login:'creator',password:'secret'}});assert.equal(denied.status,429);assert.ok(Number(denied.retry)>0);
 for(let i=0;i<3;i++)assert.equal((await request('/auth/register',{body:{login:'blocked',password:'secret'}})).status,403);
 assert.equal((await request('/auth/register',{body:{}})).status,429);
 for(let i=0;i<30;i++)assert.equal((await request('/invites/validate',{body:{invite:'bad'}})).status,403);
 assert.equal((await request('/invites/validate',{body:{invite:'bad'}})).status,429);
 for(let i=0;i<100;i++)assert.equal((await request('/invites',{cookie,body:{}})).status,201);
 assert.equal((await request('/invites',{cookie,body:{}})).status,429);
 const relogin='sacura_session='+auth.session(await auth.login({login:'creator',password:'secret'}));
 assert.equal((await request('/invites',{cookie:relogin,body:{}})).status,429);
 storage.db.prepare('UPDATE auth_rate_limits SET expires_at=?').run(Date.now()-1);
 assert.equal((await request('/auth/login',{body:{login:'creator',password:'secret'}})).status,200);
 assert.equal((await request('/invites',{cookie,body:{}})).status,201);
});
test('trusted private proxy uses the sanitized client IP for separate budgets',async t=>{
 const {request}=await fixture(t,{trustProxy:1});
 for(let i=0;i<5;i++)assert.equal((await request('/auth/login',{body:{login:'creator',password:'wrong'},headers:{'X-Forwarded-For':'198.51.100.1'}})).status,401);
 assert.equal((await request('/auth/login',{body:{login:'creator',password:'secret'},headers:{'X-Forwarded-For':'198.51.100.1'}})).status,429);
 assert.equal((await request('/auth/login',{body:{login:'creator',password:'secret'},headers:{'X-Forwarded-For':'198.51.100.2'}})).status,200);
});
