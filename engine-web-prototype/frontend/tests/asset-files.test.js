import test from 'node:test';
import assert from 'node:assert/strict';
import {collectAssetFiles,assetFolders,folderContents,storeAssetFile,retainObjectModel,cleanAssetPath,importAssetFiles} from '../src/assetFiles.js';
const model={name:'prop.obj',src:'o prop\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n',format:'obj',bytes:55};
test('explorer combines imported files, referenced models and bundled audio without duplicates',()=>{
 const p={objects:[{id:'one',name:'Куб',model},{id:'two',name:'Копия',model}],assetFiles:[]};
 const file=storeAssetFile(p,{id:'file',name:model.name,path:'Props/prop.obj',src:model.src,kind:'model',model});
 assert.equal(file.model.src,undefined);
 const files=collectAssetFiles(p,[{id:'music',file:'music.wav',url:'/sound/music.wav'}]);
 assert.equal(files.length,2);assert.equal(files[0].users.length,2);assert.equal(files[0].model.src,model.src);assert.equal(files[1].path,'Звук/music.wav');
});
test('nested folders, empty folders and scoped recursive search survive serialization',()=>{
 const p={assetFolders:['Empty/Sub'],assetFiles:[{id:'a',path:'Models/Room/chair.obj'},{id:'b',path:'Models/lamp.obj'},{id:'c',path:'Sounds/chair.wav'}]};
 const saved=JSON.parse(JSON.stringify(p)),folders=assetFolders(saved.assetFiles,saved.assetFolders);
 assert.deepEqual(folderContents(saved.assetFiles,folders,'Models').folders,['Models/Room']);
 assert.deepEqual(folderContents(saved.assetFiles,folders,'Models').files.map(f=>f.id),['b']);
 assert.deepEqual(folderContents(saved.assetFiles,folders,'Models','CHAIR').files.map(f=>f.id),['a']);
 assert.ok(folders.includes('Empty/Sub'));assert.deepEqual(folderContents(saved.assetFiles,folders,'Empty/Sub'),{folders:[],files:[]});
});
test('same content is deduplicated and filename collisions preserve both files',()=>{
 const p={};storeAssetFile(p,{name:'file.txt',path:'Docs/file.txt',src:'one'});storeAssetFile(p,{name:'copy.txt',src:'one'});storeAssetFile(p,{name:'file.txt',path:'Docs/file.txt',src:'two'});
 assert.equal(p.assetFiles.length,2);assert.equal(p.assetFiles[1].path,'Docs/file (2).txt');
 assert.equal(cleanAssetPath('../Assets\\Props/./chair.obj'),'Assets/Props/chair.obj');
});
test('models remain in the library when the last instance is removed or replaced',()=>{
 const object={id:'one',name:'Prop',model},p={objects:[]};retainObjectModel(p,object);retainObjectModel(p,object);
 const restored=JSON.parse(JSON.stringify(p));assert.equal(restored.assetFiles.length,1);assert.equal(collectAssetFiles(restored)[0].model.src,model.src);
 const shared={objects:[{...object,id:'two'}]};retainObjectModel(shared,object);assert.equal(shared.assetFiles,undefined);
});
test('directory imports preserve relative paths and parse reusable model data',async()=>{
 const imported=await importAssetFiles([{name:'prop.obj',webkitRelativePath:'Props/Furniture/prop.obj',size:model.src.length,text:async()=>model.src}],'Imported');
 assert.equal(imported[0].path,'Imported/Props/Furniture/prop.obj');assert.equal(imported[0].model.format,'obj');assert.equal(imported[0].kind,'model');
 const large=await importAssetFiles([{name:'large.obj',size:100*1024*1024,text:async()=>model.src}]);
 assert.equal(large[0].bytes,100*1024*1024);
 await assert.rejects(importAssetFiles([{name:'large.obj',size:100*1024*1024+1}]),/100 МБ/);
 await assert.rejects(importAssetFiles([{name:'large.bin',size:4*1024*1024}]),/3 МБ/);
});
