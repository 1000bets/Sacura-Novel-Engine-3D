import test from 'node:test';
import assert from 'node:assert/strict';
import {copySceneObjects,pasteSceneObjects,cloneStoryNodes,clipboardCommand} from '../src/editorClipboard.js';

test('objects paste into another scene with independent transforms and light settings',()=>{
 const source={id:'lamp',name:'Свет',type:'Источник света',light:{intensity:35},transforms:{a:{position:[1,2,3],rotation:[0,0,0],scale:[1,1,1]}}};
 const snapshot=copySceneObjects([source],{id:'a',kind:'living'});
 source.light.intensity=99;
 const [first]=pasteSceneObjects(snapshot,{id:'b'},1),[second]=pasteSceneObjects(snapshot,{id:'b'},2);
 assert.notEqual(first.id,source.id);assert.notEqual(first.id,second.id);
 assert.equal(first.subsceneId,'b');assert.deepEqual(Object.keys(first.transforms),['b']);
 assert.deepEqual(first.transforms.b.position,[1.65,2,3]);assert.deepEqual(second.transforms.b.position,[2.3,2,3]);
 assert.equal(first.light.intensity,35);first.light.intensity=1;assert.equal(second.light.intensity,35);
 assert.deepEqual(snapshot[0].clipboardTransform.position,[1,2,3]);
});

test('story paste remaps internal routes and nested IDs without linking to original nodes',()=>{
 const snapshot=[{id:'one',kind:'choice',choices:[{id:'answer',next:'two'},{id:'external',next:'elsewhere'}],bindings:[{id:'bind',eventId:'shared',overrides:{value:2}}],batches:{ON_START:[{id:'batch',bindingIds:['bind']}]}},{id:'two',kind:'dialogue',next:'one',bindings:[]}];
 const [one,two]=cloneStoryNodes(snapshot),again=cloneStoryNodes(snapshot);
 assert.equal(one.choices[0].next,two.id);assert.equal(two.next,one.id);assert.equal(one.choices[1].next,null);
 assert.notEqual(one.choices[0].id,'answer');assert.notEqual(one.bindings[0].id,'bind');assert.notEqual(one.batches.ON_START[0].id,'batch');
 assert.deepEqual(one.batches.ON_START[0].bindingIds,[one.bindings[0].id]);assert.equal(one.bindings[0].eventId,'shared');
 one.bindings[0].overrides.value=8;assert.equal(snapshot[0].bindings[0].overrides.value,2);assert.notEqual(again[0].id,one.id);
});

test('clipboard shortcuts support Ctrl and Cmd, including Russian keyboard, without intercepting editing',()=>{
 const event={code:'KeyC',key:'с',ctrlKey:true,target:{closest:()=>null}};
 assert.equal(clipboardCommand(event),'copy');assert.equal(clipboardCommand({...event,code:'KeyV'}),'paste');
 assert.equal(clipboardCommand({...event,ctrlKey:false,metaKey:true}),'copy');
 for(const patch of [{target:{closest:()=>({})}},{target:{isContentEditable:true}},{repeat:true},{defaultPrevented:true},{altKey:true},{ctrlKey:false}])assert.equal(clipboardCommand({...event,...patch}),null);
});
