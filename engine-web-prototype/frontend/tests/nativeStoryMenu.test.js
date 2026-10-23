import test from 'node:test';
import assert from 'node:assert/strict';
import {nativeStoryMenu} from '../src/nativeStoryMenu.js';
test('native graph menu sends graph coordinates and stable IDs through commands',async()=>{
 const story={document:{nodes:[{id:'line',kind:'Line',bindings:[]}],events:[]}},calls=[],selections=[];
 const items=nativeStoryMenu({story,context:{kind:'native-story',position:{x:123,y:456}},selection:{nodeId:'line'},command:async(m,p)=>{calls.push([m,p]);return {document:{nodes:[...story.document.nodes,{id:'cpp-choice',kind:'Choice'}]}};},onSelect:s=>selections.push(s)});
 await items.find(i=>i.label==='Выбор игрока').action();
 assert.deepEqual(calls,[['story.addNode',{kind:'Choice',x:123,y:456}]]);assert.deepEqual(selections,[{nodeId:'cpp-choice'}]);
 assert.ok(items.find(i=>i.label==='Скриптинг').disabled);assert.equal(story.document.nodes.length,1);
});
test('phase menu creates and reuses events in the chosen batch without editing snapshots',async()=>{
 const story={document:{nodes:[{id:'line',kind:'Line',bindings:[]}],events:[{id:'existing',name:'Existing event'}]}},calls=[],selected=[];
 const items=nativeStoryMenu({story,context:{kind:'native-event',nodeId:'line',phase:'AFTER',batchId:'batch'},command:async(m,p)=>{calls.push([m,p]);return {document:{nodes:[{id:'line',bindings:[{id:'cpp-binding',eventId:'existing'}]}]}};},onSelect:v=>selected.push(v)});
 await items.find(i=>i.label==='Переход камеры').action();await items.find(i=>i.label==='Existing event').action();
 assert.deepEqual(calls,[['story.addEvent',{nodeId:'line',phase:'AFTER',batchId:'batch',actionKind:'CameraCut'}],['story.addEvent',{nodeId:'line',phase:'AFTER',batchId:'batch',eventId:'existing'}]]);
 assert.equal(selected[0].bindingId,'cpp-binding');assert.equal(story.document.nodes[0].bindings.length,0);
 assert.ok(nativeStoryMenu({story,context:{kind:'native-event',nodeId:'deleted',phase:'AFTER'}}).every(i=>i.disabled));
});
