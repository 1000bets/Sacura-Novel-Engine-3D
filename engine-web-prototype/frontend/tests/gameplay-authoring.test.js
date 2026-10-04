import test from 'node:test';
import assert from 'node:assert/strict';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {prepareGameplayScene,addGameplayEnemy,addGameplayVictory} from '../src/gameplayAuthoring.js';
import {readProject} from '../src/projectFiles.js';
import {validateStudio} from '../src/studioModel.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
const runtime=()=>new PreviewRuntime(new SoundDesk(()=>({play:()=>Promise.resolve(),pause(){},addEventListener(){},removeEventListener(){},duration:1,currentTime:0})));
const settle=async rt=>{for(let i=0;i<100&&rt.game.flights.size;i++)await new Promise(r=>setTimeout(r,10));assert.equal(rt.snapshot.error,null);};
function build(){const p=createEmptyProject('FPS из браузера'),scene=p.subscenes[0];prepareGameplayScene(p,'fps',scene.id);const enemy=addGameplayEnemy(p,scene.id),win=addGameplayVictory(p,scene.id);return {p,scene,enemy,win};}
test('browser authoring buttons create a portable FPS from an empty project with clean diagnostics',()=>{const {p,scene,enemy,win}=build();assert.equal(p.chapters[0].beats.find(b=>b.id===scene.entry).kind,'gameplay');assert.equal(p.gameplay.controller.eyeHeight,1.25);assert.equal(p.gameplay.actors[enemy].maxHP,50);assert.ok(p.gameplay.actors[enemy].ai);assert.deepEqual(validateStudio(p),[]);assert.deepEqual(readProject(JSON.stringify(p)),p);const before=p.chapters[0].beats.length;assert.equal(addGameplayVictory(p,scene.id),win);prepareGameplayScene(p,'fps',scene.id);assert.equal(p.chapters[0].beats.length,before);assert.equal(p.gameplay.rules.filter(r=>r.trigger==='death').length,1);});
test('FPS authored with browser tools can shoot an enemy and enter the native victory ending',async()=>{const {p,scene,enemy,win}=build(),rt=runtime();try{await rt.start(p,scene.entry);const position=rt.game.position(enemy);for(let i=0;i<2;i++){assert.equal(rt.game.shoot({origin:[position[0],1.25,position[2]+2],direction:[0,0,-1]}),true);rt.game.state.clock+=.31;}await settle(rt);assert.equal(rt.snapshot.variables.kills,1);assert.equal(rt.snapshot.beatId,win);}finally{rt.stop();}});
test('FPS authored with browser tools takes enemy damage and reaches Game Over',async()=>{const {p,scene,enemy}=build(),rt=runtime();try{await rt.start(p,scene.entry);rt.snapshot.world.positions[enemy]=[0,0,-1];rt.game.tick(.05);assert.equal(rt.snapshot.variables.hp,90);rt.game.damage(p.gameplay.playerId,100);await settle(rt);const end=p.chapters[0].beats.find(b=>b.id===rt.snapshot.beatId);assert.equal(end.kind,'end');assert.equal(end.ending,'Game Over');}finally{rt.stop();}});
