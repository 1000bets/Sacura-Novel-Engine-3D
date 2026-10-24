import {writeFile} from 'node:fs/promises';
import * as Three from '../engine-web-prototype/frontend/node_modules/three/build/three.module.js';

const Geometries = {
  TemplateTorso:new Three.CapsuleGeometry(0.23,0.55,4,12),
  TemplateHead:new Three.SphereGeometry(0.19,16,12),
  TemplateLeg:new Three.CapsuleGeometry(0.085,0.3,4,8),
  TemplateArm:new Three.CapsuleGeometry(0.075,0.38,4,8),
  TemplateEye:new Three.SphereGeometry(0.026,8,6),
  TemplateMouth:new Three.BoxGeometry(0.07,0.014,0.008),
  TemplateBox:new Three.BoxGeometry(0.5,0.5,0.5),
  TemplateSphere:new Three.SphereGeometry(0.3,24,16),
  TemplateCylinder:new Three.CylinderGeometry(0.28,0.28,0.6,24)
};
const Result = {};
for (const [Name,Geometry] of Object.entries(Geometries)) {
  Result[Name] = {
    positions:Array.from(Geometry.attributes.position.array),
    normals:Array.from(Geometry.attributes.normal.array),
    coordinates:Array.from(Geometry.attributes.uv.array),
    indices:Array.from(Geometry.index.array)
  };
}
await writeFile(new URL('../Root/Verification/WebTemplateOracle/geometry.json',import.meta.url),JSON.stringify(Result));
