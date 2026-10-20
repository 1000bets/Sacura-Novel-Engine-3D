import {parse} from '@babel/parser';
import {compileMechanicScript} from './mechanicScriptCompiler.js';
import {assertSchema,assertSerializable} from './valueSchemas.js';

export const sourceFingerprint=source=>{let hash=2166136261;for(const ch of source)hash=Math.imul(hash^ch.charCodeAt(0),16777619)>>>0;return source.length+':'+hash.toString(16);};
const fail=(message,node)=>{throw Object.assign(Error(message),{line:node?.loc?.start.line||1,column:(node?.loc?.start.column||0)+1});};
const keyOf=p=>p.computed?fail('В декларации используйте постоянное имя поля.',p):p.key?.name??p.key?.value;
function literal(n){
 if(!n) return undefined;
 if(['StringLiteral','NumericLiteral','BooleanLiteral','NullLiteral'].includes(n.type))return n.value??null;
 if(n.type==='UnaryExpression'&&n.operator==='-'&&n.argument.type==='NumericLiteral')return -n.argument.value;
 if(n.type==='ArrayExpression')return n.elements.map(literal);
 if(n.type==='ObjectExpression'){const out={};for(const p of n.properties){const key=keyOf(p);if(p.type!=='ObjectProperty'||['__proto__','prototype','constructor'].includes(key)||Object.hasOwn(out,key))fail('Метаданные должны содержать уникальные обычные поля.',p);out[key]=literal(p.value);}return out;}
 fail('Метаданные описываются литералами. Вычисления и функции размещайте в run().',n);
}
const properties=n=>{if(n?.type!=='ObjectExpression')fail('Ожидается объект декларации.',n);const out=new Map();for(const p of n.properties){if(p.type==='SpreadElement')fail('В декларации нельзя использовать spread.',p);const key=keyOf(p);if(typeof key!=='string'||!key.trim()||['__proto__','prototype','constructor'].includes(key)||out.has(key))fail('В декларации нужны уникальные обычные имена полей.',p);out.set(key,p);}return out;};
export function parseComponentSource(source){
 if(typeof source!=='string'||source.length>200000)throw Error('Исходник компонента: максимум 200 000 символов.');
 let ast;try{ast=parse(source,{sourceType:'script'});}catch(e){throw Object.assign(Error(e.message),{line:e.loc?.line,column:(e.loc?.column??0)+1});}
 const declarations=ast.program.body.filter(n=>n.type==='VariableDeclaration').flatMap(n=>n.declarations).filter(n=>n.id?.name==='component');
 if(declarations.length!==1)fail('Объявите один const component = { name, parameters, state, methods }.',ast.program);
 const fields=properties(declarations[0].init),read=(key,defaultValue)=>fields.has(key)?literal(fields.get(key).value):defaultValue;
 const result={name:read('name','Компонент'),parameters:read('parameters',{type:'object',properties:{}}),stateSchema:read('state',{type:'object',properties:{}}),references:read('references',{type:'object',properties:{}}),stateVersion:read('stateVersion',1),initialState:read('initialState',{}),stateFromParameters:read('stateFromParameters',{}),lifecycle:read('lifecycle',{}),subscriptions:read('subscriptions',[]),signals:read('signals',{}),methods:{},locations:{}};
 for(const s of [result.parameters,result.stateSchema,result.references]){assertSchema(s);if(s.type!=='object')fail('Параметры, state и references должны быть object schema.',declarations[0]);}
 for(const [id,p]of properties(fields.get('methods')?.value)){
  const fields=properties(p.value),run=fields.get('run');if(!run||!['ObjectMethod','FunctionExpression','ArrowFunctionExpression'].includes(run.type==='ObjectProperty'?run.value?.type:run.type))fail('Добавьте реализацию run(ctx) для метода '+id,p);
  const method={name:fields.has('name')?literal(fields.get('name').value):id,parameters:fields.has('parameters')?literal(fields.get('parameters').value):{type:'object',properties:{}},result:fields.has('result')?literal(fields.get('result').value):{type:'void'}};
  assertSchema(method.parameters);assertSchema(method.result);if(method.parameters.type!=='object')fail('Метод принимает именованные аргументы.',p);
  result.methods[id]=method;result.locations[id]={line:run.loc.start.line,column:run.loc.start.column+1};
 }
 // IDs inside source text cannot be rewritten during package import. Bind
 // concrete project identities in instance slots or engine.ref dependencies.
 const checkReferences=value=>{if(!value||typeof value!=='object')return;if(value.kind==='reference'&&['direct','internal'].includes(value.mode))fail('Прямые ID в метаданных непереносимы. Используйте ссылку экземпляра или engine.ref("Имя").',declarations[0]);Object.values(value).forEach(checkReferences);};
 checkReferences(result);assertSerializable(result);return result;
}
export const componentExecutableSource=(source,methodId)=>source+'\nfunction main(ctx) { return component.methods['+JSON.stringify(methodId)+'].run(ctx); }';
export function compileComponentSource(project,resource){
 const declaration=parseComponentSource(resource.source),methodId=Object.keys(declaration.methods)[0];
 // The normal compiler collects the same portable dependency table. No code is
 // evaluated to obtain metadata, including imports and top-level expressions.
 const script=compileMechanicScript(project,componentExecutableSource(resource.source,methodId||''),resource.dependencies||{});
 return {declaration,dependencies:script.dependencies,fingerprint:sourceFingerprint(resource.source)};
}
export function assertPublishedSource(resource){
 if(!resource||resource.version!==1||resource.kind!=='component.code')throw Error('Ресурс исходника компонента недоступен.');
 if(!resource.approved||resource.status!=='ready'||resource.fingerprint!==sourceFingerprint(resource.source))throw Object.assign(Error(resource.path+': '+(resource.error?.message||'Исходник изменён или импортирован. Откройте компонент и сохраните для запуска.')),{resourceId:resource.id,line:resource.error?.line,column:resource.error?.column});
}
