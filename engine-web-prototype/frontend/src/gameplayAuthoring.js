import {uid} from './model.js';
import {createCharacter,setCharacterInScene} from './characterModel.js';
import {applyGamePreset,newRule} from './gameplayModel.js';
export function prepareGameplayScene(p,preset,sceneId){if(preset==='fps'&&!p.objects.length){const empty=p.subscenes.find(s=>s.id===sceneId)||p.subscenes[0];empty.kind='empty';empty.navMesh={enabled:true,show:false,center:[0,0,0],size:[16,20]};p.objects.push({id:uid('floor'),name:'Пол игровой сцены',type:'Меш',primitive:'box',active:true,color:'#404b5c',subsceneId:empty.id,collision:{enabled:false},transforms:{[empty.id]:{position:[0,-.1,0],rotation:[0,0,0],scale:[32,.4,40]}}});}const scene=p.subscenes.find(s=>s.id===sceneId)||p.subscenes[0],hero=p.objects.find(o=>o.id===p.gameplay.playerId)||p.objects.find(o=>o.type==='Персонаж'&&o.subsceneId===scene.id)||createCharacter(p,scene.id,'Главный герой');setCharacterInScene(p,hero.id,scene.id,true);hero.active=true;applyGamePreset(p,preset,hero.id);hero.collision={enabled:true,custom:true,size:[.6,1.6,.6],offset:[0,.8,0]};const chapter=p.chapters.find(c=>c.subsceneId===scene.id)||p.chapters[0];let playable=chapter.beats.find(b=>b.kind==='gameplay');if(!playable){playable={id:uid('play'),kind:'gameplay',speaker:'',text:preset==='quest'?'Найдите предметы и решите загадку.':'Победите противников. WASD — движение, мышь — огонь, R — перезарядка, пробел — прыжок, E — предмет, Esc — пауза.',next:null,bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]},mode:'SEQUENTIAL'};chapter.beats.push(playable);scene.entry=playable.id;}if(preset!=='quest'&&!p.gameplay.rules.some(r=>r.trigger==='death'&&r.objectId===hero.id)){const end={id:uid('gameover'),kind:'end',speaker:'',text:'Game Over',ending:'Game Over',bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]},mode:'SEQUENTIAL',next:null};chapter.beats.push(end);p.gameplay.rules.push({...newRule(),name:'Смерть героя → Game Over',trigger:'death',sceneId:scene.id,objectId:hero.id,nextBeatId:end.id});}scene.entry=playable.id;p.gameplay.hudWidgetId=p.ui.hud;return playable.id;}
export function addGameplayEnemy(p,sceneId){
 const scene=p.subscenes.find(s=>s.id===sceneId);if(!scene||!p.gameplay.enabled)throw new Error('Сначала подготовьте игровую сцену.');
 const count=Object.keys(p.gameplay.actors).filter(id=>p.gameplay.actors[id].team==='enemy').length;
 const enemy=createCharacter(p,scene.id,'Противник '+(count+1));
 enemy.color='#bc596a';enemy.collision={enabled:true,custom:true,size:[.6,1.6,.6],offset:[0,.8,0]};
 const hero=p.objects.find(o=>o.id===p.gameplay.playerId),origin=hero?.transforms?.[scene.id]?.position||[0,0,0];
 enemy.transforms[scene.id].position=[origin[0]+[0,-2,2][count%3],origin[1],origin[2]-5-Math.floor(count/3)*2];
 p.gameplay.actors[enemy.id]={team:'enemy',maxHP:50,ai:{speed:1,range:1.6,damage:10,interval:2,detection:20}};
 return enemy.id;
}
export function addGameplayVictory(p,sceneId){
 const scene=p.subscenes.find(s=>s.id===sceneId),chapter=p.chapters.find(c=>c.subsceneId===sceneId);
 if(!scene||!chapter||!p.gameplay.enabled)throw new Error('Сначала подготовьте игровую сцену.');
 const existing=p.gameplay.rules.find(r=>r.trigger==='victory'&&r.sceneId===sceneId);if(existing)return existing.nextBeatId;
 const end={id:uid('victory'),kind:'end',speaker:'',text:'Все противники побеждены!',ending:'Победа',bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]},mode:'SEQUENTIAL',next:null};
 chapter.beats.push(end);p.gameplay.rules.push({...newRule(),name:'Все противники побеждены → Победа',trigger:'victory',sceneId,nextBeatId:end.id});return end.id;
}
