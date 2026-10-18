export const JOURNAL_LIMIT=128;
// Bound both the ring and individual values. These are observed values, never
// re-evaluated expressions, and truncated collections are labelled explicitly.
export function journalValue(value,depth=0,budget={left:192}){
 if(--budget.left<0||depth>5)return '… сокращено';
 if(value===undefined)return 'не задано';
 if(typeof value==='string')return value.length>256?value.slice(0,256)+'…':value;
 if(typeof value==='number'||typeof value==='boolean'||value===null)return value;
 if(Array.isArray(value))return [...value.slice(0,16).map(v=>journalValue(v,depth+1,budget)),...(value.length>16?['… ещё '+(value.length-16)]:[])];
 if(typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([,v])=>v!==undefined).slice(0,16).map(([k,v])=>[k,journalValue(v,depth+1,budget)]));
 return 'неподдержанное значение';
}
export function lastRuleExecution(trace,mechanic){
 if(!mechanic)return null;const matches=e=>Boolean(e.ruleId&&mechanic.ruleId&&e.ruleId===mechanic.ruleId||e.instanceId&&e.instanceId===mechanic.id||e.ruleId?.startsWith(mechanic.id+':'));
 const last=[...(trace||[])].reverse().find(e=>e.seq!==undefined&&matches(e));if(!last)return null;
 return {seq:last.seq,entries:trace.filter(e=>e.seq===last.seq&&(matches(e)||e.kind==='publish'))};
}
