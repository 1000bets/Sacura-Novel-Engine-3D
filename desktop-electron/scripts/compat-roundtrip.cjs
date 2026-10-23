// Runs in Electron main, uses its actual isolated renderer Bridge and Qt shell CLI.
const fs=require('node:fs/promises');
const path=require('node:path');
const assert=require('node:assert/strict');
const {spawn}=require('node:child_process');
const {setTimeout:delay}=require('node:timers/promises');
exports.run=async({app,window,getViewport,replaceProject,createNativeProject})=>{
 const root=process.env.SACURA_SOURCE_ROOT||path.resolve(__dirname,'../..');
 const output=process.env.SACURA_COMPAT_OUTPUT||path.join(root,'build/compat-roundtrip-'+Date.now());
 const report={checks:[],passed:false};
 const wait=async(fn)=>{const end=Date.now()+60000;while(!await fn()){if(Date.now()>end)throw Error('Round-trip timed out');await delay(50);}};
 const capture=async filename=>{
   // DOM readiness can precede Chromium's first compositor surface after restart.
   for(let attempt=0;;attempt++){
     try{await fs.writeFile(path.join(output,filename),(await window.webContents.capturePage()).toPNG());return;}
     catch(error){if(attempt>=7||!String(error).includes('UnknownVizError'))throw error;await delay(250);}
   }
 };
 const renderer=async(method,params={})=>{
   const r=await window.webContents.executeJavaScript(`(async()=>{const bridge=window.sacuraDesktop.bridge;const {context}=await bridge.connect();return bridge.request({method:${JSON.stringify(method)},params:${JSON.stringify(params)},context});})()`);
   assert.equal(r.ok,true,method+': '+JSON.stringify(r));return r.value;
 };
 try{
   await fs.mkdir(output,{recursive:true});
   if(process.env.SACURA_TEST_BOOT_DELAY==='1'){
     await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('[data-boot-splash]')"));
     assert.equal(window.getBackgroundColor().toLowerCase(),'#11121b');
     assert.equal(await window.webContents.executeJavaScript("getComputedStyle(document.querySelector('[data-boot-splash]')).backgroundColor"),'rgb(17, 18, 27)');
     assert.equal(await window.webContents.executeJavaScript("getComputedStyle(document.querySelector('[data-boot-retry]')).display"),'none');
     await capture('electron-loading.png');
     report.checks.push('HTML loading screen is visible before the React bundle loads; BrowserWindow and splash have the same dark background');
   }
   await wait(async()=>!window.webContents.isLoading()&&await window.webContents.executeJavaScript('!!window.sacuraDesktop?.bridge'));
   assert.equal(getViewport().state.phase,'idle');
   await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('[data-native-launcher]')"));
   assert.equal(await window.webContents.executeJavaScript("!!document.querySelector('[data-native-surface]')"),false);
   assert.equal(await window.webContents.executeJavaScript("!!document.querySelector('[data-native-overlay]')"),false);
   assert.equal(await window.webContents.executeJavaScript("Array.from(document.querySelectorAll('button')).some(b=>b.textContent.trim()==='Стоп')"),false);
   await capture('electron-startup.png');
   report.checks.push('Launcher shows create/open/recent projects; Editor and viewport are not mounted before project choice');
   await assert.rejects(()=>replaceProject(path.join(output,'Missing.project')));
   assert.equal((await window.webContents.executeJavaScript('window.sacuraDesktop.nativeSession()')).loading,null);
   const creation=createNativeProject('RoundTrip',output);
   creation.catch(()=>{}); // Report through the awaited operation below, including screenshot failures.
   if(process.env.SACURA_TEST_BOOT_DELAY==='1'){
     await wait(()=>window.webContents.executeJavaScript("document.querySelector('[data-project-loading] .sacura-loading-project')?.textContent==='RoundTrip'"));
     await capture('electron-project-loading.png');
   }
   const created=await creation;assert.ok(created);
   await wait(()=>getViewport().bridge.connected);
   await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('[data-native-surface]')"));
   await wait(()=>window.webContents.executeJavaScript("!document.querySelector('[data-project-loading]')&&!document.querySelector('[data-boot-splash]')"));
   report.checks.push('Project loading overlay clears after native Engine readiness; failed project validation clears loading state');
   const recent=await window.webContents.executeJavaScript('window.sacuraDesktop.nativeListProjects()');
   const recentProject=recent.find(p=>p.path===created.path);assert.ok(recentProject);
   await window.webContents.executeJavaScript('window.sacuraDesktop.nativeCloseProject()');
   await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('[data-native-launcher]')&&!document.querySelector('[data-native-surface]')"));
   await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('.project-home-list button')"));
   await capture('electron-recents.png');
   await assert.rejects(()=>window.webContents.executeJavaScript("window.sacuraDesktop.nativeOpenRecent('invalid-id')"));
   await window.webContents.executeJavaScript(`window.sacuraDesktop.nativeOpenRecent(${JSON.stringify(recentProject.id)})`);
   await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('[data-native-surface]')"));
   await delay(500);
   window.webContents.send('native-test',{action:'create-story'});
   await wait(()=>getViewport().bridgeTest?.action==='create-story');
   const firstStory=await renderer('story.get');assert.equal(firstStory.document.nodes.length,1);
   const firstId=firstStory.document.startNodeId;
   await wait(()=>window.webContents.executeJavaScript(`!!document.querySelector('.react-flow__node.selected[data-id="${firstId}"]')`));
   await capture('electron-created-story.png');
   const menu=async(context,choose)=>{
     const token=Date.now()+Math.random();window.webContents.send('native-test',{action:'story-menu',context,choose,token});
     await wait(()=>getViewport().bridgeTest?.token===token);assert.equal(getViewport().bridgeTest.ok,true,getViewport().bridgeTest.error);
   };
   assert.equal(await window.webContents.executeJavaScript("!!document.querySelector('select[aria-label=\"Тип ноды\"]')"),false);
   const nodeContext={kind:'native-story',nodeId:firstId,position:{x:500,y:150},x:750,y:600};
   await menu(nodeContext);await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('.editor-context-menu')"));
   await capture('electron-node-menu.png');
   await menu(nodeContext,'Выбор игрока');
   let menuStory=await renderer('story.get');const choiceNode=menuStory.document.nodes.find(n=>n.kind==='Choice');assert.ok(choiceNode);assert.equal(menuStory.positions[choiceNode.id].x,500);
   await renderer('story.undo');assert.equal((await renderer('story.get')).document.nodes.length,1);await renderer('story.redo');
   const eventContext={kind:'native-event',nodeId:firstId,phase:'BEFORE',x:750,y:600};
   await menu(eventContext);await wait(()=>window.webContents.executeJavaScript("!!document.querySelector('.editor-context-menu')"));await capture('electron-event-menu.png');
   await menu(eventContext,'Перемещение объекта');
   menuStory=await renderer('story.get');const addedEvent=menuStory.document.events.find(e=>e.groups[0].actions[0].kind==='MoveTo');assert.ok(addedEvent);
   await renderer('story.undo');assert.equal(((await renderer('story.get')).document.events||[]).length,0);await renderer('story.redo');
   await menu({...eventContext,phase:'AFTER'});
   await wait(()=>window.webContents.executeJavaScript(`!!document.querySelector('[data-menu-id="${addedEvent.id}"]')`));
   await menu({...eventContext,phase:'AFTER'},addedEvent.id);
   menuStory=await renderer('story.get');assert.equal(menuStory.document.events.length,1);assert.equal(menuStory.document.nodes.find(n=>n.id===firstId).bindings.length,2);
   await renderer('story.save');
   report.checks.push('Shared context menu creates native Choice at graph coordinates, creates MoveTo Event and reuses its ID; native Undo/Redo and save work; node type dropdown removed');
   report.checks.push('Close returns to launcher; recent project opens through isolated IPC; Create Story handler opens and selects the native starting Line immediately');
   report.project=created.path;report.checks.push('Electron main -> C++ ProjectGenerator creates .project and native startup scene');
   await renderer('scene.create',{path:'Scenes/Interchange.scene'});
   const parent=await renderer('object.create',{name:'Parent'});
   const object=await renderer('object.create',{name:'Electron object',parentId:parent.id});
   const editId=object.id;
   await renderer('transform.set',{objectId:editId,transform:{position:[2,3,4],rotation:[0,0,0,1],scale:[1,2,1]}});
   const mesh=await renderer('object.addComponent',{objectId:editId,typeId:'engine.MeshRendererComponent'});
   const light=await renderer('object.addComponent',{objectId:editId,typeId:'engine.LightComponent'});
   const cameraObject=await renderer('object.create',{name:'Camera'});
   await renderer('object.addComponent',{objectId:cameraObject.id,typeId:'engine.CameraComponent'});
   const imported=[];
   for(const [source,destination] of [['Samples/SampleProject/Content/Meshes/TestCube.glb','Models/RoundTrip.glb'],['Samples/RenderValidation/Content/Textures/CutoutMask.png','Textures/RoundTripTexture.png']]){
     const r=await getViewport().bridge.request('assets.import',{sourcePath:path.join(root,source),destination});assert.equal(r.ok,true,JSON.stringify(r));imported.push(r.value.id);
   }
   let assets=await renderer('assets.list');const model=assets.find(a=>a.id===imported[0]);assert.ok(model,'Imported identity absent: '+JSON.stringify({imported,assets:assets.filter(a=>!a.builtin)}));assert.ok(model.subassets.some(s=>s.typeId==='StaticMesh'),'Missing static mesh: '+JSON.stringify(model));
   const properties=[{objectId:editId,componentId:mesh.id,property:'mesh_asset_id',value:model.id},
     {objectId:editId,componentId:mesh.id,property:'mesh_sub_asset_id',value:model.subassets.find(s=>s.typeId==='StaticMesh').id}];
   await renderer('component.setAsset',{objectId:editId,componentId:mesh.id,property:'mesh_asset_id',assetId:model.id,subAssetId:properties[1].value});
   const lp=await renderer('component.getProperties',{objectId:editId,componentId:light.id});
   const intensity=lp.find(p=>p.id==='intensity');if(intensity){const p={objectId:editId,componentId:light.id,property:'intensity',value:3.5};await renderer('component.setProperty',p);properties.push(p);}
   let story=await renderer('story.create',{path:'Stories/InterchangeStory.story'});
   assert.equal(story.document.nodes.length,1);
   story=await renderer('story.addNode',{kind:'Action',x:300,y:100});
   const action=story.document.nodes.find(n=>n.kind==='Action');
   story=await renderer('story.updateNode',{nodeId:action.id,patch:{actions:[{id:'move-roundtrip',kind:'MoveTo',durationSeconds:1,targetObjectId:editId,position:[5,3,4]}]}});
   const line=story.document.nodes.find(n=>n.kind==='Line');
   if(line){await renderer('story.updateNode',{nodeId:line.id,patch:{speaker:'Narrator',text:'Round-trip dialogue'}});await renderer('story.connect',{sourceId:line.id,port:-1,targetId:action.id});}
   story=await renderer('story.addNode',{kind:'End',x:600,y:100});
   await renderer('story.connect',{sourceId:action.id,port:-1,targetId:story.document.nodes.find(n=>n.kind==='End').id});
   await renderer('story.setStart',{nodeId:line.id});
   await renderer('story.addEvent',{nodeId:line.id,phase:'ON_START'});
   await renderer('project.save',{useCurrentAsStartup:true});
   const hierarchy=await renderer('scene.getHierarchy');assets=(await renderer('assets.list')).filter(a=>!a.builtin);story=await renderer('story.get');
   const expected={hierarchy,assets,story,properties,editId};
   const manifest=path.join(output,'semantic-manifest.json');await fs.writeFile(manifest,JSON.stringify(expected,null,2));
   const frameStart=getViewport().state.presentation?.frames||0;
   await fs.writeFile(path.join(output,'viewport-readiness.json'),JSON.stringify({visible:window.isVisible(),minimized:window.isMinimized(),bounds:getViewport().lastBounds,renderer:await window.webContents.executeJavaScript("(()=>{const e=document.querySelector('[data-native-surface]');return {hidden:document.hidden,visible:e?.checkVisibility(),rect:e?.getBoundingClientRect().toJSON()};})()")},null,2));
   await wait(()=>getViewport().state.presentation?.frames>frameStart+10&&getViewport().state.stats?.resolvedMeshes>=1);
   await wait(()=>window.webContents.executeJavaScript(`document.body.innerText.includes('Electron object')&&!document.body.innerText.includes('STALE_SCENE')`));
   report.before=getViewport().state.presentation;await fs.writeFile(path.join(output,'electron-before.png'),(await window.webContents.capturePage()).toPNG());
   await getViewport().stop();
   const qt=process.env.SACURA_QT_COMPAT_HOST||path.join(root,'build/windows-msvc-debug/ElectronNative/Debug/SakuraEditorCompatibilityTest.exe');
   await new Promise((resolve,reject)=>{
     const child=spawn(qt,[created.path,manifest],{windowsHide:true,env:{...process.env,PYTHONHOME:path.join(path.dirname(qt),'Python'),SAKURA_EDITOR_LAYOUT_FILE:path.join(output,'qt-layout.ini')},stdio:['ignore','pipe','pipe']});
     let log='';const timer=setTimeout(()=>{child.kill();reject(Error('Qt round-trip timeout'));},60000);
     child.stdout.on('data',b=>log+=b);child.stderr.on('data',b=>log+=b);child.on('error',reject);
     child.on('close',async code=>{clearTimeout(timer);await fs.writeFile(path.join(output,'qt.log'),log);code===0&&log.includes('QT_COMPATIBILITY_PASS')?resolve():reject(Error('Qt failed: '+log.slice(-5000)));});
   });
   report.checks.push('Qt EditorMainWindow loads scene, reflected components, persistent IDs, parents, assets, subassets and Story Graph; Qt command stack edits and saves');
   const relocated=path.join(output,'Перенос проекта');await fs.cp(path.dirname(created.path),relocated,{recursive:true});
   report.reopenedProject=path.join(relocated,path.basename(created.path));
   await replaceProject(report.reopenedProject);await wait(()=>getViewport().bridge.connected);
   const after=await renderer('scene.getHierarchy');
   const expectedAfter=structuredClone(hierarchy);const edited=expectedAfter.find(o=>o.id===editId);edited.name='Edited in Qt';edited.transform.position[0]=42;
   assert.deepEqual(after,expectedAfter);
   for(const p of properties){const props=await renderer('component.getProperties',picks(p,['objectId','componentId']));assert.deepEqual(p.property==='mesh_sub_asset_id'?props.find(v=>v.id==='mesh_asset_id').subAssetId:props.find(v=>v.id===p.property).value,p.value);}
   const reopenedStory=await renderer('story.get');assert.deepEqual(reopenedStory.document,story.document);assert.deepEqual(reopenedStory.positions,story.positions);
   const reopenedAssets=await renderer('assets.list');for(const a of assets){const actual=reopenedAssets.find(v=>v.id===a.id);assert.ok(actual);assert.deepEqual(picks(actual,['id','path','typeId','subassets']),picks(a,['id','path','typeId','subassets']));}
   report.checks.push('Electron reopens Qt edits; semantic transforms, component properties, every object/component/asset/subasset ID, story nodes/links/layout and target references preserved');
   report.checks.push('Project folder relocated before reopen; relative startup and Content paths resolve without migration');
   report.passed=true;await getViewport().stop();await fs.writeFile(path.join(output,'report.json'),JSON.stringify(report,null,2));console.log('COMPATIBILITY_PASS '+output);app.exit(0);
 }catch(error){report.error=error.stack;await fs.mkdir(output,{recursive:true});await fs.writeFile(path.join(output,'report.json'),JSON.stringify(report,null,2));console.error(error);await getViewport().stop();app.exit(1);}
};
function picks(value,keys){return Object.fromEntries(keys.map(k=>[k,value[k]]));}
