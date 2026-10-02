import * as THREE from 'three';

// Semantic names let Mixamo's namespace survive FBX/Blender name sanitization.
const aliases=new Map();
function alias(role,...names){for(const name of [role,...names])aliases.set(name.toLowerCase().replace(/[^a-z0-9]/g,''),role);}
alias('Hips','pelvis');alias('Spine');alias('Spine1','chest');alias('Spine2','upperchest');alias('Neck');alias('Head');
for(const side of ['Left','Right']){
 const suffix=side==='Left'?'l':'r';
 alias(`${side}Shoulder`,`${side}clavicle`,`shoulder${suffix}`);
 alias(`${side}Arm`,`${side}upperarm`,`upperarm${suffix}`);
 alias(`${side}ForeArm`,`${side}lowerarm`,`forearm${suffix}`);
 alias(`${side}Hand`,`hand${suffix}`);
 alias(`${side}UpLeg`,`${side}thigh`,`${side}upperleg`,`thigh${suffix}`);
 alias(`${side}Leg`,`${side}calf`,`${side}lowerleg`,`shin${suffix}`);
 alias(`${side}Foot`,`foot${suffix}`);alias(`${side}ToeBase`,`${side}toes`,`toe${suffix}`);
 for(const finger of ['Thumb','Index','Middle','Ring','Pinky'])for(let i=1;i<=3;i++)alias(`${side}Hand${finger}${i}`);
}
export function humanoidBoneRole(name){
 const normalized=name.split(/[|:]/).pop().replace(/^(mixamorig\d*|armature)[_:]?/i,'').toLowerCase().replace(/[^a-z0-9]/g,'');
 return aliases.get(normalized)||null;
}
function rig(root,label){
 // GLB nodes can contain a pose; use the actual skin bind pose for conversion.
 const skeletons=new Set();root.traverse(node=>{if(node.skeleton)skeletons.add(node.skeleton);});
 for(const skeleton of skeletons)skeleton.pose();
 root.updateMatrixWorld(true);
 const skinBones=new Set([...skeletons].flatMap(skeleton=>skeleton.bones));
 const bones=new Map();
 root.traverse(bone=>{
  if(!bone.isBone||(skinBones.size&&!skinBones.has(bone)))return;
  const role=humanoidBoneRole(bone.name);if(!role)return;
  if(bones.has(role)){
   // FBXLoader can duplicate one FBX bone across the character's skin meshes.
   if(bone.ID!==undefined&&bone.ID===bones.get(role).ID)return;
   throw new Error(`${label}: несколько костей соответствуют ${role}. Нужен один humanoid-скелет.`);
  }
  bones.set(role,bone);
 });
 const required=['Hips','Spine','Head',...['Left','Right'].flatMap(side=>['Arm','ForeArm','Hand','UpLeg','Leg','Foot'].map(part=>side+part))];
 const missing=required.filter(role=>!bones.has(role));
 if(missing.length)throw new Error(`${label}: не распознан humanoid-скелет. Не найдены кости: ${missing.join(', ')}. Используйте имена Mixamo или поддерживаемые humanoid-имена.`);
 return bones;
}
function legLength(bones){
 const point=role=>bones.get(role).getWorldPosition(new THREE.Vector3());
 return point('LeftUpLeg').distanceTo(point('LeftLeg'))+point('LeftLeg').distanceTo(point('LeftFoot'));
}

export function retargetMixamoAnimations(source,target,clips,{inPlace=true}={}){
 const sourceBones=rig(source,'Файл анимации'),targetBones=rig(target,'Модель персонажа');
 const sourceLength=legLength(sourceBones),targetLength=legLength(targetBones);
 if(sourceLength<1e-6||targetLength<1e-6)throw new Error('У скелета нулевая длина ног. Проверьте bind pose модели.');
 const ratio=targetLength/sourceLength;
 return clips.map(clip=>{
  const tracks=[];
  for(const track of clip.tracks){
   const binding=THREE.PropertyBinding.parseTrackName(track.name);
   const role=humanoidBoneRole(binding.objectName==='bones'?String(binding.objectIndex):binding.nodeName||'');
   const from=sourceBones.get(role),to=targetBones.get(role);
   if(!from||!to)continue;
   if(!Array.from(track.values).every(Number.isFinite))throw new Error('Анимация содержит повреждённые ключи.');
   const components=binding.propertyName==='quaternion'?4:binding.propertyName==='position'?3:null;
   if(components&&(track.values.length!==track.times.length*components||!track.validate()))throw new Error('Анимация содержит повреждённые ключи.');
   if(binding.propertyName==='quaternion'){
    const fromParent=from.parent?.getWorldQuaternion(new THREE.Quaternion())||new THREE.Quaternion();
    const fromRestInverse=from.getWorldQuaternion(new THREE.Quaternion()).invert();
    const toRest=to.getWorldQuaternion(new THREE.Quaternion());
    const toParentInverse=(to.parent?.getWorldQuaternion(new THREE.Quaternion())||new THREE.Quaternion()).invert();
    const values=new Float32Array(track.values.length),q=new THREE.Quaternion();
    for(let i=0;i<values.length;i+=4){
     q.fromArray(track.values,i).premultiply(fromParent).multiply(fromRestInverse).multiply(toRest).premultiply(toParentInverse).normalize().toArray(values,i);
    }
    tracks.push(new THREE.QuaternionKeyframeTrack(`${to.name}.quaternion`,track.times.slice(),values,track.getInterpolation()));
   }else if(role==='Hips'&&binding.propertyName==='position'){
    // Convert displacement through both parents, including centimetre/metre scale.
    const fromMatrix=new THREE.Matrix3().setFromMatrix4(from.parent?.matrixWorld||new THREE.Matrix4());
    const toInverse=new THREE.Matrix3().setFromMatrix4(to.parent?.matrixWorld||new THREE.Matrix4()).invert();
    const values=new Float32Array(track.values.length),v=new THREE.Vector3();
    for(let i=0;i<values.length;i+=3){
     v.fromArray(track.values,i).sub(from.position).applyMatrix3(fromMatrix).multiplyScalar(ratio);
     if(inPlace){v.x=0;v.z=0;}
     v.applyMatrix3(toInverse).add(to.position).toArray(values,i);
    }
    tracks.push(new THREE.VectorKeyframeTrack(`${to.name}.position`,track.times.slice(),values,track.getInterpolation()));
   }
  }
  if(!tracks.some(track=>track.name.endsWith('.quaternion')))throw new Error(`В анимации «${clip.name}» нет совместимых поворотов humanoid-костей.`);
  return new THREE.AnimationClip(clip.name,clip.duration,tracks,clip.blendMode);
 });
}
