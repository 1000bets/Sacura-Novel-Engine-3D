import {operationDefinition} from '../operationRegistry.js';
import {registerCapability} from '../capabilityRegistry.js';

// Installed engine extension, edited here and rebuilt by Vite / build:player.
// This module is deliberately absent from transferable authoring package content.
registerCapability({id:'sacura.reward-extension',operations:[{definition:{
 id:'sacura.reward.scale',version:1,pure:true,
 parameters:{type:'object',properties:{
  reward:{type:'number',default:100,label:'Награда'},
  multiplier:{type:'number',default:1,label:'Множитель'},
 }},result:{type:'number'},contexts:['event','function','rule','mechanic','widget'],
 editor:{label:'Рассчитать награду',category:'Ресурсы',description:'Умножить награду на коэффициент.'},
 source:{path:'src/extensions/reward.js',line:15},
 engineModule:{id:'sacura.reward-extension',version:1},
},execute:({reward,multiplier})=>reward*multiplier}]});
export const rewardOperation=operationDefinition('sacura.reward.scale',1);
