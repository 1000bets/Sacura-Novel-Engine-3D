import React,{useEffect,useLayoutEffect,useRef,useState} from 'react';
import {createPortal} from 'react-dom';
import './anchoredPopover.css';

// Render outside scrollable editor panels, while keeping the popup next to its control.
export default function AnchoredPopover({anchor,onClose,width=260,align='start',className='',role='dialog',label,id,children}){
 const OwnerDocument = anchor.ownerDocument;
 const OwnerWindow = OwnerDocument.defaultView;
 const panel=useRef(null),close=useRef(onClose),[position,setPosition]=useState(null);
 const positioned=position!==null;
 close.current=onClose;
 useLayoutEffect(()=>{
  const place=()=>{
   if(!anchor?.isConnected){close.current();return;}
   const box=anchor.getBoundingClientRect(),margin=8,gap=6;
   const availableWidth=Math.max(0,OwnerWindow.innerWidth-margin*2),popupWidth=Math.min(width,availableWidth);
   const below=Math.max(0,OwnerWindow.innerHeight-box.bottom-gap-margin),above=Math.max(0,box.top-gap-margin);
   const desiredHeight=Math.min(panel.current.scrollHeight,OwnerWindow.innerHeight-margin*2);
   const upwards=desiredHeight>below&&above>below,maxHeight=upwards?above:below;
   const height=Math.min(desiredHeight,maxHeight);
   setPosition({width:popupWidth,maxHeight,left:Math.max(margin,Math.min(align==='end'?box.right-popupWidth:box.left,OwnerWindow.innerWidth-popupWidth-margin)),top:upwards?box.top-gap-height:box.bottom+gap});
  };
  const scroll=e=>{if(!panel.current?.contains(e.target))place();};
  place();OwnerWindow.addEventListener('resize',place);OwnerWindow.addEventListener('scroll',scroll,true);
  return()=>{OwnerWindow.removeEventListener('resize',place);OwnerWindow.removeEventListener('scroll',scroll,true);};
 },[anchor,width,align]);
 useEffect(()=>{
  const outside=e=>{if(!panel.current?.contains(e.target)&&!anchor?.contains(e.target))close.current();};
  OwnerDocument.addEventListener('pointerdown',outside,true);
  return()=>OwnerDocument.removeEventListener('pointerdown',outside,true);
 },[anchor]);
 useLayoutEffect(()=>{
  if(!positioned)return;
  const initial=panel.current?.querySelector('input:checked')||panel.current?.querySelector('[role="menuitem"]:not(:disabled),input:not(:disabled),button:not(:disabled)');
  initial?.focus({preventScroll:true});
 },[anchor,positioned]);
 return createPortal(<div ref={panel} id={id} role={role} aria-label={label} className={'anchored-popover '+className}
  style={position||{width:Math.min(width,OwnerWindow.innerWidth-16),visibility:'hidden'}}
  onBlur={e=>{if(e.relatedTarget&&!e.currentTarget.contains(e.relatedTarget)&&!anchor?.contains(e.relatedTarget))close.current();}}
  onKeyDown={e=>{
   if(e.key==='Escape'){e.preventDefault();e.stopPropagation();close.current();anchor?.focus({preventScroll:true});return;}
   if(e.key==='Tab'){close.current();anchor?.focus({preventScroll:true});return;}
   if(role!=='menu'||!['ArrowDown','ArrowUp','Home','End'].includes(e.key))return;
   e.preventDefault();e.stopPropagation();
   const items=[...panel.current.querySelectorAll('[role="menuitem"]:not(:disabled)')],current=items.indexOf(OwnerDocument.activeElement);
   const index=e.key==='Home'?0:e.key==='End'?items.length-1:(current+(e.key==='ArrowDown'?1:-1)+items.length)%items.length;
   items[index]?.focus();
  }}>{children}</div>,OwnerDocument.body);
}
