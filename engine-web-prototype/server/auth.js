import {randomBytes,randomUUID,scrypt as derive,timingSafeEqual,createHash} from 'node:crypto';
import {promisify} from 'node:util';
import {httpError} from './storage.js';
const scrypt=promisify(derive);
const digest=token=>createHash('sha256').update(token).digest('hex');
export const SESSION_SECONDS=30*24*60*60;
export function createAuth(db){
 db.exec(`CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY,login TEXT NOT NULL UNIQUE,password_hash TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY,user_id TEXT NOT NULL REFERENCES users(id),expires_at INTEGER NOT NULL);`);
 function credentials(body){
  if(typeof body?.login!=='string'||!body.login.trim()||typeof body?.password!=='string'||!body.password)throw httpError(400,'Укажите логин и пароль.');
  return {login:body.login.trim(),password:body.password};
 }
 async function register(body){
  const {login,password}=credentials(body);
  const salt=randomBytes(16).toString('hex'),hash=(await scrypt(password,salt,64)).toString('hex'),id=randomUUID();
  try{db.prepare('INSERT INTO users VALUES(?,?,?)').run(id,login,salt+':'+hash);}
  catch(error){if(db.prepare('SELECT id FROM users WHERE login=?').get(login))throw httpError(409,'Этот логин уже занят.');throw error;}
  return {id,login};
 }
 return {
  register,
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
   const user=db.prepare('SELECT id,login FROM users WHERE login=?').get(login)||await register({login,password});
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
