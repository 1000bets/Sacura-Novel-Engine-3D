// Resize the drawing buffer immediately before rendering, never between frames.
// CSS owns the canvas layout so changing its buffer cannot resize its container.
export function createRendererViewport(renderer,camera,element,Observer=ResizeObserver){
 let pending=element.getBoundingClientRect(),width=0,height=0;
 const observer=new Observer(entries=>{pending=entries[0]?.contentRect||element.getBoundingClientRect();});
 observer.observe(element);
 return {
  update(){
   const nextWidth=Math.round(pending.width),nextHeight=Math.round(pending.height);
   if(nextWidth<1||nextHeight<1)return false;
   if(nextWidth===width&&nextHeight===height)return false;
   width=nextWidth;height=nextHeight;
   renderer.setSize(width,height,false);camera.aspect=width/height;camera.updateProjectionMatrix();return true;
  },
  dispose(){observer.disconnect();},
 };
}
