import {t as tr, useLocale} from './i18n.jsx';
import {audioActionOptions} from './audioTransitions.js';
import React,{useEffect,useState} from 'react';
import {soundDesk} from './audio.js';
import {audioEnvelope} from './audioEnvelope.js';
import './interactionTools.css';
export default function AudioEnvelopeEditor({asset,action,onChange}){
 useLocale();
 const [data,setData]=useState(null),[error,setError]=useState(false);
 useEffect(()=>{let live=true;setData(null);setError(false);soundDesk.waveform(asset).then(d=>live&&setData(d)).catch(()=>live&&setError(true));return()=>{live=false;};},[asset.url]);
 const duration=data?.duration||0,{fadeIn,fadeOut}=audioActionOptions(action);
 const x=t=>duration?Math.min(300,t/duration*300):0;
 const points=Array.from({length:101},(_,i)=>{const time=i/100*duration;return `${i*3},${52-audioEnvelope(time,duration,fadeIn,fadeOut)*44}`;}).join(' ');
 const drag=(e,key)=>{e.preventDefault();const svg=e.currentTarget.ownerSVGElement;const update=event=>{const rect=svg.getBoundingClientRect(),time=Math.max(0,Math.min(duration,(event.clientX-rect.left)/rect.width*duration));onChange({[key+'Enabled']:true,[key]:Math.round((key==='fadeIn'?time:duration-time)*100)/100});};e.currentTarget.setPointerCapture(e.pointerId);update(e);};
 return <div className="audio-envelope-editor"><strong>{tr("Огибающая громкости")}</strong>
 <svg viewBox="0 0 300 60" preserveAspectRatio="none" aria-label={tr("График звука: плавный вход и затухание")}>
 {data?.peaks.map((p,i)=><line key={i} x1={i*3+1} x2={i*3+1} y1={30-p*22} y2={30+p*22} stroke="var(--muted, #88989e)" strokeWidth="1.8"/>)}
 <polyline points={points} fill="none" stroke="#bca1ef" strokeWidth="2"/>
 {duration>0&&[['fadeIn',x(fadeIn)],['fadeOut',x(duration-fadeOut)]].map(([key,cx])=><circle key={key} cx={cx} cy="8" r="6" fill="#e5d7ff" onPointerDown={e=>drag(e,key)} onPointerMove={e=>{if(e.currentTarget.hasPointerCapture(e.pointerId))drag(e,key);}} onPointerUp={e=>e.currentTarget.releasePointerCapture(e.pointerId)}/>)}
 </svg><small>{duration?tr("{0} с · перетащите маркеры входа и выхода", [duration.toFixed(2)]):error?tr("Не удалось прочитать аудио. Длительность переходов можно задать ниже."):tr("Читаем звуковой файл…")}</small>
 <small>{tr("Fade In применяется при запуске. Fade Out — перед концом файла или выбранной точкой завершения.")}</small></div>;
}
