import {updateObjectHighlight} from './sceneEffects.js';
import React, { useEffect, useRef } from "react";
import * as THREE from "three";
import {createSceneGizmo,groupObjects,meshGeometry,cloneSceneObject} from './SceneGizmo.js';
import {BUILTIN_TRANSFORMS,resolvedPosition} from './sceneEditing.js';
import {createCameraRig} from './CameraRig.js';
import {createSceneNavigation} from './SceneNavigation.js';
import {atmosphere,gameCamera,decoratePaper,objectPosition,addCharacterDetails,animatePose} from './sceneEffects.js';

export default function Scene({
  objects,
  state,
  selected, selectedIds,
  onSelect,
  onLetter,
  onInteract,
  mode = "scene",
  showGrid = true, sceneId="living", editTool, editSpace, snap, onTransform, onTransforms, focusRequest, editing=true, cameraScene, selectedCameraId, cameraPreviewId, cameraPilotId, onCameraChange, onCameraSelect, cameraApi, showCameras=true,
}) {
  const host = useRef(),
    api = useRef();
  const callbacks = useRef();
  callbacks.current = { onSelect, onLetter, onInteract, mode, state,objects,selected,selectedIds,sceneId,kind:"living",editTool,editSpace,snap,onTransform,onTransforms,focusRequest,editing,cameraScene,selectedCameraId,cameraPreviewId,cameraPilotId,onCameraChange,onCameraSelect,showCameras };
  useEffect(() => {
    const element = host.current;
    let renderer;
    try {
      renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    } catch {
      element.textContent =
        "3D-превью недоступно в этом браузере. Сценарий и редакторы продолжают работать.";
      return;
    }
    renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.setClearColor(0x191c22);
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.25;
    element.appendChild(renderer.domElement);
    const scene = new THREE.Scene();
    scene.fog = new THREE.Fog(0x191c22, 20, 40);
    const camera = new THREE.PerspectiveCamera(58, 1, 0.05, 400);
    camera.position.set(-2.8,1.65,2.8);
    const controls = createSceneNavigation(camera, renderer.domElement,()=>callbacks.current);
    controls.target.set(.3,1.02,-1.3);
    camera.lookAt(controls.target);
    const hemi=new THREE.HemisphereLight(0xbfcce7, 0x3d292d, 2);scene.add(hemi);
    const sunlight = new THREE.DirectionalLight(0xa5b8e9, 3);
    sunlight.position.set(-3, 9, -4);
    sunlight.castShadow = true;
    sunlight.shadow.mapSize.set(1024, 1024);
    scene.add(sunlight);
    const lamp = new THREE.PointLight(0xffa553, 35, 8);
    lamp.position.set(-2.7, 1.6, -1.5);
    scene.add(lamp);
    const meshes = [],
      characters = new Map(),
      pickables = [];
    const mat = (color, extra = {}) =>
      new THREE.MeshStandardMaterial({ color, roughness: 0.83, ...extra });
    const box = (w, h, d, x, y, z, color) => {
      const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat(color));
      m.position.set(x, y, z);
      m.castShadow = true;
      m.receiveShadow = true;
      scene.add(m);
      meshes.push(m);
      return m;
    };
    const prop=(id,build)=>{const start=scene.children.length;build();const group=groupObjects(scene,scene.children.slice(start),id,BUILTIN_TRANSFORMS[id]);pickables.push(group);return group;};
    prop('room-floor',()=>{
    box(9, 0.22, 6.5, 0, -0.15, 0, 0x49404a);
    for (let x = -4; x < 4.5; x += 0.45)
      box(0.014, 0.012, 6.5, x, -0.024, 0, 0x6c6263);
    });
    prop('room-back-wall',()=>{box(9, 3, 0.16, 0, 1.3, -3.2, 0x515c63);box(9, 0.08, 0.13, 0, 0.05, -3.05, 0x777c79);});
    prop('room-left-wall',()=>{box(0.16, 3, 6.5, -4.5, 1.3, 0, 0x515961);box(0.14, 0.08, 6.4, -4.35, 0.05, 0, 0x777c79);});
    prop('room-right-wall',()=>box(.16,3.3,6.5,4.5,1.45,0,0x666067));
    prop('room-ceiling',()=>box(9,.12,6.5,0,3.04,0,0x8b8180));
    // Window, simple mullions and rain; everything in the scene is a primitive.
    let glass;
    prop('room-window',()=>{
    box(2.35, 2.2, 0.13, 1.35, 1.53, -3.06, 0x242e3c);
    glass = box(2.08, 1.94, 0.04, 1.35, 1.53, -2.97, 0x7b9eac);
    glass.material.emissive = new THREE.Color(0x34424f);
    box(0.07, 2, 0.09, 1.35, 1.53, -2.9, 0x9ca4a3);
    box(2.15, 0.07, 0.09, 1.35, 1.55, -2.9, 0x9ca4a3);
    box(2.6, 0.14, 0.48, 1.35, 0.48, -2.9, 0x88918d);
    });
    const fireplaceStart=scene.children.length;
    box(1.45, 1.75, 0.72, -3.1, 0.85, -2.65, 0x8d7c74);
    box(1.68, 0.18, 0.94, -3.1, 1.67, -2.57, 0xa39485);
    box(0.95, 1, 0.05, -3.1, 0.64, -2.26, 0x29282b);
    for (let i = 0; i < 5; i++) {
      const flame = new THREE.Mesh(
        new THREE.ConeGeometry(
          0.09 + Math.random() * 0.06,
          0.4 + Math.random() * 0.2,
          5,
        ),
        mat(0xf4ae58, { emissive: 0xe6852f, emissiveIntensity: 1.3 }),
      );
      flame.position.set(-3.42 + i * 0.16, 0.43, -2.2);
      scene.add(flame);
    }
    const fireplace=groupObjects(scene,scene.children.slice(fireplaceStart),"fireplace",BUILTIN_TRANSFORMS.fireplace);pickables.push(fireplace);
    fireplace.attach(lamp);
    prop('room-rug',()=>{
    box(3, 0.18, 2.2, -0.6, 0.01, 0.8, 0x776370);
    for (let i = 0; i < 4; i++)
      box(3, 0.005, 0.023, -0.6, 0.11, 0.05 + i * 0.45, 0x99858c);
    });
    const sofaStart=scene.children.length;
    box(2.55, 0.4, 0.9, 2.8, 0.38, 0.6, 0x9e8175);
    box(2.55, 0.67, 0.26, 2.8, 0.74, 0.97, 0xa58c7b);
    box(0.24, 0.63, 0.95, 1.56, 0.58, 0.6, 0xb49b85);
    box(0.24, 0.63, 0.95, 4.04, 0.58, 0.6, 0xb49b85);
    for (let i = 0; i < 3; i++)
      box(0.72, 0.14, 0.7, 2.02 + i * 0.77, 0.63, 0.51, 0xc0a48e);
    const sofa=groupObjects(scene,scene.children.slice(sofaStart),"room-sofa",BUILTIN_TRANSFORMS["room-sofa"]);pickables.push(sofa);
    const tableStart=scene.children.length;
    box(1.8, 0.12, 1.05, -0.25, 0.71, 1.7, 0xa58568);
    for (const x of [-0.98, 0.48])
      for (const z of [1.3, 2.1]) box(0.09, 0.66, 0.09, x, 0.34, z, 0x605450);
    const table=groupObjects(scene,scene.children.slice(tableStart),"room-table",BUILTIN_TRANSFORMS["room-table"]);pickables.push(table);
    const letter = box(0.47, 0.012, 0.3, -0.35, 0.782, 1.55, 0xf4e4bd);
    letter.rotation.y = 0.18;
    letter.userData.id = "letter";
    decoratePaper(letter,'Письмо · осмотреть');
    pickables.push(letter);
    const door = box(1, 2.5, 0.08, 3.55, 1.13, -3.08, 0x718c87);
    door.geometry.translate(.5,0,0);door.position.x=3.05;
    door.userData.id = "door";
    pickables.push(door);
    door.attach(box(0.07, 0.07, 0.1, 3.88, 1, -2.99, 0xbda477));
    prop('room-cup',()=>box(0.23, 0.23, 0.23, 0.2, 0.85, 1.72, 0xadb6b3));
    prop('room-side-table',()=>{
    box(0.75, 0.12, 0.75, -3.65, 0.78, 0.85, 0x7a6655);
    box(0.15, 0.9, 0.15, -3.65, 0.38, 0.85, 0x605047);
    });
    prop('room-lamp',()=>{
    const shade = new THREE.Mesh(
      new THREE.ConeGeometry(0.32, 0.42, 12, 1, true),
      mat(0xd6b886, { side: THREE.DoubleSide }),
    );
    shade.position.set(-3.65, 1.73, 0.85);
    scene.add(shade);
    box(0.05, 0.68, 0.05, -3.65, 1.12, 0.85, 0xb49d7a);
    });
    const grid = new THREE.GridHelper(36, 36, 0x323842, 0x282d35);
    grid.position.y = -0.28;
    scene.add(grid);
    const ring = new THREE.Mesh(
      new THREE.RingGeometry(0.34, 0.365, 48),
      new THREE.MeshBasicMaterial({ color: 0xc4adff, side: THREE.DoubleSide }),
    );
    ring.rotation.x = -Math.PI / 2;
    ring.position.y = 0.02;
    scene.add(ring);
    const rainGeometry = new THREE.BufferGeometry();
    const drops = [];
    for (let i = 0; i < 90; i++) {
      let x = 0.4 + Math.random() * 1.9,
        y = 0.7 + Math.random() * 1.7;
      drops.push(x, y, -2.86, x - 0.035, y - 0.15, -2.86);
    }
    rainGeometry.setAttribute(
      "position",
      new THREE.Float32BufferAttribute(drops, 3),
    );
    const rain = new THREE.LineSegments(
      rainGeometry,
      new THREE.LineBasicMaterial({
        color: 0xcad3dd,
        transparent: true,
        opacity: 0.35,
      }),
    );
    const updateAtmosphere=atmosphere(scene,{kind:'living',renderer,sun:sunlight,hemi});
    const resize = () => {
      const { width, height } = element.getBoundingClientRect();
      if (width < 1 || height < 1) return;
      renderer.setSize(width, height);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
    };
    const ro = new ResizeObserver(resize);
    ro.observe(element);
    resize();
    const gizmo=createSceneGizmo(scene,camera,renderer.domElement,controls,()=>callbacks.current,()=>pickables);
    const cameraRig=createCameraRig(scene,camera,controls,renderer.domElement,()=>callbacks.current);if(cameraApi)cameraApi.current=cameraRig;
    const ray = new THREE.Raycaster(),
      mouse = new THREE.Vector2();
    let down;
    const pointerDown = (e) => {
      down = e.button===0&&!e.altKey ? [e.clientX, e.clientY] : null;
      renderer.domElement.closest(".scene-viewport")?.focus({preventScroll:true});
    };
    const click = (e) => {
      if (e.button!==0 || e.altKey || controls.blocksClick() || cameraRig.blocksClick() || gizmo.blocksClick() || !down || Math.hypot(e.clientX - down[0], e.clientY - down[1]) > 5)
        return;
      const r = element.getBoundingClientRect();
      mouse.set(
        ((e.clientX - r.left) / r.width) * 2 - 1,
        (-(e.clientY - r.top) / r.height) * 2 + 1,
      );
      ray.setFromCamera(mouse, camera);
      const cameraHit=cameraRig.pick(ray);if(cameraHit){callbacks.current.onCameraSelect?.(cameraHit);return;}
      const h = ray.intersectObjects(
        pickables.filter((m) => m.visible),
        true,
      ).find(h=>{let o=h.object;while(o){if(!o.visible)return false;o=o.parent;}return true;});
      if (h) {
        let o = h.object;
        while (!o.userData.id && o.parent) o = o.parent;
        if (o.userData.id) {
          if (callbacks.current.mode === "game") {
            if (callbacks.current.onInteract)
              callbacks.current.onInteract(o.userData.id);
            else if (o.userData.id === "letter") callbacks.current.onLetter?.();
          } else callbacks.current.onSelect?.(o.userData.id,{additive:e.shiftKey||e.ctrlKey||e.metaKey});
        }
      }else if(callbacks.current.mode==='game')callbacks.current.onInteract?.(null);else callbacks.current.onSelect?.(null,{additive:e.shiftKey||e.ctrlKey||e.metaKey});
    };
    renderer.domElement.addEventListener("pointerdown", pointerDown);
    renderer.domElement.addEventListener("pointerup", click);
    api.current = {
      scene,
      camera,
      controls,
      grid,
      door,
      characters,
      gizmo,
      pickables,
      ring,
      rain,
      glass,
      sunlight,
      letter,
      mat,
      box,
    };
    let frame,previous=performance.now(),animTime=0;
    const render = () => {
      frame = requestAnimationFrame(render);
      const now=performance.now(),dt=Math.min(.05,(now-previous)/1000);previous=now;

      const live = callbacks.current.state;
      animTime=updateAtmosphere(live,dt);
      pickables.forEach(mesh=>{const o=callbacks.current.objects.find(o=>o.id===mesh.userData.id);if(!o){mesh.visible=false;return;}mesh.visible=o.active&&live.visible?.[o.id]!==false;
        gizmo.apply(mesh,o,resolvedPosition(o,live,'living'));
        if((o.builtin||o.id)==='door'&&callbacks.current.mode==='game'){mesh.userData.openAngle=THREE.MathUtils.damp(mesh.userData.openAngle||0,live.doors?.[o.id]==='Открыть'?-1.25:0,3,live.paused||live.pausedDoors?.[o.id]?0:dt);mesh.rotation.y+=mesh.userData.openAngle;}
        if(mesh.userData.marker)mesh.userData.marker.visible=live.interactionTarget===o.id||live.highlights?.[o.id];
        updateObjectHighlight(mesh,!!live.highlights?.[o.id],animTime);
        if(callbacks.current.mode==='scene'&&callbacks.current.editing)animatePose(mesh,null,0,false,true);
        else animatePose(mesh,live.poses?.[o.id],animTime,!!live.motions?.[o.id]&&!live.motions[o.id].stopped&&!live.motions[o.id].paused&&live.motions[o.id].progress<1,live.paused);
      });
      gizmo.update();
      cameraRig.update(dt,gizmo.dragging);
      controls.tick(dt);
      letter.userData.marker.visible=live.interactionTarget==='letter'||live.highlights?.letter;
      ring.scale.setScalar(live.interactionTarget?1+Math.sin(animTime*3)*.06:1);
      lamp.intensity=live.lighting==='Выключить'?0:live.lighting==='Холодный свет'?12:35;
      lamp.color.set(live.lighting==='Холодный свет'?'#98beff':'#ffa553');
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
      renderer.domElement.removeEventListener('pointerdown',pointerDown);
      renderer.domElement.removeEventListener('pointerup',click);
      scene.traverse((o) => {
        o.geometry?.dispose();
        if (o.material)
          Array.isArray(o.material)
            ? o.material.forEach((m) => m.dispose())
            : o.material.dispose();
      });
      renderer.dispose();
      renderer.domElement.remove();
      api.current = null;
    };
  }, []);
  useEffect(() => {
    const a = api.current;
    if (!a) return;
    const coords = {
      камин: [-1.95, 0, -0.4],
      окно: [1.15, 0, -1.85],
      стол: [-0.9, 0, 1],
      диван: [2.7, 0, -0.4],
    };
    objects.forEach((o, i) => {
      if(o.builtin&&!a.pickables.some(m=>m.userData.id===o.id)){const original=a.pickables.find(m=>m.userData.id===o.builtin);if(original){const copy=cloneSceneObject(original,o.id);a.scene.add(copy);a.characters.set(o.id,copy);a.pickables.push(copy);}}
      if (o.type === "Персонаж" && !a.characters.has(o.id)) {
        const group = new THREE.Group();
        const body = new THREE.Mesh(
          new THREE.CapsuleGeometry(0.23, 0.65, 5, 12),
          a.mat(o.color),
        );
        body.position.y = 0.64;
        body.castShadow = true;
        const head = new THREE.Mesh(
          new THREE.SphereGeometry(0.18, 20, 12),
          a.mat(0xe1c6ad),
        );
        head.position.y = 1.26;
        head.castShadow = true;
        group.add(body, head);
        addCharacterDetails(group,body.material);
        group.userData.id = o.id;
        a.scene.add(group);
        a.characters.set(o.id, group);
        a.pickables.push(group);
      } else if (
        !a.pickables.some(m=>m.userData.id===o.id) &&
        !a.characters.has(o.id)
      ) {
        const m = a.box(0.5, 0.5, 0.5, 0, 0.25, 0, o.color || "#98b4ac");
        m.geometry.dispose();m.geometry=meshGeometry(o.primitive);m.userData.primitive=o.primitive||"box";
        m.userData.id = o.id;
        a.characters.set(o.id, m);
        a.pickables.push(m);
      }
      const mesh = a.characters.get(o.id);
      if (mesh) {
        mesh.visible = o.active && state.visible?.[o.id] !== false;

      }
    });
    a.characters.forEach((mesh, id) => {
      if (!objects.some((o) => o.id === id)) mesh.visible = false;
    });
    const highlighted=Object.keys(state.highlights||{}).find(id=>state.highlights[id]);
    const selectedMesh =
      a.characters.get(state.interactionTarget || highlighted || selected) ||
      ((state.interactionTarget || highlighted || selected) === "letter"
        ? a.letter
        : (state.interactionTarget || highlighted || selected) === "door"
          ? a.door
          : null);
    a.ring.visible =
      (mode === "scene" || !!state.interactionTarget || !!highlighted) && !!selectedMesh;
    a.ring.material.color.set(state.interactionTarget ? 0xe1c48d : 0xc69eb2);
    a.ring.position.y = state.interactionTarget === "letter" ? 0.8 : 0.02;
    a.grid.visible = showGrid && mode === "scene";
    a.door.visible =
      !!objects.find((o) => o.id === "door")?.active &&
      state.visible?.door !== false;
    if (selectedMesh) {
      a.ring.position.x = selectedMesh.position.x;
      a.ring.position.z = selectedMesh.position.z;
    }
    a.letter.visible =
      !!objects.find((o) => o.id === "letter")?.active &&
      state.visible?.letter !== false;
    a.rain.visible = false;
    a.glass.material.color.set(
      state.time === "Ночь"
        ? 0x465b77
        : state.time === "Рассвет"
          ? 0xcbb39c
          : 0x7b9eac,
    );

  }, [objects, state, selected, mode, showGrid]);
  return (
    <div
      className="scene-canvas"
      ref={host}
      aria-label="3D-сцена. ЛКМ — выбор; Alt + ЛКМ — вращение; средняя кнопка — панорама; ПКМ + WASD/QE — полёт; колесо — приближение; F — фокус."
    />
  );
}
