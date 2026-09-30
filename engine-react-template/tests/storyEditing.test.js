import test from 'node:test';
import assert from 'node:assert/strict';
import {playbackStartId,deleteStoryNode} from '../src/storyEditing.js';
import {upgradeProject,allBeats} from '../src/studioModel.js';
import {PreviewRuntime} from '../src/runtime.js';

test('normal playback ignores the selected inserted node and follows its wires',async()=>{
 const p=upgradeProject();
 for(const b of allBeats(p)){b.bindings=[];b.batches={};}
 const chapter=p.chapters.find(c=>c.beats.some(b=>b.id==='a3'));
 chapter.beats.push({id:'inserted',kind:'dialogue',speaker:'Алиса',text:'Новая реплика',next:'a4',bindings:[],batches:{}});
 chapter.beats.find(b=>b.id==='a3').next='inserted';
 const runtime=new PreviewRuntime({stopAll(){}});
 await runtime.start(p,playbackStartId(p,'inserted'));
 for(let i=0;i<4;i++)await runtime.advance();
 assert.deepEqual(runtime.snapshot.history,['a1','a2','a3','inserted','a4']);
 assert.equal(playbackStartId(p,'inserted',true),'inserted');
 assert.equal(playbackStartId(p,'garden-walk'),'garden-entry');
 runtime.stop();
});
test('deleting a node clears incoming routes and preserves shared events and unrelated nodes',()=>{
 const p=upgradeProject();
 const scene=p.subscenes[0];
 const entry=scene.entry,successor=allBeats(p).find(b=>b.id===entry).next;
 const choice=allBeats(p).find(b=>b.kind==='choice');
 choice.choices[0].next=entry;
 const before=structuredClone(p.events),count=allBeats(p).length;
 const result=deleteStoryNode(p,entry);
 assert.equal(result.nextId,successor);
 assert.equal(scene.entry,successor);
 assert.equal(allBeats(p).length,count-1);
 assert.equal(choice.choices[0].next,null);
 assert.deepEqual(p.events,before);
 assert.ok(allBeats(p).every(b=>b.next!==entry&&(b.choices||[]).every(c=>c.next!==entry)));
});
test('last node and unknown node deletion leave the project untouched',()=>{
 const p={chapters:[{subsceneId:'scene',beats:[{id:'only'}]}],subscenes:[{id:'scene',entry:'only'}]};
 const before=structuredClone(p);
 assert.ok(deleteStoryNode(p,'only').error);
 assert.ok(deleteStoryNode(p,'missing').error);
 assert.deepEqual(p,before);
});

test('rewiring a running or finished snapshot stops stale playback; restart follows visible routes',async()=>{
 const p=upgradeProject();
 for(const b of allBeats(p)){b.bindings=[];b.batches={};}
 const a1=allBeats(p).find(b=>b.id==='a1');
 a1.next=null;
 const runtime=new PreviewRuntime({stopAll(){}});
 await runtime.start(p,'a1');
 await runtime.advance();
 assert.equal(runtime.snapshot.phase,'ERROR');
 assert.match(runtime.snapshot.error,/a1.*Дальше/);
 a1.next='a2';
 assert.equal(runtime.invalidateStoryPreview(p),true);
 assert.equal(runtime.snapshot.phase,'EDIT');
 assert.equal(runtime.running,false);
 await runtime.start(p,'a1');
 await runtime.advance();
 assert.equal(runtime.snapshot.beatId,'a2');
 allBeats(p).find(b=>b.id==='a2').next='a4';
 assert.equal(runtime.invalidateStoryPreview(p),true);
 await runtime.start(p,'a2');
 await runtime.advance();
 assert.equal(runtime.snapshot.beatId,'a4');
 runtime.stop();
});

test('text edits do not invalidate routes, but undoing a connection does',async()=>{
 const p=upgradeProject();
 for(const b of allBeats(p)){b.bindings=[];b.batches={};}
 const runtime=new PreviewRuntime({stopAll(){}});
 const before=structuredClone(p);
 allBeats(p).find(b=>b.id==='a1').next='a3';
 await runtime.start(p,'a1');
 allBeats(p).find(b=>b.id==='a1').text='Edited';
 assert.equal(runtime.invalidateStoryPreview(p),false);
 assert.equal(runtime.invalidateStoryPreview(before),true);
 assert.equal(runtime.snapshot.phase,'EDIT');
});

test('a visible story wire survives save/load and is followed by playback',async()=>{
 const {applyConnections}=await import('../src/storyConnections.js');
 const {buildStoryFlow}=await import('../src/storyFlowModel.js');
 const {readProject}=await import('../src/projectFiles.js');
 const p=upgradeProject();
 for(const b of allBeats(p)){b.bindings=[];b.batches={};}
 allBeats(p).find(b=>b.id==='a1').next=null;
 assert.equal(applyConnections(p,[{source:'a1',sourceHandle:'next',target:'a2',targetHandle:'in'}]),null);
 const saved=readProject(JSON.stringify(p));
 const flow=buildStoryFlow(saved);
 assert.equal(flow.nodes.find(n=>n.id==='a1').data.ports[0].next,'a2');
 assert.equal(flow.edges.find(e=>e.source==='a1').target,'a2');
 const runtime=new PreviewRuntime({stopAll(){}});
 await runtime.start(saved,'a1');
 await runtime.advance();
 assert.equal(runtime.snapshot.beatId,'a2');
 assert.notEqual(runtime.snapshot.phase,'FINISHED');
 runtime.stop();
});

test('only an explicit ending finishes; a missing continuation reports its source',async()=>{
 const p=upgradeProject();
 for(const b of allBeats(p)){b.bindings=[];b.batches={};}
 const runtime=new PreviewRuntime({stopAll(){}});
 for(const next of [null,'deleted-node']){
  allBeats(p).find(b=>b.id==='a1').next=next;
  await runtime.start(p,'a1');
  await runtime.advance();
  assert.equal(runtime.snapshot.phase,'ERROR');
  assert.match(runtime.snapshot.error,/a1/);
 }
 await runtime.start(p,'d6');
 await runtime.advance();
 assert.equal(runtime.snapshot.phase,'FINISHED');
 runtime.stop();
});
