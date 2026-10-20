import test from 'node:test';
import assert from 'node:assert/strict';
import {moveEventBinding} from '../src/eventPlacement.js';
import {upgradeProject,allBeats,addToBatch,batchesFor,bindingActions} from '../src/studioModel.js';
import {createObjectGroup,addObjectsToGroup,sceneObjectGroups} from '../src/objectGroups.js';
import {collectAssetFiles,updateAssetFile} from '../src/assetFiles.js';
import {projectAudioAssets} from '../src/audioAssets.js';
import {makeAction} from '../src/model.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';

test('drag placement moves an existing event across phases and beats with overrides and order intact',()=>{
 const p=upgradeProject(),[a,b]=allBeats(p),e=p.events.find(e=>e.standardPreset),binding=addToBatch(p,a.id,'BEFORE',null,e.id);
 binding.actionOverrides={custom:{footstepAssetId:'steps'}};
 const other=addToBatch(p,b.id,'AFTER',null,e.id),batch=batchesFor(b,'AFTER').find(batch=>batch.bindingIds.includes(other.id));
 assert.ok(moveEventBinding(p,a.id,binding.id,b.id,'AFTER',batch.id,other.id));
 assert.equal(a.bindings.some(item=>item.id===binding.id),false);
 assert.equal(b.bindings.find(item=>item.id===binding.id).hook,'AFTER');
 assert.deepEqual(b.batches.AFTER.find(g=>g.id===batch.id).bindingIds,[binding.id,other.id]);
 assert.equal(binding.actionOverrides.custom.footstepAssetId,'steps');
 assert.ok(moveEventBinding(p,b.id,binding.id,b.id,'ON_START'));
 assert.equal(b.bindings.filter(item=>item.id===binding.id).length,1);
 const restored=upgradeProject(JSON.parse(JSON.stringify(p)));
 assert.equal(allBeats(restored).find(item=>item.id===b.id).bindings.find(item=>item.id===binding.id).hook,'ON_START');
 assert.equal(moveEventBinding(p,b.id,binding.id,'missing','AFTER'),false);
});
test('object groups persist, move membership without duplicates and keep original transforms',()=>{
 const p={objects:[{id:'a',transforms:{s:{position:[1,2,3]}}},{id:'b'},{id:'c'}]},before=structuredClone(p.objects);
 const first=createObjectGroup(p,'s',['a','b']),second=createObjectGroup(p,'s',['b','c']);
 assert.deepEqual(first.objectIds,['a']);assert.deepEqual(second.objectIds,['b','c']);
 addObjectsToGroup(p,first.id,['b','b']);assert.deepEqual(first.objectIds,['a','b']);assert.deepEqual(second.objectIds,['c']);
 assert.deepEqual(p.objects,before);
 const restored=JSON.parse(JSON.stringify(p));assert.equal(sceneObjectGroups(restored,'s',restored.objects).length,2);
 assert.equal(createObjectGroup(p,'s',['missing','a']),null);
});
test('file rename preserves extension, updates references, builtin audio metadata and rejects collisions',()=>{
 const p={objects:[{id:'o',model:{src:'mesh',name:'old.obj'}}],assetFiles:[{id:'f',name:'old.obj',path:'Models/old.obj',src:'mesh',kind:'model',model:{name:'old.obj',format:'obj'}}]};
 const file=collectAssetFiles(p)[0];updateAssetFile(p,file,{name:'Chair'});
 assert.equal(p.assetFiles[0].path,'Models/Chair.obj');assert.equal(p.objects[0].model.name,'Chair.obj');
 assert.throws(()=>updateAssetFile(p,file,{name:'../bad'}),/имя файла/);
 p.assetFiles.push({id:'other',path:'Models/Taken.obj'});assert.throws(()=>updateAssetFile(p,file,{name:'Taken'}),/существует/);
 const audio=collectAssetFiles(p,projectAudioAssets(p)).find(f=>f.audioId==='music-main');
 updateAssetFile(p,audio,{name:'New theme',audioKind:'sound'});
 const restored=JSON.parse(JSON.stringify(p)),builtin=projectAudioAssets(restored).find(f=>f.id==='music-main');
 assert.equal(builtin.kind,'sound');assert.equal(builtin.file,'New theme.wav');assert.match(builtin.url,/MainMenuSound/);
 assert.equal(collectAssetFiles(restored,projectAudioAssets(restored)).find(f=>f.audioId==='music-main').name,'New theme.wav');
});
class Audio extends EventTarget{constructor(){super();this.duration=1;this.currentTime=0;this.volume=1;}play(){return Promise.resolve();}pause(){}}
async function previewMove(assetId,cancel=false){
 const p=upgradeProject(),event=p.events.find(e=>e.standardPreset&&e.groups[0].actions[0].type==='move');
 event.groups=[event.groups[0]];const action=event.groups[0].actions[0];action.duration=.1;action.footstepAssetId=assetId;action.footstepVolume=.4;
 p.assetFiles=[{id:'steps',name:'steps.wav',path:'Sounds/steps.wav',kind:'audio',src:'fake',audioKind:'sound'}];
 const desk=new SoundDesk(()=>new Audio()),played=[];const play=desk.play.bind(desk);desk.play=(id,options)=>{played.push({id,options});return play(id,options);};
 const rt=new PreviewRuntime(desk);
 rt.delay=async (ms,token,runId)=>{rt.assert(token,runId);if(cancel&&rt.snapshot.world.motions?.[action.target]?.progress>.4){rt.stop();rt.assert(token,runId);}};
 await rt.previewEvent(p,event.id,allBeats(p)[0].id);
 const tracks=[...desk.tracks.values()];rt.stop();return {played,tracks};
}
test('move actions default to silence; selected footsteps loop only during movement',async()=>{
 assert.equal(makeAction('move').footstepAssetId,'');
 assert.deepEqual((await previewMove('')).played,[]);
 const result=await previewMove('steps');assert.equal(result.played.length,1);assert.equal(result.played[0].id,'steps');
 assert.equal(result.played[0].options.loop,true);assert.equal(result.played[0].options.duck,false);assert.equal(result.played[0].options.volume,.4);
 assert.ok(result.tracks.every(track=>track.status==='stopped'));
 const cancelled=await previewMove('steps',true);assert.ok(cancelled.tracks.every(track=>track.status==='stopped'));
});
