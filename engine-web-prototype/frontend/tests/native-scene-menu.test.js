import test from 'node:test';
import assert from 'node:assert/strict';
import {Quaternion,Vector3} from 'three';
import {nativeScenePlacement} from '../src/nativeScenePlacement.js';
import {nativeSceneMenuItems} from '../src/nativeSceneMenu.js';

test('native scene placement uses the clicked ground point and current camera pose',()=>{
  const view={cameraPosition:[4,3,6],cameraTarget:[0,0,0]};
  const center=nativeScenePlacement(view,400,300,800,600);
  assert.ok(center);
  assert.deepEqual(center.cameraTransform.position,view.cameraPosition);
  assert.equal(center.cameraAspect,4/3);
  const direction=new Vector3(0,0,-1).applyQuaternion(new Quaternion(...center.cameraTransform.rotation));
  const expected=new Vector3(...view.cameraTarget).sub(new Vector3(...view.cameraPosition)).normalize();
  assert.ok(direction.distanceTo(expected)<1e-6);
  assert.ok(center.worldPosition.every(Number.isFinite));
  assert.ok(Math.abs(center.worldPosition[1])<1e-5);
  const side=nativeScenePlacement(view,550,300,800,600);
  assert.notDeepEqual(side.worldPosition,center.worldPosition);
  assert.equal(nativeScenePlacement(null,400,300,800,600),null);
});

test('native scene actions create placed components, duplicate and guard unsupported entries',async()=>{
  const calls=[],selected=[];
  const context={worldPosition:[2,0,3],cameraAspect:4/3,cameraTransform:{position:[4,3,6],rotation:[0,0,0,1],scale:[1,1,1]}};
  const items=nativeSceneMenuItems({context,selection:'one',objects:[{id:'one'}],editing:true,editSpace:'world',showGrid:true,
    command:async(method,params)=>{calls.push([method,params]);return {id:'created'};},select:id=>selected.push(id),
    focus:()=>calls.push(['focus']),setEditSpace:value=>calls.push(['space',value]),setShowGrid:fn=>calls.push(['grid',fn(true)])});
  const find=id=>items.flatMap(item=>item.children||[item]).find(item=>item.id===id);
  for(const [id,label] of [['cube','Куб'],['sphere','Сфера'],['cylinder','Цилиндр']]){
    assert.equal(find(`scene.${id}`).disabled,false);
    await find(`scene.${id}`).action();
    assert.deepEqual(calls.at(-1),['object.create',{name:label,componentType:'engine.MeshRendererComponent',
      transform:{position:[2,.5,3],rotation:[0,0,0,1],scale:[1,1,1]},primitive:id}]);
  }
  await find('scene.light').action();
  assert.deepEqual(calls[3],['object.create',{name:'Источник освещения',componentType:'engine.LightComponent',transform:{position:[2,0,3],rotation:[0,0,0,1],scale:[1,1,1]}}]);
  await find('scene.camera').action();
  assert.deepEqual(calls[4],['object.create',{name:'Камера',componentType:'engine.CameraComponent',transform:context.cameraTransform,cameraAspect:4/3}]);
  await find('scene.duplicate').action();
  assert.deepEqual(calls[5],['object.duplicate',{objectId:'one'}]);
  find('scene.axes.local').action();find('scene.grid').action();find('scene.frame').action();
  assert.deepEqual(calls.slice(6),[['space','local'],['grid',false],['focus']]);
  assert.deepEqual(selected,['created','created','created','created','created','created']);
  for(const id of ['scene.character','scene.interactive','scene.sound','scene.group'])assert.ok(find(id).disabled&&find(id).reason);
  const noSelection=nativeSceneMenuItems({context,selection:null,objects:[],editing:true,command(){},select(){},focus(){},setEditSpace(){},setShowGrid(){}});
  assert.ok(noSelection.find(item=>item.id==='scene.delete').disabled);
  assert.ok(noSelection.find(item=>item.id==='scene.axes').children.every(item=>item.disabled));
});
