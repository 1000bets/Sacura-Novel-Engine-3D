export function registrationRoute(location){
 return {registration:location.pathname==='/register',token:new URLSearchParams(location.search).get('invite')||''};
}
export async function inviteRequest(path='',{body,fetcher=fetch}={}){
 const response=await fetcher('/api/invites'+path,{credentials:'same-origin',...(body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{})});
 const data=await response.json();
 if(!response.ok)throw Object.assign(new Error(data.error||'Не удалось выполнить запрос.'),{status:response.status});
 return data;
}
export function validateInvite(token){return inviteRequest('/validate',{body:{invite:token}});}
export function createInvite(){return inviteRequest('',{body:{}});}
export async function copyInviteLink(url){
 if(!globalThis.navigator?.clipboard?.writeText)throw new Error('Выделите и скопируйте ссылку вручную: буфер обмена недоступен.');
 await navigator.clipboard.writeText(url);
}
