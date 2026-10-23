import React,{useEffect,useRef} from 'react';
import './nativeDialogue.css';

export default function NativeDialogue({state,session,onAdvance,onChoice}){
  const audio=useRef(null);
  useEffect(()=>{
    if(!state?.startupAudio||!session)return;
    const cue=new Audio(`sacura://startup-audio/${encodeURIComponent(session)}/cue.wav`);
    cue.volume=.55;
    audio.current=cue;
    return()=>{cue.pause();cue.src='';if(audio.current===cue)audio.current=null;};
  },[session,state?.startupAudio]);
  useEffect(()=>{
    if(!audio.current)return;
    if(state?.play==='playing')audio.current.play().catch(()=>{});
    else audio.current.pause();
  },[state?.play,session,state?.startupAudio]);
  const dialogue=state?.dialogue;
  if(!dialogue||!['line','choice','failed'].includes(dialogue.phase))return null;
  const enabled=state.play==='playing';
  return <div className="native-dialogue" role="region" aria-label="Диалог истории">
    {dialogue.speaker&&<strong className="native-dialogue-speaker">{dialogue.speaker}</strong>}
    <p aria-live="polite">{dialogue.text||dialogue.error}</p>
    {dialogue.phase==='choice'&&<div className="native-dialogue-choices">{dialogue.choices.map((choice,index)=><button key={index} disabled={!enabled||!dialogue.canChoose} onClick={()=>onChoice(index)}>{choice}</button>)}</div>}
    {dialogue.phase==='line'&&<button className="native-dialogue-next" disabled={!enabled||!dialogue.canAdvance} onClick={onAdvance}>Дальше →</button>}
    {!enabled&&<small>Пауза</small>}
  </div>;
}
