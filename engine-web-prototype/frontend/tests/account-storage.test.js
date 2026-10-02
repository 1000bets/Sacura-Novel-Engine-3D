import test from 'node:test';
import assert from 'node:assert/strict';
import {accountStorage,setStorageUser} from '../src/accountStorage.js';
import {backupProject,BACKUP_KEY,storeTemplate,readTemplates} from '../src/projectLifecycle.js';
test('project backups and templates remain separate when accounts switch in one browser',()=>{
 const previous=globalThis.localStorage,values=new Map();
 globalThis.localStorage={getItem:key=>values.get(key)??null,setItem:(key,value)=>values.set(key,value)};
 try{
  globalThis.localStorage.setItem(BACKUP_KEY,JSON.stringify({title:'Old shared backup'}));
  setStorageUser('alice');backupProject(accountStorage,{title:'Alice'});
  storeTemplate(accountStorage,{format:'sacura-project-template',version:1,name:'Alice template',project:{title:'Alice'}});
  setStorageUser('bob');assert.equal(accountStorage.getItem(BACKUP_KEY),null);assert.deepEqual(readTemplates(accountStorage),[]);
  backupProject(accountStorage,{title:'Bob'});
  setStorageUser('alice');assert.equal(JSON.parse(accountStorage.getItem(BACKUP_KEY)).title,'Alice');assert.equal(readTemplates(accountStorage)[0].name,'Alice template');
  setStorageUser('bob');assert.equal(JSON.parse(accountStorage.getItem(BACKUP_KEY)).title,'Bob');
  setStorageUser(null);assert.equal(JSON.parse(accountStorage.getItem(BACKUP_KEY)).title,'Old shared backup');
 }finally{setStorageUser(null);if(previous===undefined)delete globalThis.localStorage;else globalThis.localStorage=previous;}
});
