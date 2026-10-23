// No native fallback/polyfill in web. This module only wraps the isolated preload.
export function createNativeBridgeClient() {
  const api=globalThis.window?.sacuraDesktop?.isDesktop && window.sacuraDesktop.bridge;
  if(!api)return null;
  let context=null,connected=false,disposed=false,eventGeneration=0;
  const listeners=new Set();
  const accept = next => {
    if(!next)return;
    if(!context||next.session!==context.session||next.epoch>context.epoch||next.epoch===context.epoch&&next.revision>=context.revision)context=next;
  };
  const unsubscribe=api.subscribe(event=>{
    if(disposed)return;
    eventGeneration++;
    if(event.event==='disconnected') {
      if(event.context&&context&&event.context.session!==context.session)return;
      connected=false;
    } else {accept(event.context);connected=true;}
    for(const listener of listeners)listener(event);
  });
  return {
    async connect(){const before=eventGeneration;const info=await api.connect();if(!disposed&&before===eventGeneration){accept(info.context);connected=info.connected;}return info;},
    /** @template {keyof import('../../../desktop-electron/bridge/contract').Commands} K
     * @param {K} method
     * @param {import('../../../desktop-electron/bridge/contract').Commands[K]['params']} params
     * @param {import('../../../desktop-electron/bridge/contract').Context|null} [expectedContext] Context of the UI snapshot being edited.
     * @returns {Promise<import('../../../desktop-electron/bridge/contract').Result<import('../../../desktop-electron/bridge/contract').Commands[K]['result']>>}
     */
    async request(method,params={},expectedContext=context) {
      if(disposed||!connected)return {version:1,id:null,ok:false,context,error:{code:'DISCONNECTED',message:'Native Bridge is disconnected'}};
      if(!expectedContext||expectedContext.session!==context?.session||expectedContext.epoch!==context?.epoch)return {version:1,id:null,ok:false,context,error:{code:'STALE_SCENE',message:'Displayed scene has changed; refresh before editing'}};
      const submitted=context;
      try {
        const result=await api.request({method,params,context:submitted});
        // An old process reply must not restore its context after replacement.
        if(context?.session===submitted.session)accept(result.context);
        return result;
      }catch(error){return {version:1,id:null,ok:false,context,error:{code:'TRANSPORT_ERROR',message:error.message}};}
    },
    subscribe(listener){listeners.add(listener);return ()=>listeners.delete(listener);},
    dispose(){disposed=true;connected=false;listeners.clear();unsubscribe();},
    get context(){return context;}
  };
}
