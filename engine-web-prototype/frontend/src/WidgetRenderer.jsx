import {t as tr, useLocale} from './i18n.jsx';
import React,{useEffect,useId,useRef,useState} from 'react';
import {scopeWidgetCss,widgetRects,isUnmodifiedDefaultWidget} from './widgetModel.js';
import './widgets.css';

export default function WidgetRenderer({widget,context={},onAction,onChoice,choiceEnabled=()=>true,editing=false,width,height}){
 useLocale();
 const ref=useRef(null),[size,setSize]=useState({width:width||1280,height:height||720}),token=useId().replace(/[^a-zA-Z0-9]/g,''),scope='widget-scope-'+token;
 useEffect(()=>{if(width&&height){setSize({width,height});return;}const node=ref.current;if(!node)return;const observer=new ResizeObserver(([entry])=>setSize({width:entry.contentRect.width,height:entry.contentRect.height}));observer.observe(node);return()=>observer.disconnect();},[width,height]);
 const rects=widgetRects(widget,size.width,size.height),scale=Math.min(size.width/widget.referenceSize[0],size.height/widget.referenceSize[1]);
 const custom=scopeWidgetCss(widget.css,'.'+scope);
 const standard=isUnmodifiedDefaultWidget(widget);
 const textFor=e=>({speaker:context.speaker,dialogueText:context.text,status:context.status,title:context.title}[e.role]??(standard?tr(e.text):e.text));
 return <div ref={ref} className={'widget-surface '+scope+(editing?' widget-editing':'')} style={width&&height?{width,height}:undefined} data-widget={widget.id} onPointerDown={e=>e.stopPropagation()} onPointerUp={e=>e.stopPropagation()}>
  {custom.css&&<style>{custom.css}</style>}
  {widget.elements.map((e,index)=>{
   const rect=rects[e.id];let parent=widget.elements.find(x=>x.id===e.parentId),hidden=!e.visible,seen=new Set();while(parent&&!seen.has(parent.id)){seen.add(parent.id);hidden||=!parent.visible;parent=widget.elements.find(x=>x.id===parent.parentId);}
   if(!rect||hidden||(e.role==='choices'&&!context.choices?.length&&!editing))return null;
   const s=e.style||{},style={left:rect.x,top:rect.y,width:rect.width,height:rect.height,zIndex:index+1,'--we-bg':s.background||'transparent','--we-color':s.color||'#fff4f8','--we-font-size':Math.max(11,(s.fontSize||24)*scale)+'px','--we-font-family':s.fontFamily||'system-ui','--we-font-weight':s.fontWeight||400,'--we-radius':(s.borderRadius||0)*scale+'px','--we-border-width':(s.borderWidth||0)*scale+'px','--we-border-color':s.borderColor||'transparent','--we-padding':(s.padding||0)*scale+'px','--we-opacity':s.opacity??1,'--we-align':s.textAlign||'left','--we-gap':(s.gap??8)*scale+'px'};
   const props={key:e.id,className:'widget-element widget-'+e.type,style,'data-element':e.id,'data-role':e.role};
   if(e.type==='button')return <button {...props} disabled={!editing&&((e.action==='advance'&&!context.canAdvance)||(e.action==='resume'&&!context.paused))} onClick={()=>!editing&&onAction?.(e.action)}>{textFor(e)}</button>;
   if(e.type==='choices')return <div {...props}>{(context.choices||[{id:'preview',label:tr('Пример ответа')}]).map(choice=><button key={choice.id} className={context.selectedChoiceId===choice.id?'input-selected':undefined} aria-pressed={context.selectedChoiceId===choice.id} disabled={!editing&&(!context.ready||!choiceEnabled(choice))} onClick={()=>!editing&&onChoice?.(choice.id)}>{choice.label}</button>)}</div>;
   if(e.type==='image')return <div {...props}>{e.src?<img src={e.src} alt={e.text||e.name} draggable={false}/>:editing?tr("Изображение"):null}</div>;
   const advance=e.role==='dialogueText'&&context.canAdvance&&!editing;
   return <div {...props} role={advance?'button':undefined} tabIndex={advance?0:undefined} aria-label={advance?tr("Продолжить диалог"):undefined} onClick={()=>advance&&onAction?.('advance')} onKeyDown={event=>{if(advance&&['Enter',' '].includes(event.key)){event.preventDefault();event.stopPropagation();onAction?.('advance');}}}>{e.type==='text'?textFor(e):null}</div>;
  })}
 </div>;
}

export function WidgetMenu({widget,label,...props}){
 useLocale();
 const ref=useRef();
 useEffect(()=>{const previous=document.activeElement;ref.current?.querySelector('button:not(:disabled)')?.focus();return()=>{if(previous?.isConnected)previous.focus();};},[widget.id]);
 return <div ref={ref} className="widget-menu-overlay" role="dialog" aria-label={tr(label)} onKeyDown={event=>{
  if(event.key!=='Tab')return;
  const buttons=[...ref.current.querySelectorAll('button:not(:disabled)')];if(!buttons.length)return;
  const index=buttons.indexOf(document.activeElement);
  if(event.shiftKey&&index<=0){event.preventDefault();buttons.at(-1).focus();}
  else if(!event.shiftKey&&(index===buttons.length-1||index===-1)){event.preventDefault();buttons[0].focus();}
 }}><WidgetRenderer widget={widget} {...props}/></div>;
}
