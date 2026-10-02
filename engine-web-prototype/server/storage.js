import {DatabaseSync} from 'node:sqlite';
import {createHash} from 'node:crypto';
import {mkdirSync} from 'node:fs';
import {dirname} from 'node:path';
import {S3Client, PutObjectCommand, GetObjectCommand, HeadBucketCommand, CreateBucketCommand} from '@aws-sdk/client-s3';

export const MESH_LIMIT=100*1024*1024;
export const meshPattern=/^\/api\/meshes\/([a-f0-9]{64}\.(?:glb|obj))$/;
export function httpError(status,message){return Object.assign(new Error(message),{status});}
export function validateMesh(body,format){
 if(!Buffer.isBuffer(body)||!body.length||body.length>MESH_LIMIT)throw httpError(413,'Размер модели должен быть от 1 байта до 100 МБ.');
 if(!['glb','obj'].includes(format))throw httpError(400,'Поддерживаются GLB и OBJ.');
 if(format==='glb'){
  if(body.length<20||body.readUInt32LE(0)!==0x46546c67||body.readUInt32LE(4)!==2||body.readUInt32LE(8)!==body.length||body.readUInt32LE(16)!==0x4e4f534a)throw httpError(400,'Повреждён GLB.');
  const length=body.readUInt32LE(12);if(20+length>body.length)throw httpError(400,'Повреждён GLB.');
  let json;try{json=JSON.parse(body.subarray(20,20+length).toString().trim());}catch{throw httpError(400,'Повреждён GLB JSON.');}
  if([...(json.buffers||[]),...(json.images||[])].some(r=>r.uri&&!r.uri.startsWith('data:')))throw httpError(400,'GLB должен содержать встроенные ресурсы.');
 }else if(!/^v\s+[-+.\d]/m.test(body.toString('utf8')))throw httpError(400,'OBJ не содержит вершин.');
}
export function createStorage({filename,s3,bucket}){
 mkdirSync(dirname(filename),{recursive:true});
 const db=new DatabaseSync(filename);
 db.exec(`PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON; PRAGMA busy_timeout=5000;
 CREATE TABLE IF NOT EXISTS projects(id TEXT PRIMARY KEY,title TEXT NOT NULL,json TEXT NOT NULL,revision INTEGER NOT NULL,updated_at TEXT NOT NULL);
 CREATE TABLE IF NOT EXISTS project_versions(project_id TEXT NOT NULL,revision INTEGER NOT NULL,json TEXT NOT NULL,created_at TEXT NOT NULL,PRIMARY KEY(project_id,revision));
 CREATE TABLE IF NOT EXISTS meshes(key TEXT PRIMARY KEY,bytes INTEGER NOT NULL,format TEXT NOT NULL,created_at TEXT NOT NULL);`);
 // Rebuild the legacy single-owner tables once; keep their data unassigned until owner migration.
 if(!db.prepare('PRAGMA table_info(projects)').all().some(column=>column.name==='owner_id')){
  db.exec(`BEGIN IMMEDIATE;
  ALTER TABLE projects RENAME TO legacy_projects;
  ALTER TABLE project_versions RENAME TO legacy_versions;
  CREATE TABLE projects(owner_id TEXT NOT NULL,id TEXT NOT NULL,title TEXT NOT NULL,json TEXT NOT NULL,revision INTEGER NOT NULL,updated_at TEXT NOT NULL,PRIMARY KEY(owner_id,id));
  CREATE TABLE project_versions(owner_id TEXT NOT NULL,project_id TEXT NOT NULL,revision INTEGER NOT NULL,json TEXT NOT NULL,created_at TEXT NOT NULL,PRIMARY KEY(owner_id,project_id,revision));
  INSERT INTO projects SELECT '',id,title,json,revision,updated_at FROM legacy_projects;
  INSERT INTO project_versions SELECT '',project_id,revision,json,created_at FROM legacy_versions;
  DROP TABLE legacy_projects; DROP TABLE legacy_versions; COMMIT;`);
 }
 db.exec('CREATE TABLE IF NOT EXISTS user_meshes(user_id TEXT NOT NULL,key TEXT NOT NULL,PRIMARY KEY(user_id,key));');
 return {
  db,
  async ready(){db.prepare('SELECT 1').get();await s3.send(new HeadBucketCommand({Bucket:bucket}));},
  list(owner){return db.prepare('SELECT id,title,revision,updated_at AS updatedAt FROM projects WHERE owner_id=? ORDER BY updated_at DESC,id').all(owner);},
  listOthers(owner){return db.prepare('SELECT projects.id,projects.title,projects.revision,projects.updated_at AS updatedAt,users.id AS ownerId,users.login AS ownerLogin FROM projects JOIN users ON users.id=projects.owner_id WHERE projects.owner_id<>? ORDER BY projects.updated_at DESC,users.login,projects.id').all(owner);},
  get(owner,id){const row=db.prepare('SELECT json,revision,updated_at AS updatedAt FROM projects WHERE owner_id=? AND id=?').get(owner,id);if(!row)throw httpError(404,'Проект не найден.');return {project:JSON.parse(row.json),revision:row.revision,updatedAt:row.updatedAt};},
  save(owner,id,project,expectedRevision){
   if(!project||project.id!==id||![1,2].includes(project.version)||typeof project.title!=='string'||!project.title.trim()||project.title.length>120||!Array.isArray(project.objects)||!Array.isArray(project.chapters)||!project.chapters.length||!Array.isArray(project.events))throw httpError(400,'Некорректный проект.');
   if(!Number.isSafeInteger(expectedRevision)||expectedRevision<0)throw httpError(400,'Нужна версия проекта.');
   const check=value=>{
    if(!value||typeof value!=='object')return;
    if(value.model?.src&&!meshPattern.test(value.model.src))throw httpError(400,'Сначала загрузите модели в S3.');
    if(value.kind==='model'&&value.src&&!meshPattern.test(value.src))throw httpError(400,'Сначала загрузите модели в S3.');
    for(const child of Object.values(value)){
     if(typeof child==='string'&&child.startsWith('/api/meshes/')){
      const match=child.match(meshPattern);if(!match||!db.prepare('SELECT key FROM user_meshes WHERE user_id=? AND key=?').get(owner,match[1]))throw httpError(400,'Модель не найдена в хранилище.');
     }else if(child&&typeof child==='object')check(child);
    }
   };check(project);
   const json=JSON.stringify(project),updatedAt=new Date().toISOString();
   db.exec('BEGIN IMMEDIATE');
   try{
    const current=db.prepare('SELECT revision FROM projects WHERE owner_id=? AND id=?').get(owner,id)?.revision||0;
    if(current!==expectedRevision)throw httpError(409,'Проект изменён в другой вкладке. Экспортируйте текущие изменения и откройте серверную версию.');
    const revision=current+1;
    db.prepare('INSERT INTO projects VALUES(?,?,?,?,?,?) ON CONFLICT(owner_id,id) DO UPDATE SET title=excluded.title,json=excluded.json,revision=excluded.revision,updated_at=excluded.updated_at').run(owner,id,project.title,json,revision,updatedAt);
    db.prepare('INSERT INTO project_versions VALUES(?,?,?,?,?)').run(owner,id,revision,json,updatedAt);
    db.prepare('DELETE FROM project_versions WHERE owner_id=? AND project_id=? AND revision<=?').run(owner,id,revision-20);
    db.exec('COMMIT');return {project,revision,updatedAt};
   }catch(error){db.exec('ROLLBACK');throw error;}
  },
  async upload(owner,body,format){
   validateMesh(body,format);
   const key=createHash('sha256').update(body).digest('hex')+'.'+format;
   if(!db.prepare('SELECT key FROM meshes WHERE key=?').get(key)){
    await s3.send(new PutObjectCommand({Bucket:bucket,Key:`meshes/${key}`,Body:body,ContentType:format==='glb'?'model/gltf-binary':'text/plain'}));
    db.prepare('INSERT OR IGNORE INTO meshes VALUES(?,?,?,?)').run(key,body.length,format,new Date().toISOString());
   }
   db.prepare('INSERT OR IGNORE INTO user_meshes VALUES(?,?)').run(owner,key);
   return {src:`/api/meshes/${key}`,bytes:body.length,format};
  },
  async mesh(owner,key,{allowOthers=false}={}){if(!(allowOthers?db.prepare('SELECT key FROM user_meshes WHERE key=?').get(key):db.prepare('SELECT key FROM user_meshes WHERE user_id=? AND key=?').get(owner,key)))throw httpError(404,'Модель не найдена.');return s3.send(new GetObjectCommand({Bucket:bucket,Key:`meshes/${key}`}));}
 };
}
export async function connectStorage(env=process.env){
 if(!env.S3_BUCKET)throw new Error('S3_BUCKET is required');
 const s3=new S3Client({region:env.S3_REGION||'us-east-1',...(env.S3_ENDPOINT?{endpoint:env.S3_ENDPOINT}:{}),forcePathStyle:env.S3_FORCE_PATH_STYLE==='true',...(env.S3_ACCESS_KEY?{credentials:{accessKeyId:env.S3_ACCESS_KEY,secretAccessKey:env.S3_SECRET_KEY}}:{})});
 if(env.S3_CREATE_BUCKET==='true'){
  try{await s3.send(new HeadBucketCommand({Bucket:env.S3_BUCKET}));}
  catch(error){if(error.$metadata?.httpStatusCode!==404&&error.name!=='NotFound')throw error;await s3.send(new CreateBucketCommand({Bucket:env.S3_BUCKET}));}
 }
 return createStorage({filename:env.DB_PATH||'/data/projects.sqlite',s3,bucket:env.S3_BUCKET});
}
