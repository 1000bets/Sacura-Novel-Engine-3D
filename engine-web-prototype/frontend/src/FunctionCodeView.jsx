import React,{useMemo,useState} from 'react';
import {Copy,Check,GitBranch} from 'lucide-react';
import {functionCode} from './functionCode.js';
import {t as tr,useLocale} from './i18n.jsx';

function Highlight({text}){
 const pattern=/"(?:\\.|[^"\\])*"|\/\/.*$|\b(?:async|function|await|return|if|else|let|const|true|false|null|undefined|number|string|boolean|Promise|Array|Value)\b|\b\d+(?:\.\d+)?\b/g,parts=[];let offset=0;
 for(const match of text.matchAll(pattern)){if(match.index>offset)parts.push(text.slice(offset,match.index));const token=match[0],kind=token.startsWith('//')?'comment':token.startsWith('"')?'string':/^\d/.test(token)?'number':'keyword';parts.push(<span className={'code-'+kind} key={match.index}>{token}</span>);offset=match.index+token.length;}
 parts.push(text.slice(offset));return parts;
}
export default function FunctionCodeView({project,fn,onNode,nodeLabel}){
 const locale=useLocale(),source=useMemo(()=>functionCode(project,fn,{translate:tr}),[project,fn,locale]),[status,setStatus]=useState('');
 const copy=async()=>{try{await navigator.clipboard.writeText(source.text);setStatus('Скопировано');}catch{setStatus('Не удалось скопировать. Выделите текст и нажмите Ctrl+C или ⌘C.');}};
 return <section className="function-code" aria-label={tr('Код функции')} onKeyDown={e=>e.stopPropagation()}>
  <header className="function-code-toolbar"><span>{tr('Текст Blueprint · только чтение')}</span><button type="button" aria-label={tr('Копировать код')} onClick={copy}>{status==='Скопировано'?<Check size={14} aria-hidden="true"/>:<Copy size={14} aria-hidden="true"/>}{tr('Копировать код')}</button></header>
  <p className="function-code-help">{tr('Псевдокод обновляется вместе с графом. Номер строки открывает соответствующий узел.')}</p>
  <pre tabIndex={0} aria-label={tr('Текст функции')}><code>{source.lines.map((line,index)=><span className="function-code-line" key={index}>{line.nodeId?<button type="button" className="function-code-number" aria-label={tr('Показать узел в Blueprint · {0}',[nodeLabel(line.nodeId)])} title={tr('Показать узел в Blueprint')} onClick={()=>onNode(line.nodeId)}><span>{index+1}</span><GitBranch size={11} aria-hidden="true"/></button>:<span className="function-code-number" aria-hidden="true">{index+1}</span>}<span className="function-code-text"><Highlight text={line.text}/>{'\n'}</span></span>)}</code></pre>
  <span className="function-code-status" role="status">{tr(status)}</span>
 </section>;
}
