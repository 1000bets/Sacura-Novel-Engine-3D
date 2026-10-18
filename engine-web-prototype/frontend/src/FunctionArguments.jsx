import {SchemaValue} from './SchemaFields.jsx';
import React from 'react';
import {GameField,GameSelect,GameNumber} from './GameplayCommandFields.jsx';
export default function FunctionArguments({project,functionId,parameters,value={},onChange,expressionOptions=[]}) {
  const fn=project.functions.find(f=>f.id===functionId);
  return (parameters??fn?.parameters)?.map(a=>{
    const v=value[a.name]??a.default,patch=v=>onChange({...value,[a.name]:v});
    if(a.schema)return <SchemaValue key={a.name} project={project} schema={a.schema} value={v} label={a.label||a.name} parameters={value} expressionOptions={expressionOptions} onChange={patch}/>;
    if(expressionOptions.length)return <SchemaValue key={a.name} project={project} schema={a.referenceType?{type:'reference',referenceType:a.referenceType}:{type:a.type}} value={a.referenceType&&typeof v==='string'?{kind:'reference',type:a.referenceType,mode:'direct',id:v}:v} label={a.label||a.name} expressionOptions={expressionOptions} onChange={next=>patch(next?.mode==='direct'?next.id:next)}/>;
    if(a.referenceType){const records=({item:project.gameplay.items,object:project.objects,scene:project.subscenes,event:project.events,function:project.functions})[a.referenceType]||[];return <GameSelect key={a.name} label={a.label||a.name} value={v} options={[["","Выберите…"],...records.map(i=>[i.id,i.name])]} onChange={patch}/>;}
    if(v&&typeof v==='object'&&!['array','struct'].includes(a.type))return <p key={a.name}>{a.label||a.name}: используется выражение аргумента.</p>;
    if(a.type==='number')return <GameNumber key={a.name} label={a.label||a.name} value={v??0} min={-Number.MAX_SAFE_INTEGER} onChange={patch}/>;
    if(a.type==='boolean')return <GameSelect key={a.name} label={a.label||a.name} value={String(v??false)} options={[["true","Да"],["false","Нет"]]} onChange={v=>patch(v==='true')}/>;
    if(a.type==='string')return <GameField key={a.name} label={a.label||a.name}><input value={v??''} onChange={e=>patch(e.target.value)}/></GameField>;
    return <p key={a.name}>{a.label||a.name}: сложный параметр настраивается в графе функции.</p>;
  });
}
