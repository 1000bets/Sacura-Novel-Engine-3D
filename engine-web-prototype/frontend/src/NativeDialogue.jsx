import React,{useEffect,useRef} from 'react';
import './nativeDialogue.css';
import WidgetRenderer from './WidgetRenderer.jsx';
import {t as Translate, useLocale} from './i18n.jsx';

export default function NativeDialogue({state,session,onAdvance,onChoice,onGameAction}){
  useLocale();
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
  const enabled=state.play==='playing'||state.gameRunning&&!state.gamePaused;
  const View=state.gameViews?.find(Entry=>Entry.widgets?.dialogue);
  if(View&&dialogue.phase!=='failed'){
    let Choices=dialogue.choices.map((Label,Index)=>({id:String(Index),label:Label,enabled:true}));
    if(View.dialogueChoices){Choices=View.dialogueChoices;}
    let Status = Translate('Нажмите, чтобы продолжить');
    if (dialogue.phase === 'choice') {
      Status = Translate('Выберите ответ');
    }
    return <WidgetRenderer widget={View.widgets.dialogue} context={{variables:View.variables||{},speaker:dialogue.speaker,text:dialogue.text,status:Status,choices:Choices,ready:enabled,canAdvance:enabled&&dialogue.canAdvance}}
      onAction={(Action, Element)=>{
        if (Action === 'advance') {
          onAdvance();
        } else if (enabled && Element?.nativeAction) {
          onGameAction?.({objectId:View.objectId,componentId:View.componentId,action:Element.nativeAction});
        }
      }} onChoice={Identifier=>onChoice(Number(Identifier))} choiceEnabled={Choice=>Choice.enabled!==false}/>;
  }
  return <div className="native-dialogue" role="region" aria-label="Диалог истории">
    {dialogue.speaker&&<strong className="native-dialogue-speaker">{dialogue.speaker}</strong>}
    <p aria-live="polite">{dialogue.text||dialogue.error}</p>
    {dialogue.phase==='choice'&&<div className="native-dialogue-choices">{dialogue.choices.map((choice,index)=><button key={index} disabled={!enabled||!dialogue.canChoose} onClick={()=>onChoice(index)}>{choice}</button>)}</div>}
    {dialogue.phase==='line'&&<button className="native-dialogue-next" disabled={!enabled||!dialogue.canAdvance} onClick={onAdvance}>Дальше →</button>}
    {!enabled&&<small>Пауза</small>}
  </div>;
}
