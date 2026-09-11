import test from 'node:test';
import assert from 'node:assert/strict';
import {createProject} from '../src/model.js';
import {upgradeProject,allBeats,newEvent,normalizeBatches,validateStudio,fixStudio,chooseNext,sceneFor,bindingActions} from '../src/studioModel.js';
import {SoundDesk} from '../src/audio.js';
import {PreviewRuntime} from '../src/runtime.js';

const tick=()=>new Promise(resolve=>setTimeout(resolve,4));
function sceneProject(events,beats){return {version:2,title:'test',events,objects:[{id:'alice',type:'Персонаж',active:true}],variables:{trust:0,letter:false},subscenes:[{id:'living',entry:beats[0].id,name:'Room',location:'Room',weather:'Дождь',time:'Закат'}],chapters:[{id:'c',subsceneId:'living',beats:beats.map(normalizeBatches)}]};}
const binding=(id,eventId,hook='ON_START',join='EVENT_END')=>({id,eventId,hook,join,overrides:{},actionOverrides:{}});
const line=(id,bindings=[],next=null,extra={})=>({id,kind:'dialogue',speaker:'Алиса',text:id,mode:'SEQUENTIAL',bindings,next,...extra});
class AudioMock {
 constructor(){this.tracks=new Map();this.calls=[];this.ducks=new Set();}
 stopAll(){this.calls.push('stopAll');for(const t of this.tracks.values())t.finish();}
 play(assetId,options){this.calls.push('play:'+options.key);const t={assetId,status:'playing',...options};t.done=new Promise(resolve=>t.finish=resolve);this.tracks.set(options.key,t);return t.done;}
 get(key){return this.tracks.get(key);} pause(key){this.calls.push('pause:'+key);} resume(key){this.calls.push('resume:'+key);} stop(key){this.calls.push('stop:'+key);this.tracks.get(key)?.finish();}
 fade(){return Promise.resolve();}duck(){}
}

test('migration retains user templates, explicit edges, properties and is idempotent',()=>{
 const old=createProject();old.events.push({...newEvent(),id:'my-event',name:'Пользовательское событие',custom:{value:7}});old.chapters[0].beats[0].custom='keep';old.chapters[1].beats.find(b=>b.id==='lh3').next='a2';
 const p=upgradeProject(old);assert.equal(p.events.find(e=>e.id==='my-event').custom.value,7);assert.equal(allBeats(p)[0].custom,'keep');assert.equal(allBeats(p).find(b=>b.id==='lh3').next,'a2');assert.deepEqual(upgradeProject(p),p);assert.equal(old.version,1);
 const ids=allBeats(p).flatMap(b=>b.bindings.map(x=>x.id));assert.equal(new Set(ids).size,ids.length);
});
test('all bindings belong to exactly one phase batch, mixed order is explicit',()=>{
 const p=upgradeProject();for(const b of allBeats(p)){const batchIds=Object.values(b.batches).flatMap(groups=>groups.flatMap(g=>g.bindingIds));assert.deepEqual([...batchIds].sort(),b.bindings.map(x=>x.id).sort());}
 const b=allBeats(p).find(b=>b.id==='a1');assert.deepEqual(b.batches.BEFORE.map(g=>g.mode),['PARALLEL','SEQUENTIAL']);assert.equal(b.batches.ON_START[0].mode,'PARALLEL');assert.equal(b.batches.AFTER[0].mode,'SEQUENTIAL');
});
test('different choices lead to distinct subscenes, nested choices and three endings',()=>{
 const p=upgradeProject(),vars={trust:5,letter:true};assert.equal(chooseNext(p,'choice1','hide',vars).id,'ls1');assert.equal(chooseNext(p,'choice1','honest',{trust:0}),null);
});
test('graph is traversable and includes all three endings with concrete destinations',()=>{
 const p=upgradeProject(),seen=new Set(),queue=['a1'];while(queue.length){const id=queue.shift();if(seen.has(id))continue;seen.add(id);const b=allBeats(p).find(b=>b.id===id);assert.ok(b,id);for(const next of b.kind==='choice'?b.choices.map(c=>c.next):b.next?[b.next]:[])queue.push(next);}
 assert.ok(seen.has('garden-end'));assert.ok(seen.has('station-end'));assert.ok(seen.has('d6'));assert.equal(sceneFor(p,chooseNext(p,'ls3').id).id,'station');assert.equal(sceneFor(p,chooseNext(p,'la3').id).id,'garden');
});
test('parallel Move conflicts and correction changes real waits',()=>{
 const p=upgradeProject(),issue=validateStudio(p).find(i=>i.id.startsWith('overlap-l3'));assert.ok(issue);const fixed=fixStudio(p,issue);assert.ok(!validateStudio(fixed).some(i=>i.id.startsWith('overlap-l3')));assert.equal(allBeats(fixed).find(b=>b.id==='l3').batches.ON_START[0].mode,'SEQUENTIAL');
});
test('unjoined movement remains a conflict across phase and batch boundaries',()=>{
 const a=newEvent(),b=newEvent();const p=sceneProject([a,b],[line('a',[binding('first',a.id,'BEFORE','NONE'),binding('second',b.id)])]);const issue=validateStudio(p).find(i=>i.id.startsWith('unjoined-'));assert.ok(issue);assert.ok(!validateStudio(fixStudio(p,issue)).length);
});
test('local overrides cannot create a hidden duplicate Move',()=>{
 const e=newEvent();const pose=newEvent('pose').groups[0].actions[0];e.groups[0].actions.push(pose);const b=binding('b',e.id);b.actionOverrides[pose.id]={type:'move',target:'alice'};const p=sceneProject([e],[line('a',[b])]);assert.equal(bindingActions(p,b).filter(a=>a.type==='move').length,2);assert.ok(validateStudio(p).some(i=>i.id.startsWith('local-')));
});
test('runtime really waits BEFORE, starts parallel audio together, then executes AFTER',async()=>{
 const makeVoice=name=>{const e=newEvent('sound','world',name,name);e.groups[0].actions[0].assetId='voice-narrator';return e;};const pre=makeVoice('pre'),left=makeVoice('left'),right=makeVoice('right'),after=newEvent('variable','trust','+1');const bs=[binding('pre',pre.id,'BEFORE'),binding('left',left.id),binding('right',right.id),binding('after',after.id,'AFTER')];const b=line('a',bs,null,{batches:{BEFORE:[{id:'before',mode:'SEQUENTIAL',bindingIds:['pre']}],ON_START:[{id:'parallel',mode:'PARALLEL',bindingIds:['left','right']}],AFTER:[{id:'after',mode:'SEQUENTIAL',bindingIds:['after']}]}});const p=sceneProject([pre,left,right,after],[b]),audio=new AudioMock(),runtime=new PreviewRuntime(audio);runtime.delay=async(_,token)=>runtime.assert(token);const start=runtime.start(p,'a');await tick();assert.equal(runtime.snapshot.textVisible,false);assert.equal(runtime.snapshot.variables.trust,0);audio.get(pre.groups[0].actions[0].id).finish();await tick();assert.equal(runtime.snapshot.textVisible,true);assert.ok(audio.get(left.groups[0].actions[0].id));assert.ok(audio.get(right.groups[0].actions[0].id));assert.equal(runtime.snapshot.ready,false);audio.get(left.groups[0].actions[0].id).finish();await tick();assert.equal(runtime.snapshot.ready,false);audio.get(right.groups[0].actions[0].id).finish();await start;assert.equal(runtime.snapshot.ready,true);await runtime.advance();assert.equal(runtime.snapshot.variables.trust,1);assert.equal(runtime.snapshot.phase,'FINISHED');runtime.stop();
});
test('false binding conditions skip work; selecting an object cannot skip a dialogue',async()=>{
 const e=newEvent('variable','trust','+1'),b=binding('b',e.id);b.condition='letter';const p=sceneProject([e],[line('a',[b],'b'),line('b')]),rt=new PreviewRuntime(new AudioMock());await rt.start(p,'a');assert.equal(rt.snapshot.variables.trust,0);assert.equal(rt.snapshot.states.b,'skipped');await rt.advance(null,'alice');assert.equal(rt.snapshot.beatId,'a');await rt.advance();assert.equal(rt.snapshot.beatId,'b');rt.stop();
});
test('ending cancels late unjoined actions before they can restart music',async()=>{
 const e=newEvent('move','alice','окно');e.groups[0].actions[0].duration=.1;e.groups.push(newEvent('music','audio','music').groups[0]);const audio=new AudioMock(),rt=new PreviewRuntime(audio);const p=sceneProject([e],[line('a',[binding('b',e.id,'BEFORE','NONE')])]);await rt.start(p,'a');await rt.advance();assert.equal(rt.snapshot.phase,'FINISHED');await new Promise(r=>setTimeout(r,170));assert.ok(!audio.calls.some(x=>x.startsWith('play:')));rt.stop();
});
test('subscene exit stops local sound and preserves music owned by the scene',async()=>{
 const music=newEvent('music','audio','theme');music.owner='Scene';const voice=newEvent('sound','world','voice');voice.groups[0].actions[0].assetId='voice-alice';
 const p=sceneProject([music,voice],[line('a',[binding('music',music.id,'BEFORE','FLOW_END'),binding('voice',voice.id,'ON_START','NONE')],'b')]);
 p.subscenes.push({id:'garden',name:'Garden',entry:'b',weather:'Гроза',time:'Ночь'});p.chapters.push({id:'garden',subsceneId:'garden',beats:[normalizeBatches(line('b'))]});
 const audio=new AudioMock(),rt=new PreviewRuntime(audio);rt.delay=async(_,token)=>rt.assert(token);await rt.start(p,'a');await tick();assert.ok(audio.get(voice.groups[0].actions[0].id));await rt.advance();assert.equal(rt.snapshot.world.location,'garden');assert.equal(rt.snapshot.world.weather,'Гроза');assert.ok(audio.calls.includes('stop:'+voice.groups[0].actions[0].id));assert.ok(!audio.calls.includes('stop:background'));assert.equal(rt.snapshot.effects.music.owner,'Scene');rt.stop();
});
class FakeAudio extends EventTarget{constructor(){super();this.volume=1;this.currentTime=0;this.duration=10;this.loop=false;}play(){return Promise.resolve();}pause(){}}
test('real audio transport ducks music and restores it on pause, resume, end and stop',async()=>{
 const desk=new SoundDesk(()=>new FakeAudio());desk.play('music-main',{key:'music',volume:.8,loop:true});desk.play('voice-bob',{key:'voice',duck:true});await tick();assert.ok(desk.get('music').audio.volume<.21);desk.pause('voice');assert.equal(desk.get('music').audio.volume,.8);desk.resume('voice');await tick();assert.ok(desk.get('music').audio.volume<.21);desk.get('voice').audio.dispatchEvent(new Event('ended'));assert.equal(desk.get('music').audio.volume,.8);desk.play('voice-alice',{key:'voice2'});await tick();desk.stop('voice2');assert.equal(desk.get('music').audio.volume,.8);desk.stopAll();
});
test('interrupting fade settles its promise and does not deadlock AFTER',async()=>{
 for(const command of ['pause','stop','setVolume']){const desk=new SoundDesk(()=>new FakeAudio());desk.play('music-main',{key:'bg',volume:.6});await tick();const fade=desk.fade('bg',0,2);desk[command]('bg',.3);const result=await Promise.race([fade.then(()=>true),new Promise(resolve=>setTimeout(()=>resolve(false),100))]);assert.equal(result,true,command);desk.stopAll();}
});
