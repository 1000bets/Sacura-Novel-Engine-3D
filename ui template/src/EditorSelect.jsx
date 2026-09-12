import React,{useEffect,useId,useRef,useState} from 'react';
import {createPortal} from 'react-dom';
import {Icon} from './StudioParts.jsx';

// A themed popup for the scene switcher, independent of the OS select palette.
export default function EditorSelect({value,options,onChange,label}){
 const trigger=useRef(),popup=useRef(),id=useId(),[open,setOpen]=useState(false),[active,setActive]=useState(0),[rect,setRect]=useState(null);
 const selected=Math.max(0,options.findIndex(o=>o[0]===value));
 const close=()=>{setOpen(false);trigger.current?.focus();};
 const choose=index=>{if(options[index])onChange(options[index][0]);close();};
 useEffect(()=>{
  if(!open)return;
  const place=()=>{const r=trigger.current.getBoundingClientRect();setRect({left:Math.max(8,Math.min(r.left,window.innerWidth-288)),top:r.bottom+4,width:Math.min(Math.max(r.width,280),window.innerWidth-16),maxHeight:Math.max(100,window.innerHeight-r.bottom-16)});};
  const outside=e=>{if(!trigger.current?.contains(e.target)&&!popup.current?.contains(e.target))setOpen(false);};
  place();document.addEventListener('pointerdown',outside);window.addEventListener('resize',place);window.addEventListener('scroll',place,true);
  return()=>{document.removeEventListener('pointerdown',outside);window.removeEventListener('resize',place);window.removeEventListener('scroll',place,true);};
 },[open]);
 useEffect(()=>{if(open)popup.current?.querySelector('[aria-selected="true"]')?.scrollIntoView({block:'nearest'});},[active,open]);
 const keyDown=e=>{
  if(['ArrowDown','ArrowUp','Home','End','Enter',' ','Escape'].includes(e.key))e.preventDefault();
  if(e.key==='Escape'){setOpen(false);e.stopPropagation();return;}
  if(e.key==='Tab'){setOpen(false);return;}
  if(!open&&['ArrowDown','ArrowUp','Enter',' '].includes(e.key)){setActive(selected);setOpen(true);return;}
  if(e.key==='ArrowDown')setActive(i=>Math.min(options.length-1,i+1));
  if(e.key==='ArrowUp')setActive(i=>Math.max(0,i-1));
  if(e.key==='Home')setActive(0);if(e.key==='End')setActive(options.length-1);
  if(open&&['Enter',' '].includes(e.key))choose(active);
 };
 return <><button ref={trigger} className="scene-selector" role="combobox" aria-label={label} aria-expanded={open} aria-controls={open?id:undefined} aria-activedescendant={open?id+'-'+active:undefined} aria-haspopup="listbox" onKeyDown={keyDown} onClick={()=>{setActive(selected);setOpen(v=>!v);}}><span>{options[selected]?.[1]||'Выберите сабсцену'}</span><Icon name="ChevronDown" size={14}/></button>
  {open&&rect&&createPortal(<div id={id} role="listbox" aria-label={label} ref={popup} className="editor-select-popup" style={rect}>{options.map(([optionValue,text],i)=><button key={optionValue} id={id+'-'+i} role="option" aria-selected={i===active} tabIndex={-1} className={value===optionValue?'chosen':''} onMouseEnter={()=>setActive(i)} onPointerDown={e=>e.preventDefault()} onClick={()=>choose(i)}><span>{text}</span>{value===optionValue&&<Icon name="Check" size={14}/>}</button>)}</div>,document.body)}
 </>;
}
