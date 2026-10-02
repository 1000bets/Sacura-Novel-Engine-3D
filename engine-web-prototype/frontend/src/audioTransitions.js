// All positions and fades use seconds. A quarter note is one BPM beat.
export const END_MODES=[['file','До конца аудиофайла'],['marker','До ближайшего маркера'],['event','До конца события'],['immediate','Сразу']];
export const QUANTIZATION=[['off','Выключена'],['1/16','1/16 ноты'],['1/8','1/8 ноты'],['1/4','1 доля'],['1/2','2 доли'],['bar','1 такт'],['2bars','2 такта'],['4bars','4 такта'],['8bars','8 тактов']];
export const audioSeconds=value=>Number.isFinite(Number(value))?Math.max(0,Number(value)):0;
export function normalizeAudioTiming(source={}){
 const markers=[...new Set((Array.isArray(source.endMarkers)?source.endMarkers:[]).filter(v=>v!==null&&v!==''&&Number.isFinite(Number(v))&&Number(v)>0).map(Number))].sort((a,b)=>a-b);
 return {endMarkers:markers,bpm:Math.min(400,Math.max(20,Number(source.bpm)||120)),beatsPerBar:Math.min(16,Math.max(1,Math.round(Number(source.beatsPerBar)||4))),beatUnit:[2,4,8,16].includes(Number(source.beatUnit))?Number(source.beatUnit):4,quantization:QUANTIZATION.some(([v])=>v===source.quantization)?source.quantization:'off'};
}
export function audioActionOptions(action={}){
 const fadeIn=audioSeconds(action.fadeIn??action.fade??(action.type==='music'?1:0));
 const fadeOut=audioSeconds(action.fadeOut??(action.type==='stop'?action.duration??2:0));
 return {fadeIn:action.fadeInEnabled===false?0:fadeIn,fadeOut:action.fadeOutEnabled===false?0:fadeOut,endMode:END_MODES.some(([v])=>v===action.endMode)?action.endMode:(action.type==='stop'?'immediate':'marker')};
}
export function quantizationSeconds(asset){
 if(asset?.kind!=='music')return 0;
 const timing=normalizeAudioTiming(asset),quarter=60/timing.bpm,bar=quarter*timing.beatsPerBar*4/timing.beatUnit;
 return ({'1/16':quarter/4,'1/8':quarter/2,'1/4':quarter,'1/2':quarter*2,bar,'2bars':bar*2,'4bars':bar*4,'8bars':bar*8})[timing.quantization]||0;
}
// Strictly AFTER the current position. File end is the fallback if no marker remains.
export function nextAudioBoundary(asset,position,duration,loop=false,mode='marker'){
 const time=audioSeconds(position),length=Number(duration),valid=Number.isFinite(length)&&length>0;
 if(mode==='immediate'||mode==='event')return time;
 if(!valid)return time;
 const cycle=loop?Math.floor(time/length):0,local=loop?time-cycle*length:time;
 if(mode==='file')return (cycle+1)*length;
 const points=normalizeAudioTiming(asset).endMarkers.filter(v=>v<=length);
 const next=points.find(v=>v>local+1e-6),candidates=[];
 if(next!==undefined)candidates.push(cycle*length+next);
 else if(loop&&points.length)candidates.push((cycle+1)*length+points[0]);
 const interval=quantizationSeconds(asset);
 if(interval)candidates.push((Math.floor((time+1e-6)/interval)+1)*interval);
 if(!points.length&&!interval)return time;
 const boundary=candidates.length?Math.min(...candidates):(cycle+1)*length;
 return loop?boundary:Math.min(length,boundary);
}
