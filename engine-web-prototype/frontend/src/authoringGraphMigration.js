import {migrateFunctionGraph} from './functionGraph.js';
import {ensureHandlerGraphs} from './handlerGraphs.js';

export const AUTHORING_GRAPH_VERSION=1;
export function ensureAuthoringGraphs(project){
 if(project.authoringGraphVersion!==undefined&&project.authoringGraphVersion!==AUTHORING_GRAPH_VERSION)throw Error('Версия авторской модели недоступна. Исходный проект сохранён.');
 if(project.handlerGraphVersion!==undefined&&project.handlerGraphVersion!==1)throw Error('Версия обработчиков недоступна. Исходный проект сохранён.');
 for(const f of project.functions||[])if(f.graphVersion!==undefined&&f.graphVersion!==1)throw Error('Версия графа функции «'+f.name+'» недоступна. Исходный проект сохранён.');
 const stateful=new Map();
 const mark=(id,schema)=>{if(id)stateful.set(id,schema);};
 for(const d of project.gameplay?.mechanicDefinitions||[]){
  mark(d.logic?.functionId,d.stateSchema);
  for(const r of [...d.rules||[],...d.machine?.transitions||[]]){
   mark(r.conditionFunctionId,d.stateSchema);for(const a of [...r.actions||[],...r.otherwise||[]])if(a.kind==='function')mark(a.functionId,d.stateSchema);
  }
  for(const s of d.machine?.states||[])for(const a of [...s.enter||[],...s.exit||[]])if(a.kind==='function')mark(a.functionId,d.stateSchema);
 }
 const beforeFunctions=project.functions,beforeVersion=project.handlerGraphVersion;
 project.functions=(project.functions||[]).map(f=>{
  if(f.graphVersion===1)return f;
  const next=migrateFunctionGraph(f,{persistentState:stateful.has(f.id)});
  if(stateful.has(f.id)&&f.graphVersion===undefined){next.stateSchema=structuredClone(stateful.get(f.id));next.initialState=structuredClone(f.locals||{});}
  return next;
 });
 try{ensureHandlerGraphs(project);}catch(error){project.functions=beforeFunctions;if(beforeVersion===undefined)delete project.handlerGraphVersion;else project.handlerGraphVersion=beforeVersion;throw error;}
 project.authoringGraphVersion=AUTHORING_GRAPH_VERSION;return project;
}
