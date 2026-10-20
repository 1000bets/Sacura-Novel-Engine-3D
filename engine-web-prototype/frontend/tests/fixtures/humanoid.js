import * as THREE from 'three';

export const joints=[
 ['Hips',null,[0,100,0]],['Spine',0,[0,15,0]],['Spine1',1,[0,15,0]],['Head',2,[0,25,0]],
 ...['Left','Right'].flatMap((side,index)=>{
  const offset=4+index*6,x=index?-1:1;
  return [[`${side}Arm`,2,[x*15,10,0]],[`${side}ForeArm`,offset,[x*25,0,0]],[`${side}Hand`,offset+1,[x*20,0,0]],
   [`${side}UpLeg`,0,[x*10,0,0]],[`${side}Leg`,offset+3,[0,-45,0]],[`${side}Foot`,offset+4,[0,-45,0]]];
 }),
];
export function humanoid(prefix='mixamorig',scale=1){
 const root=new THREE.Group(),bones=joints.map(([name,,position])=>{const bone=new THREE.Bone();bone.name=prefix+name;bone.position.fromArray(position).multiplyScalar(scale);return bone;});
 joints.forEach(([,parent],i)=>(parent===null?root:bones[parent]).add(bones[i]));root.updateMatrixWorld(true);
 return {root,bones};
}
export function mixamoFbx({animated=true,texture=false}={}){
 const models=joints.map(([name,,position],i)=>`\tModel: ${i+1}, "Model::mixamorig:${name}", "LimbNode" {\n\t\tProperties70:  {\n\t\t\tP: "Lcl Translation", "Lcl Translation", "", "A",${position.join(',')}\n\t\t}\n\t}`).join('\n');
 const connections=joints.map(([,parent],i)=>`\tC: "OO",${i+1},${parent===null?0:parent+1}`).join('\n');
 const curves=['X','Y','Z'].map((axis,i)=>`\tAnimationCurve: ${101+i*10}, "AnimCurve::${axis}", "" {\n\t\tKeyTime: *2 {\n\t\t\ta: 0,46186158000\n\t\t}\n\t\tKeyValueFloat: *2 {\n\t\t\ta: 0,${i===0?90:0}\n\t\t}\n\t}`).join('\n');
 const text=`; FBX 7.4.0 project file\nFBXHeaderExtension:  {\n\tFBXVersion: 7400\n}\nObjects:  {\n${models}\n${texture?`\tVideo: 200, "Video::ignored", "Clip" {\n\t\tFilename: "https://example.invalid/texture.png"\n\t}\n\tTexture: 201, "Texture::ignored", "" {\n\t\tFileName: "texture.png"\n\t}`:''}
${animated?`\tAnimationCurveNode: 100, "AnimCurveNode::R", "" {\n\t}\n${curves}\n\tAnimationLayer: 102, "AnimLayer::BaseLayer", "" {\n\t}\n\tAnimationStack: 103, "AnimStack::Wave", "" {\n\t}`:''}
}\nConnections:  {\n${connections}\n${texture?'\tC: "OO",200,201':''}
${animated?'\tC: "OP",100,5,"Lcl Rotation"\n\tC: "OP",101,100,"d|X"\n\tC: "OP",111,100,"d|Y"\n\tC: "OP",121,100,"d|Z"\n\tC: "OO",100,102\n\tC: "OO",102,103':''}
}\n`;
 return new TextEncoder().encode(";"+"-".repeat(250)+"\n"+text).buffer;
}
export function humanoidGlb({skinned=true}={}){
 const {bones}=humanoid('',.01),chunks=[],bufferViews=[],accessors=[];let offset=0;
 function accessor(array,type,componentType,count,extra={}){
  const bytes=new Uint8Array(array.buffer,array.byteOffset,array.byteLength);chunks.push({offset,bytes});
  const view=bufferViews.push({buffer:0,byteOffset:offset,byteLength:bytes.length})-1;offset+=Math.ceil(bytes.length/4)*4;
  return accessors.push({bufferView:view,componentType,count,type,...extra})-1;
 }
 const position=accessor(new Float32Array([-.1,0,0,.1,0,0,0,1.6,0]),'VEC3',5126,3,{min:[-.1,0,0],max:[.1,1.6,0]});
 const weights=accessor(new Float32Array([1,0,0,0,1,0,0,0,1,0,0,0]),'VEC4',5126,3);
 const skinJoints=accessor(new Uint16Array(12),'VEC4',5123,3);
 const inverse=accessor(new Float32Array(bones.flatMap(b=>b.matrixWorld.clone().invert().toArray())),'MAT4',5126,bones.length);
 const nodes=joints.map(([name,,p],i)=>({name,translation:p.map(v=>v*.01),children:joints.flatMap(([,parent],j)=>parent===i?[j]:[])}));nodes.push({name:'Body',mesh:0,...(skinned?{skin:0}:{})});
 const json={asset:{version:'2.0'},scene:0,scenes:[{nodes:[0,nodes.length-1]}],nodes,meshes:[{primitives:[{attributes:{POSITION:position,WEIGHTS_0:weights,JOINTS_0:skinJoints}}]}],skins:[{joints:bones.map((_,i)=>i),inverseBindMatrices:inverse}],buffers:[{byteLength:offset}],bufferViews,accessors};
 const text=new TextEncoder().encode(JSON.stringify(json)),length=Math.ceil(text.length/4)*4,buffer=new ArrayBuffer(28+length+offset),view=new DataView(buffer);
 view.setUint32(0,0x46546c67,true);view.setUint32(4,2,true);view.setUint32(8,buffer.byteLength,true);view.setUint32(12,length,true);view.setUint32(16,0x4e4f534a,true);
 new Uint8Array(buffer,20,length).fill(32);new Uint8Array(buffer,20,text.length).set(text);view.setUint32(20+length,offset,true);view.setUint32(24+length,0x004e4942,true);
 for(const chunk of chunks)new Uint8Array(buffer,28+length+chunk.offset,chunk.bytes.length).set(chunk.bytes);
 return buffer;
}
