import {fieldsFromAst} from './serializedFields.js';
import {parse} from '@babel/parser';
import {operationDefinition} from './operationRegistry.js';
import {GAME_COMMANDS} from './gameplayModel.js';
import {referenceRecords} from './valueSchemas.js';

export const SCRIPT_SOURCE_LIMIT=200000;
export function parseMechanicScript(source){
 if(typeof source!=='string'||source.length>SCRIPT_SOURCE_LIMIT)throw Error('Скрипт слишком большой (максимум 200 000 символов).');
 let ast;try{ast=parse(source,{sourceType:'script'});}catch(e){throw Object.assign(Error(e.message.replace(/ \(\d+:\d+\)$/,'')),{line:e.loc?.line,column:(e.loc?.column??0)+1});}
 if(!ast.program.body.some(n=>n.type==='FunctionDeclaration'&&n.id?.name==='main'))throw Object.assign(Error('Добавьте функцию main(ctx) — это вход механики.'),{line:1,column:1});
 fieldsFromAst(ast);return ast;
}
export function compileMechanicScript(project,source,previous={}){
 const ast=parseMechanicScript(source),dependencies=structuredClone(previous),groups=['functions','events','references','globals','signals','operations'];if(dependencies.variables){dependencies.globals??=dependencies.variables;delete dependencies.variables;}for(const group of groups)dependencies[group]??={};
 const bind=(group,name,records,idKey)=>{if(dependencies[group][name])return;const found=records.filter(r=>r.name===name||r.id===name);if(found.length!==1)throw Error('Укажите уникальное имя зависимости: '+name);dependencies[group][name]={[idKey]:found[0].id};};
 const variable=name=>{if(dependencies.globals[name])return;if(!Object.hasOwn(project.variables,name))throw Error('Переменная не найдена: '+name);dependencies.globals[name]={path:'global.'+name};};
 const visit=node=>{if(!node||typeof node!=='object')return;if(node.type==='MemberExpression'&&(node.object?.name==='global'||node.object?.type==='MemberExpression'&&node.object.property?.name==='global')){const name=node.computed?node.property?.value:node.property?.name;if(typeof name==='string')variable(name);else for(const name of Object.keys(project.variables))variable(name);}
  if(node.type==='CallExpression'&&node.callee.type==='MemberExpression'&&node.callee.object?.name==='engine'){
   const method=node.callee.property?.name,name=node.arguments[0]?.value;
   if(method==='writeBinding')dependencies.operations['sacura.variable.set']={operationId:'sacura.variable.set',operationVersion:1};
   if(['operation','call','start','event','ref','signal','waitSignal'].includes(method)&&typeof name!=='string')throw Object.assign(Error('Укажите имя зависимости строкой в engine.'+method+'("Имя", …). Значения параметров могут вычисляться.'),{line:node.loc.start.line,column:node.loc.start.column+1});
   if(method==='operation'&&typeof name==='string')dependencies.operations[name]={operationId:name,operationVersion:node.arguments[2]?.value??1};
   if(method==='operation'&&typeof name==='string'&&!operationDefinition(name,node.arguments[2]?.value??1)&&!GAME_COMMANDS[name])throw Object.assign(Error('Операция не установлена: '+name),{line:node.loc.start.line,column:node.loc.start.column+1});
   if(typeof name==='string'&&['call','start'].includes(method))bind('functions',name,project.functions,'functionId');
   if(typeof name==='string'&&method==='event')bind('events',name,project.events,'eventId');
   if(typeof name==='string'&&['signal','waitSignal'].includes(method))dependencies.signals[name]??={signalId:name};
   if(typeof name==='string'&&method==='ref'&&!dependencies.references[name]){const records=['object','scene','project','resource','item','widget','mechanic','beat'].flatMap(type=>referenceRecords(project,type).filter(r=>r.name===name).map(r=>({type,id:r.id})));if(records.length!==1)throw Error('Объект зависимости не найден или имя неоднозначно: '+name);dependencies.references[name]={kind:'reference',type:records[0].type,mode:'direct',id:records[0].id};}
  }
  for(const [key,value]of Object.entries(node))if(!['loc','start','end','extra','comments','tokens'].includes(key)){if(Array.isArray(value))value.forEach(visit);else if(value&&typeof value==='object')visit(value);}
 };visit(ast.program);
 return {version:1,source,compiledSource:source,dependencies,approved:true,hasOtherwise:ast.program.body.some(n=>n.type==='FunctionDeclaration'&&n.id?.name==='otherwise')};
}
