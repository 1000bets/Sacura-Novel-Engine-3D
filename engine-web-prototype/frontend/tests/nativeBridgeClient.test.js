import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createNativeBridgeClient} from '../src/nativeBridgeClient.js';

test('web has no native bridge, even if an unmarked bridge field is present',()=>{
  globalThis.window={};assert.equal(createNativeBridgeClient(),null);
  window.sacuraDesktop={bridge:{}};assert.equal(createNativeBridgeClient(),null);delete globalThis.window;
});
test('late replies and connect snapshots cannot resurrect an old process',async()=>{
  let event,connectResolve,requestResolve,unsubscribed=false;
  globalThis.window={sacuraDesktop:{isDesktop:true,bridge:{subscribe(cb){event=cb;return()=>{unsubscribed=true;};},connect(){return new Promise(r=>connectResolve=r);},request(){return new Promise(r=>requestResolve=r);}}}};
  const client=createNativeBridgeClient(),connecting=client.connect(),old={session:'old',epoch:1,revision:1},next={session:'new',epoch:1,revision:1};
  event({event:'state.changed',context:old});const pending=client.request('bridge.describe',{});
  event({event:'disconnected',context:old});event({event:'state.changed',context:next});
  connectResolve({connected:true,context:old});await connecting;
  requestResolve({ok:true,context:old,value:{}});await pending;
  assert.deepEqual(client.context,next);
  assert.equal((await client.request('object.destroy',{objectId:'same-id'},old)).error.code,'STALE_SCENE');
  event({event:'disconnected',context:old});assert.deepEqual(client.context,next);
  client.dispose();assert.ok(unsubscribed);assert.equal((await client.request('bridge.describe',{})).error.code,'DISCONNECTED');delete globalThis.window;
});
