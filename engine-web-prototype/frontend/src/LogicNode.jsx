import {t as tr, useLocale} from './i18n.jsx';
import {variableType,runtimeVariableType,VARIABLE_TYPES} from './variableModel.js';
import React from 'react';
import {Handle,Position} from '@xyflow/react';
import {Icon} from './StudioParts.jsx';
import {ValueField} from './ChoiceInspector.jsx';
import {VALUE_TYPES,variableName,typedValue} from './choiceModel.js';
import {compareSymbols,isPureNode,MATH_OPERATIONS,logicType} from './logicModel.js';
import './logicNodes.css';

const typeNames={number:'Число',integer:'Целое',boolean:'Да / нет',string:'Текст'};
const compareNames={eq:'Равно',ne:'Не равно',gt:'Больше',gte:'Больше или равно',lt:'Меньше',lte:'Меньше или равно'};
const mathSymbols={add:'+',sub:'−',mul:'×',div:'÷',mod:'%',pow:'xⁿ',min:'min',max:'max',negate:'−x',abs:'|x|',round:'≈',floor:'⌊x⌋',ceil:'⌈x⌉',sqrt:'√'};
const nodeAppearance={
 literal:{icon:'Braces',category:'Константа',title:'Значение'},
 math:{icon:'Calculator',category:'Математика'},
 compare:{icon:'Equal',category:'Сравнение'},
 and:{symbol:'∧',category:'Логика',title:'И'},
 or:{symbol:'∨',category:'Логика',title:'ИЛИ'},
 not:{symbol:'¬',category:'Логика',title:'НЕ'},
 convert:{icon:'ArrowLeftRight',category:'Преобразование',title:'Конвертация'},
 branch:{icon:'GitFork',category:'Ход истории',title:'Если'},
 variable:{icon:'Database',category:'Чтение переменной'},
 'set-variable':{icon:'Database',category:'Запись переменной'},
};

// Keep native selects interactive while positioning their chevrons consistently at any graph zoom.
function SelectShell({children}){
 useLocale();
 return <span className="utility-select">{children}<Icon name="ChevronDown" size={14}/></span>;
}
function UtilitySelect({children,...props}){
 useLocale();
 return <SelectShell><select {...props}>{children}</select></SelectShell>;
}
function UtilityValueField(props){
 useLocale();
 return props.type==='boolean'?<SelectShell><ValueField {...props}/></SelectShell>:<ValueField {...props}/>;
}

export default function LogicNode({data,selected}){
 useLocale();
 const n=data.beat,patch=values=>data.onLogicChange(n.id,values),pure=isPureNode(n);
 const variable=['variable','set-variable'].includes(n.kind);
 const type=variable?variableType(data.project,n.variable)||n.valueType||'string':logicType(data.project,n);
 const typeLabel=typeNames[type]||'Значение';
 const variableTypeLabel=VARIABLE_TYPES.find(([id])=>id===type)?.[1].split(' · ')[0]||typeLabel;
 const appearance=nodeAppearance[n.kind]||nodeAppearance.branch;
 const title=variable?variableName(n.variable)||'Переменная не найдена':n.kind==='math'?(MATH_OPERATIONS[n.operator||'add']?.label||'Операция').split(' · ')[0]:n.kind==='compare'?compareNames[n.operator]||'Сравнить':appearance.title;
 const symbol=n.kind==='math'?mathSymbols[n.operator||'add']:n.kind==='compare'?compareSymbols[n.operator]:appearance.symbol;
 const header=<header className="moment-grab utility-heading">
  <div className={"utility-symbol"+(symbol?.length>2?" utility-symbol-word":"")} aria-hidden="true">{symbol||<Icon name={appearance.icon} size={20}/>}</div>
  <div className="utility-identity"><small>{tr(appearance.category)}</small><strong>{variable?title:tr(title)}</strong></div>
  {variable&&<span className="utility-mode">{n.kind==='variable'?'GET':'SET'}</span>}
 </header>;
 const issue=data.issue&&<div className="moment-issue"><Icon name="TriangleAlert" size={14}/>{tr(data.issue.title)}</div>;
 const input=(port,label,value,inputType='number')=><div className={'logic-row utility-input'+(n.inputs?.[port]?' connected':'')} key={port}>
  <Handle className={'data-pin pin-type-'+inputType} type="target" position={Position.Left} id={port} title={inputType==='boolean'?tr("Вход Bool · соедините с круглым выходом Get Bool или сравнения"):tr(label)+tr(" · вход значения")} aria-label={tr(label)+tr(" · вход ")+inputType}/>
  <label className="utility-input-label"><span className={'utility-port-label'+(['A','B'].includes(label)?' operand':'')}>{tr(label)}</span>{n.inputs?.[port]?<span className="utility-connected"><Icon name="Link2" size={14}/>{tr("Подключено")}</span>:<UtilityValueField label={tr(label)+' '+n.id} type={inputType} value={value} onChange={v=>patch({[port]:v})}/>}</label>
 </div>;
 const output=<div className="logic-row logic-output utility-output"><span><Icon name="ArrowRight" size={14}/>{variable||n.kind==='literal'?tr("Значение"):tr("Результат")}</span><span className={'utility-type type-'+type}>{variable?variableTypeLabel:tr(typeLabel)}</span><Handle className={'data-pin pin-type-'+type} type="source" position={Position.Right} id="value" title={(variable?tr("Значение"):tr("Результат"))+' · '+tr(typeLabel)+tr(" · выход")} aria-label={tr("Выход · ")+tr(typeLabel)}/></div>;
 const className='story-moment utility-node logic-'+n.kind+(n.kind==='variable'?' variable-get': ' logic-node')+(n.kind==='set-variable'?' variable-set':'')+(n.kind==='branch'?' logic-if':'')+' type-'+type+(selected?' selected':'');
 if(n.kind==='variable')return <article className={className} onDoubleClick={()=>data.onInspectVariable?.(n.variable)}>{header}<div className="utility-get-meta"><span className="variable-type-dot"/>{variableTypeLabel}<span>{tr("Значение переменной")}</span></div><Handle className={'data-pin pin-type-'+type} type="source" position={Position.Right} id="value" title={tr("Значение · ")+tr(typeLabel)+tr(" · выход")} aria-label={tr("Выход · ")+tr(typeLabel)}/>{issue}</article>;
 return <article className={className} onDoubleClick={variable?()=>data.onInspectVariable?.(n.variable):undefined}>
  {header}
  <div className="logic-body nodrag" onClick={e=>e.stopPropagation()}>
   {n.kind==='set-variable'&&<><div className="logic-row utility-execution"><Handle type="target" position={Position.Left} id="in" title={tr("Выполнить Set")}/><span>{tr("Выполнить")}</span><span>{tr("Далее")}</span><Handle type="source" position={Position.Right} id="next" title={tr("Продолжить после Set")}/></div>{input('value','Значение',n.value,runtimeVariableType(data.project,n.variable)||n.valueType)}</>}
   {n.kind==='literal'&&<><label>{tr("Тип значения")}<UtilitySelect aria-label={tr("Тип значения ")+n.id} value={n.valueType||'string'} onChange={e=>patch({valueType:e.target.value,value:''})}>{VALUE_TYPES.map(([v,label])=><option key={v} value={v}>{tr(label)}</option>)}</UtilitySelect></label><label>{tr("Значение")}<UtilityValueField label={tr("Значение ")+n.id} type={n.valueType} value={n.value} onChange={value=>patch({value})}/></label></>}
   {n.kind==='compare'&&<><div className="utility-settings"><label>{tr("Оператор")}<UtilitySelect aria-label={tr("Оператор ")+n.id} value={n.operator} onChange={e=>patch({operator:e.target.value,...(!['eq','ne'].includes(e.target.value)?{valueType:'number',a:typedValue(n.a,'number'),b:typedValue(n.b,'number')}:{})})}>{Object.entries(compareSymbols).map(([v,label])=><option key={v} value={v}>{tr(label)}</option>)}</UtilitySelect></label><label>{tr("Тип")}<UtilitySelect aria-label={tr("Тип сравнения ")+n.id} value={n.valueType||'number'} onChange={e=>patch({valueType:e.target.value,a:'',b:''})}>{VALUE_TYPES.filter(([v])=>['eq','ne'].includes(n.operator)||v==='number').map(([v,label])=><option key={v} value={v}>{tr(label)}</option>)}</UtilitySelect></label></div>{input('a','A',n.a,n.valueType)}{input('b','B',n.b,n.valueType)}</>}
   {n.kind==='math'&&<><label>{tr("Операция")}<UtilitySelect aria-label={tr("Математическая операция ")+n.id} value={n.operator||'add'} onChange={e=>patch({operator:e.target.value})}>{Object.entries(MATH_OPERATIONS).map(([id,op])=><option key={id} value={id}>{tr(op.label)}</option>)}</UtilitySelect></label>{input('a','A',n.a??0,'number')}{MATH_OPERATIONS[n.operator||'add']?.arity===2&&input('b','B',n.b??0,'number')}</>}
   {n.kind==='convert'&&<><label>{tr("Преобразовать в")}<UtilitySelect aria-label={tr("Тип конвертации ")+n.id} value={n.valueType||'string'} onChange={e=>patch({valueType:e.target.value})}>{VALUE_TYPES.map(([id,label])=><option key={id} value={id}>{tr(label)}</option>)}</UtilitySelect></label>{input('value','Значение',n.value??'',n.inputType||'string')}<small className="utility-help">{tr("Да / нет: true / false, да / нет, 1 / 0. Число: корректная запись числа.")}</small></>}
   {n.kind==='not'&&input('a','Условие',n.a??false,'boolean')}
   {['and','or'].includes(n.kind)&&<>{input('a','A',n.a??false,'boolean')}{input('b','B',n.b??false,'boolean')}</>}
   {n.kind==='branch'&&<><div className="logic-row utility-execution"><Handle type="target" position={Position.Left} id="in" title={tr("Выполнить проверку")}/><Icon name="Play" size={14}/><span>{tr("Выполнить")}</span></div>{input('condition','Условие',n.condition??false,'boolean')}<p className="utility-help">{tr("Круглый вход принимает Да / нет. Без связи используется значение поля.")}</p></>}
  </div>
  {pure||n.kind==='set-variable'?output:<footer>{data.ports.map(port=><div className={'moment-route logic-if-route logic-if-'+port.id} key={port.id}><span><Icon name={port.id==='true'?'Check':'X'} size={16}/><strong>{tr(port.label)}</strong></span><small>{port.id==='true'?tr("Условие выполнено"):tr("Условие не выполнено")}</small><Handle type="source" position={Position.Right} id={port.id} title={tr(port.label)+tr(" · ход истории")}/></div>)}</footer>}
  {issue}
 </article>;
}
