import React, { useEffect, useRef } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import Scene from "./Scene.jsx";

function Exterior({
  kind,
  objects,
  state,
  onSelect,
  onInteract,
  mode = "scene",
  showGrid = true,
}) {
  const host = useRef(),
    live = useRef();
  live.current = { state, onSelect, onInteract, mode, showGrid };
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
    renderer.setClearColor("#18212a");
    element.appendChild(renderer.domElement);
    const scene = new THREE.Scene(),
      camera = new THREE.PerspectiveCamera(40, 1, 0.1, 100);
    camera.position.set(8, 7, 10);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.target.set(0, 0.6, 0);
    controls.enableDamping = true;
    controls.enablePan = false;
    controls.maxPolarAngle = 1.45;
    scene.add(new THREE.HemisphereLight(0xc5d7ea, 0x253831, 2));
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
      scene.add(mesh);
      if (id) {
        mesh.userData.id = id;
        picks.push(mesh);
      }
      return mesh;
    };
    if (kind === "garden") {
      box(9, 0.2, 7, 0, -0.15, 0, "#3c544b");
      box(1.6, 0.025, 7, 0, 0, 0, "#899286");
      for (const [x, z] of [
        [-3, -2],
        [3, -2],
        [-3, 2],
        [3, 2],
      ]) {
        box(0.25, 2, 0.25, x, 0.9, z, "#776358");
        const leaves = new THREE.Mesh(
          new THREE.IcosahedronGeometry(1.35, 0),
          new THREE.MeshStandardMaterial({ color: "#527b68" }),
        );
        leaves.position.set(x, 2.7, z);
        scene.add(leaves);
      }
      box(2, 0.18, 0.6, 2, 0.65, 0.5, "#a49076");
      box(2, 0.65, 0.12, 2, 1, 0.8, "#887861");
      box(0.4, 0.018, 0.25, 2, 0.76, 0.4, "#efdfb7", "garden-note");
      for (const x of [1.25, 2.75]) box(0.12, 0.6, 0.4, x, 0.3, 0.5, "#6c6b68");
      box(3, 0.6, 0.2, -2, 0.3, -3, "#6e8272");
    } else {
      box(9, 0.2, 6, 0, -0.1, 0, "#687575");
      for (const z of [-1.6, -2.4]) box(10, 0.07, 0.08, 0, 0.06, z, "#b7b6ab");
      for (let x = -4; x < 5; x += 0.7)
        box(0.12, 0.06, 1, x, 0.02, -2, "#5c534e");
      box(7, 1.8, 1.5, 0, 1, -2, "#638c8c");
      for (let x = -2.5; x < 3; x += 1.2)
        box(0.8, 0.65, 0.05, x, 1.2, -1.22, "#e3c58d");
      box(5, 0.14, 2, 0, 2.8, 1, "#9aaba3");
      for (const x of [-2, 2]) box(0.12, 2.8, 0.12, x, 1.4, 1.4, "#829a91");
      box(0.65, 0.8, 0.4, 1.4, 0.4, 1.2, "#aaa39a");
      box(0.4, 0.018, 0.26, 1.4, 0.82, 1.2, "#e6d6b5", "ticket");
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
        g.position.set(-0.6 + i * 1.3, 0, 0.4);
        g.userData.id = o.id;
        scene.add(g);
        picks.push(g);
        chars.push(g);
      });
    objects
      .filter(
        (o) =>
          o.type !== "Персонаж" &&
          !["garden-note", "ticket", "letter", "door", "fireplace"].includes(
            o.id,
          ),
      )
      .forEach((o) =>
        box(0.5, 0.5, 0.5, 0, 0.25, 1, o.color || "#a3969f", o.id),
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
    scene.add(rain);
    const ro = new ResizeObserver(() => {
      const { width, height } = element.getBoundingClientRect();
      if (width < 1 || height < 1) return;
      renderer.setSize(width, height);
      camera.aspect = width / Math.max(height, 1);
      camera.updateProjectionMatrix();
    });
    ro.observe(element);
    let down;
    const ray = new THREE.Raycaster();
    const press = (e) => (down = [e.clientX, e.clientY]);
    const click = (e) => {
      if (!down || Math.hypot(e.clientX - down[0], e.clientY - down[1]) > 5)
        return;
      const r = element.getBoundingClientRect();
      ray.setFromCamera(
        new THREE.Vector2(
          ((e.clientX - r.left) / r.width) * 2 - 1,
          (-(e.clientY - r.top) / r.height) * 2 + 1,
        ),
        camera,
      );
      let hit = ray.intersectObjects(
        picks.filter((m) => m.visible),
        true,
      )[0]?.object;
      if (hit) {
        while (!hit.userData.id && hit.parent) hit = hit.parent;
        if (live.current.mode === "game")
          live.current.onInteract?.(hit.userData.id);
        else live.current.onSelect?.(hit.userData.id);
      }
    };
    renderer.domElement.addEventListener("pointerdown", press);
    renderer.domElement.addEventListener("pointerup", click);
    let frame, lastMode, editorCamera, lastCamera;
    const render = () => {
      frame = requestAnimationFrame(render);
      const { state, mode, showGrid } = live.current;
      if (lastMode !== mode) {
        if (lastMode === "scene")
          editorCamera = {
            position: camera.position.clone(),
            target: controls.target.clone(),
          };
        if (mode === "scene" && editorCamera) {
          camera.position.copy(editorCamera.position);
          controls.target.copy(editorCamera.target);
        }
        controls.enabled = mode === "scene";
        lastMode = mode;
        lastCamera = null;
      }
      if (mode === "game" && lastCamera !== state.camera) {
        camera.position.set(
          ...(state.camera === "Крупный план" ? [4, 3.6, 6] : [8, 7, 10]),
        );
        controls.target.set(0, 0.6, 0);
        camera.lookAt(controls.target);
        lastCamera = state.camera;
      }
      if (controls.enabled) controls.update();
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
        if (mesh.material) {
          mesh.material.emissive.set(
            state.interactionTarget === o.id ? "#967554" : "#000000",
          );
          mesh.material.emissiveIntensity = 0.8;
        }
        if (!["garden-note", "ticket"].includes(o.id)) {
          const position = anchors[state.positions?.[o.id] || o.position];
          if (position) mesh.position.set(...position);
          mesh.rotation.y = state.poses?.[o.id] === "задумчивость" ? 0.4 : 0;
        }
      });
      sun.intensity = live.current.state.time === "Ночь" ? 0.7 : 2.6;
      rain.visible = live.current.state.weather !== "Ясно";
      if (!state.paused && !state.weatherPaused)
        rain.position.y = (-(Date.now() % 900) / 900) * 0.2;
      renderer.render(scene, camera);
    };
    render();
    return () => {
      cancelAnimationFrame(frame);
      ro.disconnect();
      controls.dispose();
      scene.traverse((o) => {
        o.geometry?.dispose();
        o.material?.dispose();
      });
      renderer.dispose();
      renderer.domElement.remove();
    };
  }, [kind, objects]);
  return (
    <div
      ref={host}
      className="scene-canvas"
      aria-label={
        kind === "garden"
          ? "3D сад с запиской на скамье"
          : "3D станция с билетом"
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
