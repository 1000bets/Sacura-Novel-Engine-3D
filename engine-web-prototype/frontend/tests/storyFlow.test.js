import test from 'node:test';
import assert from 'node:assert/strict';
import {buildStoryFlow,arrangeStoryFlow} from '../src/storyFlowModel.js';
import {upgradeProject,allBeats,edgesFor,batchesFor,bindingActions,PHASES} from '../src/studioModel.js';

test('unified flow keeps every authored beat, route and event in its owning moment',()=>{
 const p=upgradeProject(),before=structuredClone(p),model=buildStoryFlow(p);
 assert.equal(model.nodes.length,allBeats(p).length);
 for(const beat of allBeats(p)){
  const node=model.nodes.find(n=>n.id===beat.id);
  assert.ok(node);
  for(const phase of PHASES){
   const actual=node.data.phases.find(ph=>ph.id===phase.id).batches;
   const expected=batchesFor(beat,phase.id);
   assert.deepEqual(actual.map(b=>[b.id,b.mode,b.events.map(e=>e.binding.id)]),expected.map(b=>[b.id,b.mode,b.bindings.map(e=>e.id)]));
   for(const batch of actual)for(const event of batch.events){
    assert.deepEqual(event.actions.map(a=>a.id),bindingActions(p,event.binding).map(a=>a.id));
   }
  }
  for(const edge of edgesFor(p,beat))assert.ok(model.edges.some(e=>e.source===beat.id&&e.target===edge.to&&e.sourceHandle===(edge.choiceId?'choice:'+edge.choiceId:'next')));
 }
 assert.deepEqual(p,before);
});

test('unified flow preserves cycles, separate answers to the same target, and missing destinations',()=>{
 const p={objects:[],events:[],subscenes:[{id:'a',entry:'choice'},{id:'b',entry:'return'}],chapters:[
  {id:'a',subsceneId:'a',beats:[{id:'choice',kind:'choice',choices:[{id:'yes',label:'yes',next:'return',condition:'trust',threshold:4},{id:'no',label:'no',next:'return'},{id:'broken',label:'broken',next:'gone'}]}]},
  {id:'b',subsceneId:'b',beats:[{id:'return',next:'choice'},{id:'end',kind:'end',next:'choice'}]}]};
 const m=buildStoryFlow(p);
 assert.equal(m.edges.filter(e=>e.source==='choice'&&e.target==='return').length,2);
 assert.ok(m.edges.find(e=>e.source==='return'&&e.target==='choice').data.crossScene);
 assert.ok(m.nodes.some(n=>n.id==='missing:gone'));
 assert.equal(m.edges.filter(e=>e.source==='end').length,0);
 assert.equal(m.nodes.find(n=>n.id==='choice').data.ports[0].condition,'Доверие ≥ 4');
 const layout=arrangeStoryFlow(m.nodes,m.edges);
 assert.ok(layout.every(n=>Number.isFinite(n.position.x)&&Number.isFinite(n.position.y)));
});

test('local action overrides stay with each event placement',()=>{
 const p=upgradeProject(),beat=allBeats(p).find(b=>b.bindings.length),binding=beat.bindings[0];
 const action=bindingActions(p,binding)[0];
 binding.actionOverrides={[action.id]:{duration:17}};
 const n=buildStoryFlow(p).nodes.find(n=>n.id===beat.id);
 const event=n.data.phases.flatMap(ph=>ph.batches).flatMap(b=>b.events).find(e=>e.binding.id===binding.id);
 assert.equal(event.actions.find(a=>a.id===action.id).duration,17);
});
