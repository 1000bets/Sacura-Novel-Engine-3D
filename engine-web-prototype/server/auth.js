import {randomBytes,randomUUID,scrypt as derive,timingSafeEqual,createHash} from 'node:crypto';
import {readFileSync} from 'node:fs';
import {promisify} from 'node:util';
import {httpError} from './storage.js';
const scrypt=promisify(derive);
const digest=token=>createHash('sha256').update(token).digest('hex');
export const SESSION_SECONDS=30*24*60*60;
export function createAuth(db){
 db.exec(`CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY,login TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,user_id TEXT NOT NULL REFERENCES users(id),expires_at INTEGER NOT NULL);`);
 db.exec(readFileSync(new URL('./migrations/001-invites.sql',import.meta.url),'utf8'));
 function credentials(body){
  if(typeof body?.login!=='string'||!body.login.trim()||typeof body?.password!=='string'||!body.password)throw httpError(400,'Укажите логин и пароль.');
  return {login:body.login.trim(),password:body.password};
 }
 async function prepareUser(body){
  const {login,password}=credentials(body);
  const salt=randomBytes(16).toString('hex'),hash=(await scrypt(password,salt,64)).toString('hex'),id=randomUUID();
  return {id,login,passwordHash:salt+':'+hash};
 }
 function insertUser({id,login,passwordHash}){
  try{db.prepare('INSERT INTO users VALUES(?,?,?)').run(id,login,passwordHash);}
  catch(error){if(db.prepare('SELECT id FROM users WHERE login=?').get(login))throw httpError(409,'Этот логин уже занят.');throw error;}
  return {id,login};
 }
 function validateInvite(token){
  if(typeof token!=='string'||! /^[a-f0-9]{64}$/.test(token))throw httpError(403,'Для регистрации нужна действующая ссылка приглашения.');
  const invite=db.prepare('SELECT * FROM invites WHERE token_hash=?').get(digest(token));
  if(!invite)throw httpError(403,'Приглашение не найдено. Попросите новую ссылку.');
  if(invite.used_at!==null)throw httpError(403,'Приглашение уже использовано. Попросите новую ссылку.');
  if(invite.expires_at<=Date.now())throw httpError(403,'Срок приглашения истёк. Попросите новую ссылку.');
  return invite;
 }
 return {
  validateInvite(token){return {expiresAt:new Date(validateInvite(token).expires_at).toISOString()};},
  createInvite(userId){
   const token=randomBytes(32).toString('hex'),createdAt=Date.now(),expiresAt=createdAt+72*60*60*1000;
   db.prepare('INSERT INTO invites(id,token_hash,created_by_user_id,created_at,expires_at) VALUES(?,?,?,?,?)').run(randomUUID(),digest(token),userId,createdAt,expiresAt);
   return {token,expiresAt:new Date(expiresAt).toISOString()};
  },
  async register(body){
   validateInvite(body?.invite);
   // Do expensive password derivation before taking SQLite's write lock.
   const prepared=await prepareUser(body);
   db.exec('BEGIN IMMEDIATE');
   try{
    const invite=validateInvite(body.invite),user=insertUser(prepared);
    db.prepare('UPDATE invites SET used_at=?,used_by_user_id=? WHERE id=?').run(Date.now(),user.id,invite.id);
    db.exec('COMMIT');return user;
   }catch(error){db.exec('ROLLBACK');throw error;}
  },
  rateLimit(key,limit,windowMs){
   const now=Date.now();
   db.prepare('DELETE FROM auth_rate_limits WHERE expires_at<=?').run(now);
   const row=db.prepare(`INSERT INTO auth_rate_limits VALUES(?,1,?)
    ON CONFLICT(key) DO UPDATE SET attempts=attempts+1 RETURNING attempts,expires_at`).get(key,now+windowMs);
   return {allowed:row.attempts<=limit,retryAfter:Math.max(1,Math.ceil((row.expires_at-now)/1000))};
  },
  async login(body){
   const {login,password}=credentials(body),row=db.prepare('SELECT * FROM users WHERE login=?').get(login);
   const [salt,hash]=(row?.password_hash||'00000000000000000000000000000000:'+ '0'.repeat(128)).split(':');
   const actual=await scrypt(password,salt,64);
   if(!row||!timingSafeEqual(actual,Buffer.from(hash,'hex')))throw httpError(401,'Неверный логин или пароль.');
   return {id:row.id,login:row.login};
  },
  session(user){
   db.prepare('DELETE FROM sessions WHERE expires_at<=?').run(Date.now());
   const token=randomBytes(32).toString('hex');
   db.prepare('INSERT INTO sessions VALUES(?,?,?)').run(digest(token),user.id,Date.now()+SESSION_SECONDS*1000);return token;
  },
  user(token){return db.prepare('SELECT users.id,users.login FROM sessions JOIN users ON users.id=sessions.user_id WHERE token_hash=? AND expires_at>?').get(digest(token||''),Date.now());},
  logout(token){db.prepare('DELETE FROM sessions WHERE token_hash=?').run(digest(token||''));},
  async migrateOwner(login,password){
   if(!login||!password)return;
   const user=db.prepare('SELECT id,login FROM users WHERE login=?').get(login)||insertUser(await prepareUser({login,password}));
   const legacy=db.prepare("SELECT json FROM projects WHERE owner_id=''").all();
   db.exec('BEGIN IMMEDIATE');
   try{
    db.prepare("UPDATE projects SET owner_id=? WHERE owner_id=''").run(user.id);
    db.prepare("UPDATE project_versions SET owner_id=? WHERE owner_id=''").run(user.id);
    const link=value=>{
     if(typeof value==='string'){
      const match=value.match(/^\/api\/meshes\/([a-f0-9]{64}\.(?:glb|obj))$/);
      if(match&&db.prepare('SELECT key FROM meshes WHERE key=?').get(match[1]))db.prepare('INSERT OR IGNORE INTO user_meshes VALUES(?,?)').run(user.id,match[1]);
     }else if(value&&typeof value==='object')Object.values(value).forEach(link);
    };
    legacy.forEach(row=>link(JSON.parse(row.json)));
    db.exec('COMMIT');
   }catch(error){db.exec('ROLLBACK');throw error;}

  }
 };
}
