import {createNativeBridgeClient} from './nativeBridgeClient.js';

const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const reconcile=(old,next)=>{const result=next.map(value=>old.find(item=>item.id===value.id&&same(item,value))||value);return result.length===old.length&&result.every((v,i)=>v===old[i])?old:result;};
export function createWebEditorAdapter(actions){return {kind:'web',...actions};}

// This store contains read-only server snapshots and UI selection, never a world or simulator.
export function createDesktopEditorAdapter(client=createNativeBridgeClient()) {
  if(!client)return null;
  let snapshot={connected:false,context:null,state:null,objects:[],components:[],types:[],assets:[],story:null,error:null};
  let disposed=false,refreshing=false,dirty=false,serial=0,domains=new Set(['assets','story']),waiters=[];
  const listeners=new Set();
  const publish=patch=>{snapshot={...snapshot,...patch};for(const listener of listeners)listener();};
  const read=async(method,params={})=>{
    // Reads can race scene/project replacement. Retry against the updated native
    // context; mutations below are never retried implicitly.
    let r;for(let attempt=0;attempt<3;attempt++){r=await client.request(method,params);if(r.ok||r.error.code!=='STALE_SCENE')break;}
    if(!r.ok)throw Object.assign(Error(r.error.message),{code:r.error.code});return r;
  };
  async function refresh(){
    dirty=true;if(refreshing)return new Promise(resolve=>waiters.push(resolve));refreshing=true;
    try{while(dirty&&!disposed){dirty=false;const generation=serial;
      const [hierarchy,description]=await Promise.all([read('scene.getHierarchy'),read('bridge.describe')]);
      if(generation!==serial)continue;
      if(hierarchy.context.epoch!==description.context.epoch||hierarchy.context.session!==description.context.session){dirty=true;continue;}
      const selected=description.value.selection;
      const types=snapshot.types.length?snapshot.types:(await read('reflection.getComponentTypes')).value;
      const components=selected?await read('object.getComponents',{objectId:selected}):{value:[]};
      const properties=await Promise.all(components.value.map(async component=>({...component,properties:(await read('component.getProperties',{objectId:selected,componentId:component.id})).value})));
      if(generation!==serial)continue;
      const requestedDomains=[...domains];for(const key of requestedDomains)domains.delete(key);
      const domainPatch={};for(const key of requestedDomains){const value=(await read(key==='assets'?'assets.list':'story.get')).value;domainPatch[key]=same(snapshot[key],value)?snapshot[key]:value;}
      if(generation!==serial)continue;
      publish({...domainPatch,types,connected:true,context:description.context,state:same(snapshot.state,description.value)?snapshot.state:description.value,objects:reconcile(snapshot.objects,hierarchy.value),components:reconcile(snapshot.components,properties)});
    }}catch(error){if(!disposed)publish({error:error.code+': '+error.message});}finally{refreshing=false;for(const resolve of waiters)resolve();waiters=[];if(dirty&&!disposed)queueMicrotask(refresh);}
  }
  const off=client.subscribe(event=>{
    if(event.event==='disconnected'){serial++;domains=new Set(['assets','story']);publish({connected:false,context:null,state:null,objects:[],components:[],types:[],assets:[],story:null,error:event.message});return;}
    for(const key of ['assets','story'])if(event.changes?.[key])domains.add(key);
    if(!snapshot.context||snapshot.context.session!==event.context.session||snapshot.context.epoch!==event.context.epoch){serial++;domains=new Set(['assets','story']);publish({context:event.context,objects:[],components:[],state:null,types:[],assets:[],story:null,error:null});}
    refresh();
  });
  const adapter={kind:'desktop',subscribe:callback=>{listeners.add(callback);return()=>listeners.delete(callback);},getSnapshot:()=>snapshot,
    async start(){await client.connect();if(client.context&&!disposed)await refresh();},
    async command(method,params={},context=snapshot.context){
      const result=await client.request(method,params,context);
      if(!result.ok){publish({error:result.error.code+': '+result.error.message});throw Object.assign(Error(result.error.message),{code:result.error.code});}
      if(method.startsWith('story.'))domains.add('story');if(method.startsWith('assets.'))domains.add('assets');
      publish({error:null});await refresh();return result.value;
    },
    refresh,clearError:()=>publish({error:null}),
    dispose(){disposed=true;serial++;off();client.dispose();listeners.clear();},
    // Asset/Story adapters extend these reads; no caller writes a native snapshot.
    async loadDomain(method,key){const generation=serial,result=await read(method);if(disposed||generation!==serial)throw Object.assign(Error('Native session changed while reading'),{code:'STALE_SCENE'});publish({[key]:same(snapshot[key],result.value)?snapshot[key]:result.value});return result.value;}
  };
  return adapter;
}

// Euler angles are presentation units only. C++ remains authoritative for transforms.
export function quaternionToEuler([x,y,z,w]){
  const t=2*(w*y-z*x);return [Math.atan2(2*(w*x+y*z),1-2*(x*x+y*y)),Math.asin(Math.max(-1,Math.min(1,t))),Math.atan2(2*(w*z+x*y),1-2*(y*y+z*z))].map(n=>n*180/Math.PI);
}
export function eulerToQuaternion(angles){
  const [x,y,z]=angles.map(n=>Number(n)*Math.PI/360),cx=Math.cos(x),cy=Math.cos(y),cz=Math.cos(z),sx=Math.sin(x),sy=Math.sin(y),sz=Math.sin(z);
  return [sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz];
}
export function presentationObject(object,sceneId='native'){
  return {...object,transforms:{[sceneId]:{...object.transform,rotation:quaternionToEuler(object.transform.rotation)}}};
}
