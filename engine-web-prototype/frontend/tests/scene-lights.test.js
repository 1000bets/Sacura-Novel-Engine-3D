import test from 'node:test';
import assert from 'node:assert/strict';
import {createLightObject,updateLightObject} from '../src/sceneLights.js';

test('point light updates independently of its editor marker',()=>{
 const object={id:'light',color:'#ff8800',light:{intensity:50,distance:12,decay:2}};
 const group=createLightObject(object),light=group.userData.pointLight,marker=group.userData.lightMarker;
 assert.equal(group.userData.id,'light');
 assert.equal(light.isPointLight,true);
 assert.equal(light.intensity,50);
 assert.equal(light.distance,12);
 assert.equal(light.color.getHexString(),'ff8800');
 updateLightObject(group,{...object,color:'#ffffff',light:{intensity:80,distance:0}},false);
 assert.equal(marker.visible,false);
 assert.equal(light.visible,true);
 assert.equal(light.intensity,80);
 assert.equal(light.distance,0);
 assert.equal(light.color.getHexString(),'ffffff');
 updateLightObject(group,object,true);
 assert.equal(marker.visible,true);
});

test('light parameters have safe defaults and separate instances',()=>{
 const first=createLightObject({id:'one'}),second=createLightObject({id:'two'});
 assert.equal(first.userData.pointLight.intensity,35);
 assert.equal(first.userData.pointLight.distance,10);
 updateLightObject(first,{light:{intensity:-1,distance:NaN,decay:Infinity}},true);
 assert.equal(first.userData.pointLight.intensity,0);
 assert.equal(first.userData.pointLight.distance,10);
 assert.equal(first.userData.pointLight.decay,2);
 assert.equal(second.userData.pointLight.intensity,35);
});
