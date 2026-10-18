import {ownerKey} from './capabilities/shared.js';
// Read old saves into the same native maps. Never replay initialization or starter items.
export function adaptSavedCapabilities(session){
 const s=session.state,g=session.config;
 s.behaviors??={};s.disabled??={};s.directions??={};s.inventories??={};s.selectedWeapons??={};s.resources??={};
 if(s.capabilitySchemaVersion!==1){
  s.inventories[ownerKey(g.inventory.owner)]??={items:structuredClone(s.inventory||[]),equipment:structuredClone(s.equipment||{})};
  if(s.weaponId){const w=g.weapons.find(w=>w.id===s.weaponId);if(w)s.selectedWeapons[w.ownerId]=w.id;}
  for(const [id,a]of Object.entries(s.actors)){if(a.dead){s.disabled[id]=true;for(const w of g.weapons.filter(w=>w.ownerId===id))if(s.weapons[w.id])s.weapons[w.id].enabled=false;if(session.world.playerControl?.characterId===id)session.world.playerControl=null;}if(g.actors[id]?.ai)s.behaviors[id]={nextAttack:a.nextAttack||0,direction:[0,0,0],enabled:!a.dead};if(!Number.isFinite(g.actors[id]?.maxHP))delete s.actors[id];}
  for(const r of g.resources||[])s.resources[r.id]=r.id===(g.legacyCounterId||'legacy-kills')?s.kills||0:r.initial;
 }
 s.cameraParameters??=g.cameraSettings;s.capabilitySchemaVersion=1;
}
