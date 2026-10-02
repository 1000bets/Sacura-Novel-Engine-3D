import test from 'node:test';
import assert from 'node:assert/strict';
import {audioActionOptions,normalizeAudioTiming,quantizationSeconds,nextAudioBoundary} from '../src/audioTransitions.js';
import {projectAudioAssets} from '../src/audioAssets.js';
import {SoundDesk} from '../src/audio.js';
import {BufferPlayer} from '../src/AudioPlayer.js';
import {PreviewRuntime} from '../src/runtime.js';
import {upgradeProject,newEvent,allBeats} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';
class Audio extends EventTarget {duration=10;currentTime=0;timelineTime=0;volume=1;play(){return Promise.resolve();}pause(){} move(time){this.timelineTime=time;this.currentTime=this.loop?time%this.duration:time;this.dispatchEvent(new Event('timeupdate'));}}
async function setup(t,asset={kind:'sound',endMarkers:[2,5,8]},options={}){
 t.mock.timers.enable({apis:['Date','setInterval'],now:1000});
 const desk=new SoundDesk(()=>new Audio());desk.setAssets([{id:'clip',url:'clip.wav',...asset}]);t.after(()=>desk.stopAll());
 desk.play('clip',{key:'clip',volume:.8,...options});await desk.get('clip').started;return {desk,track:desk.get('clip'),move:time=>desk.get('clip').audio.move(time)};
}
const near=(a,b)=>assert.ok(Math.abs(a-b)<1e-9,`${a} ≈ ${b}`);

test('BPM and musical size yield subdivisions, beats and bars on the track timeline',()=>{
 const asset={kind:'music',bpm:120,beatsPerBar:4,beatUnit:4,quantization:'bar'};
 assert.equal(quantizationSeconds(asset),2);assert.equal(quantizationSeconds({...asset,quantization:'1/16'}),.125);
 assert.equal(quantizationSeconds({...asset,beatsPerBar:6,beatUnit:8}),1.5);
 assert.equal(quantizationSeconds({...asset,quantization:'4bars'}),8);
 assert.equal(quantizationSeconds({...asset,kind:'sound'}),0);
 assert.equal(nextAudioBoundary(asset,2,30,false),4);
 assert.equal(nextAudioBoundary({...asset,endMarkers:[3]},2,30,false),3);
 assert.equal(nextAudioBoundary(asset,3.8,3.9,false),3.9);
});
test('marker end is strictly in the future, wraps loops and falls back at the last file end',()=>{
 const asset={endMarkers:[2,5,8]};
 assert.equal(nextAudioBoundary(asset,2,10),5);
 assert.equal(nextAudioBoundary(asset,9,10),10);
 assert.equal(nextAudioBoundary(asset,9,10,true),12);
 assert.equal(nextAudioBoundary(asset,29,10,true),32);
 assert.equal(nextAudioBoundary(asset,12.5,10,true,'file'),20);
 assert.equal(nextAudioBoundary({},3,10),3);
 assert.equal(nextAudioBoundary(asset,3,10,false,'immediate'),3);
});
test('legacy fades and stop duration remain readable; explicit switches disable them',()=>{
 assert.deepEqual(audioActionOptions({type:'stop',duration:1.25}),{fadeIn:0,fadeOut:1.25,endMode:'immediate'});
 assert.equal(audioActionOptions({fade:.7}).fadeIn,.7);
 assert.deepEqual(audioActionOptions({fadeIn:2,fadeOut:3,fadeInEnabled:false,fadeOutEnabled:false,endMode:'file'}),{fadeIn:0,fadeOut:0,endMode:'file'});
 assert.deepEqual(normalizeAudioTiming({endMarkers:[5,2,5,-2,'bad',Infinity,null],bpm:-5,beatsPerBar:0,quantization:'bad'}),{endMarkers:[2,5],bpm:20,beatsPerBar:4,beatUnit:4,quantization:'off'});
});
test('asset timing survives export/import, kind changes and project switching',()=>{
 const project=upgradeProject();project.audioSettings.assets={'music-main':{endMarkers:[3,7],bpm:90,beatsPerBar:3,beatUnit:4,quantization:'2bars'}};
 const loaded=readProject(JSON.stringify(project)),asset=projectAudioAssets(loaded).find(a=>a.id==='music-main');
 assert.deepEqual(asset.endMarkers,[3,7]);assert.equal(quantizationSeconds(asset),4);
 loaded.assetOverrides={'audio-file:music-main':{audioKind:'sound'}};assert.equal(quantizationSeconds(projectAudioAssets(loaded).find(a=>a.id===asset.id)),0);
 assert.deepEqual(projectAudioAssets(upgradeProject()).find(a=>a.id===asset.id).endMarkers,[]);
});
test('stop waits for the next marker and fades exactly towards it while preserving the fader',async t=>{
 const {desk,track,move}=await setup(t,undefined,{fadeOut:2});move(3);
 const done=desk.stop('clip');assert.equal(track.pending.at,5);assert.equal(track.status,'playing');near(track.audio.volume,.8);
 move(4);near(track.audio.volume,.4);near(track.volume,.8);
 move(5);await done;assert.equal(track.status,'stopped');assert.equal(track.pending,null);
});
test('marker release clips an overlong fade without a gain jump',async t=>{
 const {desk,track,move}=await setup(t,undefined,{fadeOut:10});move(4.5);const from=track.audio.volume,done=desk.stop('clip');
 near(track.audio.volume,from);desk.applyVolumes();near(track.audio.volume,from);
 move(4.75);near(track.audio.volume,from/2);move(5);await done;
});
test('looped marker and quantized stops retain the full playback clock',async t=>{
 const {desk,track,move}=await setup(t,{kind:'music',endMarkers:[2],bpm:120,quantization:'bar'},{loop:true});
 move(11);const done=desk.stop('clip');assert.equal(track.pending.at,12);move(11.9);assert.equal(track.status,'playing');move(12);await done;assert.equal(track.status,'stopped');
});
test('file stop lets a repeating clip finish the current pass even without markers',async t=>{
 const {desk,track,move}=await setup(t,{kind:'sound'},{loop:true,endMode:'file'});move(12);
 const done=desk.stop('clip');assert.equal(track.pending.at,20);move(19.5);assert.equal(track.status,'playing');move(20);await done;
});
test('pause preserves an outstanding boundary request and resumes it on the audio clock',async t=>{
 const {desk,track,move}=await setup(t,undefined,{fadeOut:2});move(3);let ended=false;
 const done=desk.stop('clip').then(()=>ended=true);desk.pause('clip');t.mock.timers.tick(4000);await Promise.resolve();assert.equal(ended,false);assert.equal(track.status,'paused');
 await desk.resume('clip');move(4);near(track.audio.volume,.4);move(5);await done;assert.equal(track.status,'stopped');
});
test('immediate fade freezes with global pause and ends after its remaining duration',async t=>{
 const {desk,track}=await setup(t);const done=desk.stop('clip',{endMode:'immediate',fadeOut:2});t.mock.timers.tick(1000);desk.pause('clip');t.mock.timers.tick(5000);assert.equal(track.status,'paused');
 await desk.resume('clip');t.mock.timers.tick(1010);await done;assert.equal(track.status,'stopped');
});
test('safe pause keeps the marker position and resume applies its own fade in',async t=>{
 const {desk,track,move}=await setup(t);move(3);const paused=desk.pause('clip',{endMode:'marker',fadeOut:1});move(5);await paused;
 assert.equal(track.status,'paused');assert.equal(track.audio.currentTime,5);
 await desk.resume('clip',{fadeIn:2});near(track.audio.volume,0);move(6);near(track.audio.volume,.4);
});
test('force shutdown and a replacement cannot be affected by an older boundary',async t=>{
 const {desk,track,move}=await setup(t);move(3);const done=desk.stop('clip');desk.stopAll();await done;assert.equal(track.pending,null);
 desk.play('clip');const current=desk.get('clip');await current.started;track.audio.move(5);t.mock.timers.tick(3000);assert.equal(current.status,'playing');
});
function bufferOutput(){
 const sources=[],parameters=[];const context={currentTime:10,createBufferSource(){const source={connect(){},disconnect(){},start(...args){this.startArgs=args;},stop(...args){this.stopArgs=args;}};sources.push(source);return source;},createGain(){const calls=[],gain={value:1,setValueAtTime(...args){calls.push(['set',...args]);},linearRampToValueAtTime(...args){calls.push(['ramp',...args]);},cancelScheduledValues(...args){calls.push(['cancel',...args]);},setTargetAtTime(){}};parameters.push(calls);return {gain,connect(){},disconnect(){}};}};
 return {sources,parameters,context,normalized:false,master:{},unlock:async()=>{},load:async()=>({buffer:{duration:10},gain:1})};
}
test('Web Audio schedules the exact boundary and release independently of sidechain/fader updates',async()=>{
 const output=bufferOutput(),player=new BufferPlayer('clip',output);player.loop=true;await player.play();
 try{output.context.currentTime=23;assert.equal(player.timelineTime,13);assert.equal(player.currentTime,3);let ended=0;
 player.scheduleBoundary(2,1,()=>ended++);assert.deepEqual(output.sources[0].stopArgs,[25]);assert.deepEqual(output.parameters[1],[['set',1,23],['set',1,24],['ramp',0,25]]);
 player.volume=.2;assert.equal(output.parameters[1].length,3);output.context.currentTime=25.08;output.sources[0].onended();assert.equal(ended,1);assert.equal(player.timelineTime,15);assert.equal(player.currentTime,5);
 }finally{player.pause();}
});
test('Web Audio resumes a loop at its local position and preserves its musical clock',async()=>{
 const output=bufferOutput(),player=new BufferPlayer('clip',output);player.loop=true;await player.play();
 try{output.context.currentTime=33;player.pause();output.context.currentTime=100;await player.play();assert.deepEqual(output.sources[1].startArgs,[0,3]);assert.equal(player.timelineTime,23);
 player.scheduleBoundary(1,2,()=>{});assert.deepEqual(output.parameters[3],[['set',.5,100],['ramp',0,101]]);
 }finally{player.pause();}
});
async function runtimeSetup(t,type='sound',retention='AUTO_CLOSE_ON_FLOW_END'){
 const project=upgradeProject(),event=newEvent(type,'audio','Тест');event.retention=retention;Object.assign(event.groups[0].actions[0],{assetId:'music-main',endMode:'event',fadeInEnabled:false,fadeOutEnabled:false});event.groups.push({id:'after',actions:[{id:'after-action',type:'variable',target:'trust',value:1,operation:'set'}]});project.events.push(event);
 const desk=new SoundDesk(()=>new Audio());t.after(()=>desk.stopAll());const rt=new PreviewRuntime(desk);rt.delay=async(_,token,runId)=>rt.assert(token,runId);return {project,event,desk,rt,beat:allBeats(project)[0]};
}
test('end-of-event sound does not deadlock later groups and finishes at event close',async t=>{
 const {project,event,desk,rt,beat}=await runtimeSetup(t);await rt.previewEvent(project,event.id,beat.id);
 assert.equal(rt.snapshot.variables.trust,1);assert.equal([...desk.tracks.values()][0].status,'stopped');assert.equal(Object.values(rt.snapshot.instances)[0].status,'done');rt.stop();
});
test('held event keeps its event-scoped audio until the instance is explicitly stopped',async t=>{
 const {project,event,desk,rt,beat}=await runtimeSetup(t,'sound','HOLD_UNTIL_STOPPED');await rt.previewEvent(project,event.id,beat.id);
 const track=[...desk.tracks.values()][0];assert.equal(track.status,'playing');rt.controlInstance(track.runId,'stop');await Promise.resolve();assert.equal(track.status,'stopped');rt.stop();
});
test('end-of-event stop is deferred past later actions, then completes without a circular wait',async t=>{
 const {project,event,desk,rt,beat}=await runtimeSetup(t,'stop');desk.setAssets(projectAudioAssets(project));desk.play('music-main',{key:'background'});await desk.get('background').started;
 // previewEvent initializes playback; keep a running preview before creating the background.
 const init=newEvent('wait','world','Пауза');project.events.push(init);await rt.previewEvent(project,init.id,beat.id);
 desk.play('music-main',{key:'background'});await desk.get('background').started;
 await rt.previewEvent(project,event.id,beat.id);assert.equal(rt.snapshot.variables.trust,1);assert.equal(desk.get('background').status,'stopped');rt.stop();
});

test('a voice retains its sidechain until its safe marker stop actually completes',async t=>{
 const {desk,track,move}=await setup(t,{kind:'voice',endMarkers:[5]});
 assert.ok(desk.ducks.has('clip'));move(2);const done=desk.stop('clip');assert.ok(desk.ducks.has('clip'));
 move(5);await done;assert.equal(track.status,'stopped');assert.equal(desk.ducks.has('clip'),false);
});
test('the active music effect remains stopping while audio waits for its boundary',async t=>{
 const {desk,track,move}=await setup(t,{kind:'music',endMarkers:[5]});const rt=new PreviewRuntime(desk);rt.snapshot.effects.music={audioKey:'clip',status:'held'};
 move(2);rt.controlEffect('music','stop');assert.equal(rt.snapshot.effects.music.status,'stopping');assert.equal(track.status,'playing');
 move(5);await Promise.resolve();assert.equal(rt.snapshot.effects.music.status,'stopped');
});
