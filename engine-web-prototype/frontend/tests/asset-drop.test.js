import test from 'node:test';
import assert from 'node:assert/strict';
import {hasDroppedFiles,dropAssetFiles} from '../src/assetDrop.js';
import {importAssetFiles} from '../src/assetFiles.js';
test('OS file drop cancels browser navigation and imports a model into the selected folder',async()=>{
 const file={name:'room.obj',size:4*1024*1024,text:async()=> 'v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n'},files=[file];
 let prevented=false,stopped=false,dragging=true,result;
 const event={dataTransfer:{types:['Files'],files},preventDefault(){prevented=true;},stopPropagation(){stopped=true;}};
 const dragDepth={current:3};
 dropAssetFiles(event,{dragDepth,onDragging:value=>dragging=value,onFiles:input=>{result=importAssetFiles(input,'Models/Room');}});
 files.length=0;
 const imported=await result;
 assert.ok(prevented&&stopped);assert.equal(dragging,false);assert.equal(dragDepth.current,0);
 assert.equal(imported[0].path,'Models/Room/room.obj');assert.equal(imported[0].model.format,'obj');
});
test('disabled drops are consumed without importing; ordinary text drags are ignored',()=>{
 let count=0,prevented=false;
 const event={dataTransfer:{types:['Files'],files:[{}]},preventDefault(){prevented=true;},stopPropagation(){}};
 dropAssetFiles(event,{dragDepth:{current:1},onDragging:()=>{},onFiles:()=>count++,disabled:true});
 assert.ok(prevented);assert.equal(count,0);
 assert.equal(hasDroppedFiles({dataTransfer:{types:['text/plain'],files:[]}}),false);
 assert.ok(hasDroppedFiles({dataTransfer:{items:[{kind:'file'}],files:[]}}));
});
