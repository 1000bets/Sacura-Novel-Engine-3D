import express from 'express';
import {createAuth,SESSION_SECONDS} from './auth.js';
import {pipeline} from 'node:stream/promises';
import {MESH_LIMIT,httpError} from './storage.js';
export function createApp(storage,{secureCookies=false}={}){
 const auth=createAuth(storage.db);
 const token=req=>req.headers.cookie?.split(';').map(value=>value.trim()).find(value=>value.startsWith('sacura_session='))?.slice(15)||'';
 const cookieOptions={httpOnly:true,sameSite:'lax',secure:secureCookies,path:'/'};
 const app=express();app.disable('x-powered-by');
 app.get('/api/health',async(req,res)=>{try{await storage.ready();res.json({status:'ok'});}catch{res.status(503).json({status:'unavailable'});}});
 app.use((req,res,next)=>{
  res.set('Cache-Control','no-store');
  if(!['GET','HEAD','OPTIONS'].includes(req.method)){
   if(req.headers['sec-fetch-site']==='cross-site')return res.status(403).json({error:'Запрос с другого сайта запрещён.'});
   if(req.headers.origin){try{if(new URL(req.headers.origin).host!==req.headers.host)return res.status(403).json({error:'Недопустимый Origin.'});}catch{return res.status(403).end();}}
  }
  next();
 });
 app.post('/api/auth/register',express.json({limit:'16kb'}),async(req,res)=>{
  const user=await auth.register(req.body);
  res.cookie('sacura_session',auth.session(user),{...cookieOptions,maxAge:SESSION_SECONDS*1000});res.status(201).json({user});
 });
 app.post('/api/auth/login',express.json({limit:'16kb'}),async(req,res)=>{
  const user=await auth.login(req.body);
  res.cookie('sacura_session',auth.session(user),{...cookieOptions,maxAge:SESSION_SECONDS*1000});res.json({user});
 });
 app.post('/api/auth/logout',(req,res)=>{auth.logout(token(req));res.clearCookie('sacura_session',cookieOptions);res.json({ok:true});});
 app.use((req,res,next)=>{
  req.user=auth.user(token(req));
  if(!req.user)return res.status(401).json({error:'Войдите в аккаунт.'});
  if(req.headers['x-sacura-user']&&req.headers['x-sacura-user']!==req.user.id)return res.status(401).json({error:'Аккаунт изменился. Обновите страницу.'});
  next();
 });
 app.get('/api/auth',(req,res)=>res.json({user:req.user}));
 app.get('/api/projects',(req,res)=>res.json(storage.list(req.user.id)));
 app.get('/api/projects/:id',(req,res)=>res.json(storage.get(req.user.id,req.params.id)));
 app.put('/api/projects/:id',express.json({limit:'256mb'}),(req,res)=>{
  if(!/^[\w-]{1,120}$/.test(req.params.id))throw httpError(400,'Некорректный ID.');
  res.json(storage.save(req.user.id,req.params.id,req.body.project,req.body.expectedRevision));
 });
 app.post('/api/meshes',express.raw({type:'application/octet-stream',limit:MESH_LIMIT}),async(req,res)=>res.status(201).json(await storage.upload(req.user.id,req.body,req.query.format)));
 app.get('/api/meshes/:key',async(req,res)=>{
  if(!/^[a-f0-9]{64}\.(glb|obj)$/.test(req.params.key))throw httpError(404,'Модель не найдена.');
  const object=await storage.mesh(req.user.id,req.params.key);
  res.set('Content-Type',req.params.key.endsWith('.glb')?'model/gltf-binary':'text/plain; charset=utf-8');
  res.set('X-Content-Type-Options','nosniff');
  res.set('Cache-Control','no-store');
  if(object.ContentLength!==undefined)res.set('Content-Length',String(object.ContentLength));
  await pipeline(object.Body,res);
 });
 app.use((req,res)=>res.status(404).json({error:'Маршрут не найден.'}));
 app.use((error,req,res,next)=>{
  if(res.headersSent)return next(error);
  const status=error.status||500;
  if(status>=500)console.error(error);
  res.status(status).json({error:status>=500?'Хранилище недоступно. Повторите сохранение.':status===413?'Превышен размер запроса.':error.type==='entity.parse.failed'?'Некорректный JSON.':error.message});
 });return app;
}
