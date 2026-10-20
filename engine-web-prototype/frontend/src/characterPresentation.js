import {t,literalLabel} from './i18n.js';
import {characterAnimationOptions,characterAnimations} from './characterModel.js';

export function animationLabel(character,clip) {
 return character?.model||clip.basePose||character?.animationDetails?.[clip.id]?.name
  ? clip.name : t(clip.name);
}
export function animationOptions(character,current) {
 const clips=characterAnimations(character);
 return characterAnimationOptions(character,current).map(([id,label])=>{
  const clip=clips.find(item=>item.id===id);
  return [id,literalLabel(clip?animationLabel(character,clip):t(label))];
 });
}
