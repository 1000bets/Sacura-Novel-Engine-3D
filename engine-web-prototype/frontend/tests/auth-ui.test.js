import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,waitFor,cleanup}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),define:{'import.meta.env.VITE_SERVER_STORAGE':JSON.stringify('true')},server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:AuthGate}=await server.ssrLoadModule('/src/AuthGate.jsx');
const {default:ProjectHome}=await server.ssrLoadModule('/src/ProjectHome.jsx');
const {default:ProjectDialog}=await server.ssrLoadModule('/src/ProjectDialog.jsx');
const token='a'.repeat(64),url='http://localhost/register?invite='+token;
const response=(data,status=200)=>({ok:status<400,status,json:async()=>data});
let calls=[];
function mock({user=null,validation={expiresAt:'2026-10-04T10:00:00Z'},validationStatus=200,registrationStatus=201}={}){
 calls=[];
 globalThis.fetch=async(path,options={})=>{
  calls.push({path,options});
  if(path==='/api/auth')return user?response({user}):response({error:'Войдите в аккаунт.'},401);
  if(path.startsWith('/api/invites/validate'))return response(validation,validationStatus);
  if(path==='/api/auth/register')return registrationStatus===201?response({user:{id:'new',login:'new'}},201):response({error:'Приглашение уже использовано.'},registrationStatus);
  if(path==='/api/invites')return response({url,expiresAt:'2026-10-04T10:00:00Z'},201);
  if(path==='/api/projects')return response([]);
  throw new Error('Unexpected request '+path);
 };
}
afterEach(()=>{cleanup();localStorage.clear();window.history.replaceState(null,'','/');});
after(async()=>{await server.close();dom.window.close();});
test('public login has no register button or signup link',async()=>{
 mock();render(React.createElement(AuthGate));await screen.findByRole('button',{name:'Войти'});
 assert.equal(screen.queryByText('Создать аккаунт'),null);assert.equal(screen.queryByRole('button',{name:'Зарегистрироваться'}),null);assert.equal(screen.queryByRole('link'),null);
});
test('/register without token hides working form and offers login',async()=>{
 window.history.replaceState(null,'','/register');mock();render(React.createElement(AuthGate));
 assert.match((await screen.findByRole('alert')).textContent,/только по приглашению/);assert.equal(screen.queryByLabelText('Пароль'),null);
 assert.equal(screen.getByRole('link',{name:'Вернуться к входу'}).getAttribute('href'),'/');assert.equal(calls.filter(c=>c.path.includes('/validate')).length,0);
});
test('valid invite reveals form, submits token and preserves auto login',async()=>{
 window.history.replaceState(null,'',url);mock();render(React.createElement(AuthGate));
 fireEvent.change(await screen.findByLabelText('Логин'),{target:{value:'new'}});fireEvent.change(screen.getByLabelText('Пароль'),{target:{value:'secret'}});
 fireEvent.click(screen.getByRole('button',{name:'Зарегистрироваться'}));await screen.findByRole('button',{name:'Пригласить пользователя'});
 const request=calls.find(c=>c.path==='/api/auth/register');assert.deepEqual(JSON.parse(request.options.body),{login:'new',password:'secret',invite:token});assert.equal(window.location.pathname,'/');assert.equal(window.location.search,'');
});
for(const message of ['Приглашение не найдено.','Срок приглашения истёк.','Приглашение уже использовано.'])test(message+' hides registration',async()=>{
 window.history.replaceState(null,'',url);mock({validation:{error:message},validationStatus:403});render(React.createElement(AuthGate));
 assert.equal((await screen.findByRole('alert')).textContent,message);assert.equal(screen.queryByLabelText('Пароль'),null);
});
test('invite consumed after validation is rejected and form disappears',async()=>{
 window.history.replaceState(null,'',url);mock({registrationStatus:403});render(React.createElement(AuthGate));
 fireEvent.change(await screen.findByLabelText('Логин'),{target:{value:'new'}});fireEvent.change(screen.getByLabelText('Пароль'),{target:{value:'secret'}});fireEvent.click(screen.getByRole('button',{name:'Зарегистрироваться'}));
 await screen.findByRole('alert');assert.equal(screen.queryByLabelText('Пароль'),null);
});
test('authenticated invite visitor sees existing account message without validation or registration',async()=>{
 window.history.replaceState(null,'',url);mock({user:{id:'existing',login:'alice'}});render(React.createElement(AuthGate));
 await screen.findByRole('heading',{name:'Вы уже вошли'});assert.equal(screen.queryByLabelText('Пароль'),null);assert.equal(calls.filter(c=>c.path.includes('/invites')).length,0);
 fireEvent.click(screen.getByRole('button',{name:'Перейти в главное меню'}));await screen.findByRole('button',{name:'Пригласить пользователя'});
});
test('main menu creates invite, copies URL, traps focus and restores trigger on Escape',async()=>{
 mock();const copied=[];Object.defineProperty(navigator,'clipboard',{configurable:true,value:{writeText:async value=>copied.push(value)}});
 render(React.createElement(React.StrictMode,null,React.createElement(ProjectHome,{user:{id:'alice',login:'alice'}})));
 const trigger=screen.getByRole('button',{name:'Пригласить пользователя'});trigger.focus();fireEvent.click(trigger);
 await screen.findByRole('dialog');
 const field=await screen.findByLabelText('Ссылка приглашения');assert.equal(field.value,url);assert.equal(field.readOnly,true);assert.ok(screen.getByText(/Действует до/));
 assert.equal(calls.filter(c=>c.path==='/api/invites'&&c.options.method==='POST').length,1);
 fireEvent.click(screen.getByRole('button',{name:'Копировать ссылку'}));await screen.findByRole('status');assert.deepEqual(copied,[url]);
 
 const last=screen.getByRole('button',{name:'Копировать ссылку'});last.focus();fireEvent.keyDown(last,{key:'Tab'});assert.ok(document.activeElement===screen.getAllByRole('button',{name:'Закрыть',exact:true})[0],document.activeElement.outerHTML);
 fireEvent.keyDown(screen.getByRole('dialog'),{key:'Escape'});await waitFor(()=>assert.equal(screen.queryByRole('dialog'),null));assert.ok(document.activeElement===trigger,document.activeElement.outerHTML);
});
test('invite creation and clipboard failures are actionable',async()=>{
 mock();const original=fetch;globalThis.fetch=(path,options)=>path==='/api/invites'?Promise.resolve(response({error:'Слишком много попыток.'},429)):original(path,options);
 render(React.createElement(ProjectHome,{user:{id:'alice',login:'alice'}}));fireEvent.click(screen.getByRole('button',{name:'Пригласить пользователя'}));
 assert.equal((await screen.findByRole('alert')).textContent,'Слишком много попыток.');globalThis.fetch=original;fireEvent.click(screen.getByRole('button',{name:'Создать приглашение'}));await screen.findByLabelText('Ссылка приглашения');
 Object.defineProperty(navigator,'clipboard',{configurable:true,value:undefined});fireEvent.click(screen.getByRole('button',{name:'Копировать ссылку'}));assert.match((await screen.findByRole('alert')).textContent,/вручную/);
});
test('shared dialog preserves project creation fields and submission',async()=>{
 let result;render(React.createElement(ProjectDialog,{mode:'new',templates:[],onClose:()=>{},onSubmit:value=>{result=value;}}));
 fireEvent.change(screen.getByLabelText('Название проекта'),{target:{value:'Story'}});fireEvent.click(screen.getByRole('button',{name:'Создать проект'}));assert.deepEqual(result,{name:'Story',templateId:''});
});
