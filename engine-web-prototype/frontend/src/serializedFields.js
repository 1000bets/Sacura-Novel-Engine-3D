import {parse} from '@babel/parser';
import {assertSchema,schemaFromValue,schemaDefaults,validateValue} from './valueSchemas.js';

const unsafe=new Set(['__proto__','prototype','constructor']);
const fail=(message,node)=>{throw Object.assign(Error('SerializeField: '+message),{line:node?.loc?.start.line||1,column:(node?.loc?.start.column||0)+1});};
function literal(node){
 if(!node)fail('укажите значение по умолчанию.');
 if(['StringLiteral','NumericLiteral','BooleanLiteral','NullLiteral'].includes(node.type))return node.value??null;
 if(node.type==='Identifier'&&node.name==='undefined')return undefined;
 if(node.type==='UnaryExpression'&&node.operator==='-'&&node.argument.type==='NumericLiteral')return -node.argument.value;
 if(node.type==='ArrayExpression')return node.elements.map(literal);
 if(node.type==='ObjectExpression'){
  const value={};for(const p of node.properties){const key=p.key?.name??p.key?.value;if(p.type!=='ObjectProperty'||p.computed||typeof key!=='string'||unsafe.has(key)||Object.hasOwn(value,key))fail('используйте уникальные обычные имена полей.',p);value[key]=literal(p.value);}return value;
 }
 fail('default и схема должны быть литералами; вычисления размещайте в main().',node);
}
const walk=(node,visit)=>{if(!node||typeof node!=='object')return;visit(node);for(const [key,v]of Object.entries(node))if(!['loc','start','end','extra','comments','tokens'].includes(key)){if(Array.isArray(v))v.forEach(x=>walk(x,visit));else if(v&&typeof v==='object')walk(v,visit);}};
export function fieldsFromAst(ast){
 const fields=[],calls=new Set();
 for(const statement of ast.program.body){
  if(statement.type!=='VariableDeclaration')continue;
  for(const d of statement.declarations){const call=d.init;
   if(call?.type!=='CallExpression'||call.callee.type!=='Identifier'||call.callee.name!=='SerializeField')continue;
   if(d.id.type!=='Identifier'||unsafe.has(d.id.name))fail('объявите обычную переменную: const speed = SerializeField(5).',d);
   if(call.arguments.length<1||call.arguments.length>2)fail('ожидается SerializeField(default, schema?).',call);
   const value=literal(call.arguments[0]),options=call.arguments[1]?literal(call.arguments[1]):{};
   if(!options||typeof options!=='object'||Array.isArray(options)||Object.hasOwn(options,'default'))fail('второй аргумент — схема без default; default задаётся первым аргументом.',call);
   if((value===undefined||value===null)&&!options.type)fail('для undefined/null укажите type во втором аргументе.',call);
   const schema={...(value===undefined||value===null?{}:schemaFromValue(value)),...options,...(value===undefined?{}:{default:value})};
   try{assertSchema(schema);if(['void','variableValue','signalPayload','componentArguments','componentResult'].includes(schema.type))throw Error('выберите обычный тип параметра.');if(value!==undefined){const errors=validateValue(schema,value,{checkReferences:false,allowDeferred:true});if(errors.length)throw Error(errors.map(e=>e.message).join(' '));}}
   catch(error){fail(error.message,call);}
   const portable=x=>{if(!x||typeof x!=='object')return;if(x.kind==='reference'&&['direct','internal'].includes(x.mode||'direct')||x.type==='reference'&&Object.hasOwn(x,'componentDefinitionId'))fail('конкретные ссылки выбирайте у экземпляра в инспекторе; ID внутри исходника непереносимы.',call);Object.values(x).forEach(portable);};portable(schema);
   fields.push({name:d.id.name,label:schema.label||d.id.name,schema,...(value===undefined?{}:{default:value}),start:call.start,end:call.end});calls.add(call);
  }
 }
 const callees=new Set([...calls].map(call=>call.callee));
 walk(ast.program,node=>{if(node.type==='Identifier'&&node.name==='SerializeField'&&!callees.has(node))fail('SerializeField используется только как инициализатор переменной на верхнем уровне скрипта.',node);});
 return fields;
}
const cache=new Map();
export function serializedFields(source){
 if(!source?.includes('SerializeField'))return [];
 if(cache.has(source))return cache.get(source);
 let ast;try{ast=parse(source,{sourceType:'script'});}catch(error){throw Object.assign(Error(error.message),{line:error.loc?.line,column:(error.loc?.column??0)+1});}
 const fields=fieldsFromAst(ast);if(cache.size>=64)cache.delete(cache.keys().next().value);cache.set(source,fields);return fields;
}
export const fieldParameters=source=>serializedFields(source).map(({name,label,schema,default:value})=>({name,label,schema:structuredClone(schema),...(value===undefined?{}:{default:structuredClone(value)})}));
export function lowerSerializedFields(source){
 for(const field of [...serializedFields(source)].reverse()){
  // __ctx is the same invocation context passed to main/condition/otherwise.
  // Preserve original line numbers, including multiline schema declarations.
  const original=source.slice(field.start,field.end),key=JSON.stringify(field.name),fallback=field.default===undefined?'undefined':JSON.stringify(field.default),read='(Object.prototype.hasOwnProperty.call(__ctx.args,'+key+')?__ctx.args['+key+']:'+fallback+')';
  source=source.slice(0,field.start)+read+original.replace(/[^\r\n]/g,' ')+source.slice(field.end);
 }
 return source;
}
const signature=value=>JSON.stringify(value,(key,v)=>v&&typeof v==='object'&&!Array.isArray(v)?Object.fromEntries(Object.keys(v).sort().map(k=>[k,v[k]])):v);
export function assertScriptFieldParameters(fn){
 if(!fn.mechanicScript?.compiledSource)return;
 for(const field of fieldParameters(fn.mechanicScript.compiledSource)){
  const parameter=fn.parameters.find(p=>p.name===field.name);
  if(!parameter||signature(parameter.schema)!==signature(field.schema)||signature(parameter.default===undefined?schemaDefaults(parameter.schema):parameter.default)!==signature(field.default)||parameter.label!==field.label)throw Error('Поле SerializeField «'+field.name+'» управляется исходником. Измените объявление в коде и скомпилируйте скрипт.');
 }
}

// Drafts may contain syntax errors; keep them portable without executing them.
export function usesSerializedFields(script){
 if(!script)return false;
 if(serializedFields(script.compiledSource).length)return true;
 try{return serializedFields(script.source).length>0;}catch{return /\bSerializeField\b/.test(script.source||'');}
}
