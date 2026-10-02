import {audioActionOptions,nextAudioBoundary,normalizeAudioTiming} from './audioTransitions.js';
import {audioEnvelope} from './audioEnvelope.js';
import {AUDIO_ASSETS} from './studioModel.js';
import {AudioOutput,BufferPlayer} from './AudioPlayer.js';
import {normalizeSidechain} from './audioSettings.js';
export class SoundDesk {
 constructor(factory){this.output=factory?null:new AudioOutput();this.factory=factory||(url=>new BufferPlayer(url,this.output));this.assets=new Map(AUDIO_ASSETS.map(asset=>[asset.id,asset]));this.tracks=new Map();this.listeners=new Set();this.peaks=new Map();this.ducks=new Set();this.sidechain=normalizeSidechain();this.duckGain=1;this.duckTarget=1;this.duckTransition=null;this.duckTimer=null;}
 setAssets(assets){this.assets=new Map(assets.map(asset=>[asset.id,asset]));}
 unlock(){return this.output?.unlock()||Promise.resolve();}
 subscribe(fn){this.listeners.add(fn);return()=>this.listeners.delete(fn);}
 emit(){this.listeners.forEach(fn=>fn());}
 get(key){return this.tracks.get(key);}
 async waveform(asset){
  if(!this.peaks.has(asset.url)){
   const pending=(async()=>{const {buffer}=await this.output.load(asset.url);const data=buffer.getChannelData(0),peaks=[];for(let i=0;i<100;i++){const start=Math.floor(i*data.length/100),end=Math.floor((i+1)*data.length/100);let peak=0;for(let j=start;j<end;j+=5)peak=Math.max(peak,Math.abs(data[j]));peaks.push(peak);}return {peaks,duration:buffer.duration};})();
   this.peaks.set(asset.url,pending);pending.catch(()=>this.peaks.delete(asset.url));
  }return this.peaks.get(asset.url);
 }
 duckLevel(now=Date.now()){
  const transition=this.duckTransition;if(!transition)return this.duckGain;
  const progress=Math.max(0,Math.min(1,(now-transition.start)/transition.duration)),eased=progress*progress*(3-2*progress);
  return transition.from+(this.duckTarget-transition.from)*eased;
 }
 setSidechain(settings){
  const next=normalizeSidechain(settings),previous=this.sidechain;
  if(Object.keys(next).every(key=>next[key]===previous[key]))return;
  this.sidechain=next;
  const target=this.ducks.size?Math.pow(10,-next.reductionDb/20):1,direction=target<this.duckLevel()?'attack':'release';
  this.updateDucking(Boolean(this.duckTransition&&next[direction]!==previous[direction]));
  this.applyVolumes();
 }
 updateDucking(retime=false){
  const target=this.ducks.size?Math.pow(10,-this.sidechain.reductionDb/20):1;
  if(target===this.duckTarget&&!retime)return;
  const now=Date.now();this.duckGain=this.duckLevel(now);this.duckTarget=target;
  clearInterval(this.duckTimer);this.duckTimer=null;
  if(target===this.duckGain){this.duckTransition=null;return;}
  this.duckTransition={from:this.duckGain,start:now,duration:(target<this.duckGain?this.sidechain.attack:this.sidechain.release)*1000};
  this.duckTimer=setInterval(()=>{
   const now=Date.now();this.duckGain=this.duckLevel(now);
   if(now>=this.duckTransition.start+this.duckTransition.duration){this.duckGain=this.duckTarget;this.duckTransition=null;clearInterval(this.duckTimer);this.duckTimer=null;}
   // Playing tracks already refresh the UI through their 80 ms timeupdate.
   this.applyVolumes(!this.duckTransition);
  },16);
 }
 position(t){return Number.isFinite(t.audio.timelineTime)?t.audio.timelineTime:t.audio.currentTime;}
 applyVolumes(notify=true){
  const gain=this.duckLevel();
  for(const t of this.tracks.values()){
   const p=t.pending,position=this.position(t),attack=audioEnvelope(Math.max(0,position-(t.fadeOrigin||0)),Infinity,t.fadeIn,0);
   const natural=t.audio.loop?1:audioEnvelope(t.audio.currentTime,t.audio.duration,0,t.fadeOut);
   const release=p&&!p.scheduled&&!p.suspended&&p.fadeOut?Math.max(0,Math.min(1,(p.immediate?p.fadeOut-(p.suspended?p.elapsed:(Date.now()-p.start)/1000):p.at-position)/p.fadeOut)):1;
   const volume=Math.max(0,Math.min(1,t.volume*t.envelope*attack*(p?p.from:natural)*release*(t.kind==='music'?gain:1)));
   if(t.audio.volume!==volume)t.audio.volume=volume;
  }
  if(notify)this.emit();
 }
 refreshVolumes(){this.updateDucking();this.applyVolumes();}
 duck(key,on){on?this.ducks.add(key):this.ducks.delete(key);this.refreshVolumes();}
 play(assetId,options={}){
  const {key=assetId,volume=.75,loop=false,duck=true}=options,{fadeIn,fadeOut,endMode}=audioActionOptions(options);
  const asset=this.assets.get(assetId);if(!asset)return Promise.reject(new Error('Назначьте звуковой файл'));
  this.unlock().catch(()=>{});this.stopNow(key);const audio=this.factory(asset.url);audio.preload='auto';audio.loop=loop;
  const t={key,assetId,audio,kind:asset.kind,status:'loading',volume,envelope:1,fadeIn,fadeOut,endMode,asset:{...asset,...normalizeAudioTiming(asset)},duck:duck&&asset.kind==='voice',error:null,revision:0};this.tracks.set(key,t);
  t.done=new Promise(resolve=>{t.finish=resolve;});
  const current=()=>this.tracks.get(key)===t;
  const finish=status=>{if(!current()||['stopped','ended'].includes(t.status))return;this.cancelPending(t);this.cancelFade(t);t.status=status;this.ducks.delete(key);this.refreshVolumes();t.finish();};
  const fail=e=>{if(!current()||['stopped','paused'].includes(t.status))return;t.error=e?.name==='NotAllowedError'?'Звук заблокирован. Нажмите «Слушать», чтобы включить аудио.':`${asset.file}: ${e?.message||'не удалось декодировать файл'}`;finish('error');};
  audio.addEventListener('ended',()=>finish('ended'));audio.addEventListener('error',fail);
  audio.addEventListener('timeupdate',()=>{if(!current())return;this.checkPending(t);this.applyVolumes();});audio.addEventListener('loadedmetadata',()=>current()&&this.applyVolumes());this.refreshVolumes();
  t.started=Promise.resolve().then(()=>{if(current()&&t.status==='loading')return audio.play();}).then(()=>{if(!current()||['stopped','paused'].includes(t.status))return;t.status='playing';if(t.duck)this.ducks.add(key);this.refreshVolumes();}).catch(fail);
  this.emit();return t.done;
 }
 cancelPending(t){if(!t?.pending)return;const pending=t.pending;t.pending=null;t.audio.cancelBoundary?.();clearInterval(pending.timer);pending.resolve();}
 checkPending(t){const p=t.pending;if(p&&!p.suspended&&!p.scheduled&&(p.immediate?(Date.now()-p.start)/1000>=p.fadeOut:this.position(t)>=p.at-1e-6)){this.completeBoundary(t,p);}}
 completeBoundary(t,p){if(t.pending!==p)return;this.cancelPending(t);if(p.command==='pause')this.pauseNow(t.key);else this.stopNow(t.key);}
 transition(key,command,options={}){
  const t=this.get(key);if(!t||['ended','stopped','error'].includes(t.status))return Promise.resolve();
  if(t.status==='paused'){if(command==='stop')this.stopNow(key);return Promise.resolve();}
  const opts=audioActionOptions({fadeIn:t.fadeIn,fadeOut:t.fadeOut,endMode:t.endMode,...options});
  if(t.pending){if(t.pending.command===command&&t.pending.mode===opts.endMode&&t.pending.requestedFadeOut===opts.fadeOut)return t.pending.done;this.cancelPending(t);}
  const begin=()=>{
   if(this.get(key)!==t||t.status!=='playing')return Promise.resolve();
   const position=this.position(t),mode=opts.endMode==='event'?'marker':opts.endMode;
   const boundary=nextAudioBoundary(t.asset,position,t.audio.duration,t.audio.loop,mode),immediate=boundary<=position+1e-6;
   const target=immediate?position+opts.fadeOut:boundary,at=t.audio.loop?target:Math.min(t.audio.duration,target);
   if(at<=position+1e-6){command==='pause'?this.pauseNow(key):this.stopNow(key);return Promise.resolve();}
   this.cancelFade(t);const p={at,from:t.audio.loop?1:audioEnvelope(t.audio.currentTime,t.audio.duration,0,t.fadeOut),fadeOut:Math.min(opts.fadeOut,at-position),requestedFadeOut:opts.fadeOut,command,mode:opts.endMode,immediate,start:Date.now()};p.done=new Promise(resolve=>{p.resolve=resolve;});t.pending=p;
   p.scheduled=t.audio.scheduleBoundary?.(at-position,p.fadeOut,()=>this.completeBoundary(t,p))||false;
   if(!p.scheduled)p.timer=setInterval(()=>{if(this.get(key)!==t){this.cancelPending(t);return;}this.checkPending(t);this.applyVolumes();},16);
   this.emit();return p.done;
  };
  return t.status==='loading'?t.started.then(begin):begin();
 }
 pauseNow(key,preservePending=false){const t=this.get(key);if(!t||!['playing','loading'].includes(t.status))return;t.revision++;if(preservePending&&t.pending){const p=t.pending;clearInterval(p.timer);t.audio.cancelBoundary?.();p.elapsed=p.immediate?(Date.now()-p.start)/1000:0;p.suspended=true;p.scheduled=false;}else this.cancelPending(t);this.cancelFade(t);t.audio.pause();t.status='paused';this.ducks.delete(key);this.refreshVolumes();}
 pause(key,options){return options&&typeof options==='object'?this.transition(key,'pause',options):this.pauseNow(key,true);}
 resume(key,options={}){const t=this.get(key);if(!t||t.status==='playing')return Promise.resolve();this.unlock().catch(()=>{});if(['ended','stopped','error'].includes(t.status)){this.play(t.assetId,{key,volume:t.volume,loop:t.audio.loop,duck:t.duck,fadeIn:t.fadeIn,fadeOut:t.fadeOut,endMode:t.endMode,...options});return this.get(key).started;}const rev=++t.revision;
  if(options.fadeIn!==undefined){t.fadeIn=options.fadeIn;t.fadeOrigin=this.position(t);}this.applyVolumes();
  return Promise.resolve(t.audio.play()).then(()=>{if(this.get(key)!==t||t.revision!==rev)return;t.error=null;t.envelope=1;t.status='playing';if(t.pending?.suspended){const p=t.pending;p.suspended=false;if(p.immediate)p.start=Date.now()-p.elapsed*1000;p.scheduled=t.audio.scheduleBoundary?.(Math.max(0,p.at-this.position(t)),p.fadeOut,()=>this.completeBoundary(t,p))||false;if(!p.scheduled)p.timer=setInterval(()=>{this.checkPending(t);this.applyVolumes();},16);}if(t.duck)this.ducks.add(key);this.refreshVolumes();}).catch(e=>{if(this.get(key)!==t||t.revision!==rev)return;t.error=e.message;t.status='error';t.finish();this.emit();});}
 stopNow(key){const t=this.get(key);if(!t)return;t.revision++;this.cancelPending(t);this.cancelFade(t);t.audio.pause();t.audio.currentTime=0;t.status='stopped';this.ducks.delete(key);t.finish?.();this.refreshVolumes();}
 stop(key,options){if(!options||typeof options!=='object')options=undefined;const t=this.get(key);if(!t)return Promise.resolve();return this.transition(key,'stop',options||{endMode:t.endMode,fadeOut:t.fadeOut});}
 stopAll(){for(const key of this.tracks.keys())this.stopNow(key);this.ducks.clear();clearInterval(this.duckTimer);this.duckTimer=null;this.duckTransition=null;this.duckGain=this.duckTarget=1;this.refreshVolumes();}
 seek(key,value){const t=this.get(key);if(t&&Number.isFinite(t.audio.duration)){this.cancelPending(t);t.audio.currentTime=Math.max(0,Math.min(t.audio.duration,value));}this.emit();}
 setVolume(key,value){const t=this.get(key);if(t){this.cancelFade(t);t.envelope=1;t.volume=Number(value);this.refreshVolumes();}}
 cancelFade(t){clearInterval(t.fadeTimer);t.finishFade?.();t.finishFade=null;}
 fade(key,to,seconds=1){const t=this.get(key);if(!t)return Promise.resolve();this.cancelFade(t);const from=t.volume*t.envelope,start=Date.now();return new Promise(resolve=>{t.finishFade=resolve;t.fadeTimer=setInterval(()=>{if(this.get(key)!==t){this.cancelFade(t);return;}const q=Math.min(1,(Date.now()-start)/(Math.max(.01,seconds)*1000));t.envelope=(from+(to-from)*q)/Math.max(.0001,t.volume);this.refreshVolumes();if(q>=1)this.cancelFade(t);},40);});}
}
export const soundDesk=new SoundDesk();
export const clockLabel=value=>Number.isFinite(value)?`${Math.floor(value/60)}:${String(Math.floor(value%60)).padStart(2,'0')}`:'0:00';
