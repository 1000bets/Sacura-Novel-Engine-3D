import * as THREE from 'three';

export function createLightObject(object){
 const group=new THREE.Group();group.userData.id=object.id;
 const light=new THREE.PointLight(),marker=new THREE.Mesh(new THREE.SphereGeometry(.12,12,8),new THREE.MeshBasicMaterial());
 group.add(light,marker);group.userData.pointLight=light;group.userData.lightMarker=marker;
 updateLightObject(group,object,true);return group;
}
export function updateLightObject(group,object,editing){
 const light=group.userData.pointLight;if(!light)return;
 const nonnegative=(value,fallback)=>Number.isFinite(value)?Math.max(0,value):fallback;
 light.color.set(object.color||'#fff0d0');light.intensity=nonnegative(object.light?.intensity,35);light.distance=nonnegative(object.light?.distance,10);light.decay=nonnegative(object.light?.decay,2);
 group.userData.lightMarker.visible=editing;group.userData.lightMarker.material.color.copy(light.color);
}
