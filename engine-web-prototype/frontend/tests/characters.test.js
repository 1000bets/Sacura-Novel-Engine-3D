import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {upgradeProject,validateStudio} from '../src/studioModel.js';
import {createCharacter,setCharacterInScene,characterScenes,characterAnimationOptions,characterAnimationId,enabledCharacterAnimations,characterAnimations} from '../src/characterModel.js';
import {parseCharacterGlb,validateCharacterGlb,disposeCharacterAsset,importCharacterFile} from '../src/characterAssets.js';
import {updateCharacterVisual,disposeCharacterVisual} from '../src/characterVisual.js';

function glb(json,bin){
 const text=new TextEncoder().encode(JSON.stringify(json)),jsonLength=Math.ceil(text.length/4)*4,binLength=bin?Math.ceil(bin.byteLength/4)*4:0;
 const buffer=new ArrayBuffer(20+jsonLength+(bin?8+binLength:0)),view=new DataView(buffer);
 view.setUint32(0,0x46546c67,true);view.setUint32(4,2,true);view.setUint32(8,buffer.byteLength,true);view.setUint32(12,jsonLength,true);view.setUint32(16,0x4e4f534a,true);
 new Uint8Array(buffer,20,jsonLength).fill(32);new Uint8Array(buffer,20,text.length).set(text);
 if(bin){view.setUint32(20+jsonLength,binLength,true);view.setUint32(24+jsonLength,0x004e4942,true);new Uint8Array(buffer,28+jsonLength,bin.byteLength).set(new Uint8Array(bin));}
 return buffer;
}
export function animatedCharacterFixture(){
 const data=new Float32Array([-.2,0,0,.2,0,0,0,1.6,0, 0,1, 0,0,0, .5,0,0]);
 return glb({asset:{version:'2.0'},scene:0,scenes:[{nodes:[0]}],nodes:[{name:'Body',mesh:0}],meshes:[{primitives:[{attributes:{POSITION:0}}]}],buffers:[{byteLength:data.byteLength}],bufferViews:[{buffer:0,byteOffset:0,byteLength:36},{buffer:0,byteOffset:36,byteLength:8},{buffer:0,byteOffset:44,byteLength:24}],accessors:[{bufferView:0,componentType:5126,count:3,type:'VEC3',min:[-.2,0,0],max:[.2,1.6,0]},{bufferView:1,componentType:5126,count:2,type:'SCALAR',min:[0],max:[1]},{bufferView:2,componentType:5126,count:2,type:'VEC3'}],animations:[{name:'Walk',samplers:[{input:1,output:2}],channels:[{sampler:0,target:{node:0,path:'translation'}}]}]},data.buffer);
}

test('characters start in the chosen location and gain additional locations without appearing everywhere',()=>{
 const p=upgradeProject(),character=createCharacter(p,'garden','  Герой  ');
 assert.equal(character.name,'Герой');assert.deepEqual(characterScenes(p,character).map(scene=>scene.id),['garden']);
 setCharacterInScene(p,character.id,'living',true);assert.deepEqual(characterScenes(p,character).map(scene=>scene.id).sort(),['garden','living']);
 setCharacterInScene(p,character.id,'garden',false);assert.deepEqual(characterScenes(p,character).map(scene=>scene.id),['living']);
 assert.deepEqual(upgradeProject(JSON.parse(JSON.stringify(p))),p);
 assert.throws(()=>createCharacter(p,'missing','Герой'));assert.throws(()=>createCharacter(p,'garden',' '));
});
test('per-character animation pools preserve unavailable actions and prioritize walk while moving',()=>{
 const character={model:{animations:[{id:'model:0',name:'Idle'},{id:'model:1',name:'Walk'}]},extraAnimations:[{id:'wave',name:'Wave'}],enabledAnimations:['model:0','model:1'],idleAnimation:'model:0',walkAnimation:'model:1'};
 assert.deepEqual(enabledCharacterAnimations(character).map(clip=>clip.id),['model:0','model:1']);
 assert.equal(characterAnimationId(character,'model:0',true),'model:1');assert.equal(characterAnimationId(character,undefined,false),'model:0');
 assert.equal(characterAnimationId(character,'wave',false),null);assert.match(characterAnimationOptions(character,'wave')[0][1],/Недоступная/);
 character.enabledAnimations=[];assert.deepEqual(characterAnimationOptions(character),[]);assert.equal(characterAnimationId(character,undefined,true),null);
});
test('disabled animation used by the scenario is reported after saving and loading',()=>{
 const project=upgradeProject(),character=project.objects.find(object=>object.id==='alice');
 character.enabledAnimations=['стоит'];
 assert.ok(validateStudio(project).some(issue=>issue.id.startsWith('action-animation-')));
 const loaded=upgradeProject(JSON.parse(JSON.stringify(project)));assert.deepEqual(loaded.objects.find(object=>object.id==='alice').enabledAnimations,['стоит']);
 assert.ok(validateStudio(loaded).some(issue=>issue.id.startsWith('action-animation-')));
});
test('GLB parser rejects missing data and external dependencies and reads embedded mesh and clips',async()=>{
 assert.throws(()=>validateCharacterGlb(new ArrayBuffer(2)),/GLB/);
 assert.throws(()=>validateCharacterGlb(glb({asset:{version:'2.0'},buffers:[{uri:'https://example.com/model.bin'}]})),/Внешние/);
 const buffer=animatedCharacterFixture(),asset=await parseCharacterGlb(buffer);
 assert.equal(asset.animations[0].name,'Walk');assert.equal(asset.animations[0].duration,1);assert.ok(asset.scene.getObjectByName('Body').isMesh);disposeCharacterAsset(asset.scene);
});
test('uploaded character replaces placeholder, plays actual clip, pauses, restores bind pose and cleans up',async()=>{
 const mesh=new THREE.Group(),placeholder=new THREE.Group();mesh.add(placeholder);
 const buffer=animatedCharacterFixture(),character={type:'Персонаж',model:{src:'data:model/gltf-binary;base64,'+Buffer.from(buffer).toString('base64'),animations:[{id:'model:0',name:'Walk'}]},idleAnimation:'model:0'};
 updateCharacterVisual(mesh,character,.1,null,false,false,false);
 for(let i=0;i<20&&!mesh.userData.characterVisual.mixer;i++)await new Promise(resolve=>setImmediate(resolve));
 const visual=mesh.userData.characterVisual;assert.ok(visual.mixer);assert.equal(placeholder.visible,false);
 updateCharacterVisual(mesh,character,.5,null,false,false,false);assert.equal(visual.root.getObjectByName('Body').position.x,.25);
 const time=visual.mixer.time;updateCharacterVisual(mesh,character,.5,null,false,true,false);assert.equal(visual.mixer.time,time);
 updateCharacterVisual(mesh,character,.1,null,false,false,true);assert.equal(visual.root.getObjectByName('Body').position.x,0);
 disposeCharacterVisual(mesh);assert.equal(placeholder.visible,true);assert.equal(mesh.children.length,1);assert.equal(mesh.userData.characterVisual,undefined);
});

test('custom standard movement retains its description and plays its base pose at chosen speed',()=>{
 const project=upgradeProject(),character=project.objects.find(item=>item.id==='alice');
 character.extraAnimations=[{id:'greeting',name:'Приветствие',basePose:'улыбка',description:'Поднимает руку, приветствуя друга.'}];
 character.animationDetails={greeting:{name:'Радостное приветствие',description:'Радуется встрече и поднимает руку.'}};
 character.animationSettings={greeting:{speed:2}};character.enabledAnimations=['greeting'];
 const loaded=upgradeProject(JSON.parse(JSON.stringify(project))).objects.find(item=>item.id==='alice');
 const clip=characterAnimations(loaded).find(item=>item.id==='greeting');
 assert.equal(clip.name,'Радостное приветствие');assert.equal(clip.description,'Радуется встрече и поднимает руку.');
 assert.equal(characterAnimationOptions(loaded)[0][0],'greeting');
 const mesh=new THREE.Group(),head=new THREE.Group(),left=new THREE.Group();mesh.userData.isCharacter=true;mesh.userData.parts={head,left};
 updateCharacterVisual(mesh,loaded,.5,'greeting',false,false,false);
 assert.equal(head.rotation.z,-.13);assert.equal(left.rotation.z,-.6);assert.equal(mesh.userData.characterTime,1);
 updateCharacterVisual(mesh,loaded,.5,'greeting',false,true,false);assert.equal(mesh.userData.characterTime,1);
 disposeCharacterVisual(mesh);
});

test('imported animations describe their channels without inventing the meaning of the movement',async()=>{
 const file=new File([animatedCharacterFixture()],'movement.glb');
 const clips=await importCharacterFile(file,{animationsOnly:true});
 assert.match(clips[0].description,/перемещение/);assert.match(clips[0].description,/уточните/);assert.ok(clips[0].clip);
});
