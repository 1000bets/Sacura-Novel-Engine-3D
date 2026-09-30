import React,{useEffect,useRef,useState} from 'react';
import {registerAnimationThumbnail} from './animationThumbnails.js';

export default function AnimationThumbnail({character,clip}){
 const canvas=useRef(),live=useRef(),[error,setError]=useState('');live.current={character,clip};
 useEffect(()=>{setError('');return registerAnimationThumbnail(canvas.current,()=>live.current,setError);},[character.id,character.model?.src,clip.id]);
 return <div className="animation-thumbnail"><canvas ref={canvas} role="img" aria-label={'Движение «'+clip.name+'» на модели персонажа'}/>{error&&<span>{error}</span>}<small>Предпросмотр движения</small></div>;
}
