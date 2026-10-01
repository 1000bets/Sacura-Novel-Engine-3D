import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {importMeshFile,parseMeshAsset,MAX_MESH_BYTES} from '../src/meshAssets.js';
import {createMeshObject,updateMeshVisual,disposeMeshVisual,applyMaterialAssignments,disposeMaterialAssignments,MATERIAL_PRESETS} from '../src/meshVisual.js';
import {disposeCharacterAsset} from '../src/characterAssets.js';

const obj='o Triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Body\nf 1 2 3\no Second\nv 0 0 1\nusemtl Detail\nf 1 2 4\n';
const file=(text=obj,name='custom.obj')=>({name,size:text.length,text:async()=>text});
test('OBJ import preserves geometry, discovers slots and survives project serialization',async()=>{
 const model=await importMeshFile(file());assert.equal(model.format,'obj');assert.deepEqual(model.slots.map(s=>s.name),['Body','Detail']);
 const restored=JSON.parse(JSON.stringify({objects:[{id:'custom',model}]}));const root=await parseMeshAsset(restored.objects[0].model);
 assert.equal(root.children.length,2);assert.equal(root.children[0].geometry.attributes.position.count,3);disposeCharacterAsset(root);
});
test('unsupported, oversized and empty models fail before attachment',async()=>{
 await assert.rejects(importMeshFile(file(obj,'model.fbx')),/GLB или OBJ/);
 await assert.rejects(importMeshFile({...file(),size:MAX_MESH_BYTES+1}),/3 МБ/);
 await assert.rejects(importMeshFile(file('v 0 0 0\n')),/нет геометрии/);
});
test('slot assignments override the whole object and reset to imported materials',async()=>{
 const model=await importMeshFile(file()),root=await parseMeshAsset(model);const originals=root.children.map(m=>m.material);
 applyMaterialAssignments(root,{model,materialOverrides:{all:MATERIAL_PRESETS.matte,'1':MATERIAL_PRESETS.metal}});
 assert.equal(root.children[0].material.metalness,0);assert.equal(root.children[1].material.metalness,1);
 assert.notEqual(root.children[0].material,originals[0]);
 applyMaterialAssignments(root,{model,materialOverrides:{}});assert.equal(root.children[0].material,originals[0]);assert.equal(root.children[1].material,originals[1]);
 disposeMaterialAssignments(root);disposeCharacterAsset(root);
});
test('custom mesh replacement, reset and pending disposal keep the placeholder usable',async()=>{
 const model=await importMeshFile(file()),object={id:'object',model,color:'#ffffff'},host=createMeshObject(object);
 updateMeshVisual(host,object);await new Promise(resolve=>setTimeout(resolve,0));
 assert.equal(host.children.length,2);assert.equal(host.userData.placeholder.visible,false);
 updateMeshVisual(host,{...object,model:null});assert.equal(host.children.length,1);assert.equal(host.userData.placeholder.visible,true);
 updateMeshVisual(host,object);disposeMeshVisual(host);await new Promise(resolve=>setTimeout(resolve,0));assert.equal(host.children.length,1);
 disposeCharacterAsset(host);
});
test('malformed saved GLB fails without hiding the primitive',async()=>{
 const host=createMeshObject({id:'bad'});updateMeshVisual(host,{model:{format:'glb',src:'data:;base64,YmFk'}});await new Promise(resolve=>setTimeout(resolve,0));
 assert.ok(host.userData.meshError);assert.equal(host.userData.placeholder.visible,true);disposeMeshVisual(host);disposeCharacterAsset(host);
});
test('primitive color and original materials are restored after overrides',()=>{
 const host=createMeshObject({id:'primitive',color:'#ffffff'}),original=host.children[0].material;
 applyMaterialAssignments(host,{color:'#ffffff',materialOverrides:{all:MATERIAL_PRESETS.metal}});assert.equal(host.children[0].material.metalness,1);
 applyMaterialAssignments(host,{color:'#ff0000',materialOverrides:{}});assert.equal(host.children[0].material,original);assert.equal(original.color.getHexString(),'ff0000');disposeMeshVisual(host);disposeCharacterAsset(host);
});

test('embedded GLB loads geometry and named materials without external resources',async()=>{
 const data=Buffer.alloc(36);[0,0,0,1,0,0,0,1,0].forEach((n,i)=>data.writeFloatLE(n,i*4));
 const document={asset:{version:'2.0'},buffers:[{byteLength:36}],bufferViews:[{buffer:0,byteOffset:0,byteLength:36}],accessors:[{bufferView:0,componentType:5126,count:3,type:'VEC3',min:[0,0,0],max:[1,1,0]}],materials:[{name:'GLB surface',pbrMetallicRoughness:{baseColorFactor:[1,0,0,1],metallicFactor:.2,roughnessFactor:.4}}],meshes:[{primitives:[{attributes:{POSITION:0},material:0}]}],nodes:[{mesh:0}],scenes:[{nodes:[0]}],scene:0};
 const json=Buffer.from(JSON.stringify(document));const padded=Buffer.alloc(Math.ceil(json.length/4)*4,32);json.copy(padded);
 const glb=Buffer.alloc(12+8+padded.length+8+data.length);glb.writeUInt32LE(0x46546c67,0);glb.writeUInt32LE(2,4);glb.writeUInt32LE(glb.length,8);glb.writeUInt32LE(padded.length,12);glb.writeUInt32LE(0x4e4f534a,16);padded.copy(glb,20);const offset=20+padded.length;glb.writeUInt32LE(data.length,offset);glb.writeUInt32LE(0x004e4942,offset+4);data.copy(glb,offset+8);
 const model={format:'glb',src:'data:model/gltf-binary;base64,'+glb.toString('base64')},root=await parseMeshAsset(model);
 assert.equal(root.children[0].geometry.attributes.position.count,3);assert.equal(root.children[0].material.name,'GLB surface');
 const original=root.children[0].material;applyMaterialAssignments(root,{model,materialOverrides:{all:MATERIAL_PRESETS.stone}});assert.equal(root.children[0].material.roughness,1);
 disposeMaterialAssignments(root);assert.equal(root.children[0].material,original);assert.equal(original.roughness,.4);disposeCharacterAsset(root);
});
