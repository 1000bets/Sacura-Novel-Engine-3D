import React,{useEffect,useRef,useState} from 'react';
import {UserPlus} from 'lucide-react';
import Dialog from './Dialog.jsx';
import {createInvite,copyInviteLink} from './invites.js';

export default function InviteDialog({onClose}){
 const [invite,setInvite]=useState(null),[busy,setBusy]=useState(true),[error,setError]=useState(''),[copied,setCopied]=useState(false);
 const pending=useRef(false),started=useRef(false);
 useEffect(()=>{if(!started.current){started.current=true;create();}},[]);
 async function create(){
  if(pending.current)return;
  pending.current=true;setBusy(true);setError('');
  try{setInvite(await createInvite());}catch(error){setError(error.message);}
  finally{pending.current=false;setBusy(false);}
 }
 async function copy(){
  setError('');setCopied(false);
  try{await copyInviteLink(invite.url);setCopied(true);}
  catch(error){setError(error.message||'Не удалось скопировать ссылку. Выделите и скопируйте её вручную.');}
 }
 return <Dialog title="Пригласить пользователя" icon={UserPlus} onClose={onClose} busy={busy} onSubmit={()=>{if(!invite)create();}}>
  <p>Одноразовая ссылка позволит создать аккаунт. Она действует 72 часа.</p>
  {invite&&<><label className="field"><span>Ссылка приглашения</span><input readOnly value={invite.url} onFocus={event=>event.target.select()}/></label><p>Действует до {new Date(invite.expiresAt).toLocaleString('ru-RU')}</p></>}
  {error&&<p className="project-dialog-error" role="alert">{error}</p>}
  {copied&&<p role="status">Ссылка скопирована.</p>}
  <footer><button type="button" disabled={busy} onClick={onClose}>Закрыть</button>{invite?<button type="button" className="primary" onClick={copy}>Копировать ссылку</button>:<button type="submit" className="primary" disabled={busy}>{busy?'Создаём…':'Создать приглашение'}</button>}</footer>
 </Dialog>;
}
