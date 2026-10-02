import {t as tr,plural} from './i18n.js';
export const ANSWER_VARIABLE='$answer';
export const VALUE_TYPES=[['string','Текст'],['number','Число'],['boolean','Да / нет']];
export const OPERATORS=[['eq','равно'],['ne','не равно'],['gt','больше'],['gte','не меньше'],['lt','меньше'],['lte','не больше']];
export const variableName=id=>({trust:tr("Доверие"),letter:tr("Письмо найдено"),[ANSWER_VARIABLE]:tr("Последний ответ игрока")}[id]||id);
export function typedValue(value,type='string'){
 if(type==='array'||type==='struct'){let v=value===''||value===undefined?(type==='array'?[]:{}):value;try{if(typeof v==='string')v=JSON.parse(v);}catch{throw new Error('Проверьте JSON значения.');}if(type==='array'&&!Array.isArray(v)||type==='struct'&&(!v||typeof v!=='object'||Array.isArray(v)))throw new Error('Значение должно быть '+(type==='array'?'массивом.':'структурой.'));return structuredClone(v);}
 if(type==='boolean')return value===true||value==='true';
 if(type==='number'){const n=Number(value);return Number.isFinite(n)?n:0;}
 return String(value??'');
}
export function availabilityOf(choice){
 if(choice.availability)return choice.availability;
 if(!choice.condition||choice.condition==='always')return null;
 return {mode:'all',rules:[choice.condition==='trust'?{variable:'trust',operator:'gte',type:'number',value:choice.threshold??3}:{variable:choice.condition,operator:'eq',type:'boolean',value:true}]};
}
export function evaluateCondition(condition,variables={}){
 if(!condition)return true;
 const rules=condition.rules||[];if(!rules.length)return false;
 const evaluate=rule=>{
  if(!Object.hasOwn(variables,rule.variable))return false;
  const raw=variables[rule.variable],type=rule.type||typeof raw;
  if(!['string','number','boolean'].includes(type))return false;
  if(type==='number'&&(!Number.isFinite(Number(raw))||!Number.isFinite(Number(rule.value))))return false;
  const a=typedValue(raw,type),b=typedValue(rule.value,type);
  if(rule.operator==='eq')return a===b;if(rule.operator==='ne')return a!==b;
  if(type!=='number')return false;
  return ({gt:()=>a>b,gte:()=>a>=b,lt:()=>a<b,lte:()=>a<=b})[rule.operator]?.()||false;
 };
 return condition.mode==='any'?rules.some(evaluate):rules.every(evaluate);
}
export function choiceAvailable(choice,variables){return evaluateCondition(availabilityOf(choice),variables);}
export function choiceValue(choice){if(choice.result?.type==='none')return undefined;return typedValue(choice.result?.value??choice.label,choice.result?.type||'string');}
export function conditionSummary(condition){
 if(!condition)return tr("Доступен всегда");
 if(!condition.rules?.length)return tr("Настройте проверку");
 return condition.rules.map(r=>`${variableName(r.variable)||'Переменная'} ${({eq:'=',ne:'≠',gt:'>',gte:'≥',lt:'<',lte:'≤'})[r.operator]||'?'} ${r.type==='boolean'?(typedValue(r.value,'boolean')?tr("да"):tr("нет")):String(r.value??'')}`).join(condition.mode==='any'?tr(" или "):tr(" и "));
}
export function newChoice(id,label='Новый ответ'){return {id,label,condition:'always',availability:null,result:{type:'none'},next:null};}
export function choiceResultType(beat){
 const types=[...new Set((beat?.choices||[]).filter(c=>c.result?.type!=='none').map(c=>c.result?.type||'string'))];
 return types.length===1?types[0]:types.length?'any':null;
}
export function storyPorts(beat){
 if(beat.kind==='end'||['variable','literal','compare','and','or','not','math','convert'].includes(beat.kind))return [];
 if(beat.kind==='branch')return [{id:'true',label:'Да',next:beat.trueNext},{id:'false',label:'Нет',next:beat.falseNext}];
 if(beat.kind!=='choice')return [{id:'next',label:'Дальше',next:beat.next}];
 return [...(beat.choices||[]).map(c=>({id:'choice:'+c.id,label:c.label,condition:c.enabledSource?'Условие из графа':conditionSummary(availabilityOf(c)),result:choiceValue(c),resultType:c.result?.type==='none'?null:c.result?.type||'string',next:c.next})),{id:'next',label:'После выбора',next:beat.choiceMode==='value'||(beat.choices||[]).some(c=>!c.next)?beat.next:null,common:true}];
}
