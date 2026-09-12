import {AUDIO_ASSETS} from './studioModel.js';
import {AudioOutput,BufferPlayer} from './AudioPlayer.js';
export class SoundDesk {
 constructor(factory){this.output=factory?null:new AudioOutput();this.factory=factory||(url=>new BufferPlayer(url,this.output));this.tracks=new Map();this.listeners=new Set();this.peaks=new Map();this.ducks=new Set();}
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
 refreshVolumes(){for(const t of this.tracks.values())t.audio.volume=Math.max(0,Math.min(1,t.volume*t.envelope*(t.kind==='music'&&this.ducks.size?Math.pow(10,-12/20):1)));this.emit();}
 duck(key,on){on?this.ducks.add(key):this.ducks.delete(key);this.refreshVolumes();}
 play(assetId,{key=assetId,volume=.75,loop=false,duck=true,fade=0}={}){
  const asset=AUDIO_ASSETS.find(a=>a.id===assetId);if(!asset)return Promise.reject(new Error('Назначьте звуковой файл'));
  this.unlock().catch(()=>{});this.stop(key);const audio=this.factory(asset.url);audio.preload='auto';audio.loop=loop;
  const t={key,assetId,audio,kind:asset.kind,status:'loading',volume,envelope:fade?0:1,duck:duck&&asset.kind==='voice',error:null,revision:0};this.tracks.set(key,t);
  t.done=new Promise(resolve=>{t.finish=resolve;});
  const current=()=>this.tracks.get(key)===t;
  const finish=status=>{if(!current()||['stopped','ended'].includes(t.status))return;this.cancelFade(t);t.status=status;this.ducks.delete(key);this.refreshVolumes();t.finish();};
  const fail=e=>{if(!current()||['stopped','paused'].includes(t.status))return;t.error=e?.name==='NotAllowedError'?'Звук заблокирован. Нажмите «Слушать», чтобы включить аудио.':`${asset.file}: ${e?.message||'не удалось декодировать файл'}`;finish('error');};
  audio.addEventListener('ended',()=>finish('ended'));audio.addEventListener('error',fail);
  audio.addEventListener('timeupdate',()=>current()&&this.emit());audio.addEventListener('loadedmetadata',()=>current()&&this.emit());this.refreshVolumes();
  t.started=Promise.resolve().then(()=>{if(current()&&t.status==='loading')return audio.play();}).then(()=>{if(!current()||['stopped','paused'].includes(t.status))return;t.status='playing';if(t.duck)this.ducks.add(key);this.refreshVolumes();if(fade)this.fade(key,volume,fade);}).catch(fail);
  this.emit();return t.done;
 }
 pause(key){const t=this.get(key);if(!t||!['playing','loading'].includes(t.status))return;t.revision++;this.cancelFade(t);t.audio.pause();t.status='paused';this.ducks.delete(key);this.refreshVolumes();}
 resume(key){const t=this.get(key);if(!t||t.status==='playing')return;this.unlock().catch(()=>{});if(['ended','stopped','error'].includes(t.status)){this.play(t.assetId,{key,volume:t.volume,loop:t.audio.loop,duck:t.duck});return;}const rev=++t.revision;Promise.resolve(t.audio.play()).then(()=>{if(this.get(key)!==t||t.revision!==rev)return;t.error=null;t.envelope=1;t.status='playing';if(t.duck)this.ducks.add(key);this.refreshVolumes();}).catch(e=>{if(this.get(key)!==t||t.revision!==rev)return;t.error=e.message;t.status='error';t.finish();this.emit();});}
 stop(key){const t=this.get(key);if(!t)return;t.revision++;this.cancelFade(t);t.audio.pause();t.audio.currentTime=0;t.status='stopped';this.ducks.delete(key);t.finish?.();this.refreshVolumes();}
 stopAll(){for(const key of this.tracks.keys())this.stop(key);this.ducks.clear();this.refreshVolumes();}
 seek(key,value){const t=this.get(key);if(t&&Number.isFinite(t.audio.duration))t.audio.currentTime=Math.max(0,Math.min(t.audio.duration,value));this.emit();}
 setVolume(key,value){const t=this.get(key);if(t){this.cancelFade(t);t.envelope=1;t.volume=Number(value);this.refreshVolumes();}}
 cancelFade(t){clearInterval(t.fadeTimer);t.finishFade?.();t.finishFade=null;}
 fade(key,to,seconds=1){const t=this.get(key);if(!t)return Promise.resolve();this.cancelFade(t);const from=t.volume*t.envelope,start=Date.now();return new Promise(resolve=>{t.finishFade=resolve;t.fadeTimer=setInterval(()=>{if(this.get(key)!==t){this.cancelFade(t);return;}const q=Math.min(1,(Date.now()-start)/(Math.max(.01,seconds)*1000));t.envelope=(from+(to-from)*q)/Math.max(.0001,t.volume);this.refreshVolumes();if(q>=1)this.cancelFade(t);},40);});}
}
export const soundDesk=new SoundDesk();
export const clockLabel=value=>Number.isFinite(value)?`${Math.floor(value/60)}:${String(Math.floor(value%60)).padStart(2,'0')}`:'0:00';
