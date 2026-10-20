import test from 'node:test';
import assert from 'node:assert/strict';
import {createStandardProject} from '../src/projectLifecycle.js';
import {upgradeProject,chooseNext} from '../src/studioModel.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';

test('Letters starter actually visits both connecting scenes instead of bypassing them via explicit branch destinations',async()=>{
 const p=createStandardProject(),legacy=upgradeProject(),beats=p.chapters.flatMap(c=>c.beats);
 // This test isolates story routing. Full staging/audio and physical interaction
 // are exercised separately in interactive-play and the recorded browser run.
 for(const b of beats){b.bindings=[];b.batches={};}
 const rt=new PreviewRuntime(new SoundDesk(()=>{}));
 try{for(const [start,expected]of [['lh3',['lh3','merge1','l4','s1']],['ch2',['ch2','merge4','d1']]]){
  await rt.start(p,start);const observed=[];for(let i=0;i<expected.length;i++){assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.ready,true);observed.push(rt.snapshot.beatId);if(i<expected.length-1)await rt.advance();}assert.deepEqual(observed,expected);
 }}finally{rt.stop();}
 assert.equal(legacy.chapters.flatMap(c=>c.beats).find(b=>b.id==='lh3').next,'s1','legacy teaching/compatibility fixture remains unchanged');
});

test('every Letters story block is reachable through actual choice outputs, rather than unused default choice exits',()=>{
 const p=createStandardProject(),beats=p.chapters.flatMap(c=>c.beats),seen=new Set(),todo=[p.subscenes[0].entry],variables={...p.variables,letter:true,trust:100};
 while(todo.length){const id=todo.pop();if(seen.has(id))continue;seen.add(id);const b=beats.find(b=>b.id===id);assert.ok(b,'available destination '+id);
  if(b.kind==='end')continue;
  for(const choice of b.kind==='choice'?b.choices:[null]){const next=chooseNext(p,id,choice?.id,variables,{});assert.ok(next,'playable exit '+id+' '+(choice?.label||''));todo.push(next.id);}
 }
 assert.deepEqual(beats.filter(b=>!seen.has(b.id)).map(b=>b.text),[]);
});
