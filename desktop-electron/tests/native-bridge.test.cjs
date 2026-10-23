const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const os = require('node:os');
const {spawn} = require('node:child_process');
const {BridgeTransport,validate} = require('../bridge/transport.cjs');
const {performance} = require('node:perf_hooks');
const root=path.resolve(__dirname,'../..');
const host=process.env.SACURA_NATIVE_HOST||path.join(root,'build/windows-msvc-debug/ElectronNative/Debug/SakuraElectronHost.exe');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
async function until(predicate,timeout=30000){const end=Date.now()+timeout;while(!predicate()){if(Date.now()>end)throw Error('Timed out');await delay(20);}}
async function launch(project){
  const bridge=await BridgeTransport.listen();const events=[];bridge.on('event',e=>events.push(e));
  const child=spawn(host,['--bridge-headless',project,bridge.path],{windowsHide:true,stdio:['pipe','pipe','pipe']});
  let output='';child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
  let exited=false;const exit=new Promise(resolve=>child.once('exit',code=>{exited=true;resolve(code);}));
  try{await until(()=>bridge.connected||exited);assert.ok(bridge.connected,output);}catch(e){child.kill();bridge.close();throw e;}
  return {bridge,events,child,output:()=>output,async close(){
    if(!exited)child.stdin.end('{"type":"shutdown"}\n');
    const watchdog=setTimeout(()=>child.kill(),10000);const code=await exit;clearTimeout(watchdog);bridge.close();assert.equal(code,0,output);
  }};
}
test('contract rejects extra fields, unsafe values, methods and malformed transforms',()=>{
  for(const [m,p] of [['eval',{}],['object.destroy',{objectId:'x',pointer:42}],['object.rename',{objectId:'x',name:'\0'}],['component.setProperty',{objectId:'x',componentId:'c',property:'p',value:Infinity}],['transform.set',{objectId:'x',transform:{position:[1],rotation:[],scale:[]}}]])assert.throws(()=>validate(m,p));
  validate('object.create',{name:'日本語 объект'});
});
test('real Engine / EditorCore bridge, errors, events and restart', {skip:process.platform!=='win32',timeout:120000}, async t=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-bridge-test-'));
  await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});
  const project=path.join(dir,'SampleProject.project');let app;const timings=[];
  try{
    app=await launch(project);let bridge=app.bridge;
    const call=async(method,params={})=>{const start=performance.now();const r=await bridge.request(method,params);timings.push(performance.now()-start);assert.equal(r.ok,true,method+': '+JSON.stringify(r));return r.value;};
    const fail=async(method,params,code,context)=>{const r=await bridge.request(method,params,context);assert.equal(r.ok,false,JSON.stringify(r));assert.equal(r.error.code,code);};
    const initial=await call('scene.getHierarchy');assert.ok(initial.length>0);
    assert.ok((await call('reflection.getComponentTypes')).some(c=>c.typeId==='engine.LightComponent'));
    assert.ok((await call('assets.getScenes')).length>0);
    const {id}=await call('object.create',{name:'Bridge object'});
    const transform={position:[2,3,4],rotation:[0,0,0,1],scale:[1,2,1]};
    await call('transform.set',{objectId:id,transform});
    assert.deepEqual(await call('transform.get',{objectId:id}),transform);
    await call('editor.undo');assert.deepEqual((await call('transform.get',{objectId:id})).position,[0,0,0]);
    await call('editor.redo');assert.deepEqual(await call('transform.get',{objectId:id}),transform);
    await call('object.rename',{objectId:id,name:'Renamed native'});
    const parent=await call('object.create',{name:'Parent'});
    await call('object.setParent',{objectId:id,parentId:parent.id});
    await fail('object.setParent',{objectId:parent.id,parentId:id},'INVALID_ARGUMENT');
    assert.equal((await call('scene.getHierarchy')).find(o=>o.id===id).parentId,parent.id);
    assert.equal((await call('selection.set',{objectId:id})).selection,id);
    const component=await call('object.addComponent',{objectId:id,typeId:'engine.LightComponent'});
    assert.ok((await call('object.getComponents',{objectId:id})).some(c=>c.id===component.id));
    const cp={objectId:id,componentId:component.id};
    const props=await call('component.getProperties',cp);
    assert.ok(props.some(p=>p.id==='light_type'&&p.typeId==='engine.int64'&&typeof p.value==='string'));
    await call('component.setProperty',{...cp,property:'light_type',value:'1'});
    assert.equal((await call('component.getProperties',cp)).find(p=>p.id==='light_type').value,'1');
    await fail('component.setProperty',{...cp,property:'light_type',value:'9223372036854775808'},'INVALID_ARGUMENT');
    await fail('component.setProperty',{...cp,property:'missing',value:1},'PROPERTY_READ_ONLY');
    await fail('component.getProperties',{...cp,objectId:parent.id},'NOT_FOUND');
    await call('object.removeComponent',cp);await fail('component.getProperties',cp,'NOT_FOUND');
    await call('editor.undo');assert.ok((await call('object.getComponents',{objectId:id})).some(c=>c.id===component.id));
    // Older commands must resolve the restored component through its persistent ID.
    await call('editor.undo');assert.equal((await call('component.getProperties',cp)).find(p=>p.id==='light_type').value,'0');
    await call('editor.redo');
    await call('object.destroy',{objectId:id});await fail('transform.get',{objectId:id},'NOT_FOUND');
    assert.equal((await call('bridge.describe')).selection,null);
    await call('editor.undo');assert.deepEqual(await call('transform.get',{objectId:id}),transform);
    await call('editor.undo'); // Property undo across subtree reconstruction.
    await call('editor.redo');
    await fail('scene.open',{path:'Scenes/Main.scene'},'UNSAVED_CHANGES');
    await fail('scene.save',{path:'../outside.scene'},'INVALID_PATH');
    await fail('scene.save',{path:'C:/outside.scene'},'INVALID_PATH');
    await call('scene.save',{path:'Scenes/Bridge.scene'});assert.equal((await call('bridge.describe')).dirty,false);
    const oldContext={...bridge.context};
    await fail('scene.open',{path:'Scenes/missing.scene'},'IO_ERROR');
    assert.ok((await call('scene.getHierarchy')).some(o=>o.id===id));
    await call('scene.open',{path:'Scenes/Bridge.scene'});
    await fail('object.destroy',{objectId:id},'STALE_SCENE',oldContext);
    await call('play.start');assert.equal((await call('bridge.describe')).play,'playing');
    await fail('object.create',{name:'Forbidden'},'PLAY_READ_ONLY');
    await call('play.pause');assert.equal((await call('bridge.describe')).play,'paused');
    await call('play.start');await call('play.stop');assert.equal((await call('bridge.describe')).play,'stopped');
    // An Engine-side edit, outside the Bridge command dispatcher, emits an invalidation.
    const eventCount=app.events.length;
    app.child.stdin.write('{"type":"animate","value":true}\n');
    await until(()=>app.events.length>eventCount);app.child.stdin.write('{"type":"animate","value":false}\n');
    assert.equal(app.events.at(-1).event,'state.changed');
    // Send malformed parameters directly, bypassing the JS validator.
    const badId='native-validation';const bad={type:'bridge.command',version:1,id:badId,session:bridge.context.session,epoch:bridge.context.epoch,method:'object.destroy',params:{objectId:17}};
    const body=Buffer.from(JSON.stringify(bad)),packet=Buffer.alloc(body.length+4);packet.writeUInt32LE(body.length);body.copy(packet,4);
    const raw=new Promise(resolve=>bridge.pending.set(badId,{resolve,timer:setTimeout(()=>resolve(null),3000)}));bridge.socket.write(packet);
    assert.equal((await raw)?.error.code,'INVALID_ARGUMENT');
    const previousSession={...bridge.context};await app.close();
    assert.equal((await bridge.request('scene.getHierarchy')).error.code,'DISCONNECTED');
    app=await launch(project);bridge=app.bridge;
    assert.notEqual(bridge.context.session,previousSession.session);
    await call('scene.open',{path:'Scenes/Bridge.scene'});
    assert.ok((await call('scene.getHierarchy')).some(o=>o.id===id&&o.name==='Renamed native'));
    assert.deepEqual(await call('transform.get',{objectId:id}),transform);
    await fail('object.destroy',{objectId:id},'STALE_SESSION',previousSession);
    await call('object.destroy',{objectId:parent.id});await fail('transform.get',{objectId:id},'NOT_FOUND');
    const sorted=timings.sort((a,b)=>a-b);t.diagnostic(JSON.stringify({commands:timings.length,roundTripMs:{p50:sorted[Math.floor(sorted.length*.5)],p95:sorted[Math.floor(sorted.length*.95)],max:sorted.at(-1)}}));
  }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
});
test('panel domains: actual AssetRegistry, StoryGraphDocument and PlaySession', {skip:process.platform!=='win32',timeout:120000},async t=>{
 const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-panels-test-'));await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});let app;
 try{
  app=await launch(path.join(dir,'SampleProject.project'));const call=async(m,p={})=>{const r=await app.bridge.request(m,p);assert.equal(r.ok,true,m+': '+JSON.stringify(r));return r.value;};
  let assets,createdStory,line,end,eventId,binding;
  await t.test('Content Browser lists real assets and imports a texture without touching source',async()=>{
   assets=await call('assets.list');assert.ok(assets.some(a=>a.typeId==='Story'));assert.ok(assets.some(a=>a.builtin));
   const source=path.join(dir,'source.png');await fs.writeFile(source,Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==','base64'));
   const imported=await call('assets.import',{sourcePath:source,destination:'Textures/BridgeImport.png'});
   assert.ok((await call('assets.list')).some(a=>a.id===imported.id&&a.typeId==='Texture'));
   const conflict=await app.bridge.request('assets.import',{sourcePath:source,destination:'Textures/BridgeImport.png'});assert.equal(conflict.ok,false);
   const traversal=await app.bridge.request('assets.import',{sourcePath:source,destination:'../outside.png'});assert.equal(traversal.error.code,'INVALID_PATH');
   const before=app.events.length;await fs.copyFile(source,path.join(dir,'Content/Textures/External.png'));await until(()=>app.events.slice(before).some(e=>e.changes?.assets));
   assert.ok((await call('assets.list')).some(a=>a.relativePath==='Textures/External.png'));
   const oldVersion=(await call('assets.list')).find(a=>a.id===imported.id).version;
   const changed=path.join(dir,'Content/Textures/BridgeImport.png'),stamp=new Date(Date.now()+2000);await fs.utimes(changed,stamp,stamp);
   assert.notEqual((await call('assets.list')).find(a=>a.id===imported.id).version,oldVersion,'same-size external updates invalidate thumbnail URLs');
  });
  await t.test('Story Graph creates/edits/connects/moves nodes through native history',async()=>{
   createdStory=await call('story.create',{path:'Stories/Bridge.story'});assert.equal(createdStory.open,true);
   assert.equal(createdStory.document.nodes.length,1);
   assert.equal(createdStory.document.nodes[0].kind,'Line');
   assert.equal(createdStory.document.startNodeId,createdStory.document.nodes[0].id);
   assert.equal(createdStory.dirty,false);
   assert.equal((await call('story.undo')).document.nodes.length,0);
   assert.equal((await call('story.redo')).document.nodes[0].id,createdStory.document.startNodeId);
   let s=await call('story.addNode',{kind:'Line',x:10,y:20});line=s.document.nodes.at(-1).id;
   s=await call('story.addNode',{kind:'End',x:400,y:20});end=s.document.nodes.find(n=>n.kind==='End').id;
   await call('story.updateNode',{nodeId:line,patch:{speaker:'Native speaker',text:'Written by React adapter'}});
   await call('story.connect',{sourceId:line,port:-1,targetId:end});
   s=await call('story.moveNodes',{positions:{[line]:{x:120,y:80}}});assert.equal(s.positions[line].x,120);
   await call('story.undo');s=await call('story.get');assert.equal(s.positions[line].x,10);await call('story.redo');
   const invalid=await app.bridge.request('story.connect',{sourceId:end,port:-1,targetId:line});assert.equal(invalid.ok,false);
   await call('story.setStart',{nodeId:line});
   s=await call('story.addNode',{kind:'Choice',x:200,y:400});const choice=s.document.nodes.find(n=>n.kind==='Choice');
   s=await call('story.updateNode',{nodeId:choice.id,patch:{prompt:'Choose a native branch',choices:[...choice.choices,{label:'New answer',nextNodeId:end}]}});
   const added=s.document.nodes.find(n=>n.id===choice.id).choices.at(-1);assert.ok(added.id,'EditorCore assigns persistent choice ID');
   await call('story.connect',{sourceId:choice.id,port:0,targetId:end});
   s=await call('story.addNode',{kind:'Action',x:500,y:400});const action=s.document.nodes.find(n=>n.kind==='Action');
   s=await call('story.updateNode',{nodeId:action.id,patch:{actions:[{kind:'Wait',durationSeconds:.1}]}});
   assert.ok(Math.abs(s.document.nodes.find(n=>n.id===action.id).actions[0].durationSeconds-.1)<1e-6);
  });
  await t.test('Events edits, placement, native Undo/Redo and save/reopen',async()=>{
   for(const actionKind of ['Wait','MoveTo','CameraCut','NavigateTo']){
    const before=await call('story.get');const added=await call('story.addEvent',{nodeId:line,phase:'BEFORE',actionKind});
    const created=added.document.events.find(e=>!(before.document.events||[]).some(old=>old.id===e.id));assert.equal(created.groups[0].actions[0].kind,actionKind);
    await call('story.undo');assert.deepEqual((await call('story.get')).document,before.document);await call('story.redo');
    const reused=await call('story.addEvent',{nodeId:line,phase:'AFTER',eventId:created.id});assert.equal(reused.document.events.length,added.document.events.length);
    assert.ok(reused.document.nodes.find(n=>n.id===line).bindings.some(b=>b.eventId===created.id&&b.hook==='AFTER'));
    await call('story.undo');await call('story.undo');
   }
   assert.equal((await app.bridge.request('story.addEvent',{nodeId:line,phase:'BEFORE',actionKind:'eval'})).error.code,'INVALID_ARGUMENT');
   assert.equal((await app.bridge.request('story.addEvent',{nodeId:line,phase:'BEFORE',eventId:'missing'})).error.code,'STORY_ERROR');
   let s=await call('story.addEvent',{nodeId:line,phase:'BEFORE'});const n=s.document.nodes.find(n=>n.id===line);binding=n.bindings[0].id;eventId=n.bindings[0].eventId;
   const event=s.document.events.find(e=>e.id===eventId);event.name='Bridge native event';event.groups[0].actions[0].durationSeconds=.125;
   await call('story.updateEvent',{eventId,event});
   s=await call('story.placeEvent',{nodeId:line,bindingId:binding,phase:'ON_START',join:'STARTED'});assert.equal(s.document.nodes.find(n=>n.id===line).bindings[0].hook,'ON_START');
   await call('story.undo');s=await call('story.get');assert.equal(s.document.nodes.find(n=>n.id===line).bindings[0].hook,'BEFORE');await call('story.redo');
   const batch=s.document.nodes.find(n=>n.id===line).batches.BEFORE?.[0];
   await call('story.save');s=await call('story.get');assert.equal(s.dirty,false);
   assets=await call('assets.list');const id=assets.find(a=>a.relativePath==='Stories/Bridge.story').id;
   await call('story.open',{assetId:id});s=await call('story.get');assert.equal(s.document.events.find(e=>e.id===eventId).name,'Bridge native event');
   await call('story.deleteNode',{nodeId:end});await call('story.undo');s=await call('story.get');assert.ok(s.document.nodes.some(n=>n.id===end));
  });
  await t.test('Play/Pause/Step/Stop uses PlayWorld, editing is rejected during simulation',async()=>{
   await call('play.start');assert.equal((await call('bridge.describe')).play,'playing');await call('play.pause');await call('play.step');assert.equal((await call('bridge.describe')).play,'paused');
   assert.equal((await app.bridge.request('story.addNode',{kind:'Line',x:0,y:0})).error.code,'PLAY_READ_ONLY');
   await call('play.stop');assert.equal((await call('bridge.describe')).play,'stopped');assert.equal((await app.bridge.request('play.step')).error.code,'INVALID_STATE');
  });
 }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
});

test('native project creation, external conflicts, version errors and saved drafts',{skip:process.platform!=='win32',timeout:120000},async()=>{
 const {projectCli}=require('../native-project.cjs');
 const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-project-compat-'));let app;
 try{
  const created=await projectCli(host,['--project-create',dir,'Portable']);
  await projectCli(host,['--project-check',created.path]);
  for(const folder of ['Content','Scripts','Config'])assert.ok((await fs.stat(path.join(dir,'Portable',folder))).isDirectory());
  await assert.rejects(()=>projectCli(host,['--project-create',dir,'../Invalid']));
  app=await launch(created.path);const call=async(m,p={})=>{const r=await app.bridge.request(m,p);assert.equal(r.ok,true,JSON.stringify(r));return r.value;};
  const retained=await call('object.create',{name:'Unsaved retained'});await call('object.addComponent',{objectId:retained.id,typeId:'engine.LightComponent'});
  const scene=(await call('bridge.describe')).path;const original=await fs.readFile(scene,'utf8');
  await fs.writeFile(scene,original+'\n');
  assert.equal((await call('bridge.describe')).externalChanges,true);
  let result=await app.bridge.request('scene.save',{});assert.equal(result.ok,false);assert.equal(await fs.readFile(scene,'utf8'),original+'\n');
  assert.equal((await call('bridge.describe')).dirty,true);
  assert.ok((await call('scene.getHierarchy')).some(o=>o.name==='Unsaved retained'));
  await call('scene.save',{path:'Scenes/Preserved.scene'});await call('project.save',{useCurrentAsStartup:true});
  await app.close();app=null;
  await projectCli(host,['--project-check',created.path]);
  const descriptor=await fs.readFile(created.path,'utf8');
  await fs.writeFile(created.path,descriptor.replace('"engineVersion": "0.1"','"engineVersion": "999"'));
  await assert.rejects(()=>projectCli(host,['--project-check',created.path]),/engineVersion/);
  await fs.writeFile(created.path,descriptor);
  const saved=path.join(dir,'Portable/Content/Scenes/Preserved.scene');const savedText=await fs.readFile(saved,'utf8');
  const invalid=JSON.parse(savedText);invalid.formatVersion=999;await fs.writeFile(saved,JSON.stringify(invalid));
  await assert.rejects(()=>projectCli(host,['--project-check',created.path]));
  await fs.writeFile(saved,savedText);
  const futureComponent=JSON.parse(savedText);futureComponent.objects[0].components[0].typeVersion=999;await fs.writeFile(saved,JSON.stringify(futureComponent));
  await assert.rejects(()=>projectCli(host,['--project-check',created.path]));await fs.writeFile(saved,savedText);
  const metaPath=saved+'.meta',meta=await fs.readFile(metaPath,'utf8');const futureMeta=JSON.parse(meta);futureMeta.schemaVersion=999;await fs.writeFile(metaPath,JSON.stringify(futureMeta));
  await assert.rejects(()=>projectCli(host,['--project-check',created.path]));await fs.writeFile(metaPath,meta);
  app=await launch(created.path);assert.ok((await call('scene.getHierarchy')).some(o=>o.name==='Unsaved retained'));
 }finally{await app?.close();await fs.rm(dir,{recursive:true,force:true});}
});
