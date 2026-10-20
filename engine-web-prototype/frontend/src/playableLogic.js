// Strip unused component implementation libraries from a build copy. References
// are native IDs (including dependency tables), never strings parsed from JS.
export function playableLogicProject(project){
 const p=structuredClone(project);delete p.gameplayMigration;delete p.world;
 const sources=p.logicResources||[],definitions=p.gameplay.mechanicDefinitions||[],functions=p.functions||[],index=new Map(),used=new Set();for(const r of [...sources,...definitions,...functions])index.set(r.id,[...(index.get(r.id)||[]),r]);
 const visit=v=>{if(typeof v==='string'){const records=index.get(v);if(records&&!used.has(v)){used.add(v);for(const record of records)scan(record);}}else if(v&&typeof v==='object')scan(v);};
 const scan=v=>{for(const [key,child]of Object.entries(v)){if(key==='source'||key==='compiledSource'||key==='packageSource')continue;visit(child);}};
 const {logicResources:omitSources,functions:omitFunctions,gameplay,...root}=p;scan(root);scan(Object.fromEntries(Object.entries(gameplay).filter(([key])=>key!=='mechanicDefinitions')));
 // Non-component public functions can be selected dynamically by ordinary
 // Events. Preserve them conservatively; source methods are rooted by types.
 for(const f of functions)if(!f.logicResourceId&&!f.componentDefinitionId)visit(f.id);
 p.logicResources=sources.filter(r=>used.has(r.id));p.gameplay.mechanicDefinitions=definitions.filter(d=>used.has(d.id));p.functions=functions.filter(f=>used.has(f.id));
 return p;
}
