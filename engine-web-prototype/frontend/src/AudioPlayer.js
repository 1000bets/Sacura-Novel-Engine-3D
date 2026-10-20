// A shared, user-unlocked output. Original files are never modified.
export class AudioOutput {
  constructor(){ this.buffers=new Map(); this.normalized=true; }
  unlock(){
    if(!this.context){
      this.context=new (window.AudioContext||window.webkitAudioContext)();
      this.master=this.context.createGain();this.analyser=this.context.createAnalyser();this.analyser.fftSize=256;this.meter=new Float32Array(256);
      this.master.connect(this.analyser);this.analyser.connect(this.context.destination);
    }
    return this.context.resume();
  }
  async load(url){
    if(!this.context) this.unlock().catch(()=>{});
    if(!this.buffers.has(url)){
      const task=(async()=>{
        const response=await fetch(url);
        if(!response.ok||response.headers.get('content-type')?.includes('text/html'))throw new Error('Аудиофайл не найден: '+decodeURIComponent(url.split('/').pop()));
        const buffer=await this.context.decodeAudioData(await response.arrayBuffer());let peak=0,sum=0,count=0;
        for(let c=0;c<buffer.numberOfChannels;c++){const data=buffer.getChannelData(c);for(let i=0;i<data.length;i++){const v=data[i];peak=Math.max(peak,Math.abs(v));sum+=v*v;count++;}}
        return {buffer,gain:Math.min(8,.16/Math.max(.001,Math.sqrt(sum/count)),.89/Math.max(.001,peak))};
      })();this.buffers.set(url,task);task.catch(()=>this.buffers.delete(url));
    }return this.buffers.get(url);
  }
  level(){if(!this.analyser)return 0;this.analyser.getFloatTimeDomainData(this.meter);return Math.max(...this.meter.map(Math.abs));}
}
export class BufferPlayer extends EventTarget {
  constructor(url,output){super();this.url=url;this.output=output;this.offset=0;this._volume=1;this.loop=false;this.serial=0;this.paused=true;}
  get duration(){return this.data?.buffer.duration||NaN;}
  get loop(){return this._loop;}
  set loop(value){this._loop=!!value;if(this.source)this.source.loop=this._loop;}
  get timelineTime(){return this.offset+(this.paused?0:this.output.context.currentTime-this.began);}
  get currentTime(){const t=this.timelineTime;return this.loop&&this.duration?t%this.duration:Math.min(t,this.duration||0);}
  set currentTime(value){const playing=!this.paused;this.pause();this.offset=Math.max(0,Number(value));if(playing)this.play().catch(()=>{});}
  get volume(){return this._volume;}
  set volume(value){this._volume=value;this.updateGain(!this.paused);}
  updateGain(smooth=false){
    if(!this.gain)return;const param=this.gain.gain,value=this._volume*(this.output.normalized?this.data.gain:1);
    // Smooth the small envelope/fader steps on the audio clock as well.
    if(smooth&&param.setTargetAtTime)param.setTargetAtTime(value,this.output.context.currentTime,.008);else param.value=value;
  }
  async play(){
    const serial=++this.serial;await this.output.unlock();this.data=await this.output.load(this.url);if(serial!==this.serial)return;
    this.dispatchEvent(new Event('loadedmetadata'));if(!this.loop&&this.offset>=this.duration)this.offset=0;
    if(this.source){this.source.onended=null;this.source.stop();this.source.disconnect();}this.gain?.disconnect();this.boundaryGain?.disconnect();
    const ctx=this.output.context,source=ctx.createBufferSource();this.gain=ctx.createGain();this.source=source;source.buffer=this.data.buffer;source.loop=this.loop;
    this.updateGain();source.connect(this.gain);this.boundaryGain=ctx.createGain();this.gain.connect(this.boundaryGain);this.boundaryGain.connect(this.output.master);this.began=ctx.currentTime;this.paused=false;
    source.onended=()=>{if(serial!==this.serial||this.paused)return;const boundary=this.boundary;this.offset=boundary?boundary.position:this.duration;this.paused=true;clearInterval(this.timer);source.disconnect();this.source=null;this.gain.disconnect();this.boundaryGain?.disconnect();this.boundary=null;if(boundary)boundary.finish();else this.dispatchEvent(new Event('ended'));};
    source.start(0,this.loop?this.offset%this.duration:this.offset);clearInterval(this.timer);this.timer=setInterval(()=>this.dispatchEvent(new Event('timeupdate')),80);
  }
  pause(){const offset=this.timelineTime;this.cancelBoundary();this.serial++;this.paused=true;this.offset=offset;clearInterval(this.timer);if(this.source){this.source.onended=null;try{this.source.stop();}catch{}this.source.disconnect();this.source=null;}this.gain?.disconnect();this.boundaryGain?.disconnect();}
  scheduleBoundary(seconds,fadeOut,finish){
    if(!this.source||this.paused)return false;
    this.cancelBoundary();const ctx=this.output.context,end=ctx.currentTime+seconds,param=this.boundaryGain.gain;
    const fade=Math.min(seconds,fadeOut);param.setValueAtTime(fadeOut?Math.min(1,seconds/fadeOut):1,ctx.currentTime);
    if(fade>0){if(seconds>=fadeOut)param.setValueAtTime(1,end-fade);param.linearRampToValueAtTime(0,end);}
    this.boundary={finish,position:this.timelineTime+seconds};this.source.stop(end);return true;
  }
  cancelBoundary(){
    if(!this.boundary)return;this.boundary=null;const ctx=this.output.context;
    // A later stop replaces a previously scheduled stop on this source.
    if(this.source&&!this.paused)this.source.stop(ctx.currentTime+1e9);
    const param=this.boundaryGain?.gain;if(param){param.cancelScheduledValues(ctx.currentTime);param.setValueAtTime(1,ctx.currentTime);}
  }
}
