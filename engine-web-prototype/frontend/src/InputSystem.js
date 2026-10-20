const magnitude=value=>Array.isArray(value)?Math.hypot(...value):Math.abs(value||0);
const zero=action=>action.valueType==='axis2d'?[0,0]:0;
const finite=value=>Number.isFinite(Number(value))?Number(value):0;
function modifier(value,binding,action){
 const deadZone=binding.deadZone||0,filter=v=>Math.abs(v)<=deadZone?0:Math.sign(v)*(Math.abs(v)-deadZone)/(1-deadZone);
 const raw=Array.isArray(value)?value.map(finite):finite(value);
 const scale=binding.scale??1;
 if(action.valueType==='axis2d')return [0,1].map(i=>filter(finite(Array.isArray(raw)?raw[i]:raw))*(Array.isArray(scale)?scale[i]:i===0?scale:0));
 return filter(Array.isArray(raw)?raw[0]:raw)*(Array.isArray(scale)?scale[0]:scale);
}

// Device-independent action state machine. Sources can be browser devices, plugins, or remote adapters.
export class InputSystem {
 constructor(settings,onAction=()=>{}){this.settings=settings;this.onAction=onAction;this.sources=new Map();this.states=new Map();this.contextIds=[];this.blocked=new Set();this.adapters=new Map();this.adapterErrors=new Map();}
 feed(device,control,value,{sourceId=device,payload}={}){this.sources.set(sourceId+'\0'+device+'\0'+control,{sourceId,device,control,value,payload});}
 registerAdapter(adapter){
  if(!adapter?.id||this.adapters.has(adapter.id))throw new Error('У адаптера должен быть уникальный ID.');
  const emit=(device,control,value,payload)=>this.feed(device,control,value,{sourceId:adapter.id,payload});
  const record={...adapter,emit};this.adapters.set(adapter.id,record);
  try{record.cleanup=adapter.start?.(emit);}catch(e){this.adapters.delete(adapter.id);this.removeSource(adapter.id,{cancel:true});throw e;}
  return ()=>{if(!this.adapters.has(adapter.id))return;this.adapters.delete(adapter.id);this.removeSource(adapter.id,{cancel:true});try{record.cleanup?.();record.stop?.();}finally{this.adapterErrors.delete(adapter.id);}};
 }
 removeSource(sourceId,{cancel=false}={}){
  for(const [key,s]of this.sources)if(s.sourceId===sourceId)this.sources.delete(key);
  if(cancel)for(const [id,state]of this.states)if(state.entry.sourceId===sourceId){this.dispatch(state.entry,'canceled',zero(state.entry.action));this.states.delete(id);if(magnitude(this.resolve().get(id)?.value)>.001)this.blocked.add(id);}
 }
 setContexts(ids){
  const next=[...new Set(ids)];if(JSON.stringify(next)===JSON.stringify(this.contextIds))return;
  this.cancel();this.contextIds=next;this.blocked.clear();
  for(const [id,entry]of this.resolve())if(magnitude(entry.value)>.001)this.blocked.add(id);
 }
 resolve(){
  const values=new Map(),claimed=new Set();
  const contexts=this.settings.contexts.filter(c=>this.contextIds.includes(c.id)).sort((a,b)=>b.priority-a.priority);
  for(const context of contexts){const claims=new Set();
   for(const binding of context.bindings){const channel=binding.device+'/'+binding.control;if(claimed.has(channel))continue;
    const action=this.settings.actions.find(a=>a.id===binding.actionId);if(!action)continue;
    if(action.consume!==false)claims.add(channel);
    const matching=[...this.sources.values()].filter(s=>s.device===binding.device&&s.control===binding.control);
    const source=matching.sort((a,b)=>magnitude(b.value)-magnitude(a.value))[0];
    const modified=modifier(source?.value||0,binding,action),entry=values.get(action.id)||{action,value:zero(action),contextId:context.id};
    if(Array.isArray(entry.value))entry.value=entry.value.map((v,i)=>v+modified[i]);else entry.value+=modified;
    if(source&&magnitude(modified)>.001){entry.payload=source.payload;entry.device=source.device;entry.sourceId=source.sourceId;}
    values.set(action.id,entry);
   }
   for(const channel of claims)claimed.add(channel);
  }
  for(const entry of values.values()){
   if(Array.isArray(entry.value)){const length=magnitude(entry.value);if(length>1)entry.value=entry.value.map(v=>v/length);}
   else entry.value=entry.action.valueType==='boolean'?Number(entry.value>=.5):Math.max(-1,Math.min(1,entry.value));
  }
  return values;
 }
 dispatch(entry,phase,value=entry.value){this.onAction({actionId:entry.action.id,action:entry.action,phase,value,payload:entry.payload,device:entry.device,sourceId:entry.sourceId,contextId:entry.contextId});}
 tick(dt=0){
  for(const adapter of this.adapters.values())try{adapter.poll?.(adapter.emit,dt);this.adapterErrors.delete(adapter.id);}catch(e){this.removeSource(adapter.id,{cancel:true});this.adapterErrors.set(adapter.id,e.message);}
  const values=this.resolve();
  for(const [id,state]of this.states)if(!values.has(id)){this.dispatch(state.entry,'canceled',zero(state.entry.action));this.states.delete(id);}
  for(const [id,entry]of values){const active=magnitude(entry.value)>.001;
   if(this.blocked.has(id)){if(!active)this.blocked.delete(id);continue;}
   let state=this.states.get(id);
   if(active){const rising=!state;if(rising){state={entry,elapsed:0,fired:false};this.states.set(id,state);this.dispatch(entry,'started');}state.entry=entry;state.elapsed+=Math.max(0,finite(dt));
    const trigger=entry.action.trigger;
    if((trigger==='pressed'&&rising)||(trigger==='hold'&&!state.fired&&state.elapsed>=entry.action.holdSeconds)||trigger==='continuous'){state.fired=true;this.dispatch(entry,'triggered');}
    else if(trigger==='hold'&&!state.fired)this.dispatch(entry,'ongoing');
   }else if(state){
    if(entry.action.trigger==='released'){this.dispatch({...state.entry,payload:entry.payload??state.entry.payload},'triggered',state.entry.value);state.fired=true;}
    this.dispatch(state.entry,state.fired?'completed':'canceled',zero(entry.action));this.states.delete(id);
   }
  }
 }
 pulse(device,control,payload,sourceId='pulse'){this.feed(device,control,1,{sourceId,payload});this.tick(0);this.removeSource(sourceId);this.tick(0);}
 cancel(){for(const state of this.states.values())this.dispatch(state.entry,'canceled',zero(state.entry.action));this.states.clear();}
 clear(){this.cancel();this.sources.clear();this.blocked.clear();}
 dispose(){this.clear();for(const adapter of this.adapters.values())try{adapter.cleanup?.();adapter.stop?.();}catch(e){this.adapterErrors.set(adapter.id,e.message);}this.adapters.clear();}
}
