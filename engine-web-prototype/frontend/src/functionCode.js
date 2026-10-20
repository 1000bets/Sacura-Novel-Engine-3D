import {migrateFunctionGraph,nodeDescription,dataReferences} from './functionGraph.js';
import {operationDefinition} from './operationRegistry.js';
import {referenceRecords,schemaDefaults} from './valueSchemas.js';

// A read-only projection, never parsed, stored as source, or executed.
const identifier=value=>{const name=String(value||'value').replace(/[^\p{L}\p{M}\p{N}_$]+/gu,'_').replace(/^_+|_+$/g,'')||'value';return /^[\p{N}]/u.test(name)?'_'+name:name;};
const reserved=new Set('await break case catch class const continue debugger default delete do else enum export extends false finally for function if import in instanceof let new null return super switch this throw true try typeof var void while with yield arguments eval state project context ref operation event signal work equal unsupported undefined args local inventory parallel timeout wait behavior Math Boolean'.split(' '));
const key=value=>/^[$_\p{L}][$_\p{L}\p{N}\p{M}]*$/u.test(value)?value:JSON.stringify(value);
const literal=value=>value===undefined?'undefined':JSON.stringify(value);
const nextNodes=n=>n.kind==='branch'?[n.trueNext,n.falseNext]:n.kind==='return'?[]:[n.next,...(n.branches||[]),n.body].filter(Boolean);
export function codeType(schema={}){
 schema??={};
 if(schema.type==='reference')return ({object:'GameObject',scene:'Scene',project:'Project',resource:'Resource',event:'Event',function:'Function',item:'Item',widget:'Widget',signal:'Signal',variable:'Variable',mechanic:'Mechanic'})[schema.referenceType]||'Reference';
 if(schema.type==='union')return (schema.variants||[]).map(codeType).join(' | ')||'Value';
 if(schema.type==='collection')return 'Array<'+codeType(schema.items)+'>';
 if(schema.type==='object')return '{ '+Object.entries(schema.properties||{}).map(([name,s])=>key(s.label||name)+(s.optional?'?':'')+': '+codeType(s)).join('; ')+' }';
 return ({integer:'number',number:'number',string:'string',boolean:'boolean',void:'void',vector:'Vector',any:'Value',unknown:'Value',array:'Array<Value>',struct:'Record<string, Value>'})[schema.type]||'Value';
}
export function functionCode(project,source,{translate=text=>text}={}){
 const f=migrateFunctionGraph(source),lines=[],nodes=new Map((f.nodes||[]).map(n=>[n.id,n])),used=new Set(reserved),names=new Map(),results=new Map(),blocks=new Map(),emitted=new Set(),queued=[],pureReads=new Set();
 const unique=value=>{const base=identifier(value);let name=base,index=2;while(used.has(name))name=base+'_'+index++;used.add(name);return name;};
 const functionNames=new Map((project.functions||[]).map(fn=>[fn.id,unique(fn.name)])),functionName=functionNames.get(f.id)||unique(f.name);
 const parameterNames=fn=>{const taken=new Set([...reserved,...functionNames.values()]),map=new Map();for(const p of fn?.parameters||[]){const base=identifier(p.label||p.schema?.label||p.name);let name=base,i=2;while(taken.has(name))name=base+'_'+i++;taken.add(name);map.set(p.name,name);}return map;};
 for(const [key,name]of parameterNames(f)){names.set('args.'+key,name);used.add(name);}
 for(const name of new Set([...Object.keys(f.localSchema?.properties||{}),...Object.keys(f.locals||{})]))names.set('local.'+name,unique(f.localSchema?.properties?.[name]?.label||name));
 const path=value=>{const [root,name,...rest]=String(value||'').split('.');let base=names.get(root+'.'+name);if(!base){const label=root==='state'?f.stateSchema?.properties?.[name]?.label||name:name;base=({global:'project',state:'state',args:'args',local:'local'})[root]||root;if(name!==undefined)base+='['+literal(label)+']';}return base+rest.map(name=>'['+literal(name)+']').join('');};
 const title=n=>n.kind==='call'||n.kind==='start'?project.functions?.find(fn=>fn.id===n.functionId)?.name||translate('Недоступная функция'):n.kind==='event'?project.events?.find(e=>e.id===n.eventId)?.name||translate('Недоступный Event'):n.kind==='operation'?translate(operationDefinition(n.call?.id,n.call?.version)?.editor.label||n.call?.id):n.name||translate(nodeDescription(project,f,n).title);
 const result=n=>{if(!results.has(n.id))results.set(n.id,unique(title(n)+'_result'));return results.get(n.id);};
 const reference=value=>{if(value.mode==='context')return 'context.'+identifier(value.role||value.type);if(value.mode==='parameter'||value.mode==='external')return path('args.'+(value.parameter||value.name||value.id));let label;try{label=referenceRecords(project,value.type).find(r=>r.id===value.id)?.name;}catch{}return 'ref.'+identifier(value.type)+'('+literal(label||value.id||'')+')';};
 const expression=(value,seen=new Set(),depth=0)=>{
  if(depth>64)return 'unsupported("expression depth")';
  if(value===null||typeof value!=='object')return literal(value);
  const read=v=>expression(v,new Set(seen),depth+1);
  if(Array.isArray(value))return '['+value.map(read).join(', ')+']';
  if(value.kind==='reference'&&!value.op)return reference(value);
  if(!value.op)return '{ '+Object.entries(value).map(([name,v])=>key(name)+': '+read(v)).join(', ')+' }';
  const a=()=>read(value.a),b=()=>read(value.b),binary={add:'+',sub:'-',mul:'*',div:'/',lt:'<',lte:'<=',gt:'>',gte:'>='};
  if(value.op==='constant')return literal(value.value);
  if(value.op==='get')return path(value.path);
  if(value.op==='node'){const n=nodes.get(value.nodeId);if(!n)return 'unsupported('+literal('missing node: '+value.nodeId)+')';if(n.kind!=='value')return result(n);if(seen.has(n.id))return 'unsupported("data cycle")';seen.add(n.id);pureReads.add(n.id);return expression(n.value,seen,depth+1);}
  if(binary[value.op])return '('+a()+' '+binary[value.op]+' '+b()+')';
  if(value.op==='and'||value.op==='or')return '(Boolean('+a()+') '+(value.op==='and'?'&&':'||')+' Boolean('+b()+'))';
  if(value.op==='not')return '!Boolean('+a()+')';
  if(value.op==='eq'||value.op==='ne')return (value.op==='ne'?'!':'')+'equal('+a()+', '+b()+')';
  if(value.op==='min'||value.op==='max')return 'Math.'+value.op+'('+a()+', '+b()+')';
  if(value.op==='array')return '['+(value.values||[]).map(read).join(', ')+']';
  if(value.op==='struct')return '{ '+Object.entries(value.fields||{}).map(([name,v])=>key(name)+': '+read(v)).join(', ')+' }';
  if(value.op==='at')return '('+a()+')?.['+b()+']';
  if(value.op==='field')return '('+a()+')'+String(value.field||'').split('.').map(k=>'?.['+literal(k)+']').join('');
  if(value.op==='length')return '('+a()+'?.length ?? 0)';
  if(value.op==='itemCount')return 'inventory.count('+read(value.itemId)+(value.owner!==undefined?', '+read(value.owner):'')+')';
  return 'unsupported('+literal(value.op)+')';
 };
 const add=(text='',depth=0,nodeId)=>lines.push({text:'  '.repeat(depth)+text,nodeId});
 const comment=(text,depth,nodeId)=>String(text||'').split(/\r?\n|[\u2028\u2029]/).forEach(line=>add('// '+line,depth,nodeId));
 if(f.graphVersion!==1){comment(translate('Неподдерживаемая версия графа. Текстовое представление недоступно.'),0);return {lines,text:lines.map(l=>l.text).join('\n')};}
 // Joins and back edges become named blocks, so shared paths are never dropped
 // or expanded forever. Ordinary sequences and branches remain inline.
 const incoming=new Map([...nodes.keys()].map(id=>[id,0]));for(const n of nodes.values())for(const id of nextNodes(n))incoming.set(id,(incoming.get(id)||0)+1);
 const boundaries=new Set([...incoming].filter(([,count])=>count>1).map(([id])=>id)),visiting=new Set(),visited=new Set();
 for(const start of nodes.keys()){const stack=[[start,false]];while(stack.length){const [id,leaving]=stack.pop();if(leaving){visiting.delete(id);continue;}if(visiting.has(id)){boundaries.add(id);continue;}if(visited.has(id)||!nodes.has(id))continue;visited.add(id);visiting.add(id);stack.push([id,true]);for(const next of nextNodes(nodes.get(id)))stack.push([next,false]);}}
 const referenced=new Set([...dataReferences(f.nodes).map(ref=>ref.nodeId),...f.nodes.filter(n=>n.output).map(n=>n.id)]);for(const n of nodes.values())if(n.kind!=='value'&&referenced.has(n.id))result(n);
 const block=id=>{if(!blocks.has(id)){blocks.set(id,unique(title(nodes.get(id))+'_block'));queued.push(id);}return blocks.get(id);};
 const emitCall=(name,params,depth,node,prefix='await ',suffix=';',labels={})=>{const entries=Object.entries(params||{});if(!entries.length){add(prefix+name+'()'+suffix,depth,node.id);return;}add(prefix+name+'({',depth,node.id);for(const [k,v]of entries)add(key(labels[k]||k)+': '+expression(v)+',',depth+1,node.id);add('})'+suffix,depth,node.id);};
 const emitNode=(n,depth)=>{
  const needsResult=referenced.has(n.id),prefix=needsResult?result(n)+' = ':'';
  if(n.comment)comment(n.comment,depth,n.id);
  if(n.kind==='set')add(path(n.path)+' = '+expression(n.value)+';',depth,n.id);
  else if(n.kind==='array')add(n.operation==='push'?path(n.path)+'.push('+expression(n.value)+');':n.operation==='remove'?path(n.path)+'.splice('+expression(n.index)+', 1);':path(n.path)+'['+expression(n.index)+'] = '+expression(n.value)+';',depth,n.id);
  else if(n.kind==='call'||n.kind==='start'){const target=project.functions?.find(fn=>fn.id===n.functionId),labels=Object.fromEntries(parameterNames(target));emitCall(n.kind==='start'?'work.start.bind(null, '+literal(target?.name||n.functionId)+')':functionNames.get(n.functionId)||'unsupported',n.args,depth,n,prefix+(n.kind==='call'?'await ':''),';',labels);}
  else if(n.kind==='operation'||n.kind==='command'){const call=n.call||{id:n.command?.command,version:n.command?.operationVersion||1,parameters:n.command?.value},definition=operationDefinition(call.id,call.version),labels=Object.fromEntries(Object.entries(definition?.parameters?.properties||{}).map(([k,s])=>[k,s.label||k]));emitCall('operation['+literal(translate(definition?.editor.label||call.id))+']',call.parameters,depth,n,prefix+'await ',';',labels);}
  else if(n.kind==='event')emitCall('event['+literal(project.events?.find(e=>e.id===n.eventId)?.name||n.eventId)+']',{join:n.join||'FLOW_END',...(n.overrides?{overrides:n.overrides}:{}),...(n.actionOverrides?{actionOverrides:n.actionOverrides}:{}),...(n.targetBindings?{targetBindings:n.targetBindings}:{})},depth,n,prefix+'await ');
  else if(n.kind==='return')add('return '+expression(n.value)+';',depth,n.id);
  else if(n.kind==='wait')add('await wait('+expression(n.duration)+', '+literal(n.timeSource||f.timeSource||'game')+');',depth,n.id);
  else if(n.kind==='signal')add(prefix+'await signal.emit('+literal(n.signalId)+', '+expression(n.payload||{})+');',depth,n.id);
  else if(n.kind==='waitSignal')add(prefix+'await signal.wait('+literal(n.signalId)+', '+expression(n.duration)+');',depth,n.id);
  else if(n.kind==='await'||n.kind==='cancel')add(prefix+(n.kind==='await'?'await ':'')+'work.'+(n.kind==='await'?'join':'cancel')+'('+expression(n.handle)+');',depth,n.id);
  else if(n.kind==='parallel'||n.kind==='timeout'){const targets=n.kind==='parallel'?(n.branches||[]):[n.body];add(prefix+'await '+(n.kind==='parallel'?'parallel([':'timeout('+expression(n.duration)+', ')+targets.map(id=>nodes.has(id)?block(id):'unsupported').join(', ')+(n.kind==='parallel'?']);':');'),depth,n.id);}
  else if(n.kind==='btSequence'||n.kind==='btSelector')add(prefix+'await behavior.'+(n.kind==='btSequence'?'sequence':'selector')+'(['+(n.children||[]).map(c=>functionNames.get(typeof c==='string'?c:c.functionId)||'unsupported').join(', ')+'], '+expression(n.args||{})+');',depth,n.id);
  else add('unsupported('+literal(n.kind)+');',depth,n.id);
  if(n.kind==='set'&&needsResult)add(result(n)+' = '+path(n.path)+';',depth,n.id);
  if(n.kind==='array'&&needsResult)add(result(n)+' = '+path(n.path)+';',depth,n.id);
  if(n.output)add(path(n.output)+' = '+result(n)+';',depth,n.id);
 };
 const walk=(start,depth,ownBlock)=>{let id=start;const chain=new Set();while(id){const n=nodes.get(id);if(!n){add('unsupported('+literal('missing node: '+id)+');',depth);return;}if(boundaries.has(id)&&id!==ownBlock||chain.has(id)||depth>40&&id!==ownBlock){add('return await '+block(id)+'();',depth,id);return;}chain.add(id);emitted.add(id);if(n.kind==='value'){add('unsupported("data node in execution path");',depth,id);return;}if(n.kind==='branch'){if(n.comment)comment(n.comment,depth,id);add('if ('+expression(n.condition)+') {',depth,id);if(n.trueNext)walk(n.trueNext,depth+1);else add('return;',depth+1,id);add('} else {',depth,id);if(n.falseNext)walk(n.falseNext,depth+1);else add('return;',depth+1,id);add('}',depth,id);return;}emitNode(n,depth);if(n.kind==='return')return;id=n.next;}add('return;',depth);};
 if(f.handler){const phase=f.handler.phase,signal=f.handler.signal?.type||f.handler.signalId;comment(phase?translate(phase==='enter'?'При входе в состояние':'При выходе из состояния'):translate('Обработчик сигнала')+(signal?' · '+signal:''),0);}
 if(f.parameters?.length){add('async function '+functionName+'({');for(const p of f.parameters)add(names.get('args.'+p.name)+' = '+expression(p.default??schemaDefaults(p.schema||{}))+',',1);add('}: {');for(const p of f.parameters)add(names.get('args.'+p.name)+'?: '+codeType(p.schema||{type:p.type})+';',1);add('} = {}): Promise<'+codeType(f.resultSchema)+'> {');}else add('async function '+functionName+'(): Promise<'+codeType(f.resultSchema)+'> {');
 for(const [full,name]of names)if(full.startsWith('local.')){const k=full.slice(6),s=f.localSchema?.properties?.[k];add('let '+name+': '+codeType(s||{type:typeof f.locals?.[k]})+' = '+expression(Object.hasOwn(f.locals||{},k)?f.locals[k]:schemaDefaults(s||{}))+';',1);}
 if(Object.keys(f.stateSchema?.properties||{}).length)comment(translate('state — состояние экземпляра; project — переменные проекта.'),1);
 for(const name of results.values())add('let '+name+': Value;',1);
 if(names.size)add();
 if(nodes.has(f.entry)&&boundaries.has(f.entry))add('return await '+block(f.entry)+'();',1,f.entry);else if(f.entry)walk(f.entry,1);else comment(translate('Точка входа не подключена.'),1);
 const emitQueued=()=>{while(queued.length){const id=queued.shift();if(emitted.has('block:'+id))continue;emitted.add('block:'+id);add();add('async function '+blocks.get(id)+'() {',1,id);walk(id,2,id);add('}',1,id);}};emitQueued();
 for(const n of nodes.values())if(n.kind!=='value'&&!emitted.has(n.id)){add();comment(translate('Не подключено к основному потоку')+' · '+title(n),1,n.id);block(n.id);emitQueued();}
 for(const n of nodes.values())if(n.kind==='value'&&!pureReads.has(n.id)){add();comment(translate('Неиспользуемое вычисление')+' · '+title(n),1,n.id);add('const '+unique(title(n))+' = () => '+expression(n.value,new Set([n.id]))+';',1,n.id);}
 add('}');return {lines,text:lines.map(l=>l.text).join('\n')};
}
