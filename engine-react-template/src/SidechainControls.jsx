import React,{useEffect,useState} from 'react';
import {SIDECHAIN,SIDECHAIN_LIMITS,normalizeSidechain} from './audioSettings.js';
import './sidechainControls.css';

const fields=[
 {key:'attack',label:'Атака, с',hint:'За сколько секунд приглушить музыку'},
 {key:'release',label:'Восстановление (release), с',hint:'За сколько секунд вернуть громкость'},
 {key:'reductionDb',label:'Снижение громкости, dB',hint:'0 dB — без приглушения'},
];

export default function SidechainControls({value,onChange}){
 const settings=normalizeSidechain(value),[draft,setDraft]=useState(settings);
 useEffect(()=>setDraft(settings),[settings.attack,settings.release,settings.reductionDb]);
 const edit=(key,raw,commit=false)=>{
  setDraft(previous=>({...previous,[key]:raw}));
  const numeric=raw.trim()===''?NaN:Number(raw),{min,max}=SIDECHAIN_LIMITS[key];
  if(commit){
   const next=normalizeSidechain({...settings,[key]:Number.isFinite(numeric)?numeric:settings[key]});
   setDraft(next);onChange(next);
  }else if(Number.isFinite(numeric)&&numeric>=min&&numeric<=max){
   onChange({...settings,[key]:numeric});
  }
 };
 return <fieldset className="sidechain-settings">
  <legend>Сайдчейн проекта</legend>
  <div className="sidechain-fields">{fields.map(({key,label,hint})=><label className="field" key={key}>
   <span>{label}</span>
   <input type="number" aria-label={label} {...SIDECHAIN_LIMITS[key]} value={draft[key]} onChange={event=>edit(key,event.target.value)} onBlur={event=>edit(key,event.target.value,true)} onKeyDown={event=>{if(event.key==='Enter')event.currentTarget.blur();}}/>
   <small>{hint}</small>
  </label>)}</div>
  <div className="sidechain-footer"><span>Применяется сразу к прослушиванию и Playtest. Сохраняется с проектом.</span><button type="button" onClick={()=>{setDraft({...SIDECHAIN});onChange({...SIDECHAIN});}}>По умолчанию</button></div>
 </fieldset>;
}
