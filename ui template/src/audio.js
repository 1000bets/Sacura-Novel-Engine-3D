import {AUDIO_ASSETS} from './studioModel.js';

// One transport shared by the sound desk and the scenario preview.
export class SoundDesk {
 constructor(factory=url=>new Audio(url)){this.factory=factory;this.tracks=new Map();this.listeners=new Set();this.peaks=new Map();this.ducks=new Set();}
 subscribe(fn){this.listeners.add(fn);return()=>this.listeners.delete(fn);}
 emit(){this.listeners.forEach(fn=>fn());}
 get(key){return this.tracks.get(key);}
 async waveform(asset){if(this.peaks.has(asset.id))return this.peaks.get(asset.id);const pending=(async()=>{const context=new (window.AudioContext||window.webkitAudioContext)();try{const response=await fetch(asset.url);if(!response.ok)throw new Error('Файл недоступен');const buffer=await context.decodeAudioData(await response.arrayBuffer());const data=buffer.getChannelData(0),peaks=[];for(let i=0;i<100;i++){const start=Math.floor(i*data.length/100),end=Math.floor((i+1)*data.length/100);let peak=0;for(let j=start;j<end;j+=5)peak=Math.max(peak,Math.abs(data[j]));peaks.push(peak);}return {peaks,duration:buffer.duration};}finally{await context.close();}})();this.peaks.set(asset.id,pending);return pending;}
 refreshVolumes(){for(const t of this.tracks.values())t.audio.volume=Math.max(0,Math.min(1,t.volume*(t.kind==='music'&&this.ducks.size?Math.pow(10,-12/20):1)));this.emit();}
 duck(key,on){on?this.ducks.add(key):this.ducks.delete(key);this.refreshVolumes();}
 play(assetId,{key=assetId,volume=.75,loop=false,duck=true,fade=0}={}){
  const asset=AUDIO_ASSETS.find(a=>a.id===assetId);if(!asset)return Promise.reject(new Error('Назначьте звуковой файл'));
  this.stop(key);const audio=this.factory(asset.url);audio.preload='auto';audio.loop=loop;
  const t={key,assetId,audio,kind:asset.kind,status:'loading',volume,duck:duck&&asset.kind==='voice',error:null};this.tracks.set(key,t);
  t.done=new Promise(resolve=>{t.finish=resolve;});
  const finish=status=>{t.status=status;this.ducks.delete(key);this.refreshVolumes();t.finish();};
  audio.addEventListener('ended',()=>finish('ended'));
  audio.addEventListener('error',()=>{t.error='Не удалось прочитать аудио. Проверьте файл в public/sound.';finish('error');});
  audio.addEventListener('timeupdate',()=>this.emit());audio.addEventListener('loadedmetadata',()=>this.emit());
  audio.volume=fade?0:volume;
  const started=audio.play();
  Promise.resolve(started).then(()=>{if(this.tracks.get(key)!==t||['stopped','paused'].includes(t.status))return; t.status='playing';if(t.duck)this.ducks.add(key);this.refreshVolumes();if(fade)this.fade(key,volume,fade);}).catch(()=>{if(['stopped','paused'].includes(t.status))return;t.error='Браузер не запустил звук. Нажмите «Слушать» ещё раз.';finish('error');});
  this.emit();return t.done;
 }
 pause(key){const t=this.get(key);if(!t||!['playing','loading'].includes(t.status))return;this.cancelFade(t);t.audio.pause();t.status='paused';this.ducks.delete(key);this.refreshVolumes();}
 resume(key){const t=this.get(key);if(!t)return;if(['ended','stopped'].includes(t.status)){this.play(t.assetId,{key,volume:t.volume,loop:t.audio.loop,duck:t.duck});return;}Promise.resolve(t.audio.play()).then(()=>{t.error=null;t.status='playing';if(t.duck)this.ducks.add(key);this.refreshVolumes();}).catch(()=>{t.error='Запуск звука заблокирован браузером';t.status='error';this.emit();});}
 stop(key){const t=this.get(key);if(!t)return;this.cancelFade(t);t.audio.pause();t.audio.currentTime=0;t.status='stopped';this.ducks.delete(key);t.finish?.();this.refreshVolumes();}
 stopAll(){for(const key of this.tracks.keys())this.stop(key);this.ducks.clear();this.refreshVolumes();}
 seek(key,value){const t=this.get(key);if(t&&Number.isFinite(t.audio.duration))t.audio.currentTime=Math.max(0,Math.min(t.audio.duration,value));this.emit();}
 setVolume(key,value){const t=this.get(key);if(t){this.cancelFade(t);t.volume=Number(value);this.refreshVolumes();}}
 cancelFade(t){clearInterval(t.fadeTimer);t.finishFade?.();t.finishFade=null;}
 fade(key,to,seconds=1){const t=this.get(key);if(!t)return Promise.resolve();this.cancelFade(t);const factor=t.kind==='music'&&this.ducks.size?Math.pow(10,-12/20):1,from=t.audio.volume/factor,start=Date.now();return new Promise(resolve=>{t.finishFade=resolve;t.fadeTimer=setInterval(()=>{const q=Math.min(1,(Date.now()-start)/(Math.max(.01,seconds)*1000));t.audio.volume=Math.max(0,Math.min(1,(from+(to-from)*q)*(t.kind==='music'&&this.ducks.size?Math.pow(10,-12/20):1)));this.emit();if(q>=1)this.cancelFade(t);},40);});}
}
export const soundDesk=new SoundDesk();
export const clockLabel=value=>Number.isFinite(value)?`${Math.floor(value/60)}:${String(Math.floor(value%60)).padStart(2,'0')}`:'0:00';
