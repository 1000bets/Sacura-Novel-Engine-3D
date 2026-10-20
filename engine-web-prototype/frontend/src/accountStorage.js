let userId=null;
export function setStorageUser(id){userId=id;}
export function storageKey(key){return userId?`sacura-user:${userId}:${key}`:key;}
export const accountStorage={
 getItem(key){return localStorage.getItem(storageKey(key));},
 setItem(key,value){localStorage.setItem(storageKey(key),value);},
};
export function currentStorageUser(){return userId;}
