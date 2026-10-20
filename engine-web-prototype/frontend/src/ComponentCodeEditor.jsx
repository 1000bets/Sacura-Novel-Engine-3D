import React,{useId,useRef,useState} from 'react';
import {ArrowLeft,Save,BookOpen,Code2,Check,FileEdit,AlertCircle,UsersRound} from 'lucide-react';
import MechanicScriptEditor from './MechanicScriptEditor.jsx';
import BlueprintFunctionWorkspace from './BlueprintFunctionWorkspace.jsx';
import {componentCodeCalls} from './componentCodeNavigation.js';
import {logicResource,editComponentSource,publishComponentSource} from './componentResources.js';
import {parseComponentSource,componentExecutableSource} from './componentSources.js';
import {validateMechanicJavaScript} from './mechanicScriptRuntime.js';
import './components.css';

export default function ComponentCodeEditor({project,resourceId,onChange,onBack,disabled=false,methodId,backLabel='К месту вызова',toolbar}){
 const resource=logicResource(project,resourceId),[busy,setBusy]=useState(false),[help,setHelp]=useState(false),[status,setStatus]=useState(''),[opened,setOpened]=useState(''),latest=useRef(),helpId=useId();latest.current=resource?.source;
 if(!resource)return <p role="alert">Исходник компонента удалён.</p>;
 const save=async()=>{if(busy||disabled)return;const source=resource.source;setBusy(true);setStatus('');try{
  const draft=structuredClone(project);publishComponentSource(draft,resourceId);await validateMechanicJavaScript(componentExecutableSource(source,Object.keys(parseComponentSource(source).methods)[0]||''));
  if(latest.current!==source){setStatus('Код изменён. Сохраните ещё раз.');return;}
  onChange(p=>publishComponentSource(p,resourceId));setStatus('Сохранено');
 }catch(error){if(latest.current===source)onChange(p=>Object.assign(logicResource(p,resourceId),{status:'error',approved:false,error:{message:error.message,line:error.line||1,column:error.column||1}}));}finally{setBusy(false);}};
 const calls=componentCodeCalls(project,resource),definition=project.gameplay.mechanicDefinitions.find(d=>d.id===resource.definitionId&&d.logicResourceId===resource.id),count=project.gameplay.mechanics.filter(m=>m.definitionId===definition?.id&&m.definitionVersion===definition?.version).length;
 const draft=resource.status==='draft',review=resource.status==='review',StatusIcon=resource.error||review?AlertCircle:draft?FileEdit:Check;
 const statusText=resource.error?resource.path+':'+resource.error.line+':'+resource.error.column+' · '+resource.error.message:busy?'Проверка…':draft?'Черновик сохранён':review?'Проверьте и сохраните код':status||'Готов к запуску';
 return <><section hidden={!!opened} className="component-code-editor" aria-label="Исходник компонента">
  <header className="component-code-toolbar">
   <div className="component-code-heading">
   {onBack&&<button className="component-icon-button component-quiet" type="button" onClick={onBack} title={backLabel} aria-label={backLabel}><ArrowLeft aria-hidden="true" size={16}/></button>}
   <div className="component-file-title" title={resource.path}><Code2 aria-hidden="true" size={17}/><strong>{resource.name}{methodId&&<span className="component-method-title"> / {methodId}</span>}</strong><span className="component-language">JS</span></div>
   </div>
   <div className="component-code-actions">{toolbar}<button className="component-primary component-save" type="button" aria-label="Сохранить компонент" title="Сохранить · ⌘S / Ctrl+S" disabled={disabled||busy} onClick={save}><Save aria-hidden="true" size={15}/><span>{busy?'Проверка…':'Сохранить'}</span></button><button className="component-icon-button component-quiet" type="button" title="Справка SDK компонентов" aria-label="Справка SDK компонентов" aria-expanded={help} aria-controls={help?helpId:undefined} onClick={()=>setHelp(!help)}><BookOpen aria-hidden="true" size={16}/></button></div>
  </header>
  {help&&<div id={helpId} className="component-sdk-help">
   <div><strong>Данные</strong><p><code>args</code> — аргументы, <code>params</code> — параметры,<br/><code>state</code> — состояние, <code>refs</code> — ссылки.</p></div>
   <div><strong>Вызовы</strong><p><code>await engine.invoke(refs.other, "add", &#123;amount: 2&#125;)</code><br/><code>await engine.emit("changed", &#123;value: state.value&#125;)</code></p></div>
   <p className="component-sdk-note">Код общий для всех подключений. Сохраните перед запуском.</p>
  </div>}
  {calls.length>0&&<details className="component-code-calls"><summary>Вызовы компонентов <span className="component-count">{calls.length}</span></summary>{calls.map((call,index)=><button type="button" key={index} disabled={!call.method} onClick={()=>setOpened(call.method.logic.functionId)}>{call.receiver} → {call.definition.name} · {call.method?.name||'Метод удалён: '+call.methodId}<small>строка {call.line}</small></button>)}</details>}
  <MechanicScriptEditor key={resource.id} source={resource.source} parseSource={parseComponentSource} label="JavaScript компонента" disabled={disabled} diagnostic={resource.error} onCompile={save} onChange={source=>{onChange(p=>editComponentSource(p,resource.id,source));setStatus('');}}/>
  <footer className="component-code-status">
   <span className="component-save-status" role={resource.error?'alert':'status'} title={resource.error?'':'Изменения применятся при следующем запуске.'}><StatusIcon aria-hidden="true" size={13}/>{statusText}</span>
   <span className="component-source-path" title={resource.path}>{resource.path}</span>
   <span className="component-code-scope" title={"Общий код для всех подключений: "+count}><UsersRound aria-hidden="true" size={13}/>Общий код</span>
  </footer>
 </section>{opened&&<BlueprintFunctionWorkspace project={project} requestedId={opened} onChange={onChange} onBack={()=>setOpened('')} backLabel="К месту вызова" breadcrumb={resource.path}/>}</>;
}
