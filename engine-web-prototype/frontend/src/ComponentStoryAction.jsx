import React,{useState} from 'react';
import ComponentCallFields from './ComponentCallFields.jsx';
import {addStoryComponentCall} from './componentStory.js';
export default function ComponentStoryAction({project,beatId,phase='ON_START',batchId,onChange,onAdded}){
 const [call,setCall]=useState({id:'sacura.component.call',version:1,parameters:{}}),[error,setError]=useState('');
 return <section className="component-story-call" aria-label="Метод компонента в истории"><ComponentCallFields project={project} call={call} onChange={setCall}/><button type="button" disabled={!call.parameters.instance?.id||!call.parameters.methodId} onClick={()=>{try{onChange(p=>addStoryComponentCall(p,beatId,phase,call,batchId));setError('');onAdded?.();}catch(e){setError(e.message);}}}>Добавить вызов в историю</button>{error&&<p role="alert">{error}</p>}</section>;
}
