export const NativeAssetDragMimeType = 'application/x-sacura-native-asset';

export function GetNativeMeshPlacement(Asset) {
  const Mesh = Asset?.subassets?.find(Subasset => Subasset.typeId === 'StaticMesh');
  if (!Mesh) {
    return null;
  }
  return {assetId:Asset.id,subAssetId:Mesh.id};
}

const cross = (a,b) => [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const dot = (a,b) => a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
const normalize = a => {const length=Math.hypot(...a);return length>1e-8?a.map(value=>value/length):null;};

function cameraRotation(right,up,forward) {
  // Column-major camera world basis: right, up, backward.
  const [m00,m10,m20]=right,[m01,m11,m21]=up,[m02,m12,m22]=forward.map(value=>-value);
  let x,y,z,w,s;
  const trace=m00+m11+m22;
  if(trace>0){s=2*Math.sqrt(trace+1);w=s/4;x=(m21-m12)/s;y=(m02-m20)/s;z=(m10-m01)/s;}
  else if(m00>m11&&m00>m22){s=2*Math.sqrt(1+m00-m11-m22);w=(m21-m12)/s;x=s/4;y=(m01+m10)/s;z=(m02+m20)/s;}
  else if(m11>m22){s=2*Math.sqrt(1+m11-m00-m22);w=(m02-m20)/s;x=(m01+m10)/s;y=s/4;z=(m12+m21)/s;}
  else{s=2*Math.sqrt(1+m22-m00-m11);w=(m10-m01)/s;x=(m02+m20)/s;y=(m12+m21)/s;z=s/4;}
  return [x,y,z,w];
}

// Matches the native viewport's 60 degree vertical FOV and Y-up camera.
export function nativeScenePlacement(viewport,x,y,width,height) {
  const position=viewport?.cameraPosition,target=viewport?.cameraTarget;
  if(!Array.isArray(position)||!Array.isArray(target)||width<=0||height<=0)return null;
  const forward=normalize(target.map((value,index)=>value-position[index]));
  const right=forward&&normalize(cross(forward,[0,1,0]));
  if(!right)return null;
  const up=cross(right,forward),halfHeight=Math.tan(Math.PI/6);
  const ray=normalize(forward.map((value,index)=>value+right[index]*(2*x/width-1)*halfHeight*width/height+up[index]*(1-2*y/height)*halfHeight));
  const ground=Math.abs(ray[1])>1e-5?-position[1]/ray[1]:-1;
  const focusDistance=Math.hypot(...target.map((value,index)=>value-position[index]));
  const distance=ground>0&&ground<400?ground:Math.max(2,focusDistance)/Math.max(.01,dot(ray,forward));
  return {worldPosition:position.map((value,index)=>value+ray[index]*distance),cameraAspect:width/height,
    cameraTransform:{position:[...position],rotation:cameraRotation(right,up,forward),scale:[1,1,1]}};
}
