import {writeFile} from 'node:fs/promises';
import * as Three from '../engine-web-prototype/frontend/node_modules/three/build/three.module.js';

const Traces = [];
for (const [Name, Geometry] of [['TemplateBox',new Three.BoxGeometry(0.5,0.5,0.5)],['TemplateSphere',new Three.SphereGeometry(0.3,24,16)],['TemplateCylinder',new Three.CylinderGeometry(0.28,0.28,0.6,24)]]) {
  for (const Rotation of [[0,0,0],[0,-1.25,0],[0.3,0.7,-0.2]]) {
    const Center = [0.1,0.2,-0.3];
    const Scale = [2,1.5,0.4];
    const Mesh = new Three.Mesh(Geometry,new Three.MeshBasicMaterial());
    Mesh.position.fromArray(Center);
    Mesh.scale.fromArray(Scale);
    Mesh.rotation.set(...Rotation,'YXZ');
    Mesh.updateMatrixWorld(true);
    for (const Horizontal of [0,0.1,0.45,0.6]) {
      for (const Vertical of [0,0.2,0.4]) {
        const Origin = [Horizontal,Vertical,2];
        const Direction = [0,0,-1];
        const Hit = new Three.Raycaster(new Three.Vector3(...Origin),new Three.Vector3(...Direction),0,10).intersectObject(Mesh)[0];
        Traces.push({geometry:Name,origin:Origin,direction:Direction,center:Center,scale:Scale,rotation:Rotation,distance:Hit?.distance??null});
      }
    }
  }
}
await writeFile(new URL('../Root/Verification/WebTemplateOracle/picking.json',import.meta.url),JSON.stringify(Traces));
