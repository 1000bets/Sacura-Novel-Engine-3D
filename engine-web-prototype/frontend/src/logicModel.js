import {runtimeVariableType} from './variableModel.js';
import {typedValue,availabilityOf,ANSWER_VARIABLE,variableName,choiceResultType} from './choiceModel.js';
export const MATH_OPERATIONS={add:{label:'Сложить · +',arity:2},sub:{label:'Вычесть · −',arity:2},mul:{label:'Умножить · ×',arity:2},div:{label:'Разделить · ÷',arity:2},mod:{label:'Остаток · %',arity:2},pow:{label:'Степень',arity:2},min:{label:'Минимум',arity:2},max:{label:'Максимум',arity:2},negate:{label:'Сменить знак',arity:1},abs:{label:'Модуль',arity:1},round:{label:'Округлить',arity:1},floor:{label:'Округлить вниз',arity:1},ceil:{label:'Округлить вверх',arity:1},sqrt:{label:'Квадратный корень',arity:1}};
export const PURE_KINDS=['variable','literal','compare','and','or','not','math','convert'];
export const isDataSource=node=>isPureNode(node)||node?.kind==='set-variable'||node?.kind==='choice'&&!!choiceResultType(node);
export const isPureNode=node=>PURE_KINDS.includes(node?.kind);
export const logicNodes=p=>(p.chapters||[]).flatMap(c=>c.beats||[]);
export const compareSymbols={eq:'==',ne:'!=',gt:'>',gte:'>=',lt:'<',lte:'<='};
export const logicType=(p,node)=>node?.kind==='choice'?choiceResultType(node):['variable','set-variable'].includes(node?.kind)?runtimeVariableType(p,node.variable)||node.valueType||'string':['compare','and','or','not'].includes(node?.kind)?'boolean':node?.kind==='math'?'number':node?.valueType||'string';
export function convertValue(value,type){
 if(type==='string')return String(value);
 if(type==='number'){const n=typeof value==='boolean'?Number(value):typeof value==='string'&&value.trim()===''?NaN:Number(value);if(!Number.isFinite(n))throw new Error('Не удалось преобразовать значение в число.');return n;}
 if(type==='boolean'){
  if(typeof value==='boolean')return value;
  if(typeof value==='number'){if(!Number.isFinite(value))throw new Error('Для конвертации нужно конечное число.');return value!==0;}
  const text=String(value).trim().toLowerCase();if(['true','да','yes','1'].includes(text))return true;if(['false','нет','no','0',''].includes(text))return false;
  throw new Error('Для Да / нет используйте true / false, да / нет или 1 / 0. Для другого текста добавьте сравнение.');
 }
 throw new Error('Выберите тип результата конвертации.');
}
export function readLogic(p,id,variables,visiting=new Set(),choiceResults={}){
 if(visiting.has(id))throw new Error('Цикл в проводах значений. Разорвите циклическую связь.');
 const node=logicNodes(p).find(n=>n.id===id);if(!isDataSource(node))throw new Error('Источник значения не найден. Переподключите провод.');
 const path=new Set(visiting).add(id),read=(port,fallback)=>node.inputs?.[port]?readLogic(p,node.inputs[port],variables,path,choiceResults):fallback;
 if(node.kind==='choice'){const result=choiceResults[id];if(!result||result.type==='none')throw new Error('Этот выбор ещё не вернул значение. Выполните выбор перед чтением результата.');return result.value;}
 if(node.kind==='variable'||node.kind==='set-variable'){if(!Object.hasOwn(variables,node.variable))throw new Error(`Переменная «${node.variable||'без имени'}» не задана.`);return variables[node.variable];}
 if(node.kind==='literal')return typedValue(node.value,node.valueType);
 if(node.kind==='convert')return convertValue(read('value',typedValue(node.value,node.inputType||'string')),node.valueType||'string');
 if(node.kind==='math'){
  const operation=MATH_OPERATIONS[node.operator];if(!operation)throw new Error('Неизвестная математическая операция.');
  const a=read('a',node.a??0),b=operation.arity===2?read('b',node.b??0):0;if(typeof a!=='number'||!Number.isFinite(a)||operation.arity===2&&(typeof b!=='number'||!Number.isFinite(b)))throw new Error('Математическая операция принимает числа.');
  if(['div','mod'].includes(node.operator)&&b===0)throw new Error('Деление на ноль.');
  const result=({add:()=>a+b,sub:()=>a-b,mul:()=>a*b,div:()=>a/b,mod:()=>a%b,pow:()=>a**b,min:()=>Math.min(a,b),max:()=>Math.max(a,b),negate:()=>-a,abs:()=>Math.abs(a),round:()=>Math.round(a),floor:()=>Math.floor(a),ceil:()=>Math.ceil(a),sqrt:()=>Math.sqrt(a)})[node.operator]();
  if(!Number.isFinite(result))throw new Error('Операция не вернула конечное число.');return result;
 }
 if(['and','or','not'].includes(node.kind)){const a=read('a',false),b=node.kind==='not'?false:read('b',false);if(typeof a!=='boolean'||node.kind!=='not'&&typeof b!=='boolean')throw new Error('Для логических операций нужны значения Да / нет.');return node.kind==='not'?!a:node.kind==='and'?a&&b:a||b;}
 const a=read('a',typedValue(node.a,node.valueType)),b=read('b',typedValue(node.b,node.valueType));
 if(typeof a!==typeof b)throw new Error('У сравнения разные типы значений.');
 if(node.operator==='eq')return a===b;if(node.operator==='ne')return a!==b;
 if(typeof a!=='number'||!Number.isFinite(a)||!Number.isFinite(b))throw new Error('Для >=, <=, > и < нужны числа.');
 return ({gte:()=>a>=b,lte:()=>a<=b,gt:()=>a>b,lt:()=>a<b})[node.operator]?.()??false;
}
export const choiceEnabled=(p,choice,variables,choiceResults={})=>choice.enabledSource?readLogic(p,choice.enabledSource,variables,new Set(),choiceResults)===true:null;
export function dataTargets(node){
 if(['compare','and','or'].includes(node.kind)||node.kind==='math'&&MATH_OPERATIONS[node.operator]?.arity===2)return ['a','b'];
 if(node.kind==='math'||node.kind==='not')return ['a'];
 if(node.kind==='branch')return ['condition'];
 if(['set-variable','convert'].includes(node.kind))return ['value'];
 if(node.kind==='choice')return (node.choices||[]).map(c=>'enabled:'+c.id);
 return [];
}
export function dataTargetType(p,node,port){
 if(node.kind==='convert')return 'any';
 if(node.kind==='math')return 'number';
 if(node.kind==='compare')return node.valueType||'number';
 if(node.kind==='set-variable')return logicType(p,node);
 return 'boolean';
}
export function getInput(node,port){return port.startsWith('enabled:')?node.choices?.find(c=>c.id===port.slice(8))?.enabledSource:node.inputs?.[port];}
export function setInput(node,port,source){if(port.startsWith('enabled:')){const c=node.choices.find(c=>c.id===port.slice(8));c.enabledSource=source||null;c.availability=null;c.condition='always';}else{node.inputs??={};if(source)node.inputs[port]=source;else delete node.inputs[port];}}
export function dataConnectionError(p,c){
 const nodes=logicNodes(p),source=nodes.find(n=>n.id===c.source),target=nodes.find(n=>n.id===c.target);
 if(!isDataSource(source)||c.sourceHandle!=='value')return 'Выберите круглый выход значения.';
 if(!c.target)return null;
 if(!target||!dataTargets(target).includes(c.targetHandle))return 'Значение подключается к круглому входу, а ход истории — к стрелке.';
 if(source.id===target.id)return 'Нельзя соединить ноду с собой.';
 const type=logicType(p,source);
 if(type==='any'&&target.kind!=='convert')return 'У ответов разные типы. Добавьте конвертацию результата выбора.';
 if(target.kind==='math'&&type!=='number')return 'Математическая операция принимает числа. Добавьте конвертацию в число.';
 if(target.kind==='set-variable'&&logicType(p,target)!==type)return 'Тип значения должен совпадать с типом переменной.';
 if(target.kind==='compare'){const other=getInput(target,c.targetHandle==='a'?'b':'a'),otherNode=nodes.find(n=>n.id===other);if(otherNode&&logicType(p,otherNode)!==type)return 'У A и B должны совпадать типы: число с числом, текст с текстом.';}
 if((c.targetHandle==='condition'||c.targetHandle.startsWith('enabled:')||['and','or','not'].includes(target.kind))&&type!=='boolean')return 'Для условия нужен результат Да / нет. Добавьте сравнение или конвертацию.';
 if(target.kind==='compare'&&['gt','gte','lt','lte'].includes(target.operator)&&type!=='number')return 'Этот оператор сравнивает числа.';
 const depends=(id,seen=new Set())=>{if(id===target.id)return true;if(seen.has(id))return false;seen.add(id);const node=nodes.find(n=>n.id===id);return node&&dataTargets(node).some(port=>{const next=getInput(node,port);return next&&depends(next,seen);});};
 if(depends(source.id))return 'Эта связь создаёт цикл значений.';
 return null;
}
// Old projects keep their conditions, now as visible wires and nodes instead of forms.
export function migrateLogicGraph(p){
 let count=0;const create=(chapter,kind,patch)=>{const id='logic-'+(++count)+'-'+Math.random().toString(36).slice(2,9);const n={id,kind,text:'',speaker:'Рассказчик',next:null,bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]},...patch};chapter.beats.push(n);return id;};
 for(const chapter of p.chapters||[])for(const beat of [...chapter.beats]){
  const fromCondition=condition=>{const values=(condition.rules||[]).map(rule=>{const variable=create(chapter,'variable',{variable:rule.variable||'',valueType:rule.type});return create(chapter,'compare',{operator:rule.operator||'eq',valueType:rule.type||'string',a:'',b:rule.value,inputs:{a:variable}});});if(!values.length)return create(chapter,'literal',{valueType:'boolean',value:false});return values.reduce((a,b)=>create(chapter,condition.mode==='any'?'or':'and',{inputs:{a,b}}));};
  if(beat.kind==='branch'&&beat.test&&!beat.inputs?.condition){beat.inputs={...beat.inputs,condition:fromCondition(beat.test)};delete beat.test;}
  for(const choice of beat.choices||[]){const condition=availabilityOf(choice);if(condition&&!choice.enabledSource){choice.enabledSource=fromCondition(condition);choice.condition='always';choice.availability=null;}}
 }
 return p;
}

// Palette entries reference the existing variable; adding a getter never changes its value.
export function variablePalette(project){
 return [...(logicNodes(project).some(n=>n.variable===ANSWER_VARIABLE)?[ANSWER_VARIABLE]:[]),...Object.keys(project.variables||{}).filter(id=>id!==ANSWER_VARIABLE)].flatMap(variable=>(variable===ANSWER_VARIABLE?['variable']:['variable','set-variable']).map(kind=>({
  id:kind+':'+variable,variable,kind,label:(kind==='variable'?'Получить · ':'Задать · ')+variableName(variable),searchText:variable+' '+variableName(variable)+' переменная '+(kind==='variable'?'get получить':'set задать'),
 })));
}
export function initializeVariableNode(project,node,variable){
 node.variable=variable;
 node.valueType=Object.hasOwn(project.variables,variable)||project.variableTypes?.[variable]?runtimeVariableType(project,variable):variable===ANSWER_VARIABLE?'string':node.valueType||'number';
 if(variable!==ANSWER_VARIABLE&&!Object.hasOwn(project.variables,variable))project.variables[variable]=typedValue('',node.valueType);
 if(node.kind==='set-variable')node.value=typedValue('',node.valueType);
}
