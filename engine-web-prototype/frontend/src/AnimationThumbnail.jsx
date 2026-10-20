import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useEffect,useRef,useState} from 'react';
import {registerAnimationThumbnail} from './animationThumbnails.js';

export default function AnimationThumbnail({character,clip}){
 useLocale();
 const canvas=useRef(),live=useRef(),[error,setError]=useState('');live.current={character,clip};
 useEffect(()=>{setError('');return registerAnimationThumbnail(canvas.current,()=>live.current,setError);},[character.id,character.model?.src,clip.id]);
 return <div className="animation-thumbnail"><canvas ref={canvas} role="img" aria-label={tr("Движение «")+clip.name+tr("» на модели персонажа")}/>{error&&<span>{message(error)}</span>}<small>{tr("Предпросмотр движения")}</small></div>;
}
