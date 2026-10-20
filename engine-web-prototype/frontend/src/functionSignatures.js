// Named ports are identities inside a function signature. Rename explicit
// references only; literal user structures and strings are never searched/replaced.
const valid=name=>typeof name==='string'&&/^[^ .]+$/.test(name)&&!['__proto__','constructor','prototype'].includes(name);
const walk=(value,visit)=>{if(!value||typeof value!=='object')return;visit(value);for(const [key,child]of Object.entries(value))if(!['locals','default','initialState','schema','localSchema','stateSchema','resultSchema'].includes(key)&&!(key==='value'&&value.op==='constant')&&!(key==='value'&&!child?.op&&(value.kind==='return'||value.kind==='set')))walk(child,visit);};
export function renameFunctionParameter(project,functionId,from,to){
 const fn=project.functions.find(f=>f.id===functionId);if(!fn||!fn.parameters.some(p=>p.name===from))throw Error('Параметр функции удалён.');
 if(from===to)return;
 if(!valid(to)||fn.parameters.some(p=>p.name===to))throw Error('Параметру нужно непустое уникальное имя без точек и пробелов.');
 const renameArgs=args=>{if(args&&Object.hasOwn(args,from)){args[to]=args[from];delete args[from];}};
 const rewriteSchema=s=>{if(!s||typeof s!=='object')return;if(['variableValue','signalPayload'].includes(s.type)&&s.parameter===from)s.parameter=to;for(const child of Object.values(s))if(child&&typeof child==='object')rewriteSchema(child);};
 const rewriteScope=f=>{
  for(const p of f.parameters||[]){if(p.name===from)p.name=to;rewriteSchema(p.schema);}rewriteSchema(f.localSchema);rewriteSchema(f.stateSchema);rewriteSchema(f.resultSchema);
  walk(f.nodes,v=>{if(v.op==='get'&&(v.path==='args.'+from||v.path?.startsWith('args.'+from+'.')))v.path='args.'+to+v.path.slice(('args.'+from).length);});
 };
 rewriteScope(fn);
 for(const root of [project.functions,project.chapters,project.events,project.gameplay?.rules,project.gameplay?.mechanicDefinitions,project.widgets,project.input])walk(root,v=>{
  if(v.functionId===functionId)renameArgs(v.args);
  if(v.kind==='script-function'&&v.functionId===functionId&&Object.hasOwn(v.inputs||{},'arg:'+from)){v.inputs['arg:'+to]=v.inputs['arg:'+from];delete v.inputs['arg:'+from];}
  if(v.conditionFunctionId===functionId)renameArgs(v.conditionArgs);
  if(v.command==='callFunction'&&v.value?.functionId===functionId)renameArgs(v.value.args);
 });
 for(const d of project.gameplay?.mechanicDefinitions||[])if(d.logic?.functionId===functionId&&d.parameters.properties[from]){
  d.parameters.properties[to]=d.parameters.properties[from];delete d.parameters.properties[from];rewriteSchema(d.parameters);
  if(d.exposed)d.exposed=d.exposed.map(name=>name===from?to:name);
  for(const m of project.gameplay.mechanics||[])if(m.definitionId===d.id&&m.definitionVersion===d.version)renameArgs(m.values);
  const handlers=new Set();for(const r of [...d.rules||[],...d.machine?.transitions||[]])if(r.handlerFunctionId)handlers.add(r.handlerFunctionId);
  for(const s of d.machine?.states||[])for(const phase of ['enter','exit'])if(s[phase+'FunctionId'])handlers.add(s[phase+'FunctionId']);
  for(const id of [...handlers]){const h=project.functions.find(f=>f.id===id);if(h?.admission?.functionId)handlers.add(h.admission.functionId);}
  for(const id of handlers){const h=project.functions.find(f=>f.id===id);if(h&&h.id!==functionId)rewriteScope(h);}
  walk(d,v=>{if(v.op==='get'&&(v.path==='args.'+from||v.path?.startsWith('args.'+from+'.')))v.path='args.'+to+v.path.slice(('args.'+from).length);});
 }
 return fn;
}
