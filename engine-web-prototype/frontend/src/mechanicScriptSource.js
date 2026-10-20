import {migrateFunctionGraph} from './functionGraph.js';
import {schemaDefaults,referenceRecords} from './valueSchemas.js';
import {operationDefinition} from './operationRegistry.js';

const js=v=>v===undefined?'undefined':JSON.stringify(v);
const identifier=v=>String(v||'value').replace(/[^\p{L}\p{N}_$]/gu,'_').replace(/^[\p{N}]/u,'_$&');
// One-way conversion of existing mechanic programs. JavaScript becomes the authoring
// source on compilation; the legacy graph is retained for other editor surfaces.
export function mechanicScriptSource(project,source){
 const f=migrateFunctionGraph(source),nodes=new Map(f.nodes.map(n=>[n.id,n])),used=new Set(['main','condition','ctx','args','global','state','engine','local','cache']),aliases=new Map(),deps={functions:{},events:{},references:{},globals:{},signals:{}};
 const unique=v=>{const base=identifier(v);let name=base,i=2;while(used.has(name))name=base+'_'+i++;used.add(name);return name;};
 for(const n of f.nodes)aliases.set(n.id,unique(n.name||n.kind));
 const bind=(kind,id,type)=>{const records=kind==='functions'?project.functions:kind==='events'?project.events:referenceRecords(project,type),record=records.find(r=>r.id===id);let key=record?.name||id;const entries=deps[kind];if(entries[key]&&entries[key][kind==='functions'?'functionId':'eventId']!==id)key+=' · '+id;entries[key]=kind==='functions'?{functionId:id}:kind==='events'?{eventId:id}:{kind:'reference',type,mode:'direct',id};return js(key);};
 const path=value=>{const [root,name,...rest]=value.split('.');if(root==='global')deps.globals[name]={path:'global.'+name};return root+[name,...rest].filter(v=>v!==undefined).map(v=>'['+js(v)+']').join('');};
 const expr=(v,depth=0)=>{if(depth>64)throw Error('Выражение слишком глубоко.');const read=x=>expr(x,depth+1);if(v===null||typeof v!=='object')return js(v);if(Array.isArray(v))return '['+v.map(read).join(', ')+']';if(!v.op){if(v.kind==='reference'&&['direct','internal'].includes(v.mode||'direct'))return 'engine.ref('+bind('references',v.id,v.type)+')';return '{'+Object.entries(v).map(([k,x])=>js(k)+': '+read(x)).join(', ')+'}';}
  const a=()=>read(v.a),b=()=>read(v.b),binary={add:'+',sub:'-',mul:'*',div:'/',lt:'<',lte:'<=',gt:'>',gte:'>=',and:'&&',or:'||'};
  if(v.op==='constant')return js(v.value);if(v.op==='get')return path(v.path);if(v.op==='node'){const n=nodes.get(v.nodeId);if(!n)throw Error('Источник данных удалён.');return n.kind==='value'?read(n.value):'cache['+js(aliases.get(n.id))+']';}
  if(binary[v.op])return '('+a()+' '+binary[v.op]+' '+b()+')';if(v.op==='eq'||v.op==='ne')return '(JSON.stringify('+a()+') '+(v.op==='eq'?'===':'!==')+' JSON.stringify('+b()+'))';if(v.op==='not')return '!('+a()+')';if(v.op==='min'||v.op==='max')return 'Math.'+v.op+'('+a()+', '+b()+')';if(v.op==='array')return read(v.values||[]);if(v.op==='struct')return read(v.fields||{});if(v.op==='field')return '('+a()+')'+String(v.field).split('.').map(k=>'?.['+js(k)+']').join('');if(v.op==='at')return '('+a()+')?.['+b()+']';if(v.op==='length')return '('+a()+'?.length ?? 0)';if(v.op==='itemCount')return 'engine.count('+read(v.itemId)+', '+read(v.owner)+')';throw Error('Выражение недоступно в JavaScript: '+v.op);
 };
 const render=(entry,guard=false)=>{const lines=[],emitted=new Set(),queued=[],blocks=new Map(),incoming=new Map();for(const n of f.nodes)for(const id of [n.next,n.trueNext,n.falseNext,...n.branches||[],n.body].filter(Boolean))incoming.set(id,(incoming.get(id)||0)+1);
  const add=(s='',d=1)=>lines.push('  '.repeat(d)+s),block=id=>{if(!blocks.has(id)){blocks.set(id,unique(aliases.get(id)+'_block'));queued.push(id);}return blocks.get(id);};
  add('const { args, global, state, engine } = ctx;');add('const local = engine.local ?? '+js({...schemaDefaults(f.localSchema||{type:'object',properties:{}}),...f.locals})+';');add('const cache = engine.cache ?? {};');
  const referenced=new Set();const references=v=>{if(!v||typeof v!=='object')return;if(v.op==='node')referenced.add(v.nodeId);Object.values(v).forEach(references);};f.nodes.forEach(references);for(const n of f.nodes)if(n.output)referenced.add(n.id);
  const walk=(start,d,own)=>{let id=start;const chain=new Set();while(id){const n=nodes.get(id);if(!n)throw Error('Узел механики удалён.');if((incoming.get(id)>1&&id!==own)||chain.has(id)){add('return '+block(id)+'();',d);return;}chain.add(id);emitted.add(id);if(n.comment)for(const line of n.comment.split(/\r?\n/))add('// '+line,d);
    if(n.kind==='branch'){if(guard&&id===f.admission?.branchId){add('return { value: Boolean('+expr(n.condition)+'), local, nodeResults: cache };',d);return;}add('if ('+(id===f.admission?.branchId?'engine.admitted':expr(n.condition))+') {',d);if(n.trueNext)walk(n.trueNext,d+1);add('} else {',d);if(n.falseNext)walk(n.falseNext,d+1);add('}',d);return;}
    if(n.kind==='return'){add('return '+expr(n.value)+';',d);return;}
    let value;
    if(n.kind==='set')value='('+path(n.path)+' = '+expr(n.value)+')';
    else if(n.kind==='array'){const target=path(n.path);add(n.operation==='push'?target+'.push('+expr(n.value)+');':n.operation==='remove'?target+'.splice('+expr(n.index)+', 1);':target+'['+expr(n.index)+'] = '+expr(n.value)+';',d);value=target;}
    else if(n.kind==='call'||n.kind==='start')value='await engine.'+(n.kind==='call'?'call':'start')+'('+bind('functions',n.functionId)+', '+expr(n.args||{})+')';
    else if(n.kind==='operation'||n.kind==='command'){const c=n.call||{id:n.command?.command,version:n.command?.operationVersion??1,parameters:n.command?.value};add('// '+(operationDefinition(c.id,c.version)?.editor?.label||n.name||c.id),d);value='await engine.operation('+js(c.id)+', '+expr(c.parameters||{})+', '+(c.version??1)+')';}
    else if(n.kind==='event')value='await engine.event('+bind('events',n.eventId)+', '+expr({join:n.join||'FLOW_END',overrides:n.overrides||{},actionOverrides:n.actionOverrides||{},targetBindings:n.targetBindings||{}})+')';
    else if(n.kind==='wait')value='await engine.wait('+expr(n.duration)+', '+js(n.timeSource||'game')+')';
    else if(n.kind==='signal'||n.kind==='waitSignal'){deps.signals[n.signalId]={signalId:n.signalId};value='await engine.'+(n.kind==='signal'?'signal':'waitSignal')+'('+js(n.signalId)+', '+expr(n.kind==='signal'?n.payload||{}:n.duration)+')';}
    else if(n.kind==='await'||n.kind==='cancel')value='await engine.'+(n.kind==='await'?'join':'cancel')+'('+expr(n.handle)+')';
    else if(n.kind==='parallel')value='await Promise.all(['+(n.branches||[]).map(id=>block(id)+'()').join(', ')+'])';
    else if(n.kind==='timeout')value='await engine.timeout('+expr(n.duration)+', '+block(n.body)+'())';
    else if(n.kind==='btSequence'||n.kind==='btSelector')value='await engine.'+(n.kind==='btSequence'?'sequence':'selector')+'(['+(n.children||[]).map(id=>bind('functions',id)).join(', ')+'], '+expr(n.args||{})+')';
    else throw Error('Операция недоступна в JavaScript: '+n.kind);
    add((referenced.has(n.id)?'cache['+js(aliases.get(n.id))+'] = ':'')+value+';',d);if(n.output)add(path(n.output)+' = cache['+js(aliases.get(n.id))+'];',d);id=n.next;
   }add('return;',d);};
  if(entry)walk(entry,1);else add('return;');while(queued.length){const id=queued.shift();if(emitted.has('block:'+id))continue;emitted.add('block:'+id);add();add('async function '+blocks.get(id)+'() {');walk(id,2,id);add('}');}return lines.filter(line=>!line.includes('const cache =')||lines.some(x=>x!==line&&/\bcache\b/.test(x))).filter(line=>!line.includes('const local =')||lines.some(x=>x!==line&&/\blocal\b/.test(x))).join('\n');
 };
 const admission=nodes.get(f.admission?.branchId),main=render(admission?.trueNext||f.entry),condition=f.admission?render(f.entry,true):null,otherwise=admission?.falseNext&&nodes.get(admission.falseNext)?.kind!=='return'?render(admission.falseNext):null;
 return {source:'// '+String(f.name).replace(/[\r\n]/g,' ')+'\n'+(condition?'// Проверяется перед запуском реакции.\nasync function condition(ctx) {\n'+condition+'\n}\n\n':'')+'async function main(ctx) {\n'+main+'\n}\n'+(otherwise?'\n// Выполняется, если условие ложно.\nasync function otherwise(ctx) {\n'+otherwise+'\n}\n':''),dependencies:deps};
}
