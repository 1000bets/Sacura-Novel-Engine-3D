import test from 'node:test';
import assert from 'node:assert/strict';
import {buildTimelineModel,cleanTimelinePositions,timelineNodeForBeat,timelineCondition,timelineVariableNodeId} from '../src/timelineModel.js';
import {upgradeProject,allBeats,edgesFor} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';

const dialogue=(id,next)=>({id,kind:'dialogue',text:id,next});
const ending=(id,title)=>({id,kind:'end',text:id,ending:title,next:null});
const choice=(id,choices)=>({id,kind:'choice',text:id,choices});
const fixture=(sceneBeats,variables={})=>({variables,subscenes:Object.entries(sceneBeats).map(([id,beats])=>({id,name:id,entry:beats[0]?.id})),chapters:Object.entries(sceneBeats).map(([id,beats])=>({id,subsceneId:id,beats}))});

test('timeline preserves every real route while compressing only linear intra-scene dialogue',()=>{
  const project=upgradeProject(),snapshot=structuredClone(project),model=buildTimelineModel(project);
  const beatIds=allBeats(project).map(beat=>beat.id);
  assert.deepEqual(new Set(model.nodes.flatMap(node=>node.beatIds)),new Set(beatIds));
  assert.equal(model.nodes.flatMap(node=>node.beatIds).length,beatIds.length);
  assert.ok(model.nodes.length<beatIds.length,'linear dialogue should be compressed');
  assert.ok(model.nodes.length>project.subscenes.length,'branches cannot collapse to three scene cards');
  for(const beat of allBeats(project))for(const edge of edgesFor(project,beat)){
    const source=model.beatToNode[edge.from],target=model.beatToNode[edge.to];
    if(source!==target)assert.ok(model.edges.some(item=>item.from===edge.from&&item.to===edge.to&&item.choiceId===edge.choiceId),`missing ${edge.from} -> ${edge.to}`);
  }
  for(const edge of model.edges)assert.ok(edgesFor(project,allBeats(project).find(beat=>beat.id===edge.from)).some(item=>item.to===edge.to&&item.choiceId===edge.choiceId));
  assert.deepEqual(project,snapshot,'building the timeline must not mutate the project');
});

test('parallel answers leading to distinct endings in the same subscene never imply a merged outcome',()=>{
  const project=fixture({home:[dialogue('intro','question'),choice('question',[
    {id:'stay',label:'Остаться',condition:'trust',threshold:5,next:'stay-line'},
    {id:'leave',label:'Уйти',condition:'always',next:'leave-line'},
  ]),dialogue('stay-line','end-stay'),ending('end-stay','Дома'),dialogue('leave-line','end-leave'),ending('end-leave','В дороге')]},{trust:3});
  const model=buildTimelineModel(project);
  assert.equal(model.counts.endings,2);
  const branches=model.edges.filter(edge=>edge.from==='question');
  assert.equal(branches.length,2);assert.notEqual(branches[0].target,branches[1].target);
  assert.equal(branches.find(edge=>edge.choiceId==='stay').conditionLabel,'Доверие ≥ 5');
  assert.equal(model.nodes.find(node=>node.entryBeatId==='question').choices[0].threshold,5);
  assert.deepEqual(model.variables[0].usedBy,[model.beatToNode.question]);
  assert.ok(model.nodes.filter(node=>node.kind==='ending').every(node=>!model.edges.some(edge=>edge.source===node.id)));
});

test('returning to the middle of a subscene keeps the actual entrance and a visible loop',()=>{
  const project=fixture({home:[dialogue('start','middle'),dialogue('middle','leave'),dialogue('leave','garden-entry')],garden:[dialogue('garden-entry','garden-choice'),choice('garden-choice',[
    {id:'return',label:'Вернуться',condition:'letter',next:'middle'},
    {id:'finish',label:'Остаться',condition:'always',next:'garden-end'},
  ]),ending('garden-end','В саду')]},{letter:true});
  const model=buildTimelineModel(project),back=model.edges.find(edge=>edge.choiceId==='return');
  assert.equal(back.to,'middle');assert.equal(back.target,model.beatToNode.middle);
  assert.notEqual(back.target,model.beatToNode.start);
  assert.equal(back.isReturn,true);assert.equal(back.crossScene,true);
  assert.equal(back.conditionLabel,'Письмо найдено = да');
  assert.equal(timelineNodeForBeat(model,'leave'),model.beatToNode.middle,'playtest marker follows the current compressed fragment');
});

test('empty scenes, unfinished branches and broken links are not fabricated game endings',()=>{
  const project=fixture({home:[choice('q',[{id:'unassigned',label:'Потом',condition:'always',next:null},{id:'deleted',label:'Туда',next:'deleted-id'}])],empty:[],draft:[dialogue('draft',null)]});
  const model=buildTimelineModel(project);
  assert.equal(model.counts.endings,0);
  assert.equal(model.nodes.filter(node=>node.kind==='missing').length,2);
  assert.ok(model.nodes.some(node=>node.sceneId==='empty'&&node.empty));
  assert.equal(model.nodes.find(node=>node.entryBeatId==='draft').openEnd,true);
  assert.equal(model.nodes.find(node=>node.entryBeatId==='draft').reachable,false);
  assert.equal(timelineNodeForBeat(model,'deleted-id'),null);
});

test('cycles and out-of-order chapter storage neither lose beats nor duplicate them',()=>{
  const project=fixture({home:[dialogue('a','b'),dialogue('c','d'),dialogue('b','c'),dialogue('d','b')],isolated:[dialogue('x','y'),dialogue('y','x')]});
  const model=buildTimelineModel(project);
  assert.deepEqual(new Set(model.nodes.flatMap(node=>node.beatIds)),new Set(['a','b','c','d','x','y']));
  assert.equal(model.nodes.flatMap(node=>node.beatIds).length,6);
  assert.equal(model.beatToNode.b,model.beatToNode.c);
  assert.equal(model.beatToNode.c,model.beatToNode.d);
  assert.ok(model.edges.some(edge=>edge.from==='d'&&edge.to==='b'&&edge.isReturn));
  assert.ok(model.edges.some(edge=>edge.from==='y'&&edge.to==='x'&&edge.isReturn));
  assert.equal(model.nodes.find(node=>node.id===model.beatToNode.x).reachable,false);
});

test('missing and custom global variables are represented without inventing thresholds or time durations',()=>{
  const project=fixture({home:[choice('q',[
    {id:'weather',label:'Пойти',condition:'stormCleared',next:'end'},
    {id:'trust',label:'Остаться',condition:'trust',next:'end'},
  ]),ending('end','Финал')]},{unrelated:'текст',trust:2});
  const model=buildTimelineModel(project),custom=model.variables.find(variable=>variable.id==='stormCleared');
  assert.equal(custom.missing,true);assert.equal(custom.value,undefined);
  assert.deepEqual(custom.usedBy,[model.beatToNode.q]);
  assert.equal(model.variables.find(variable=>variable.id==='unrelated').usedBy.length,0);
  assert.equal(timelineCondition({condition:'trust'}),'Доверие ≥ 3');
  assert.equal(timelineCondition({condition:'always'}),'Без условия');
  assert.ok(!('duration' in model));assert.ok(model.nodes.every(node=>!('duration' in node)));
});

test('the sample garden and station routes expose authored conditions and all three endings',()=>{
  const model=buildTimelineModel(upgradeProject());
  assert.equal(model.counts.scenes,3);assert.equal(model.counts.endings,3);
  assert.deepEqual(new Set(model.nodes.filter(node=>node.kind==='ending').map(node=>node.entryBeatId)),new Set(['d6','garden-end','station-end']));
  const back=model.edges.find(edge=>edge.from==='garden-choice'&&edge.choiceId==='go-home');
  assert.equal(back.to,'c1');assert.equal(back.threshold,2);assert.equal(back.conditionLabel,'Доверие ≥ 2');
  assert.ok(model.edges.some(edge=>edge.from==='station-choice'&&edge.choiceId==='return'&&edge.to==='c1'&&edge.isReturn));
});

test('manual timeline positions survive project save/load and preserve authored routes',()=>{
  const project=upgradeProject(),model=buildTimelineModel(project),storyBefore=structuredClone(project.chapters);
  const sceneId=model.beatToNode.a1,choiceId=model.beatToNode['garden-choice'],endId=model.beatToNode['station-end'];
  project.editor={theme:'dark',timelinePositions:{[sceneId]:{x:-242.5,y:0},[choiceId]:{x:3120.25,y:-146.75},[endId]:{x:0,y:840}}};
  const restored=readProject(JSON.stringify(project));
  assert.deepEqual(cleanTimelinePositions(restored),project.editor.timelinePositions);
  assert.equal(restored.editor.theme,'dark');
  assert.deepEqual(restored.chapters,storyBefore);
  assert.deepEqual(buildTimelineModel(restored).edges,model.edges,'moving cards must not rewrite narrative links');
});

test('every global variable has independent persisted coordinates even when its card is hidden',()=>{
  const project=fixture({home:[choice('q',[{id:'trust',condition:'trust',next:'end'},{id:'letter',condition:'letter',next:'end'}]),ending('end','Финал')]},{trust:2,letter:false});
  const trustId=timelineVariableNodeId('trust'),letterId=timelineVariableNodeId('letter');
  assert.notEqual(trustId,letterId);
  project.editor={timelinePositions:{[trustId]:{x:120,y:-90},[letterId]:{x:870,y:-40}}};
  const restored=JSON.parse(JSON.stringify(project));
  assert.deepEqual(cleanTimelinePositions(restored),project.editor.timelinePositions);
  restored.chapters[0].beats[0].choices=restored.chapters[0].beats[0].choices.filter(choice=>choice.condition!=='trust');
  assert.ok(cleanTimelinePositions(restored)[trustId],'unused project variables still exist and retain placement');
  delete restored.variables.trust;
  assert.ok(!Object.hasOwn(cleanTimelinePositions(restored),trustId),'deleted variables are removed from the saved layout');
  assert.deepEqual(cleanTimelinePositions(restored)[letterId],{x:870,y:-40});
});

test('stale and malformed layout entries are cleaned without mutating imported data',()=>{
  const project=fixture({home:[dialogue('start','question'),choice('question',[{id:'go',condition:'always',next:'end'}]),ending('end','Финал')]});
  const model=buildTimelineModel(project),start=model.beatToNode.start,question=model.beatToNode.question,end=model.beatToNode.end;
  const imported={[start]:{x:Infinity,y:30},[question]:{x:'10',y:20},[end]:{x:-40,y:0,selected:true},'scene:deleted':{x:10,y:20},'global-variable:deleted':{x:40,y:90}};
  const before=structuredClone(imported),cleaned=cleanTimelinePositions(project,imported);
  assert.deepEqual(cleaned,{[end]:{x:-40,y:0}});
  assert.deepEqual(imported,before);
  assert.deepEqual(cleanTimelinePositions(project,null),{});
  assert.deepEqual(cleanTimelinePositions(project,[]),{});
  project.chapters[0].beats=project.chapters[0].beats.filter(beat=>beat.id!=='end');
  assert.deepEqual(cleanTimelinePositions(project,cleaned),{},'removed ending coordinates cannot create a ghost card');
});
