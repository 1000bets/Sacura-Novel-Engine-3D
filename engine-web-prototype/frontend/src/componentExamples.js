import {createComponentResource,editComponentSource,publishComponentSource} from './componentResources.js';
export const lockedDoorSource=`const component = {
 name: "LockedDoor",
 parameters: {type:"object",properties:{InitiallyLocked:{type:"boolean",default:true,label:"Изначально заперта · IsLocked"},OpenSeconds:{type:"number",default:0.5,min:0,label:"Время открытия, сек"}}},
 state: {type:"object",properties:{IsLocked:{type:"boolean",default:true,label:"Заперта · IsLocked"}}},
 stateFromParameters: {IsLocked:"InitiallyLocked"},
 signals: {opened:{name:"Открыта",payloadSchema:{type:"object",properties:{}}}},
 methods: {
  setLocked: {name:"Разблокировать / задать замок",parameters:{type:"object",properties:{value:{type:"boolean",default:false,label:"Заперта"}}},result:{type:"boolean"},run({args,state}){state.IsLocked=args.value;return state.IsLocked;}},
  interact: {name:"Взаимодействовать",result:{type:"boolean"},async run({state,engine,self}){if(state.IsLocked){await engine.operation("sacura.game.message",{message:"Заперто"});return false;}return await engine.invoke(self,"open",{});}},
  open: {name:"Открыть",result:{type:"boolean"},async run({engine,owner,params}){await engine.operation("sacura.door.animate",{object:owner,open:true,seconds:params.OpenSeconds});await engine.emit("opened",{});return true;}}
 }
};
`;
export function installLockedDoor(project){
 const existing=project.logicResources?.find(r=>r.example==='locked-door');if(existing)return project.gameplay.mechanicDefinitions.find(d=>d.id===existing.definitionId);
 const r=createComponentResource(project,{name:'LockedDoor'});r.example='locked-door';editComponentSource(project,r.id,lockedDoorSource);publishComponentSource(project,r.id);return project.gameplay.mechanicDefinitions.find(d=>d.id===r.definitionId);
}
