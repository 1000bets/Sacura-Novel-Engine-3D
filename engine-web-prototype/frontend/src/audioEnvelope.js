const seconds=value=>Math.max(0,Number(value)||0);
export function audioEnvelope(time,duration,fadeIn=0,fadeOut=0){
 const start=seconds(fadeIn),end=seconds(fadeOut),t=seconds(time);
 return Math.max(0,Math.min(1,start?t/start:1,end&&Number.isFinite(duration)?(duration-t)/end:1));
}
