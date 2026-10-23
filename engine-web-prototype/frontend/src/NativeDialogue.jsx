import React,{useEffect,useRef} from 'react';
import WidgetRenderer from './WidgetRenderer.jsx';
import './nativeDialogue.css';

export default function NativeDialogue({state,session,widget,onAdvance,onChoice}){
  const audio=useRef(null);
  useEffect(()=>{
    if(!state?.startupAudio||!session)return;
    const cue=new Audio(`sacura://startup-audio/${encodeURIComponent(session)}/cue.wav`);
    cue.volume=.55;
    audio.current=cue;
    return()=>{cue.pause();cue.src='';if(audio.current===cue)audio.current=null;};
  },[session,state?.startupAudio,state?.startupAudioPath]);
  useEffect(()=>{
    if(!audio.current)return;
    if(state?.play==='playing')audio.current.play().catch(()=>{});
    else audio.current.pause();
  },[state?.play,session,state?.startupAudio,state?.startupAudioPath]);
  const dialogue=state?.dialogue;
  if(!dialogue||!['line','choice','failed'].includes(dialogue.phase))return null;
  const enabled=state.play==='playing';
  if(widget&&dialogue.phase!=='failed')return <div className="native-widget-overlay" role="region" aria-label="Диалог истории"><WidgetRenderer widget={widget} context={{speaker:dialogue.speaker,text:dialogue.text,status:enabled?'Нажмите, чтобы продолжить':'Пауза',title:state.project,choices:dialogue.choices.map((label,index)=>({id:String(index),label})),ready:enabled,canAdvance:enabled&&dialogue.canAdvance}} onAction={action=>{if(action==='advance')onAdvance();}} onChoice={id=>onChoice(Number(id))}/></div>;
  return <div className="native-dialogue" role="region" aria-label="Диалог истории">
    {dialogue.speaker&&<strong className="native-dialogue-speaker">{dialogue.speaker}</strong>}
    <p aria-live="polite">{dialogue.text||dialogue.error}</p>
    {dialogue.phase==='choice'&&<div className="native-dialogue-choices">{dialogue.choices.map((choice,index)=><button key={index} disabled={!enabled||!dialogue.canChoose} onClick={()=>onChoice(index)}>{choice}</button>)}</div>}
    {dialogue.phase==='line'&&<button className="native-dialogue-next" disabled={!enabled||!dialogue.canAdvance} onClick={onAdvance}>Дальше →</button>}
    {!enabled&&<small>Пауза</small>}
  </div>;
}
