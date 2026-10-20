export function interactionTargets(beat){return [...new Set(beat?.signals?.length?beat.signals:beat?.signal?[beat.signal]:[])];}
export function playerControls(beat){
 const c=beat?.controls||{};
 return {characterId:c.characterId||'',mode:['wasd','point-click','both'].includes(c.mode)?c.mode:'none',speed:Math.max(.1,Math.min(15,Number(c.speed)||2.4)),radius:Math.max(0,Number(c.radius)||0)};
}
