import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {humanoid,mixamoFbx,humanoidGlb} from './fixtures/humanoid.js';
import {humanoidBoneRole,retargetMixamoAnimations} from '../src/mixamoAnimations.js';
import {parseMixamoFbx,importCharacterFile,parseCharacterGlb,disposeCharacterAsset,MAX_ANIMATION_FILE_BYTES} from '../src/characterAssets.js';
import {updateCharacterVisual,disposeCharacterVisual} from '../src/characterVisual.js';
import {upgradeProject} from '../src/studioModel.js';
const close=(actual,expected)=>assert.ok(Math.abs(actual-expected)<1e-5,`${actual} != ${expected}`);
const dataUrl=buffer=>'data:model/gltf-binary;base64,'+Buffer.from(buffer).toString('base64');

test('humanoid mapping recognizes sanitized Mixamo namespaces and common bone aliases',()=>{
 for(const name of ['mixamorig:Hips','mixamorigHips','Armature|mixamorig1_Hips','pelvis'])assert.equal(humanoidBoneRole(name),'Hips');
 assert.equal(humanoidBoneRole('upper_arm.L'),'LeftArm');assert.equal(humanoidBoneRole('thigh_r'),'RightUpLeg');
 assert.equal(humanoidBoneRole('mixamorigLeftHandIndex2'),'LeftHandIndex2');assert.equal(humanoidBoneRole('Cube'),null);
});
test('FBX loader reads a Without Skin skeleton and animation without fetching textures',async()=>{
 const root=await parseMixamoFbx(mixamoFbx({texture:true}));
 assert.equal(root.animations.length,1);assert.equal(root.animations[0].duration,1);
 assert.ok(root.getObjectByName('mixamorigHips').isBone);assert.ok(root.animations[0].tracks.some(t=>t.name==='mixamorigLeftArm.quaternion'));
 disposeCharacterAsset(root);
 await assert.rejects(parseMixamoFbx(new ArrayBuffer(30)),/FBX Binary/);
});
test('retarget preserves bind pose and transfers rotations between different bone axes',()=>{
 const source=humanoid(),target=humanoid('',.01);
 target.bones[4].rotation.z=Math.PI/2;target.root.updateMatrixWorld(true);
 const rest=target.bones[4].quaternion.clone(),turn=new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1,0,0),Math.PI/2);
 const clip=new THREE.AnimationClip('Wave',1,[new THREE.QuaternionKeyframeTrack('mixamorigLeftArm.quaternion',[0,1],[0,0,0,1,...turn.toArray()])]);
 const [converted]=retargetMixamoAnimations(source.root,target.root,[clip]);
 assert.equal(converted.tracks[0].name,'LeftArm.quaternion');
 const first=new THREE.Quaternion().fromArray(converted.tracks[0].values).normalize();close(first.angleTo(rest),0);
 const mixer=new THREE.AnimationMixer(target.root);mixer.clipAction(converted).setLoop(THREE.LoopOnce,1).play();mixer.update(.5);
 close(target.bones[4].quaternion.angleTo(rest),Math.PI/4);
 assert.equal(clip.tracks[0].name,'mixamorigLeftArm.quaternion');
});
test('hips translation adapts units and parent transforms; in-place retains vertical motion',()=>{
 const source=humanoid(),target=humanoid('',.01);source.root.scale.setScalar(.01);source.root.updateMatrixWorld(true);
 const clip=new THREE.AnimationClip('Jump',1,[new THREE.QuaternionKeyframeTrack('mixamorigHips.quaternion',[0,1],[0,0,0,1,0,0,0,1]),new THREE.VectorKeyframeTrack('mixamorigHips.position',[0,1],[0,100,0,50,120,30])]);
 const [inPlace]=retargetMixamoAnimations(source.root,target.root,[clip]),[moving]=retargetMixamoAnimations(source.root,target.root,[clip],{inPlace:false});
 const still=inPlace.tracks[1].values,walk=moving.tracks[1].values;
 close(still[3],0);close(still[4],1.2);close(still[5],0);close(walk[3],.5);close(walk[4],1.2);close(walk[5],.3);
});
test('incompatible and ambiguous rigs fail with actionable errors',()=>{
 const source=humanoid(),target=humanoid('');
 assert.throws(()=>retargetMixamoAnimations(source.root,new THREE.Group(),[]),/не распознан humanoid/);
 const duplicate=new THREE.Bone();duplicate.name='pelvis';target.root.add(duplicate);
 assert.throws(()=>retargetMixamoAnimations(source.root,target.root,[]),/несколько костей/);
});
test('FBX upload validates size, target skin and missing animations',async()=>{
 await assert.rejects(importCharacterFile({name:'large.fbx',size:MAX_ANIMATION_FILE_BYTES+1},{animationsOnly:true}),/20 МБ/);
 await assert.rejects(importCharacterFile(new File([mixamoFbx()],'Wave.fbx'),{animationsOnly:true}),/Сначала загрузите/);
 await assert.rejects(importCharacterFile(new File([mixamoFbx({animated:false})],'Empty.fbx'),{animationsOnly:true,targetModel:{src:dataUrl(humanoidGlb())}}),/нет анимаций/);
 await assert.rejects(importCharacterFile(new File([mixamoFbx()],'Wave.fbx'),{animationsOnly:true,targetModel:{src:dataUrl(humanoidGlb({skinned:false}))}}),/нет skinned mesh/);
 await assert.rejects(importCharacterFile(new File([mixamoFbx()],'Wave.fbx')),/GLB/);
});
test('imported Mixamo clips survive project round-trip and play on the GLB target',async()=>{
 const model={src:dataUrl(humanoidGlb()),animations:[]},clips=await importCharacterFile(new File([mixamoFbx()],'Wave.fbx'),{animationsOnly:true,targetModel:model});
 assert.equal(clips[0].source,'mixamo');assert.equal(clips[0].name,'Wave');assert.equal(clips[0].inPlace,true);
 assert.ok(clips[0].clip.tracks.some(t=>t.name==='LeftArm.quaternion'));
 const project=upgradeProject();Object.assign(project.objects.find(o=>o.id==='alice'),{model,extraAnimations:clips,enabledAnimations:[clips[0].id],idleAnimation:clips[0].id});
 const character=upgradeProject(JSON.parse(JSON.stringify(project))).objects.find(o=>o.id==='alice'),mesh=new THREE.Group();
 updateCharacterVisual(mesh,character,0,null,false,false,false);
 for(let i=0;i<30&&!mesh.userData.characterVisual.mixer;i++)await new Promise(resolve=>setImmediate(resolve));
 assert.ok(mesh.userData.characterVisual.mixer);updateCharacterVisual(mesh,character,.5,null,false,false,false);
 close(mesh.userData.characterVisual.root.getObjectByName('LeftArm').rotation.x,Math.PI/4);
 disposeCharacterVisual(mesh);
 const asset=await parseCharacterGlb(humanoidGlb());disposeCharacterAsset(asset.scene);
});

// A quarter turn around source X becomes a turn around world -Z when its parent faces +X.
test('retarget accounts for rotated parent coordinate systems',()=>{
 const source=humanoid(),target=humanoid('',.01);source.root.rotation.y=Math.PI/2;
 const q=new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(1,0,0),Math.PI/2);
 const clip=new THREE.AnimationClip('Turn',1,[new THREE.QuaternionKeyframeTrack('mixamorigHips.quaternion',[0,1],[0,0,0,1,...q.toArray()])]);
 const [converted]=retargetMixamoAnimations(source.root,target.root,[clip]);
 const end=new THREE.Quaternion().fromArray(converted.tracks[0].values,4).normalize();
 const up=new THREE.Vector3(0,1,0).applyQuaternion(end);close(up.x,1);close(up.y,0);close(up.z,0);
});
