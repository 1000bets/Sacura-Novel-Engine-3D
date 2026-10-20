import {fieldParameters} from './serializedFields.js';
import {syncFunctionDefinitions} from './authoredMechanics.js';

export function applyCompiledMechanicScript(project,functionId,script){
 const f=project.functions.find(f=>f.id===functionId);if(!f)throw Error('Скрипт удалён.');
 const previous=fieldParameters(f.mechanicScript?.compiledSource),fields=fieldParameters(script.compiledSource),managed=new Set([...previous,...fields].map(p=>p.name));
 if(managed.size){
  const declared=new Map(fields.map(p=>[p.name,p])),parameters=[];
  for(const p of f.parameters){if(!managed.has(p.name))parameters.push(p);else if(declared.has(p.name)){parameters.push(declared.get(p.name));declared.delete(p.name);}}
  parameters.push(...declared.values());f.parameters=parameters;
 }
 f.mechanicScript=script;syncFunctionDefinitions(project,f.id);
 for(const d of project.gameplay.mechanicDefinitions.filter(d=>d.logic?.functionId===f.id)){d.exposed=[...new Set([...(d.exposed||[]),...fields.map(p=>p.name)])];forwardInstanceFields(project,d,f.id,fields,managed);}
}

function forwardInstanceFields(project,definition,functionId,fields,managed){
 const owned=new Set(),visit=id=>{
  if(!id||id===functionId||owned.has(id))return;
  const f=project.functions.find(f=>f.id===id);if(!f)return;owned.add(id);visit(f.admission?.functionId);
  for(const n of f.nodes||[])if(n.kind==='call'&&project.functions.find(f=>f.id===n.functionId)?.handler)visit(n.functionId);
 };
 for(const r of [...definition.rules||[],...definition.machine?.transitions||[]])visit(r.handlerFunctionId);
 const targets=new Set([functionId,...owned]),names=new Set(fields.map(f=>f.name));
 const patch=data=>{if(!data||typeof data!=='object')return;
  if(['function','call','start'].includes(data.kind)&&targets.has(data.functionId)){
   data.args??={};for(const name of managed)if(!names.has(name)&&data.args[name]?.op==='get'&&data.args[name].path==='args.'+name)delete data.args[name];
   for(const name of names)if(!Object.hasOwn(data.args,name))data.args[name]={op:'get',path:'args.'+name};
  }
  Object.values(data).forEach(v=>{if(Array.isArray(v))v.forEach(patch);else if(v&&typeof v==='object')patch(v);});
 };
 patch(definition);for(const id of owned)patch(project.functions.find(f=>f.id===id)?.nodes);
}
