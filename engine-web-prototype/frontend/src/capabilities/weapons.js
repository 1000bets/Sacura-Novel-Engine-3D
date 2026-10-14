import {registerCapability} from '../capabilityRegistry.js';
import {clamp} from '../gamePhysics.js';
import {operation,ref,length,finiteVector,session} from './shared.js';
function weaponFor(g,s,owner,weaponId){const id=weaponId||s.selectedWeapons[owner];return g.weapons.find(w=>w.id===id&&w.ownerId===owner);}
registerCapability({id:'sacura.weapon',initialize(){for(const w of this.config.weapons){if(!w.ownerId)continue;this.state.weapons[w.id]={ammo:w.magazine,reserve:w.reserve,nextShot:0,reloadUntil:0,enabled:true};this.state.selectedWeapons[w.ownerId]??=w.id;}},
 methods:{
 shoot(payload={},context={}){
  const s=this.state,ownerId=payload.ownerId||this.config.legacyOwnerId|| (this.config.weapons.length===1?this.config.weapons[0].ownerId:undefined),w=weaponFor(this.config,s,ownerId,payload.weaponId),a=w&&s.weapons[w.id];
  if(!w||!a||a.enabled===false||a.nextShot>s.clock||a.reloadUntil>s.clock)return false;
  if(a.ammo<=0){s.message='Нет патронов. Перезарядите оружие.';return false;}
  const p=this.position(ownerId);if(!p)return false;const cameraOwner=this.config.camera.targetId===ownerId;
  const origin=finiteVector(payload.origin)?payload.origin:[p[0],p[1]+(w.eyeHeight??this.config.cameraSettings.eyeHeight),p[2]],direction=finiteVector(payload.direction)?payload.direction:cameraOwner?[-Math.sin(s.yaw)*Math.cos(s.pitch),Math.sin(s.pitch),-Math.cos(s.yaw)*Math.cos(s.pitch)]:[0,0,-1],n=length(direction);if(!n)return false;
  const dir=direction.map(v=>v/n);a.ammo--;a.nextShot=s.clock+w.cooldown;if(cameraOwner)s.pitch=clamp(s.pitch+w.recoil,-1.5,1.5);
  // Commit ammo and explicit story bindings before any hit/death publication.
  this.sync();
  if(w.mode==='projectile')s.projectiles.push({id:w.id+':'+s.clock,position:[...origin],origin:[...origin],direction:dir,speed:w.projectileSpeed,remaining:w.range,damage:w.damage,owner:ownerId,sceneId:this.scene.id});
  else{const hit=this.raycast(origin,dir,w.range,[ownerId]);if(hit)this.damage(hit.id,w.damage,{...context,source:{type:'object',id:ownerId},sourceId:ownerId,initiatorId:ownerId});s.tracers.push({from:origin,to:hit?.point||origin.map((v,i)=>v+dir[i]*w.range),until:s.clock+.1});}
  this.host.emit();return true;
 },
 reload(ownerId=this.config.legacyOwnerId,weaponId){if(!ownerId&&this.config.weapons.length===1)ownerId=this.config.weapons[0].ownerId;const s=this.state,w=weaponFor(this.config,s,ownerId,weaponId),a=w&&s.weapons[w.id];if(w&&a&&a.enabled!==false&&this.object(ownerId)&&a.reserve>0&&a.ammo<w.magazine&&a.reloadUntil<=s.clock){a.reloadUntil=s.clock+Math.max(.001,w.reloadTime);a.reloadSceneId=this.scene.id;s.message='Перезарядка…';return true;}return false;}
 },commands:{shoot(v,c){return this.shoot({...v,ownerId:v.ownerId||v.target||this.config.legacyOwnerId},c);},reload(v){return this.reload(v.ownerId||v.target||this.config.legacyOwnerId,v.weaponId);}},
 cleanup(){for(const w of this.config.weapons){const a=this.state?.weapons[w.id];if(a&&(!this.object(w.ownerId)||a.reloadSceneId&&a.reloadSceneId!==this.scene.id))a.reloadUntil=0;}},
 update(dt){const s=this.state;for(const p of s.projectiles){if(p.sceneId&&p.sceneId!==this.scene.id||!this.object(p.owner)){p.remaining=0;continue;}const d=Math.min(p.remaining,p.speed*dt),hit=this.raycast(p.position,p.direction,d,[p.owner]);if(hit){this.damage(hit.id,p.damage,{sourceId:p.owner,initiatorId:p.owner});p.remaining=0;}else{p.position=p.position.map((v,i)=>v+p.direction[i]*d);p.remaining-=d;}}
  s.projectiles=s.projectiles.filter(p=>p.remaining>0);s.tracers=s.tracers.filter(t=>t.until>s.clock);
  for(const [id,a]of Object.entries(s.weapons))if(a.reloadUntil&&a.reloadUntil<=s.clock){const w=this.config.weapons.find(w=>w.id===id);if(!w||!this.object(w.ownerId)){a.reloadUntil=0;continue;}const n=Math.min(w.magazine-a.ammo,a.reserve);a.ammo+=n;a.reserve-=n;a.reloadUntil=0;s.message='';}
 },bindings:Object.fromEntries(['ammo','reserve'].map(field=>[field,{schema:{type:'integer'},validate(g,owner,id){if(!g.weapons.some(w=>w.ownerId===owner&&(!id||w.id===id)))throw Error('У владельца не подключено это оружие.');},read:(s,g,id,recordId)=>{const w=weaponFor(g,s,id,recordId);return w&&s.weapons[w.id]?.[field];}}])),
 operations:[operation('sacura.weapon.shoot','Выстрелить',{owner:ref('object','Владелец оружия'),weapon:ref('weapon','Оружие',{optional:true}),origin:{type:'vector',size:3,label:'Точка выстрела',optional:true},direction:{type:'vector',size:3,label:'Направление',optional:true}},{type:'boolean'},(p,c)=>session(c).shoot({ownerId:p.owner,weaponId:p.weapon,origin:p.origin,direction:p.direction},c)),
 operation('sacura.weapon.reload','Перезарядить',{owner:ref('object','Владелец оружия'),weapon:ref('weapon','Оружие',{optional:true})},{type:'boolean'},(p,c)=>session(c).reload(p.owner,p.weapon)),
 operation('sacura.weapon.enabled','Разрешить использование оружия',{owner:ref('object','Владелец оружия'),enabled:{type:'boolean',default:false,label:'Разрешить'}},{type:'void'},(p,c)=>{const g=session(c);for(const w of g.config.weapons.filter(w=>w.ownerId===p.owner))if(g.state.weapons[w.id])g.state.weapons[w.id].enabled=p.enabled;})]});
