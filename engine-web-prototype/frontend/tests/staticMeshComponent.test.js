import test from 'node:test';
import assert from 'node:assert/strict';
import {hasStaticMesh,canAddStaticMesh} from '../src/staticMeshComponent.js';
import {createMeshObject,updateMeshVisual,disposeMeshVisual} from '../src/meshVisual.js';

test('legacy mesh objects retain a component and empty objects can receive one', () => {
  assert.equal(hasStaticMesh({type:'Меш'}), true);
  assert.equal(hasStaticMesh({type:'Активный меш'}), true);
  assert.equal(hasStaticMesh({type:'Пустой объект'}), false);
  assert.equal(canAddStaticMesh({type:'Пустой объект'}), true);
  assert.equal(hasStaticMesh({type:'Пустой объект',staticMesh:true}), true);
  assert.equal(hasStaticMesh({type:'Меш',staticMesh:false}), false);
  assert.equal(hasStaticMesh({type:'Персонаж'}), false);
});

test('removing and restoring a primitive mesh updates its visual', () => {
  const object={id:'mesh-1',type:'Меш',primitive:'box'};
  const host=createMeshObject(object);
  updateMeshVisual(host,object);
  assert.equal(host.userData.placeholder.visible,true);
  updateMeshVisual(host,{...object,staticMesh:false});
  assert.equal(host.userData.placeholder.visible,false);
  updateMeshVisual(host,{...object,staticMesh:true});
  assert.equal(host.userData.placeholder.visible,true);
  disposeMeshVisual(host);
  host.userData.placeholder.geometry.dispose();
  host.userData.placeholder.material.dispose();
});
