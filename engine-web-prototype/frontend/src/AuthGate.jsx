import React,{useEffect,useState} from 'react';
import {registrationRoute,validateInvite} from './invites.js';
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
 const [state,setState]=useState({loading:true}),[route,setRoute]=useState(()=>registrationRoute(window.location)),[invite,setInvite]=useState({loading:true}),[login,setLogin]=useState(''),[password,setPassword]=useState(''),[busy,setBusy]=useState(false),[error,setError]=useState('');
 const mode=route.registration?'register':'login';
 function leaveInvite(){window.history.replaceState(null,'','/');setRoute({registration:false,token:''});}
 function open(user){
  setStorageUser(user.id);resetServerStorage();
  setState({user});
 }
 async function check(){
  setState({loading:true});setError('');
  try{const {user}=await authRequest('');open(user);}
  catch(error){if(error.status===401){setStorageUser(null);setState({});}else setState({failed:error.message});}
 }
 useEffect(()=>{
  if(!route.registration||state.loading||state.user)return;
  let cancelled=false;
  if(!route.token){setInvite({error:'Регистрация доступна только по приглашению. Попросите ссылку у пользователя Sacura.'});return;}
  setInvite({loading:true});
  validateInvite(route.token).then(data=>{if(!cancelled)setInvite(data);}).catch(error=>{if(!cancelled)setInvite({error:error.message});});
  return()=>{cancelled=true;};
 },[route.registration,route.token,state.loading,state.user]);
 useEffect(()=>{if(serverStorageEnabled)check();else setState({local:true});},[]);
 async function submit(event){
  event.preventDefault();if(route.registration&&(invite.loading||invite.error))return;setBusy(true);setError('');
  try{const {user}=await authRequest('/'+mode,{login,password,...(route.registration?{invite:route.token}:{})});setPassword('');if(route.registration)leaveInvite();open(user);}
  catch(error){setError(error.message);if(route.registration&&error.status===403)setInvite({error:error.message});}
  finally{setBusy(false);}
 }
 async function logout(project){
  const previous=state;
  try{if(project)await saveServerProject(project);await flushServerSaves();await authRequest('/logout',{});setStorageUser(null);resetServerStorage();setState({});leaveInvite();setError('');setPassword('');}
  catch(error){setState({...previous,logoutError:error.message});}
 }
 if(state.local)return <ProjectHome/>;
 if(state.user&&route.registration)return <main className="auth-page"><section className="auth-card"><h1>Вы уже вошли</h1><p>Вы вошли как {state.user.login}. Приглашение не использовано.</p><button className="auth-submit" onClick={leaveInvite}>Перейти в главное меню</button></section></main>;
 if(state.user)return <>{state.logoutError&&<p className="auth-logout-error" role="alert">Не удалось выйти: {state.logoutError}</p>}<ProjectHome user={state.user} onLogout={logout}/></>;
 if(state.loading)return <main className="auth-page"><p role="status">Загрузка…</p></main>;
 if(route.registration&&!state.failed&&(invite.loading||invite.error))return <main className="auth-page"><section className="auth-card"><div className="auth-brand">Sacura Novel Studio</div><h1>Регистрация по приглашению</h1>{invite.loading?<p role="status">Проверяем приглашение…</p>:<p role="alert">{invite.error}</p>}<a className="auth-switch" href="/">Вернуться к входу</a></section></main>;
 if(state.failed)return <main className="auth-page"><section className="auth-card"><p role="alert">{state.failed}</p><button onClick={check}>Повторить</button></section></main>;
 return <main className="auth-page"><section className="auth-card">
  <div className="auth-brand">Sacura Novel Studio</div>
  <h1>{mode==='login'?'Вход':'Регистрация'}</h1>
  <p>{route.registration?'Создайте аккаунт по приглашению. После регистрации вы сразу войдёте.':'Войдите, чтобы работать со своими проектами.'}</p>
  <form onSubmit={submit}>
   <label htmlFor="auth-login">Логин</label><input id="auth-login" name="username" autoComplete="username" autoFocus required value={login} onChange={event=>setLogin(event.target.value)} disabled={busy}/>
   <label htmlFor="auth-password">Пароль</label><input id="auth-password" name="password" type="password" autoComplete={mode==='login'?'current-password':'new-password'} required value={password} onChange={event=>setPassword(event.target.value)} disabled={busy}/>
   {error&&<p className="auth-error" role="alert">{error}</p>}
   <button className="auth-submit" disabled={busy}>{busy?'Загрузка…':mode==='login'?'Войти':'Зарегистрироваться'}</button>
  </form>
  {route.registration&&<a className="auth-switch" href="/">Уже есть аккаунт? Войти</a>}
 </section></main>;
}
