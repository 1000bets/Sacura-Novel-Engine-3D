import {updateObjectHighlight} from './sceneEffects.js';
import React, { useEffect, useRef } from "react";
import * as THREE from "three";
import {createCameraRig} from './CameraRig.js';
import {createSceneNavigation} from './SceneNavigation.js';
import {createSceneGizmo,groupObjects,meshGeometry,cloneSceneObject} from './SceneGizmo.js';
import {BUILTIN_TRANSFORMS,resolvedPosition} from './sceneEditing.js';
import Scene from "./Scene.jsx";
import {atmosphere,gameCamera,decoratePaper,objectPosition,addCharacterDetails,animatePose} from './sceneEffects.js';

function Exterior({
  kind,
  objects,
  state,
  onSelect,
  onInteract,
  mode = "scene",
  showGrid = true, selected, selectedIds, sceneId, editTool, editSpace, snap, onTransform, onTransforms, focusRequest, editing=true, cameraScene, selectedCameraId, cameraPreviewId, cameraPilotId, onCameraChange, onCameraSelect, cameraApi, showCameras=true,
}) {
  const host = useRef(),
    live = useRef();
  live.current = { state, onSelect, onInteract, mode, showGrid,objects,selected,selectedIds,sceneId,kind,editTool,editSpace,snap,onTransform,onTransforms,focusRequest,editing,cameraScene,selectedCameraId,cameraPreviewId,cameraPilotId,onCameraChange,onCameraSelect,showCameras };
  useEffect(() => {
    let renderer;
    try {
      renderer = new THREE.WebGLRenderer({ antialias: true });
    } catch {
      host.current.textContent =
        "3D недоступно · используйте кнопки объектов ниже";
      return;
    }
    const element = host.current;
    renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    renderer.setClearColor("#18212a");renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.shadowMap.enabled=true;
    element.appendChild(renderer.domElement);
    const scene = new THREE.Scene(),
      camera = new THREE.PerspectiveCamera(58, 1, 0.05, 400);
    camera.position.set(...gameCamera(kind,{})[0]);
    const controls = createSceneNavigation(camera, renderer.domElement,()=>live.current);
    controls.target.set(...gameCamera(kind,{})[1]);camera.lookAt(controls.target);
    const hemi=new THREE.HemisphereLight(0xc5d7ea, 0x253831, 2);scene.add(hemi);
    const sun = new THREE.DirectionalLight(0xffd5ac, 2);
    sun.position.set(-4, 9, 3);
    scene.add(sun);
    const picks = [];
    const box = (w, h, d, x, y, z, color, id) => {
      const mesh = new THREE.Mesh(
        new THREE.BoxGeometry(w, h, d),
        new THREE.MeshStandardMaterial({ color, roughness: 0.8 }),
      );
      mesh.position.set(x, y, z);
      mesh.castShadow=true;mesh.receiveShadow=true;
      scene.add(mesh);
      if (id) {
        mesh.userData.id = id;
        picks.push(mesh);
      }
      return mesh;
    };
    const prop=(id,build)=>{const start=scene.children.length;build();const group=groupObjects(scene,scene.children.slice(start),id,BUILTIN_TRANSFORMS[id]);picks.push(group);return group;};
    if (kind === "garden") {
      prop('garden-ground',()=>box(70, 0.2, 70, 0, -0.15, 0, "#3c544b"));
      prop('garden-path',()=>box(1.6, 0.025, 35, 0, 0, -10, "#899286"));
      let treeIndex=0;
      for (const [x, z] of [
        [-3, -2],
        [3, -2],
        [-3, 2],
        [5, 1],[-5,-7],[0,-9],[5,-8],[-6,4],
      ]) {
        prop(`garden-tree-${++treeIndex}`,()=>{
        box(0.25, 2, 0.25, x, 0.9, z, "#776358");
        const leaves = new THREE.Mesh(
          new THREE.IcosahedronGeometry(1.35, 0),
          new THREE.MeshStandardMaterial({ color: "#527b68" }),
        );
        leaves.position.set(x, 2.7, z);
        scene.add(leaves);
        });
      }
      const benchStart=scene.children.length;
      box(2, 0.18, 0.6, 2, 0.65, 0.5, "#a49076");
      box(2, 0.65, 0.12, 2, 1, 0.16, "#887861");
      for (const x of [1.25, 2.75]) box(0.12, 0.6, 0.4, x, 0.3, 0.5, "#6c6b68");
      picks.push(groupObjects(scene,scene.children.slice(benchStart),"garden-bench",BUILTIN_TRANSFORMS["garden-bench"]));
      decoratePaper(box(0.4, 0.018, 0.25, 2, 0.77, 0.4, "#efdfb7", "garden-note"),'Записка · осмотреть');
      prop('garden-fence',()=>box(3, 0.6, 0.2, -2, 0.3, -3, "#6e8272"));
    } else {
      prop('station-platform',()=>box(70, 0.2, 35, 0, -0.1, 0, "#687575"));
      prop('station-tracks',()=>{
      for (const z of [-1.6, -2.4]) box(10, 0.07, 0.08, 0, 0.06, z, "#b7b6ab");
      for (let x = -4; x < 5; x += 0.7)
        box(0.12, 0.06, 1, x, 0.02, -2, "#5c534e");
      });
      const trainStart=scene.children.length;
      box(7, 1.8, 1.5, 0, 1, -2, "#638c8c");
      for (let x = -2.5; x < 3; x += 1.2)
        box(0.8, 0.65, 0.05, x, 1.2, -1.22, "#e3c58d");
      picks.push(groupObjects(scene,scene.children.slice(trainStart),"station-train",BUILTIN_TRANSFORMS["station-train"]));
      prop('station-canopy',()=>{
      box(5, 0.14, 2, 0, 2.8, 1, "#9aaba3");
      for (const x of [-2, 2]) box(0.12, 2.8, 0.12, x, 1.4, 1.4, "#829a91");
      });
      prop('station-luggage',()=>box(0.65, 0.8, 0.4, 1.4, 0.4, 1.2, "#aaa39a"));
      decoratePaper(box(0.4, 0.018, 0.26, 1.4, 0.83, 1.2, "#e6d6b5", "ticket"),'Билет · предъявить');
    }
    const chars = [];
    objects
      .filter((o) => o.type === "Персонаж")
      .forEach((o, i) => {
        const g = new THREE.Group(),
          mat = new THREE.MeshStandardMaterial({ color: o.color });
        const body = new THREE.Mesh(
          new THREE.CapsuleGeometry(0.23, 0.55, 4, 12),
          mat,
        );
        body.position.y = 0.7;
        const head = new THREE.Mesh(
          new THREE.SphereGeometry(0.19, 16, 12),
          new THREE.MeshStandardMaterial({ color: "#dfc6af" }),
        );
        head.position.y = 1.26;
        g.add(body, head);
        addCharacterDetails(g,mat);
        g.position.set(-0.6 + i * 1.3, 0, 0.4);
        g.userData.id = o.id;
        scene.add(g);
        picks.push(g);
        chars.push(g);
      });
    objects.filter(o=>o.builtin&&!picks.some(m=>m.userData.id===o.id)).forEach(o=>{const source=picks.find(m=>m.userData.id===o.builtin);if(source){const copy=cloneSceneObject(source,o.id);scene.add(copy);picks.push(copy);}});
    objects
      .filter(
        (o) => !picks.some(m=>m.userData.id===o.id)&&
          o.type !== "Персонаж" &&
          !["garden-note", "ticket", "letter", "door", "fireplace", "room-table", "room-sofa", "garden-bench", "station-train"].includes(
            o.id,
          ),
      )
      .forEach((o) =>
        (()=>{const mesh=box(0.5, 0.5, 0.5, 0, 0.25, 1, o.color || "#a3969f", o.id);mesh.geometry.dispose();mesh.geometry=meshGeometry(o.primitive);mesh.userData.primitive=o.primitive||"box";return mesh;})(),
      );
    const grid = new THREE.GridHelper(20, 20, 0x344d4d, 0x293b3d);
    grid.position.y = -0.25;
    scene.add(grid);
    const rainPoints = [];
    for (let i = 0; i < 65; i++) {
      const x = Math.random() * 8 - 4,
        y = Math.random() * 4,
        z = Math.random() * 6 - 3;
      rainPoints.push(x, y, z, x - 0.04, y - 0.18, z);
    }
    const rainGeo = new THREE.BufferGeometry();
    rainGeo.setAttribute(
      "position",
      new THREE.Float32BufferAttribute(rainPoints, 3),
    );
    const rain = new THREE.LineSegments(
      rainGeo,
      new THREE.LineBasicMaterial({
        color: 0xa9c0d8,
        transparent: true,
        opacity: 0.3,
      }),
    );
    const updateAtmosphere=atmosphere(scene,{kind,renderer,sun,hemi});
    const ro = new ResizeObserver(() => {
      const { width, height } = element.getBoundingClientRect();
      if (width < 1 || height < 1) return;
      renderer.setSize(width, height);
      camera.aspect = width / Math.max(height, 1);
      camera.updateProjectionMatrix();
    });
    ro.observe(element);
    const gizmo=createSceneGizmo(scene,camera,renderer.domElement,controls,()=>live.current,()=>picks);
    const cameraRig=createCameraRig(scene,camera,controls,renderer.domElement,()=>live.current);if(cameraApi)cameraApi.current=cameraRig;
    let down;
    const ray = new THREE.Raycaster();
    const press = (e) => {down=e.button===0&&!e.altKey?[e.clientX,e.clientY]:null;renderer.domElement.closest(".scene-viewport")?.focus({preventScroll:true});};
    const click = (e) => {
      if (e.button!==0 || e.altKey || controls.blocksClick() || cameraRig.blocksClick() || gizmo.blocksClick() || !down || Math.hypot(e.clientX - down[0], e.clientY - down[1]) > 5)
        return;
      const r = element.getBoundingClientRect();
      ray.setFromCamera(
        new THREE.Vector2(
          ((e.clientX - r.left) / r.width) * 2 - 1,
          (-(e.clientY - r.top) / r.height) * 2 + 1,
        ),
        camera,
      );
      const cameraHit=cameraRig.pick(ray);if(cameraHit){live.current.onCameraSelect?.(cameraHit);return;}
      let hit = ray.intersectObjects(
        picks.filter((m) => m.visible),
        true,
      ).find(h=>{let o=h.object;while(o){if(!o.visible)return false;o=o.parent;}return true;})?.object;
      if (hit) {
        while (!hit.userData.id && hit.parent) hit = hit.parent;
        if (live.current.mode === "game")
          live.current.onInteract?.(hit.userData.id);
        else live.current.onSelect?.(hit.userData.id,{additive:e.shiftKey||e.ctrlKey||e.metaKey});
      }else if(live.current.mode==='game')live.current.onInteract?.(null);else live.current.onSelect?.(null,{additive:e.shiftKey||e.ctrlKey||e.metaKey});
    };
    renderer.domElement.addEventListener("pointerdown", press);
    renderer.domElement.addEventListener("pointerup", click);
    let frame, lastMode, editorCamera, lastCamera,previous=performance.now(),cameraReady=false;
    const render = () => {
      frame = requestAnimationFrame(render);
      const { state, mode, showGrid,objects } = live.current;
      const now=performance.now(),dt=Math.min(.05,(now-previous)/1000);previous=now;const animTime=updateAtmosphere(state,dt);
      grid.visible = showGrid && mode === "scene";
      const anchors = {
        камин: [-2, 0, 0.3],
        окно: [1, 0, -0.7],
        стол: [-0.6, 0, 0.5],
        диван: [2, 0, 0.4],
      };
      picks.forEach((mesh) => {
        const o = objects.find((o) => o.id === mesh.userData.id);
        if (!o) {
          mesh.visible = false;
          return;
        }
        mesh.visible = o.active && state.visible?.[o.id] !== false;
        if(mesh.userData.marker)mesh.userData.marker.visible=state.interactionTarget===o.id||state.highlights?.[o.id];
        gizmo.apply(mesh,o,resolvedPosition(o,state,kind));
        updateObjectHighlight(mesh,state.interactionTarget===o.id||!!state.highlights?.[o.id],animTime);
        if(mode==='scene'&&live.current.editing)animatePose(mesh,null,0,false,true);
        else animatePose(mesh,state.poses?.[o.id],animTime,!!state.motions?.[o.id]&&!state.motions[o.id].stopped&&!state.motions[o.id].paused&&state.motions[o.id].progress<1,state.paused);

      });
      gizmo.update();
      cameraRig.update(dt,gizmo.dragging);
      controls.tick(dt);
      renderer.render(scene, camera);
    };
    render();
    return () => {
      cancelAnimationFrame(frame);
      ro.disconnect();
      if(cameraApi?.current===cameraRig)cameraApi.current=null;
      cameraRig.dispose();
      gizmo.dispose();
      controls.dispose();
      renderer.domElement.removeEventListener('pointerdown',press);
      renderer.domElement.removeEventListener('pointerup',click);
      scene.traverse((o) => {
        o.geometry?.dispose();
        if(Array.isArray(o.material))o.material.forEach(material=>material.dispose());else o.material?.dispose();
      });
      renderer.dispose();
      renderer.domElement.remove();
    };
  }, [kind, objects.map(o=>o.id+o.type).join('|')]);
  return (
    <div
      ref={host}
      className="scene-canvas"
      aria-label={
        (kind === "garden" ? "3D сад" : "3D станция") + '. ЛКМ — выбор; Alt + ЛКМ — вращение; средняя кнопка — панорама; ПКМ + WASD/QE — полёт; колесо — приближение; F — фокус.'
      }
    />
  );
}
export default function LocationScene(props) {
  return (props.kind || props.state.location) === "living" ? (
    <Scene {...props} onLetter={() => props.onInteract?.("letter")} />
  ) : (
    <Exterior {...props} kind={props.kind || props.state.location} />
  );
}
