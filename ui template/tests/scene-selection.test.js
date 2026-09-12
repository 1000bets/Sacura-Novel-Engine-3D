import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import {selectedObjectIds,selectSceneObject,captureSelectionTransform,applySelectionTransform,transformSelection} from '../src/sceneSelection.js';
import {createSceneGizmo} from '../src/SceneGizmo.js';
import {setObjectTransform,objectTransform} from '../src/sceneEditing.js';

const near=(actual,expected)=>actual.forEach((value,index)=>assert.ok(Math.abs(value-expected[index])<.000001,`${actual} differs from ${expected}`));
test('Shift toggles objects across scene and hierarchy selection and ordinary click replaces the set',()=>{
 const objects=[{id:'alice'},{id:'bob'},{id:'lamp'}];let selection={kind:'beat',id:'a1'};
 selection=selectSceneObject(selection,'alice',false,objects);
 selection=selectSceneObject(selection,'bob',true,objects);assert.deepEqual(selection,{kind:'object',id:'bob',ids:['alice','bob']});
 selection=selectSceneObject(selection,'alice',true,objects);assert.deepEqual(selectedObjectIds(selection),['bob']);
 selection=selectSceneObject(selection,'lamp',true,objects);assert.deepEqual(selectedObjectIds(selection),['bob','lamp']);
 assert.equal(selectSceneObject(selection,null,true,objects),selection);
 assert.deepEqual(selectedObjectIds(selectSceneObject(selection,null,false,objects)),[]);
 assert.deepEqual(selectedObjectIds(selectSceneObject(selection,'alice',false,objects)),['alice']);
 assert.deepEqual(selectedObjectIds({kind:'object',id:'bob'}),['bob']);
});

test('stale objects from another scene or a deletion cannot enter the next additive selection',()=>{
 const selection=selectSceneObject({kind:'object',id:'old',ids:['old','kept']},'next',true,[{id:'kept'},{id:'next'}]);
 assert.deepEqual(selection.ids,['kept','next']);assert.deepEqual(selectedObjectIds(selection,[{id:'next'}]),['next']);
});

test('group inspector rotates and scales around one center, preserving individual offsets',()=>{
 const objects=[{id:'left',transforms:{room:{position:[-2,0,0],rotation:[0,0,0],scale:[1,1,1]}}},{id:'right',transforms:{room:{position:[2,0,0],rotation:[0,0,0],scale:[1,1,1]}}}];
 const result=transformSelection(objects,{id:'room',kind:'living'},{position:[3,1,0],rotation:[0,0,90],scale:[2,2,2]});
 near(result[0].value.position,[3,-3,0]);near(result[1].value.position,[3,5,0]);
 near(result[0].value.rotation,[0,0,90]);near(result[1].value.scale,[2,2,2]);
 assert.deepEqual(objects[0].transforms.room.position,[-2,0,0]);
});

test('pivot drag preserves object parents and descendants and recomputes from the initial capture',()=>{
 const scene=new THREE.Scene(),parent=new THREE.Group(),a=new THREE.Object3D(),b=new THREE.Object3D(),child=new THREE.Object3D(),pivot=new THREE.Object3D();
 parent.position.set(10,0,0);scene.add(parent,b,pivot);parent.add(a);a.position.set(-12,0,0);b.position.set(2,0,0);child.position.set(0,1,0);a.add(child);
 const capture=captureSelectionTransform([a,b],pivot);
 pivot.rotation.z=Math.PI/2;pivot.scale.setScalar(2);applySelectionTransform(capture,pivot);
 near(a.getWorldPosition(new THREE.Vector3()).toArray(),[0,-4,0]);near(b.getWorldPosition(new THREE.Vector3()).toArray(),[0,4,0]);near(child.getWorldPosition(new THREE.Vector3()).toArray(),[-2,-4,0]);
 applySelectionTransform(capture,pivot);near(b.getWorldPosition(new THREE.Vector3()).toArray(),[0,4,0]);assert.equal(a.parent,parent);assert.equal(child.parent,a);
 pivot.rotation.z=0;pivot.scale.setScalar(1);pivot.position.set(3,0,0);applySelectionTransform(capture,pivot);near(a.getWorldPosition(new THREE.Vector3()).toArray(),[1,0,0]);near(b.getWorldPosition(new THREE.Vector3()).toArray(),[5,0,0]);
});

class Surface extends EventTarget{style={};closest(){return this;}focus(){}}
test('actual gizmo drag commits one batch, stays stable until React applies it, and undo restores both objects',()=>{
 const scene=new THREE.Scene(),camera=new THREE.PerspectiveCamera(58,1.3,.1,400),canvas=new Surface(),orbit={enabled:true,target:new THREE.Vector3()},a=new THREE.Mesh(new THREE.BoxGeometry(),new THREE.MeshBasicMaterial()),b=a.clone();
 a.userData.id='a';b.userData.id='b';a.position.x=-2;b.position.x=2;scene.add(a,b);camera.position.set(0,3,8);
 const project={objects:[{id:'a',transforms:{room:{position:[-2,0,0],rotation:[0,0,0],scale:[1,1,1]}}},{id:'b',transforms:{room:{position:[2,0,0],rotation:[0,0,0],scale:[1,1,1]}}}]},before=structuredClone(project),batches=[];
 const live={mode:'scene',editing:true,sceneId:'room',objects:project.objects,selected:'b',selectedIds:['a','b'],onTransforms:changes=>batches.push(changes)};
 const gizmo=createSceneGizmo(scene,camera,canvas,orbit,()=>live,()=>[a,b]);gizmo.update();
 const control=scene.children.find(node=>node.isTransformControlsRoot).controls;assert.notEqual(control.object,a);assert.notEqual(control.object,b);
 assert.equal(scene.children.filter(node=>node.type==='BoxHelper').length,2);
 control.dragging=true;control.dispatchEvent({type:'mouseDown'});control.object.position.x+=3;control.dispatchEvent({type:'objectChange'});
 gizmo.apply(a,live.objects[0],[-2,0,0]);gizmo.apply(b,live.objects[1],[2,0,0]);near(a.position.toArray(),[1,0,0]);near(b.position.toArray(),[5,0,0]);
 control.dispatchEvent({type:'mouseUp'});control.dragging=false;assert.equal(batches.length,1);assert.equal(batches[0].length,2);
 gizmo.apply(a,live.objects[0],[-2,0,0]);near(a.position.toArray(),[1,0,0]);
 for(const {id,value}of batches[0])setObjectTransform(project,id,'room',value);
 gizmo.apply(a,live.objects[0],[]);gizmo.apply(b,live.objects[1],[]);near(a.position.toArray(),[1,0,0]);
 live.objects=before.objects;gizmo.apply(a,live.objects[0],[]);gizmo.apply(b,live.objects[1],[]);near(a.position.toArray(),[-2,0,0]);near(b.position.toArray(),[2,0,0]);
 live.focusRequest={nonce:1};gizmo.update();near(orbit.target.toArray(),[0,0,0]);
 live.selectedIds=['a'];live.selected='a';gizmo.update();assert.equal(control.object,a);assert.equal(scene.children.filter(node=>node.type==='BoxHelper').length,1);
 gizmo.dispose();assert.equal(scene.children.filter(node=>node.type==='BoxHelper').length,0);near(objectTransform(project.objects[1],'room').position,[5,0,0]);
});
