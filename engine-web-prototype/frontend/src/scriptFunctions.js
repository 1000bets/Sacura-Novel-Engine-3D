import {uid} from './model.js';
import {parseMechanicScript,compileMechanicScript} from './mechanicScriptCompiler.js';
import {assertMechanicScript} from './mechanicScriptModel.js';
import {runFunctionJavaScriptSync} from './mechanicScriptRuntime.js';
import {assertSerializable,schemaDefaults,validateValue} from './valueSchemas.js';

export const scriptFunctionType=fn=>({integer:'number',collection:'array',object:'struct'})[fn?.resultSchema?.type]||fn?.resultSchema?.type||'any';
export const scriptParameterType=parameter=>({integer:'number',collection:'array',object:'struct'})[parameter?.schema?.type||parameter?.type]||parameter?.schema?.type||parameter?.type||'any';
export function newScriptFunction(project,type='number'){
 const schema={type:['number','boolean','string'].includes(type)?type:'number'},initial=({number:0,boolean:false,string:''})[schema.type],id=uid('function');let name='Новая функция',index=2;
 while(project.functions.some(f=>f.name===name))name='Новая функция '+index++;
 return {id,name,graphVersion:1,parameters:[{name:'value',label:'Значение',schema,default:initial}],resultSchema:schema,locals:{},entry:id+'-return',nodes:[{id:id+'-return',kind:'return',value:initial}],functionScript:{version:1,source:'// Своя формула. Параметры доступны через args.\nfunction main({ args, global, engine }) {\n  return args.value;\n}\n',dependencies:{}}};
}
export function parseFunctionScript(source){
 let ast;try{ast=parseMechanicScript(source);}catch(error){if(error.message.includes('это вход механики'))error.message='Добавьте function main({ args }) и верните результат через return.';throw error;}const main=ast.program.body.find(n=>n.type==='FunctionDeclaration'&&n.id?.name==='main');
 const fail=(node,message)=>{throw Object.assign(Error(message),{line:node.loc.start.line,column:node.loc.start.column+1});};
 if(main.async||main.generator)fail(main,'Функция значения должна быть обычной function main, без async и генератора.');
 const visit=node=>{if(!node||typeof node!=='object')return;
  if(node.type==='CallExpression'&&node.callee.type==='MemberExpression'&&node.callee.object?.name==='engine'&&node.callee.property?.name!=='call')fail(node,'В функции значения доступен engine.call("Имя функции", args). Действия и ожидания выполняются в механиках.');
  if(node.type==='CallExpression'&&node.callee.name==='SerializeField')fail(node,'Параметры функции добавляются в настройках рядом с кодом.');
  for(const [key,value]of Object.entries(node))if(!['loc','start','end','extra','comments','tokens'].includes(key)){if(Array.isArray(value))value.forEach(visit);else if(value&&typeof value==='object')visit(value);}
 };visit(ast.program);return ast;
}
export function compileFunctionScript(project,source,previous={}){
 parseFunctionScript(source);const script=compileMechanicScript(project,source,previous);
 for(const [name,link]of Object.entries(script.dependencies.functions||{}))if(!project.functions.find(f=>f.id===link.functionId)?.functionScript)throw Error('Из функции значения можно вызвать другую скриптовую функцию: '+name);
 return script;
}
export function callScriptFunction(project,id,args,variables,{stack=[],deadline=Date.now()+100}={}){
 const fn=project.functions.find(f=>f.id===id),script=fn?.functionScript;if(!script)throw Error('Скриптовая функция удалена. Переподключите её вызов.');
 assertMechanicScript(script);
 if(!script.approved||!script.compiledSource)throw Error('Откройте функцию «'+fn.name+'» и скомпилируйте её перед запуском.');
 if(stack.includes(id)||stack.length>=32)throw Error('Цикл вызовов скриптовых функций: '+fn.name);
 const inputs={};for(const parameter of fn.parameters){const schema=parameter.schema||{type:parameter.type},value=args?.[parameter.name]??parameter.default??schemaDefaults(schema),issues=validateValue(schema,value,{project});if(issues.length)throw Error(fn.name+' · '+(parameter.label||parameter.name)+': '+issues.map(x=>x.message).join(' '));inputs[parameter.name]=structuredClone(value);}
 const global={};for(const [name,link]of Object.entries(script.dependencies.globals||{})){const key=link.path?.startsWith('global.')?link.path.slice(7):null;if(!key||!Object.hasOwn(variables,key))throw Error('Переменная функции не назначена: '+name);global[name]=structuredClone(variables[key]);}
 try{
  const result=runFunctionJavaScriptSync(script.compiledSource,{args:inputs,global},(name,args)=>{const target=script.dependencies.functions?.[name]?.functionId;if(!target)throw Error('Функция зависимости не назначена: '+name);return callScriptFunction(project,target,args,variables,{stack:[...stack,id],deadline});},{deadline});
  if(result!==undefined)assertSerializable(result);
  const issues=fn.resultSchema?validateValue(fn.resultSchema,result,{project}):[];if(issues.length)throw Error('Результат функции: '+issues.map(x=>x.message).join(' '));return result;
 }catch(error){error.functionId??=id;if(!error.message.startsWith(fn.name+' · '))error.message=fn.name+' · '+error.message;throw error;}
}
