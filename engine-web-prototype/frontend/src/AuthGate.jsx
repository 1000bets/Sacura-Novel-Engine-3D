import LanguagePicker from './LanguagePicker.jsx';
import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useEffect,useState} from 'react';
import ProjectHome from './ProjectHome.jsx';
import {resetServerStorage,flushServerSaves,saveServerProject,serverStorageEnabled} from './serverStorage.js';
import {setStorageUser} from './accountStorage.js';
import './auth.css';
async function authRequest(path,body){
 const response=await fetch('/api/auth'+path,{credentials:'same-origin',...(body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{})});
 const data=await response.json();
 if(!response.ok)throw Object.assign(new Error(data.error||'Не удалось выполнить запрос.'),{status:response.status});
 return data;
}
export default function AuthGate(){
 useLocale();
 const [state,setState]=useState({loading:true}),[mode,setMode]=useState('login'),[login,setLogin]=useState(''),[password,setPassword]=useState(''),[busy,setBusy]=useState(false),[error,setError]=useState('');
 function open(user){
  setStorageUser(user.id);resetServerStorage();
  setState({user});
 }
 async function check(){
  setState({loading:true});setError('');
  try{const {user}=await authRequest('');open(user);}
  catch(error){if(error.status===401){setStorageUser(null);setState({});}else setState({failed:error.message});}
 }
 useEffect(()=>{if(serverStorageEnabled)check();else setState({local:true});},[]);
 async function submit(event){
  event.preventDefault();setBusy(true);setError('');
  try{const {user}=await authRequest('/'+mode,{login,password});setPassword('');open(user);}
  catch(error){setError(error.message);}
  finally{setBusy(false);}
 }
 async function logout(project){
  const previous=state;
  try{if(project)await saveServerProject(project);await flushServerSaves();await authRequest('/logout',{});setStorageUser(null);resetServerStorage();setState({});setMode('login');setError('');setPassword('');}
  catch(error){setState({...previous,logoutError:error.message});}
 }
 if(state.local)return <ProjectHome/>;
 if(state.user)return <>{state.logoutError&&<p className="auth-logout-error" role="alert">{tr("Не удалось выйти: ")}{message(state.logoutError)}</p>}<ProjectHome user={state.user} onLogout={logout}/></>;
 if(state.loading)return <main className="auth-page"><div className="auth-language"><LanguagePicker/></div><p role="status">{tr("Загрузка…")}</p></main>;
 if(state.failed)return <main className="auth-page"><div className="auth-language"><LanguagePicker/></div><section className="auth-card"><p role="alert">{message(state.failed)}</p><button onClick={check}>{tr("Повторить")}</button></section></main>;
 return <main className="auth-page"><div className="auth-language"><LanguagePicker/></div><section className="auth-card">
  <div className="auth-brand">Sacura Novel Studio</div>
  <h1>{mode==='login'?tr("Вход"):tr("Регистрация")}</h1>
  <p>{tr("Войдите, чтобы работать со своими проектами.")}</p>
  <form onSubmit={submit}>
   <label htmlFor="auth-login">{tr("Логин")}</label><input id="auth-login" name="username" autoComplete="username" autoFocus required value={login} onChange={event=>setLogin(event.target.value)} disabled={busy}/>
   <label htmlFor="auth-password">{tr("Пароль")}</label><input id="auth-password" name="password" type="password" autoComplete={mode==='login'?'current-password':'new-password'} required value={password} onChange={event=>setPassword(event.target.value)} disabled={busy}/>
   {error&&<p className="auth-error" role="alert">{message(error)}</p>}
   <button className="auth-submit" disabled={busy}>{busy?tr("Загрузка…"):mode==='login'?tr("Войти"):tr("Зарегистрироваться")}</button>
  </form>
  <button className="auth-switch" disabled={busy} onClick={()=>{setMode(mode==='login'?'register':'login');setError('');}}>{mode==='login'?tr("Создать аккаунт"):tr("Уже есть аккаунт? Войти")}</button>
 </section></main>;
}
