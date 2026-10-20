import {parse} from '@babel/parser';
import {componentDefinitionForInstance} from './componentCatalog.js';

// Navigation only: inspect syntax, never evaluate gameplay while editing it.
export function componentCodeCalls(project,resource){
 let ast;try{ast=parse(resource.source,{sourceType:'script'});}catch{return [];}
 const definition=project.gameplay.mechanicDefinitions.find(d=>d.logicResourceId===resource.id),calls=[];
 const path=node=>node?.type==='Identifier'?node.name:node?.type==='MemberExpression'&&!node.computed?path(node.object)+'.'+node.property.name:'';
 const visit=node=>{
  if(!node||typeof node!=='object')return;
  if(node.type==='CallExpression'&&path(node.callee)==='engine.invoke'&&node.arguments[1]?.type==='StringLiteral'){
   const receiver=path(node.arguments[0]),methodId=node.arguments[1].value,targets=new Set();
   if(receiver==='self'&&definition)targets.add(definition.id+'@'+definition.version);
   if(receiver.startsWith('refs.')&&definition){
    const key=receiver.slice(5),schema=definition.references?.properties?.[key];
    if(schema?.componentDefinitionId)targets.add(schema.componentDefinitionId+'@'+(schema.componentDefinitionVersion||1));
    for(const m of project.gameplay.mechanics.filter(m=>m.definitionId===definition.id&&m.definitionVersion===definition.version)){
     const target=componentDefinitionForInstance(project,m.references?.[key]);if(target)targets.add(target.id+'@'+target.version);
    }
   }
   const matches=project.gameplay.mechanicDefinitions.filter(d=>d.componentVersion===1&&(targets.size?targets.has(d.id+'@'+d.version):Object.hasOwn(d.methods,methodId)));
   for(const d of matches)calls.push({receiver:receiver||'Получатель выражения',methodId,definition:d,method:d.methods[methodId],line:node.loc.start.line});
  }
  for(const value of Object.values(node))if(Array.isArray(value))value.forEach(visit);else if(value&&typeof value==='object')visit(value);
 };
 visit(ast);return calls;
}
