import test from 'node:test';
import assert from 'node:assert/strict';
import {componentCodeCalls} from '../src/componentCodeNavigation.js';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {createComponentResource,editComponentSource,publishComponentSource} from '../src/componentResources.js';
import {addMechanicInstance} from '../src/mechanicDescriptions.js';
import {reference} from '../src/valueSchemas.js';

function fixture(){
 const p=createMechanicsProject('Navigation'),a=createComponentResource(p,{name:'Caller'}),b=createComponentResource(p,{name:'Target'}),other=createComponentResource(p,{name:'Other'});
 for(const r of [a,b,other]){editComponentSource(p,r.id,`const component={name:${JSON.stringify(r.name)},methods:{same:{name:'Same',run(){}}}};`);publishComponentSource(p,r.id);}
 const caller=addMechanicInstance(p,a.definitionId,{kind:'project',id:p.id}),target=addMechanicInstance(p,b.definitionId,{kind:'project',id:p.id});caller.references.target=reference('mechanic',target.id);
 return {p,a:p.logicResources.find(r=>r.id===a.id),b,other};
}
test('code navigation resolves self and explicit instance references, never another type with the same method name',()=>{
 const {p,a,b}=fixture();a.source='throw Error("Navigation must not execute code");\nengine.invoke(self,"same",{});\nengine.invoke(refs.target,"same",{});';
 const calls=componentCodeCalls(p,a);assert.deepEqual(calls.map(c=>[c.definition.id,c.methodId,c.line]),[[a.definitionId,'same',2],[b.definitionId,'same',3]]);
 const d=p.gameplay.mechanicDefinitions.find(d=>d.id===b.definitionId),v2=structuredClone(d);v2.version=2;v2.methods.same.name='Other version';p.gameplay.mechanicDefinitions.push(v2);
 assert.equal(componentCodeCalls(p,a).at(-1).method.name,'Same');
 d.name='Renamed target';d.methods.same.name='Renamed method';assert.equal(componentCodeCalls(p,a).at(-1).method.name,'Renamed method');
 delete d.methods.same;const deleted=componentCodeCalls(p,a).at(-1);assert.equal(deleted.definition.id,b.definitionId);assert.equal(deleted.method,undefined,'deleted call must remain broken, not open Other.same or v2');
});
test('code navigation uses declared receiver types before an instance exists; incomplete source is safe to edit',()=>{
 const {p,a,b}=fixture(),d=p.gameplay.mechanicDefinitions.find(d=>d.id===a.definitionId);d.references.properties.target={type:'reference',referenceType:'mechanic',componentDefinitionId:b.definitionId,componentDefinitionVersion:1};p.gameplay.mechanics=[];
 a.source='engine.invoke(refs.target,"same",{});';assert.equal(componentCodeCalls(p,a).length,1);assert.equal(componentCodeCalls(p,a)[0].definition.id,b.definitionId);
 a.source='const component = { methods: (';assert.deepEqual(componentCodeCalls(p,a),[]);
});
