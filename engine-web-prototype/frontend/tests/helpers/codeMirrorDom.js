// Geometry APIs supplied by browsers but absent from jsdom's layout-free document.
export function setupCodeMirrorDom(window){
 Object.defineProperty(globalThis,'MutationObserver',{configurable:true,value:window.MutationObserver});
 Object.defineProperty(globalThis,'Window',{configurable:true,value:window.Window});
 window.requestAnimationFrame=callback=>setTimeout(callback,0);window.cancelAnimationFrame=clearTimeout;
 window.Range.prototype.getClientRects??=()=>[];
 window.Range.prototype.getBoundingClientRect??=()=>({left:0,top:0,right:0,bottom:0,width:0,height:0});
}
