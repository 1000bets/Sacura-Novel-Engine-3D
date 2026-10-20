import {inputLabel} from './inputPresentation.js';
import React,{useEffect,useRef,useState} from 'react';
import {t as tr, useLocale} from './i18n.jsx';
import './input.css';
function Control({action,send}){
 const [position,setPosition]=useState([0,0]),[held,setHeld]=useState(false),pointer=useRef(null),node=useRef(null);
 const release=()=>{pointer.current=null;setHeld(false);setPosition([0,0]);if(node.current?.type==='range')node.current.value=0;send(action.id,action.valueType==='axis2d'?[0,0]:0);};
 useEffect(()=>{const blur=()=>release();window.addEventListener('blur',blur);return()=>{window.removeEventListener('blur',blur);send(action.id,action.valueType==='axis2d'?[0,0]:0);};},[action.id]);
 const update=e=>{const r=e.currentTarget.getBoundingClientRect();let x=(e.clientX-r.left-r.width/2)/(r.width/2),y=-(e.clientY-r.top-r.height/2)/(r.height/2),length=Math.hypot(x,y);if(length>1){x/=length;y/=length;}setPosition([x,y]);send(action.id,[x,y]);};
 const down=e=>{if(pointer.current!==null)return;e.preventDefault();e.stopPropagation();e.currentTarget.closest('.scene-viewport')?.focus({preventScroll:true});pointer.current=e.pointerId;setHeld(true);e.currentTarget.setPointerCapture(e.pointerId);if(action.valueType==='axis2d')update(e);else send(action.id,1);};
 const up=e=>{if(e.pointerId!==pointer.current)return;e.preventDefault();release();if(e.currentTarget.hasPointerCapture(e.pointerId))e.currentTarget.releasePointerCapture(e.pointerId);};
 if(action.valueType==='axis1d')return <label className="touch-axis">{inputLabel(action)}<input ref={node} aria-label={inputLabel(action)} type="range" min="-1" max="1" step=".01" defaultValue="0" onChange={e=>send(action.id,Number(e.target.value))} onKeyUp={release} onPointerUp={release} onPointerCancel={release} onBlur={release}/></label>;
 return <button ref={node} className={action.valueType==='axis2d'?'touch-stick':'touch-button'} aria-label={inputLabel(action)} aria-pressed={action.valueType==='boolean'?held:undefined}
   onPointerDown={down} onPointerMove={e=>{if(e.pointerId===pointer.current&&action.valueType==='axis2d')update(e);}} onPointerUp={up} onPointerCancel={up} onLostPointerCapture={release}
   onKeyDown={e=>{const v={ArrowUp:[0,1],ArrowDown:[0,-1],ArrowLeft:[-1,0],ArrowRight:[1,0]}[e.code];if(action.valueType==='axis2d'&&v){e.preventDefault();setPosition(v);send(action.id,v);}else if(action.valueType==='boolean'&&['Space','Enter'].includes(e.code)&&!e.repeat){e.preventDefault();setHeld(true);send(action.id,1);}}}
   onKeyUp={e=>{if(['Space','Enter','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.code)){e.preventDefault();release();}}} onBlur={release}>
   {action.valueType==='axis2d'?<><span className="touch-stick-knob" style={{transform:`translate(${position[0]*26}px,${-position[1]*26}px)`}}/><small>{inputLabel(action)}</small></>:inputLabel(action)}
 </button>;
}
export default function TouchInput({settings,contexts,active,paused,playerMode}){
 useLocale();const root=useRef(),[coarse,setCoarse]=useState(false);
 useEffect(()=>{const media=window.matchMedia('(pointer: coarse)');const update=()=>setCoarse(media.matches);update();media.addEventListener('change',update);return()=>media.removeEventListener('change',update);},[]);
 if(!active||settings.touchMode==='off'||settings.touchMode==='auto'&&!coarse)return null;
 const bindings=settings.contexts.filter(c=>(paused?['system']:contexts).includes(c.id)).sort((a,b)=>b.priority-a.priority).flatMap(c=>c.bindings.filter(b=>b.device==='touch'));
 const ids=new Set(bindings.map(b=>b.actionId));
 const actions=settings.actions.filter(a=>ids.has(a.id)&&a.behavior!=='point'&&(a.behavior!=='move'||['wasd','both'].includes(playerMode)));
 const send=(id,value)=>{const viewport=root.current?.closest('.scene-viewport');if(!viewport)return;viewport.dispatchEvent(new CustomEvent('sacura-input',{detail:{device:'touch',control:bindings.find(b=>b.actionId===id)?.control||id,sourceId:'touch:'+id,value}}));};
 return <div className="touch-input" ref={root} aria-label={tr('Экранное управление')} onPointerDown={e=>e.stopPropagation()} onPointerUp={e=>e.stopPropagation()}>
   <div className="touch-axes">{actions.filter(a=>a.valueType!=='boolean').map(a=><Control key={a.id} action={a} send={send}/>)}</div>
   <div className="touch-buttons">{actions.filter(a=>a.valueType==='boolean').map(a=><Control key={a.id} action={a} send={send}/>)}</div>
 </div>;
}
