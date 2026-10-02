import {t as tr, useLocale} from './i18n.jsx';
import React,{useEffect,useState} from 'react';
import {soundDesk} from './audio.js';
import {normalizeAudioTiming,QUANTIZATION,quantizationSeconds} from './audioTransitions.js';
import './interactionTools.css';
export default function AudioTimingEditor({asset,onChange,disabled=false}){
 useLocale();
 const [data,setData]=useState(null),[error,setError]=useState(false),[newTime,setNewTime]=useState('');
 const timing=normalizeAudioTiming(asset),duration=data?.duration||0;
 useEffect(()=>{let live=true;setData(null);setError(false);soundDesk.waveform(asset).then(value=>live&&setData(value)).catch(()=>live&&setError(true));return()=>{live=false;};},[asset.url]);
 const change=patch=>{if(!disabled)onChange?.(patch);};
 const put=values=>change({endMarkers:normalizeAudioTiming({endMarkers:values}).endMarkers});
 const add=time=>{const value=Number(time);if(value>0&&Number.isFinite(value)&&(!duration||value<=duration))put([...timing.endMarkers,value]);};
 const position=e=>{const rect=e.currentTarget.getBoundingClientRect();return Math.round(Math.max(0,Math.min(duration,(e.clientX-rect.left)/rect.width*duration))*1000)/1000;};
 const interval=quantizationSeconds(asset),grid=interval&&duration?Array.from({length:Math.min(200,Math.floor(duration/interval))},(_,i)=>(i+1)*interval):[];
 return <details className="audio-timing-editor" open><summary>{tr("Маркеры конца")}{asset.kind==='music'?tr(" и квантизация"):''}</summary><fieldset disabled={disabled||!onChange}>
 <div className="audio-marker-wave" title={tr("Нажмите на волну, чтобы добавить маркер")}>
 <svg viewBox="0 0 300 60" preserveAspectRatio="none" aria-label={tr("Маркеры конца на волне звука")} onClick={e=>{if(duration)add(position(e));}}>
 {data?.peaks.map((v,i)=><line key={i} x1={i*3+1} x2={i*3+1} y1={30-v*22} y2={30+v*22} stroke="var(--muted)" strokeWidth="1.8"/>)}
 {grid.map(time=><line key={time} x1={time/duration*300} x2={time/duration*300} y1="0" y2="60" stroke="var(--border)"/>)}
 {duration>0&&timing.endMarkers.filter(time=>time<=duration).map((time,i)=><g key={time}><line x1={time/duration*300} x2={time/duration*300} y1="0" y2="60" stroke="var(--accent, #bca1ef)" strokeWidth="2"/><text x={Math.min(275,time/duration*300+3)} y="12" fill="var(--text)" fontSize="9">{i+1}</text></g>)}
 </svg></div>
 <small>{duration?tr("{0} с · нажмите на волну или введите время ниже", [duration.toFixed(3)]):error?tr("Не удалось прочитать волну. Можно задать маркеры вручную."):tr("Читаем аудиофайл…")}</small>
 <div className="audio-marker-add"><label>{tr("Новый маркер, с")}<input type="number" min="0.001" max={duration||undefined} step="0.001" value={newTime} onChange={e=>setNewTime(e.target.value)}/></label><button type="button" disabled={!(Number(newTime)>0)||!!duration&&Number(newTime)>duration} onClick={()=>{add(newTime);setNewTime('');}}>{tr("Добавить")}</button></div>
 <div className="audio-marker-list">{timing.endMarkers.map((time,i)=><div key={i}><label>{tr("Маркер ")}{i+1}{tr(", с")}<input type="number" min="0.001" max={duration||undefined} step="0.001" value={time} onChange={e=>{const value=Number(e.target.value);if(value>0&&(!duration||value<=duration))put(timing.endMarkers.map((v,j)=>i===j?value:v));}}/></label><button type="button" aria-label={tr("Удалить маркер {0}", [i+1])} onClick={()=>put(timing.endMarkers.filter((_,j)=>i!==j))}>{tr("Удалить")}</button>{duration>0&&time>duration&&<small role="alert">{tr("За концом файла · не используется")}</small>}</div>)}</div>
 {asset.kind==='music'&&<div className="audio-music-grid"><label>{tr("Темп, BPM")}<input type="number" min="20" max="400" step="0.1" value={timing.bpm} onChange={e=>change({bpm:Math.max(20,Math.min(400,Number(e.target.value)||120))})}/></label><label>{tr("Долей в такте")}<input type="number" min="1" max="16" step="1" value={timing.beatsPerBar} onChange={e=>change({beatsPerBar:Math.max(1,Math.min(16,Math.round(Number(e.target.value))||4))})}/></label><label>{tr("Длительность доли")}<select value={timing.beatUnit} onChange={e=>change({beatUnit:Number(e.target.value)})}>{[2,4,8,16].map(unit=><option key={unit} value={unit}>1/{unit}</option>)}</select></label><label>{tr("Частота квантизации")}<select value={timing.quantization} onChange={e=>change({quantization:e.target.value})}>{QUANTIZATION.map(([value,label])=><option key={value} value={value}>{tr(label)}</option>)}</select></label><small>{tr("Сетка начинается в начале трека. При завершении музыка ждёт ближайшую долю, такт или маркер. Изменение BPM не меняет скорость записи.")}</small></div>}
 </fieldset></details>;
}
