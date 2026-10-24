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
function latestViewport(app){
  const packets=app.output().split(/\r?\n/).filter(line=>line.startsWith('@sacura '));
  for(let index=packets.length-1;index>=0;index--){
    const packet=JSON.parse(packets[index].slice(8));
    if(packet.type==='viewport')return packet.data;
  }
  return null;
}
test('contract rejects extra fields, unsafe values, methods and malformed transforms',()=>{
  for(const [m,p] of [['eval',{}],['object.destroy',{objectId:'x',pointer:42}],['object.rename',{objectId:'x',name:'\0'}],['component.setProperty',{objectId:'x',componentId:'c',property:'p',value:Infinity}],['transform.set',{objectId:'x',transform:{position:[1],rotation:[],scale:[]}}]])assert.throws(()=>validate(m,p));
  validate('object.create',{name:'日本語 объект'});
});
test('visual novel template plays dialogue, branches and resolves its WAV cue', {skip:process.platform!=='win32',timeout:120000},async()=>{
  const parent=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-novel-play-'));
  let app;
  try{
    const created=spawn(host,['--project-create',parent,'NovelDemo','builtin-novel'],{windowsHide:true});
    let output='';created.stdout.on('data',chunk=>output+=chunk);created.stderr.on('data',chunk=>output+=chunk);
    const code=await new Promise(resolve=>created.once('exit',resolve));
    assert.equal(code,0,output);
    const project=path.join(parent,'NovelDemo','NovelDemo.project');
    app=await launch(project);
    const call=async(method,params={})=>{const result=await app.bridge.request(method,params);assert.equal(result.ok,true,method+': '+JSON.stringify(result));return result.value;};
    const initial=await call('bridge.describe');assert.equal(initial.startupAudio,true);
    const mira=(await call('scene.getHierarchy')).find(item=>item.name==='Мира');
    assert.ok(mira?.components.some(component=>component.typeId==='engine.CharacterComponent'));
    assert.ok(mira.components.some(component=>component.typeId==='engine.MeshRendererComponent'));
    assert.ok((await call('assets.list')).some(asset=>asset.typeId==='Audio'&&asset.name==='RainTheme.wav'));
    assert.equal((await call('story.get')).document.nodes.length,6);
    const audio=(await call('audio.resolveStartup')).absolutePath;
    const bytes=await fs.readFile(audio);assert.equal(bytes.toString('ascii',0,4),'RIFF');assert.equal(bytes.toString('ascii',8,12),'WAVE');
    let state=await call('play.start');assert.equal(state.dialogue.phase,'line');assert.match(state.dialogue.text,/Дождь/);
    state=await call('play.advanceDialogue');assert.equal(state.dialogue.speaker,'Мира');
    state=await call('play.advanceDialogue');assert.equal(state.dialogue.phase,'choice');assert.equal(state.dialogue.choices.length,2);
    state=await call('play.selectChoice',{index:0});assert.equal(state.dialogue.speaker,'Мира');
    state=await call('play.advanceDialogue');assert.equal(state.dialogue.phase,'end');
    state=await call('play.stop');assert.equal(state.dialogue.phase,'idle');
  }finally{if(app)await app.close();await fs.rm(parent,{recursive:true,force:true,maxRetries:5});}
});
test('right mouse drag turns the native viewport toward the drag direction', {skip:process.platform!=='win32',timeout:30000},async()=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-camera-input-'));
  await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});
  let app;
  try{
    app=await launch(path.join(dir,'SampleProject.project'));
    await until(()=>latestViewport(app),5000);
    const before=latestViewport(app);
    const forward=before.cameraTarget.map((value,index)=>value-before.cameraPosition[index]);
    const right=[-forward[2],0,forward[0]];
    const send=event=>app.child.stdin.write(JSON.stringify({type:'input',event:{kind:'pointer',...event}})+'\n');
    send({phase:'down',x:200,y:100,button:2,buttons:2});
    send({phase:'move',x:240,y:100,button:2,buttons:2});
    send({phase:'up',x:240,y:100,button:2,buttons:0});
    await until(()=>latestViewport(app)?.inputCount>=before.inputCount+3,5000);
    const after=latestViewport(app);
    const targetChange=after.cameraTarget.map((value,index)=>value-before.cameraTarget[index]);
    assert.ok(targetChange.reduce((sum,value,index)=>sum+value*right[index],0)>0,
      `Dragging right turned the camera left: ${JSON.stringify({before:before.cameraTarget,after:after.cameraTarget})}`);
    send({phase:'down',x:240,y:100,button:2,buttons:2});
    send({phase:'move',x:200,y:100,button:2,buttons:2});
    send({phase:'up',x:200,y:100,button:2,buttons:0});
    await until(()=>latestViewport(app)?.inputCount>=after.inputCount+3,5000);
    const restored=latestViewport(app);
    const leftChange=restored.cameraTarget.map((value,index)=>value-after.cameraTarget[index]);
    assert.ok(leftChange.reduce((sum,value,index)=>sum+value*right[index],0)<0,
      'Dragging left did not turn the camera left');
    assert.ok(restored.cameraTarget.every((value,index)=>Math.abs(value-before.cameraTarget[index])<0.001),
      'Equal right and left drags did not restore the view');
  }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
});
test('native viewport projects and picks enabled light icons', {skip:process.platform!=='win32',timeout:30000},async()=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-light-icon-'));
  await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});
  let app;
  try{
    app=await launch(path.join(dir,'SampleProject.project'));
    const call=async(method,params={})=>{
      const result=await app.bridge.request(method,params);
      assert.equal(result.ok,true,method+': '+JSON.stringify(result));
      return result.value;
    };
    const created=await call('object.create',{name:'Viewport light',componentType:'engine.LightComponent',
      transform:{position:[0,0,0],rotation:[0,0,0,1],scale:[1,1,1]}});
    const icon=()=>latestViewport(app)?.lightIcons?.find(item=>item.id===created.id);
    await until(()=>icon(),5000);
    const initial=icon();
    assert.ok(Number.isFinite(initial.x)&&Number.isFinite(initial.y));
    await call('transform.set',{objectId:created.id,
      transform:{position:[1,0,0],rotation:[0,0,0,1],scale:[1,1,1]}});
    await until(()=>Math.abs((icon()?.x??initial.x)-initial.x)>1,5000);
    const first=icon();
    const before=latestViewport(app).inputCount;
    const send=event=>app.child.stdin.write(JSON.stringify({type:'input',event:{kind:'pointer',...event}})+'\n');
    send({phase:'down',x:first.x,y:first.y,button:0,buttons:1});
    send({phase:'up',x:first.x,y:first.y,button:0,buttons:0});
    await until(()=>latestViewport(app)?.inputCount>=before+2,5000);
    assert.equal(latestViewport(app).selection?.id,created.id,'Clicking the light icon must select its object');
    const component=(await call('scene.getHierarchy')).find(item=>item.id===created.id).components
      .find(item=>item.typeId==='engine.LightComponent');
    await call('component.setEnabled',{objectId:created.id,componentId:component.id,enabled:false});
    await until(()=>!icon(),5000);
  }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
});
test('native character is visible, editable, undoable and saved in the scene', {skip:process.platform!=='win32',timeout:30000},async()=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-character-'));
  await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});
  let app;
  try{
    app=await launch(path.join(dir,'SampleProject.project'));
    const call=async(method,params={})=>{
      const result=await app.bridge.request(method,params);
      assert.equal(result.ok,true,method+': '+JSON.stringify(result));return result.value;
    };
    const starter=(await call('assets.list')).find(asset=>asset.path==='/Engine/Models/StarterCharacter.glb');
    assert.ok(starter?.subassets?.some(item=>item.typeId==='StaticMesh'));
    const created=await call('character.create',{name:'Алиса',transform:{position:[2,0,3],rotation:[0,0,0,1],scale:[1,1,1]}});
    let character=(await call('scene.getHierarchy')).find(item=>item.id===created.id);
    assert.deepEqual(character.transform.position,[2,0,3]);
    const marker=character.components.find(item=>item.typeId==='engine.CharacterComponent');
    const renderer=character.components.find(item=>item.typeId==='engine.MeshRendererComponent');
    assert.ok(marker&&renderer);
    const mesh=(await call('component.getProperties',{objectId:created.id,componentId:renderer.id}))
      .find(item=>item.id==='mesh_asset_id');
    assert.equal(mesh.value,starter.id);
    assert.equal(mesh.subAssetId,starter.subassets[0].id);
    await call('component.setProperty',{objectId:created.id,componentId:marker.id,property:'description',value:'Героиня истории'});
    await call('object.rename',{objectId:created.id,name:'Алиса Волкова'});
    await call('scene.save');
    const saved=JSON.parse(await fs.readFile(path.join(dir,'Content/Scenes/Main.scene'),'utf8'));
    assert.match(JSON.stringify(saved),/Героиня истории/);
    assert.match(JSON.stringify(saved),/engine.CharacterComponent/);
    await call('scene.open',{path:'Scenes/Main.scene'});
    character=(await call('scene.getHierarchy')).find(item=>item.id===created.id);
    assert.equal(character.name,'Алиса Волкова');
    assert.ok(character.components.some(item=>item.typeId==='engine.CharacterComponent'));
    const duplicate=await call('object.duplicate',{objectId:created.id});
    assert.ok((await call('scene.getHierarchy')).some(item=>item.id===duplicate.id&&item.components.some(component=>component.typeId==='engine.CharacterComponent')));
    await call('editor.undo');
    assert.ok(!(await call('scene.getHierarchy')).some(item=>item.id===duplicate.id));
    await call('object.destroy',{objectId:created.id});
    await call('editor.undo');
    assert.ok((await call('scene.getHierarchy')).some(item=>item.id===created.id));
  }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
});
test('native bridge imports a rigged FBX as a static character mesh', {skip:process.platform!=='win32',timeout:30000},async()=>{
  const dir=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-rigged-fbx-'));
  await fs.cp(path.join(root,'Samples/SampleProject'),dir,{recursive:true});
  let app;
  try{
    app=await launch(path.join(dir,'SampleProject.project'));
    const call=async(method,params={})=>{
      const result=await app.bridge.request(method,params);
      assert.equal(result.ok,true,method+': '+JSON.stringify(result));return result.value;
    };
    const sourcePath=path.join(root,'Root/Editor/ThirdParty/ufbx/_src/data/blender_293_half_skinned_7400_binary.fbx');
    const imported=await call('assets.import',{sourcePath,destination:'Models/RiggedFixture.glb'});
    const model=(await call('assets.list')).find(asset=>asset.id===imported.id);
    assert.equal(model.typeId,'Model');
    const subasset=model.subassets.find(item=>item.typeId==='StaticMesh');
    assert.ok(subasset);
    const character=await call('character.create',{name:'Rigged fixture'});
    const renderer=(await call('scene.getHierarchy')).find(item=>item.id===character.id).components
      .find(item=>item.typeId==='engine.MeshRendererComponent');
    await call('component.setAsset',{objectId:character.id,componentId:renderer.id,
      property:'mesh_asset_id',assetId:model.id,subAssetId:subasset.id});
    const assigned=(await call('component.getProperties',{objectId:character.id,componentId:renderer.id}))
      .find(item=>item.id==='mesh_asset_id');
    assert.equal(assigned.value,model.id);
    assert.equal(assigned.subAssetId,subasset.id);
  }finally{if(app)await app.close();await fs.rm(dir,{recursive:true,force:true,maxRetries:5});}
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
    const placed={position:[2,0,3],rotation:[0,0,0,1],scale:[1,1,1]};
    const light=await call('object.create',{name:'Menu light',transform:placed,componentType:'engine.LightComponent'});
    const lightInfo=(await call('scene.getHierarchy')).find(o=>o.id===light.id);
    assert.deepEqual(lightInfo.transform,placed);
    assert.ok(lightInfo.components.some(c=>c.typeId==='engine.LightComponent'));
    assert.equal((await call('component.getProperties',{objectId:light.id,componentId:lightInfo.components.find(c=>c.typeId==='engine.LightComponent').id})).find(p=>p.id==='light_type').value,'Point');
    const builtins=await call('assets.list');
    for(const [shape,path] of [['cube','StarterBlock'],['sphere','PrimitiveSphere'],['cylinder','PrimitiveCylinder']]){
      const asset=builtins.find(item=>item.path===`/Engine/Models/${path}.glb`);
      assert.ok(asset?.subassets?.some(item=>item.typeId==='StaticMesh'),`Missing ${shape} mesh asset`);
      const created=await call('object.create',{name:shape,transform:placed,primitive:shape});
      const component=(await call('scene.getHierarchy')).find(item=>item.id===created.id).components.find(item=>item.typeId==='engine.MeshRendererComponent');
      assert.ok(component,`Missing ${shape} renderer`);
      const properties=await call('component.getProperties',{objectId:created.id,componentId:component.id});
      assert.equal(properties.find(item=>item.id==='mesh_asset_id').value,asset.id);
    }
    for(let index=0;index<3;index++)await call('editor.undo');
    for(const typeId of ['engine.MeshRendererComponent','engine.ArrowComponent','engine.CameraComponent']){
      const created=await call('object.create',{name:typeId,transform:placed,componentType:typeId,...(typeId==='engine.CameraComponent'?{cameraAspect:4/3}:{})});
      const component=(await call('scene.getHierarchy')).find(o=>o.id===created.id).components.find(c=>c.typeId===typeId);
      assert.ok(component);
      if(typeId==='engine.CameraComponent')assert.ok(Math.abs((await call('component.getProperties',{objectId:created.id,componentId:component.id})).find(p=>p.id==='aspect_ratio').value-4/3)<1e-6);
    }
    for(let index=0;index<3;index++)await call('editor.undo');
    await call('editor.undo');assert.ok(!(await call('scene.getHierarchy')).some(o=>o.id===light.id));
    await call('editor.redo');assert.ok((await call('scene.getHierarchy')).some(o=>o.id===light.id));
    const copied=await call('object.duplicate',{objectId:light.id});
    assert.notEqual(copied.id,light.id);
    const copyInfo=(await call('scene.getHierarchy')).find(o=>o.id===copied.id);
    assert.deepEqual(copyInfo.transform,placed);
    assert.ok(copyInfo.components.some(c=>c.typeId==='engine.LightComponent'));
    await call('editor.undo');assert.ok(!(await call('scene.getHierarchy')).some(o=>o.id===copied.id));
    await fail('object.create',{name:'Bad menu object',componentType:'engine.NotAComponent'},'INVALID_ARGUMENT');
    await fail('object.create',{name:'Bad primitive',primitive:'torus'},'INVALID_ARGUMENT');
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
    assert.ok(props.some(p=>p.id==='light_type'&&p.typeId==='engine.RenderLightType'&&p.enumValues.join(',')==='Directional,Point,Spot'));
    await call('component.setProperty',{...cp,property:'light_type',value:2});
    assert.equal((await call('component.getProperties',cp)).find(p=>p.id==='light_type').value,'Spot');
    await call('editor.undo');
    await call('component.setProperty',{...cp,property:'light_type',value:'Point'});
    assert.equal((await call('component.getProperties',cp)).find(p=>p.id==='light_type').value,'Point');
    await fail('component.setProperty',{...cp,property:'light_type',value:'UnknownLight'},'PROPERTY_INVALID');
    await fail('component.setProperty',{...cp,property:'missing',value:1},'PROPERTY_READ_ONLY');
    await fail('component.getProperties',{...cp,objectId:parent.id},'NOT_FOUND');
    await call('object.removeComponent',cp);await fail('component.getProperties',cp,'NOT_FOUND');
    await call('editor.undo');assert.ok((await call('object.getComponents',{objectId:id})).some(c=>c.id===component.id));
    // Older commands must resolve the restored component through its persistent ID.
    await call('editor.undo');assert.equal((await call('component.getProperties',cp)).find(p=>p.id==='light_type').value,'Directional');
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
  await t.test('Inspector edits object active state through native Undo',async()=>{
   const object=await call('object.create',{name:'Inspector object'});
   await call('object.setActive',{objectId:object.id,active:false});
   assert.equal((await call('scene.getHierarchy')).find(o=>o.id===object.id).active,false);
   await call('editor.undo');
   assert.equal((await call('scene.getHierarchy')).find(o=>o.id===object.id).active,true);
   const component=await call('object.addComponent',{objectId:object.id,typeId:'engine.LightComponent'});
   await call('component.setEnabled',{objectId:object.id,componentId:component.id,enabled:false});
   assert.equal((await call('scene.getHierarchy')).find(o=>o.id===object.id).components.find(c=>c.id===component.id).enabled,false);
   await call('editor.undo');
   assert.equal((await call('scene.getHierarchy')).find(o=>o.id===object.id).components.find(c=>c.id===component.id).enabled,true);
   await call('object.destroy',{objectId:object.id});
  });
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
   s=await call('story.updateNode',{nodeId:line,patch:{kind:'Choice'}});assert.equal(s.document.nodes.find(n=>n.id===line).kind,'Choice');
   s=await call('story.updateNode',{nodeId:line,patch:{kind:'Line'}});assert.equal(s.document.nodes.find(n=>n.id===line).kind,'Line');
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
   const overridden={...event.groups[0].actions[0],durationSeconds:.75};
   s=await call('story.setOverrides',{nodeId:line,bindingId:binding,actions:[overridden]});
   assert.equal(s.document.nodes.find(n=>n.id===line).bindings[0].actionOverrides[overridden.id].durationSeconds,.75);
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


test('React asset migration uses native previews, placement, documents and undo', {skip:process.platform!=='win32',timeout:120000}, async()=>{
  const Parent=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-asset-transfer-'));
  let Application;
  try {
    const Created=spawn(host,['--project-create',Parent,'AssetTransfer','Empty'],{windowsHide:true});
    let Output='';Created.stdout.on('data',Chunk=>Output+=Chunk);Created.stderr.on('data',Chunk=>Output+=Chunk);
    assert.equal(await new Promise(Resolve=>Created.once('exit',Resolve)),0,Output);
    const ProjectRoot=path.join(Parent,'AssetTransfer');
    Application=await launch(path.join(ProjectRoot,'AssetTransfer.project'));
    const Call=async(Method,Params={})=>{
      const Result=await Application.bridge.request(Method,Params);
      assert.equal(Result.ok,true,Method+': '+JSON.stringify(Result));return Result.value;
    };
    const CreationTypes=await Call('assets.creationTypes');
    for (const Identifier of ['Scene','Story','Material','PostProcessMaterial','UserInterfaceMaterial','MaterialInstance']) {
      assert.ok(CreationTypes.some(Type=>Type.id==='editor.Create'+Identifier+'AssetFactory'));
    }
    await fs.mkdir(path.join(ProjectRoot,'Content','CreatedAssets'),{recursive:true});
    await Call('object.create',{name:'KeepUnsavedScene'});
    const SceneBeforeCreation=await Call('bridge.describe');
    const NewScene=await Call('assets.create',{factoryId:'editor.CreateSceneAssetFactory',path:'CreatedAssets/MenuScene.scene'});
    assert.equal(NewScene.typeId,'Scene');
    const AfterSceneCreation=await Call('bridge.describe');
    assert.equal(AfterSceneCreation.path,SceneBeforeCreation.path);
    assert.equal(AfterSceneCreation.dirty,true);
    const NewStory=await Call('assets.create',{factoryId:'editor.CreateStoryAssetFactory',path:'CreatedAssets/MenuStory.story'});
    assert.equal(NewStory.typeId,'Story');
    assert.equal((await Call('story.get')).open,false);
    const NewMaterial=await Call('assets.create',{factoryId:'editor.CreateMaterialAssetFactory',path:'CreatedAssets/MenuMaterial.material'});
    await Call('assets.create',{factoryId:'editor.CreatePostProcessMaterialAssetFactory',path:'CreatedAssets/MenuPostProcess.material'});
    await Call('assets.create',{factoryId:'editor.CreateUserInterfaceMaterialAssetFactory',path:'CreatedAssets/MenuUserInterface.material'});
    const NewInstance=await Call('assets.create',{factoryId:'editor.CreateMaterialInstanceAssetFactory',path:'CreatedAssets/MenuInstance.materialinstance',settings:{parent:{assetId:NewMaterial.id,subAssetId:null}}});
    const InstanceData=JSON.parse(await fs.readFile(path.join(ProjectRoot,'Content','CreatedAssets','MenuInstance.materialinstance'),'utf8'));
    assert.equal(InstanceData.parent.assetId,NewMaterial.id);
    assert.equal(NewInstance.typeId,'MaterialInstance');
    for (const Params of [{factoryId:'editor.CreateSceneAssetFactory',path:'CreatedAssets/MenuScene.scene'},{factoryId:'editor.CreateSceneAssetFactory',path:'../Escape.scene'},
      {factoryId:'editor.CreateMaterialAssetFactory',path:'/Engine/Escape.material'},{factoryId:'editor.CreateMaterialInstanceAssetFactory',path:'CreatedAssets/NoParent.materialinstance'},
      {factoryId:'editor.CreateUnknownAssetFactory',path:'CreatedAssets/Unknown.asset'},{factoryId:'editor.CreateMaterialAssetFactory',path:'CreatedAssets/Wrong.scene'}]) {
      assert.equal((await Application.bridge.request('assets.create',Params)).ok,false,JSON.stringify(Params));
    }
    await Call('play.start');
    assert.equal((await Application.bridge.request('assets.create',{factoryId:'editor.CreateSceneAssetFactory',path:'CreatedAssets/PlayBlocked.scene'})).error.code,'PLAY_READ_ONLY');
    await Call('play.stop');
    const ParentSetting=CreationTypes.find(Type=>Type.id==='editor.CreateMaterialInstanceAssetFactory').settings.find(Field=>Field.id==='parent');
    assert.equal(ParentSetting.required,true);
    assert.deepEqual(ParentSetting.assetTypes,['Material','MaterialInstance']);
    assert.equal(ParentSetting.value,null);
    const CharacterCreated=await Call('assets.create',{factoryId:'editor.CreateCharacterAssetFactory',path:'CreatedAssets/MenuCharacter.character',settings:{description:'Created through reflected settings'}});
    assert.equal(CharacterCreated.typeId,'Character');
    const CharacterData=JSON.parse(await fs.readFile(path.join(ProjectRoot,'Content','CreatedAssets','MenuCharacter.character'),'utf8'));
    assert.equal(CharacterData.description,'Created through reflected settings');
    const ValueTypeSetting=CreationTypes.find(Type=>Type.id==='editor.CreateInputActionAssetFactory').settings.find(Field=>Field.id==='value_type');
    assert.equal(ValueTypeSetting.typeId,'engine.InputValueType');
    assert.equal(ValueTypeSetting.value,'Boolean');
    assert.deepEqual(ValueTypeSetting.enumValues,['Boolean','Axis1D','Axis2D','Axis3D']);
    await Call('assets.create',{factoryId:'editor.CreateInputActionAssetFactory',path:'CreatedAssets/MenuInput.inputaction',settings:{value_type:'Axis2D',execute_when_paused:true}});
    const InputData=JSON.parse(await fs.readFile(path.join(ProjectRoot,'Content','CreatedAssets','MenuInput.inputaction'),'utf8'));
    assert.equal(InputData.Action.ValueType,2);
    assert.equal(InputData.Action.bExecuteWhenPaused,true);
    await Call('assets.create',{factoryId:'editor.CreateInputContextAssetFactory',path:'CreatedAssets/MenuContext.inputcontext'});
    await Call('assets.create',{factoryId:'editor.CreateWidgetLibraryAssetFactory',path:'CreatedAssets/MenuWidgets.widgets'});
    await Call('assets.create',{factoryId:'editor.CreatePrefabAssetFactory',path:'CreatedAssets/MenuPrefab.prefab'});
    assert.ok(!CreationTypes.some(Type=>Type.id==='editor.CreateShaderSourceAssetFactory'));
    assert.equal((await Application.bridge.request('assets.create',{factoryId:'editor.CreateShaderSourceAssetFactory',path:'CreatedAssets/MenuShader.hlsl'})).ok,false);
    for (const Settings of [{unregistered:true},{value_type:'Axis4D'},{value_type:'2'},{execute_when_paused:'invalid'}]) {
      assert.equal((await Application.bridge.request('assets.create',{factoryId:'editor.CreateInputActionAssetFactory',path:'CreatedAssets/Rejected.inputaction',settings:Settings})).ok,false);
    }
    const MissingParent=await Application.bridge.request('assets.create',{factoryId:'editor.CreateMaterialInstanceAssetFactory',path:'CreatedAssets/NoLeakedDefaults.materialinstance'});
    assert.equal(MissingParent.ok,false);
    assert.equal((await Call('assets.creationTypes')).find(Type=>Type.id==='editor.CreateMaterialInstanceAssetFactory').settings[0].value,null);
    const Actions=await Call('editor.actions');
    for (const [Identifier,Expected] of [['editor.CreatePointLightObjectAction','Point'],['editor.CreateSpotLightObjectAction','Spot'],['editor.CreateDirectionalLightObjectAction','Directional']]) {
      assert.ok(Actions.some(Action=>Action.id===Identifier));
      const Created=await Call('editor.executeAction',{actionId:Identifier});
      const Objects=await Call('scene.getHierarchy');
      const Light=Objects.find(Object=>Object.id===Created.id).components.find(Component=>Component.typeId==='engine.LightComponent');
      const Properties=await Call('component.getProperties',{objectId:Created.id,componentId:Light.id});
      assert.equal(Properties.find(Property=>Property.id==='light_type').value,Expected);
      await Call('editor.undo');
      assert.ok(!(await Call('scene.getHierarchy')).some(Object=>Object.id===Created.id));
    }
    const Assets=await Call('assets.list');
    const Model=Assets.find(Entry=>Entry.typeId==='Model'&&Entry.subassets.some(Subasset=>Subasset.typeId==='StaticMesh'));
    assert.ok(Model);
    const Mesh=Model.subassets.find(Entry=>Entry.typeId==='StaticMesh');
    let Preview=await Call('assets.thumbnail',{assetId:Model.id,subAssetId:Mesh.id});
    for(let Attempt=0;!Preview.ready&&Attempt<100;Attempt++){
      await delay(100);Preview=await Call('assets.thumbnail',{assetId:Model.id,subAssetId:Mesh.id});
    }
    assert.equal(Preview.ready,true);assert.equal(Buffer.from(Preview.png,'base64').subarray(1,4).toString(),'PNG');
    const Placed=await Call('assets.place',{assetId:Model.id,subAssetId:Mesh.id});
    const Object=(await Call('scene.getHierarchy')).find(Entry=>Entry.id===Placed.id);
    assert.ok(Object);const Component=Object.components.find(Entry=>Entry.typeId==='engine.MeshRendererComponent');
    const Identity={objectId:Object.id,componentId:Component.id};
    await Call('mesh.physics',{...Identity,enabled:true,simulate:false});
    assert.equal((await Call('mesh.inspect',Identity)).collision,true);
    await Call('editor.undo');
    await Call('editor.redo');
    assert.equal((await Call('mesh.inspect',Identity)).collision,true);
    const Material=Assets.find(Entry=>Entry.typeId==='Material');assert.ok(Material);
    const MaterialPath=(await Call('assets.resolve',{assetId:Material.id})).absolutePath;
    await fs.mkdir(path.join(ProjectRoot,'Content','Materials'),{recursive:true});
    await fs.copyFile(MaterialPath,path.join(ProjectRoot,'Content','Materials','TransferSurface.material'));
    const Refreshed=await Call('assets.refresh');
    const Editable=Refreshed.find(Entry=>Entry.name==='TransferSurface.material');assert.ok(Editable);
    const Opened=await Call('assetDocument.execute',{assetId:Editable.id,operation:'open'});
    const Next={...Opened.document,blendMode:'Masked'};
    const Edited=await Call('assetDocument.execute',{assetId:Editable.id,operation:'edit',revision:Opened.revision,value:Next});
    assert.equal(Edited.dirty,true);assert.equal((await Call('bridge.describe')).assetDocumentsDirty,true);
    const Stale=await Application.bridge.request('assetDocument.execute',{assetId:Editable.id,operation:'save',revision:Opened.revision});
    assert.equal(Stale.ok,false);assert.equal(Stale.error.code,'STALE_DOCUMENT');
    const Undone=await Call('assetDocument.execute',{assetId:Editable.id,operation:'undo',revision:Edited.revision});
    assert.equal(Undone.document.blendMode,Opened.document.blendMode);
    const Redone=await Call('assetDocument.execute',{assetId:Editable.id,operation:'redo',revision:Undone.revision});
    const Saved=await Call('assetDocument.execute',{assetId:Editable.id,operation:'save',revision:Redone.revision});
    assert.equal(Saved.dirty,false);
    const Builtin=await Call('assetDocument.execute',{assetId:Material.id,operation:'open'});
    assert.equal(Builtin.readOnly,true);
    const InstancePath=path.join(ProjectRoot,'Content','Materials','TransferInstance.materialinstance');
    await fs.writeFile(InstancePath,JSON.stringify({schemaVersion:1,parent:{assetId:Editable.id,subAssetId:null},overrides:{}}));
    const InstanceAssets=await Call('assets.refresh');
    const Instance=InstanceAssets.find(Entry=>Entry.relativePath==='Materials/TransferInstance.materialinstance');
    assert.ok(Instance);
    const InstanceDocument=await Call('assetDocument.execute',{assetId:Instance.id,operation:'open'});
    assert.ok(InstanceDocument.parameters.length>0);
    const Parameter=InstanceDocument.parameters[0];
    const InstanceEdit=await Call('assetDocument.execute',{assetId:Instance.id,operation:'edit',revision:InstanceDocument.revision,
      value:{...InstanceDocument.document,overrides:{[Parameter.id]:{type:Parameter.type,value:Parameter.value}}}});
    assert.equal(InstanceEdit.dirty,true);
    assert.equal(InstanceEdit.parameters[0].id,Parameter.id);
    const InstanceUndo=await Call('assetDocument.execute',{assetId:Instance.id,operation:'undo',revision:InstanceEdit.revision});
    assert.equal(Reflect.ownKeys(InstanceUndo.document.overrides).length,0);
    const MeshDocument=await Call('assetDocument.execute',{assetId:Model.id,subAssetId:Mesh.id,operation:'open'});
    const MeshPreview=await Call('assetDocument.execute',{assetId:Model.id,subAssetId:Mesh.id,operation:'preview',value:{showCollision:true,selectedCollision:0}});
    assert.equal(MeshPreview.selectedCollision,0);
    assert.deepEqual(MeshPreview.document,MeshDocument.document);

    const Rejected=await Application.bridge.request('assetDocument.execute',{assetId:Material.id,operation:'edit',revision:Builtin.revision,value:Next});
    assert.equal(Rejected.ok,false);
  } finally {
    if(Application){await Application.close();}
    await fs.rm(Parent,{recursive:true,force:true});
  }
});
