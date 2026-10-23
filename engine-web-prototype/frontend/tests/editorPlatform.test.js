import test from 'node:test';
import assert from 'node:assert/strict';
import {createDesktopEditorAdapter,createWebEditorAdapter} from '../src/editorPlatform.js';

function fixture(){
 let listener,context={session:'first',epoch:1,revision:1};
 const state={selection:null,play:'stopped'},objects=[{id:'one',name:'One'},{id:'two',name:'Two'}];
 const counts={};let intercept;
 const client={get context(){return context;},connect:async()=>({connected:true,context}),subscribe:fn=>{listener=fn;return()=>{};},dispose(){},
  async request(method){counts[method]=(counts[method]||0)+1;if(intercept){const result=intercept(method);if(result)return result;}
   const value={'scene.getHierarchy':objects,'bridge.describe':state,'reflection.getComponentTypes':[{typeId:context.session}],'assets.list':[{id:context.session}],'story.get':{open:false}}[method];
   return {ok:true,context:{...context},value:structuredClone(value)};
  }};
 return {client,counts,objects,emit:event=>listener(event),replace(){context={session:'second',epoch:1,revision:1};listener({event:'state.changed',context});},intercept:fn=>intercept=fn};
}

test('desktop snapshots retain unaffected object references and read domains only on invalidation',async()=>{
 const f=fixture(),adapter=createDesktopEditorAdapter(f.client);await adapter.start();const initial=adapter.getSnapshot();
 f.objects[0].name='Changed in C++';await adapter.refresh();const updated=adapter.getSnapshot();
 assert.notEqual(updated.objects[0],initial.objects[0]);assert.equal(updated.objects[1],initial.objects[1]);
 assert.equal(updated.assets,initial.assets);assert.equal(f.counts['assets.list'],1);
 adapter.dispose();
});

test('replacement clears project reflection and rejects an old asynchronous domain result',async()=>{
 const f=fixture(),adapter=createDesktopEditorAdapter(f.client);await adapter.start();let release;
 f.intercept(method=>{if(method!=='assets.list')return;f.intercept(null);return new Promise(resolve=>release=resolve);});
 const oldRead=adapter.loadDomain('assets.list','assets');
 f.emit({event:'disconnected',message:'Closed',context:f.client.context});assert.deepEqual(adapter.getSnapshot().types,[]);
 f.replace();await adapter.refresh();
 release({ok:true,context:{session:'first',epoch:1,revision:1},value:[{id:'stale'}]});
 await assert.rejects(oldRead,{code:'STALE_SCENE'});
 assert.equal(adapter.getSnapshot().assets[0].id,'second');assert.equal(adapter.getSnapshot().types[0].typeId,'second');adapter.dispose();
});

test('web data adapter delegates to existing presentation actions without a native dependency',()=>{
 let selected;const files=[{id:'web-asset'}],adapter=createWebEditorAdapter({files,select:id=>selected=id});
 adapter.select('web-object');assert.equal(selected,'web-object');assert.equal(adapter.files,files);assert.equal(adapter.kind,'web');
 assert.equal(createDesktopEditorAdapter(null),null);
});

test('read racing a native epoch change refreshes without a stale error banner',async()=>{
 const f=fixture();let once=true;
 f.intercept(method=>{if(method==='scene.getHierarchy'&&once){once=false;return {ok:false,context:f.client.context,error:{code:'STALE_SCENE',message:'Scene changed'}};}});
 const adapter=createDesktopEditorAdapter(f.client);await adapter.start();
 assert.equal(adapter.getSnapshot().error,null);assert.equal(adapter.getSnapshot().objects.length,2);
 assert.equal(f.counts['scene.getHierarchy'],2);adapter.dispose();
});
