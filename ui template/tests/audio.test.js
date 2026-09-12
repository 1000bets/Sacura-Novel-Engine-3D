import test from 'node:test';
import assert from 'node:assert/strict';
import {SoundDesk} from '../src/audio.js';
import {BufferPlayer} from '../src/AudioPlayer.js';
import {SIDECHAIN} from '../src/audioSettings.js';

class Audio extends EventTarget {
 constructor(){super();this.volume=1;this.currentTime=0;this.duration=10;}
 play(){return Promise.resolve();}
 pause(){}
}
const near=(actual,expected)=>assert.ok(Math.abs(actual-expected)<1e-9,`${actual} ≈ ${expected}`);
const settle=async()=>{for(let i=0;i<5;i++)await Promise.resolve();};
function setup(t){
 t.mock.timers.enable({apis:['Date','setInterval'],now:1000});
 const desk=new SoundDesk(()=>new Audio());t.after(()=>desk.stopAll());
 const advance=ms=>{while(ms>0){const step=Math.min(16,ms);t.mock.timers.tick(step);ms-=step;}};
 const play=async(asset,key,options={})=>{desk.play(asset,{key,...options});await desk.get(key).started;return desk.get(key);};
 return {desk,advance,play};
}

test('sidechain gently reaches -4 dB over attack and recovers over release',async t=>{
 const {desk,advance,play}=setup(t),music=await play('music-main','music',{volume:.8});
 await play('voice-alice','voice');near(music.audio.volume,.8);
 assert.ok(SIDECHAIN.attack>=.5&&SIDECHAIN.release>=.5);
 advance(300);assert.ok(music.audio.volume<.8&&music.audio.volume>.8*Math.pow(10,-4/20));
 advance(320);near(music.audio.volume,.8*Math.pow(10,-4/20));near(music.volume,.8);
 desk.pause('voice');const ducked=music.audio.volume;
 advance(450);assert.ok(music.audio.volume>ducked&&music.audio.volume<.8);
 advance(470);near(music.audio.volume,.8);assert.equal(desk.duckTimer,null);
});

test('pause, resume, end, stop and error all restore music through the envelope',async t=>{
 const {desk,advance,play}=setup(t),music=await play('music-main','music',{volume:.7});
 for(const finish of ['pause','end','stop','error']){
  const voice=await play('voice-bob','voice');advance(620);const ducked=music.audio.volume;
  if(finish==='pause'||finish==='stop')desk[finish]('voice');
  else voice.audio.dispatchEvent(new Event(finish==='end'?'ended':'error'));
  near(music.audio.volume,ducked);advance(920);near(music.audio.volume,.7);
  if(finish==='pause'){
   desk.resume('voice');await settle();near(music.audio.volume,.7);
   advance(620);near(music.audio.volume,ducked);desk.stop('voice');advance(920);
  }
 }
});

test('overlapping voices share one reduction and do not restart its attack',async t=>{
 const {desk,advance,play}=setup(t),music=await play('music-main','music',{volume:.8});
 await play('voice-alice','alice');advance(300);const transition=desk.duckTransition;
 await play('voice-bob','bob');assert.equal(desk.duckTransition,transition);
 advance(320);near(music.audio.volume,.8*Math.pow(10,-4/20));
 desk.stop('alice');advance(1000);near(music.audio.volume,.8*Math.pow(10,-4/20));
 desk.stop('bob');advance(920);near(music.audio.volume,.8);
});

test('new speech during release starts from the current gain without a jump',async t=>{
 const {desk,advance,play}=setup(t),music=await play('music-main','music',{volume:1});
 await play('voice-alice','alice');advance(620);desk.stop('alice');advance(320);
 const before=music.audio.volume;await play('voice-bob','bob');near(music.audio.volume,before);
 advance(160);assert.ok(music.audio.volume<before);advance(460);near(music.audio.volume,Math.pow(10,-4/20));
 // A replaced voice's late end must not release its successor's reduction.
 const old=desk.get('bob').audio;await play('voice-alice','bob');old.dispatchEvent(new Event('ended'));
 advance(620);assert.ok(desk.ducks.has('bob'));near(music.audio.volume,Math.pow(10,-4/20));
});

test('ducking respects voice opt-out, music fader, fades, and new music tracks',async t=>{
 const {desk,advance,play}=setup(t),music=await play('music-main','music',{volume:.8});
 const voice=await play('voice-alice','voice',{volume:.7,duck:false});advance(1000);near(music.audio.volume,.8);
 desk.duck('manual',true);advance(620);near(voice.audio.volume,.7);
 desk.setVolume('music',.5);near(music.audio.volume,.5*Math.pow(10,-4/20));
 const other=await play('music-main','other',{volume:.6});near(other.audio.volume,.6*Math.pow(10,-4/20));
 const fade=desk.fade('music',.2,.4);advance(420);await fade;
 near(music.volume,.5);near(music.audio.volume,.2*Math.pow(10,-4/20));
 desk.duck('manual',false);advance(920);near(music.audio.volume,.2);near(other.audio.volume,.6);
});

test('stopAll cancels pending envelope updates and resets the next playback',async t=>{
 const {desk,advance,play}=setup(t);await play('music-main','music');await play('voice-alice','voice');advance(200);
 desk.stopAll();assert.equal(desk.duckTimer,null);assert.equal(desk.duckTransition,null);assert.equal(desk.ducks.size,0);
 const music=await play('music-main','new',{volume:.8});advance(2000);near(music.audio.volume,.8);
});

test('Web Audio starts at the requested gain and smooths live envelope steps',async()=>{
 const ramps=[],param={value:1,setTargetAtTime(...args){ramps.push(args);}},source={connect(){},disconnect(){},start(){},stop(){}},
 output={normalized:true,master:{},unlock:async()=>{},load:async()=>({buffer:{duration:10},gain:2}),
  context:{currentTime:3,createBufferSource:()=>source,createGain:()=>({gain:param,connect(){},disconnect(){}})}};
 const player=new BufferPlayer('test',output);player.volume=.4;await player.play();
 try{near(param.value,.8);assert.equal(ramps.length,0);player.volume=.3;assert.deepEqual(ramps,[[.6,3,.008]]);}
 finally{player.pause();}
});
