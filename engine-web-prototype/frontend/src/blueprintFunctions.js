import {uid} from './model.js';
const safe=k=>!['__proto__','constructor','prototype'].includes(String(k));
export function pathValue(object,path){return String(path||'').split('.').filter(Boolean).reduce((v,k)=>safe(k)?v?.[k]:undefined,object);}
export function writePath(object,path,value){const keys=String(path).split('.');if(keys.some(k=>!safe(k)||!k))throw Error('Недопустимое имя поля.');let target=object;for(const k of keys.slice(0,-1)){if(!target[k]||typeof target[k]!=='object')throw Error('Поле структуры не найдено: '+k);target=target[k];}target[keys.at(-1)]=structuredClone(value);}
// Expressions are data, never JavaScript; bounded evaluation also applies to imported graphs.
export function evaluateExpression(expression,scope,depth=0){
 if(depth>64)throw Error('Выражение слишком глубоко.');if(expression===null||typeof expression!=='object'||Array.isArray(expression))return structuredClone(expression);
 if(!expression.op)return Object.fromEntries(Object.entries(expression).filter(([k])=>safe(k)).map(([k,v])=>[k,evaluateExpression(v,scope,depth+1)]));
 const e=expression,read=x=>evaluateExpression(x,scope,depth+1),a=()=>read(e.a),b=()=>read(e.b);
 if(e.op==='get')return structuredClone(pathValue(scope,e.path));
 if(e.op==='array')return (e.values||[]).map(read);
 if(e.op==='struct')return Object.fromEntries(Object.entries(e.fields||{}).filter(([k])=>safe(k)).map(([k,v])=>[k,read(v)]));
 if(e.op==='field')return structuredClone(pathValue(a(),e.field));
 if(e.op==='at')return structuredClone(a()?.[b()]);if(e.op==='length')return a()?.length??0;
 if(e.op==='and')return Boolean(a())&&Boolean(b());if(e.op==='or')return Boolean(a())||Boolean(b());if(e.op==='not')return !a();
 if(e.op==='eq')return JSON.stringify(a())===JSON.stringify(b());if(e.op==='ne')return JSON.stringify(a())!==JSON.stringify(b());
 const x=a(),y=b();if(!Number.isFinite(x)||!Number.isFinite(y))throw Error('Арифметика принимает конечные числа.');
 const ops={add:()=>x+y,sub:()=>x-y,mul:()=>x*y,div:()=>x/y,min:()=>Math.min(x,y),max:()=>Math.max(x,y),lt:()=>x<y,lte:()=>x<=y,gt:()=>x>y,gte:()=>x>=y};
 if(!ops[e.op]||e.op==='div'&&y===0)throw Error('Недопустимая арифметическая операция.');const result=ops[e.op]();if(typeof result==='number'&&!Number.isFinite(result))throw Error('Результат вне диапазона.');return result;
}
export function newBlueprintFunction(){const id=uid('function');return {id,name:'Новая функция',parameters:[{name:'amount',type:'number',default:25}],locals:{result:0},entry:id+'-set',nodes:[{id:id+'-set',kind:'set',name:'Вычислить',path:'local.result',value:{op:'get',path:'args.amount'},next:id+'-return'},{id:id+'-return',kind:'return',name:'Вернуть результат',value:{op:'get',path:'local.result'}}]};}
export async function callBlueprint(p,id,args,variables,command,{depth=0,budget={remaining:1000}}={}){
 if(depth>16)throw Error('Слишком много вложенных вызовов функции.');const f=p.functions?.find(f=>f.id===id);if(!f)throw Error('Функция не найдена.');
 for(const param of f.parameters){const value=args?.[param.name]??param.default;const type=Array.isArray(value)?'array':value&&typeof value==='object'?'struct':typeof value;if(type!==param.type)throw Error('Тип параметра '+param.name+' должен быть '+param.type+'.');}
 const scope={global:variables,args:Object.fromEntries(f.parameters.map(a=>[a.name,structuredClone(args?.[a.name]??a.default)])),local:structuredClone(f.locals||{})};
 let next=f.entry;
 while(next){if(--budget.remaining<0)throw Error('Функция превысила лимит шагов. Проверьте цикл.');const n=f.nodes.find(n=>n.id===next);if(!n)throw Error('Нода функции удалена.');const read=e=>evaluateExpression(e,scope);
 if(n.kind==='set')writePath(scope,n.path,read(n.value));
 else if(n.kind==='branch'){next=read(n.condition)?n.trueNext:n.falseNext;continue;}
 else if(n.kind==='array'){const list=pathValue(scope,n.path);if(!Array.isArray(list))throw Error('Выберите массив.');if(n.operation==='push')list.push(read(n.value));else if(n.operation==='remove'){const index=Number(read(n.index));if(!Number.isInteger(index)||index<0||index>=list.length)throw Error('Индекс вне массива.');list.splice(index,1);}else if(n.operation==='set'){const index=Number(read(n.index));if(!Number.isInteger(index)||index<0||index>=list.length)throw Error('Индекс вне массива.');list[index]=read(n.value);}else throw Error('Неизвестная операция массива.');}
 else if(n.kind==='call'){const result=await callBlueprint(p,n.functionId,read(n.args)||{},variables,command,{depth:depth+1,budget});if(n.output)writePath(scope,n.output,result);}
 else if(n.kind==='command')await command({...n.command,value:read(n.command?.value)});
 else if(n.kind==='return')return read(n.value);
 else throw Error('Неизвестная нода функции.');next=n.next;
 }
 return undefined;
}
export function validateFunctions(p){
 const issues=[],bad=(id,detail)=>issues.push({id:'function-'+id,level:'error',title:'Проверьте функцию Blueprint',detail});
 if(!Array.isArray(p.functions||[])){bad('shape','Повреждена библиотека функций.');return issues;}
 const functionIds=new Set();
 for(const f of p.functions||[]){
  if(!f?.id||!Array.isArray(f.parameters)||!Array.isArray(f.nodes)||f.nodes.some(n=>!n||typeof n!=='object')||!f.nodes.some(n=>n.id===f.entry)){bad(f?.id,'Функции нужны параметры, ноды и точка входа.');continue;}
  if(functionIds.has(f.id))bad(f.id,'ID функций повторяются.');functionIds.add(f.id);
  const params=new Set();for(const a of f.parameters){if(!a||typeof a.name!=='string'||!a.name||!safe(a.name)||params.has(a.name)||!['number','boolean','string','array','struct'].includes(a.type))bad(f.id,'Проверьте имена и типы параметров.');params.add(a?.name);}
  const ids=new Set(f.nodes.map(n=>n.id));if(ids.size!==f.nodes.length||f.nodes.some(n=>typeof n.id!=='string'||!n.id))bad(f.id,'ID нод повторяются или отсутствуют.');
  for(const n of f.nodes){if(!['set','branch','array','call','command','return'].includes(n.kind))bad(n.id,'Неизвестный тип ноды.');for(const target of [n.next,n.trueNext,n.falseNext].filter(Boolean))if(!ids.has(target))bad(n.id,'Связь ведёт в удалённую ноду.');if(n.kind==='call'&&!p.functions.some(x=>x.id===n.functionId))bad(n.id,'Вызванная функция удалена.');if(['set','array'].includes(n.kind)&&(!/^(global|local)\.[^.]+/.test(n.path)||n.path.split('.').some(k=>!safe(k))))bad(n.id,'Укажите путь global.имя или local.имя.');}
 }
 return issues;
}
