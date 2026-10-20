import {createMechanicsProject} from '../../src/projectLifecycle.js';
import {compileMechanicScript} from '../../src/mechanicScriptCompiler.js';
import {createMechanicDefinition,addMechanicInstance,createSignalDefinition} from '../../src/mechanicDescriptions.js';

export const objectSchema=properties=>({type:'object',properties});
export function componentFunction(p,id,source,properties={},result={type:'boolean'}){
 const f={id,name:id,graphVersion:1,parameters:Object.entries(properties).map(([name,schema])=>({name,schema})),locals:{},entry:id+'-return',nodes:[{id:id+'-return',kind:'return',...(result.type==='void'?{}:{value:false})}],resultSchema:result};
 f.mechanicScript=compileMechanicScript(p,source);p.functions.push(f);
 return {name:id,parameters:objectSchema(properties),result,logic:{functionId:id}};
}
export function doorProject(title='Компонентные двери'){
 const p=createMechanicsProject(title),scene=p.subscenes[0];scene.kind='empty';
 for(const id of ['door-a','door-b'])p.objects.push({id,name:id,type:'Активный меш',active:true,subsceneId:scene.id,transforms:{[scene.id]:{position:[0,0,0],rotation:[0,0,0],scale:[1,1,1]}}});
 return p;
}
// Plain authoring data + ordinary functions. No runtime recognizes this name.
export function lockedDoors(){
 const p=doorProject(),signal=createSignalDefinition(p,{id:'user.door.opened',name:'Дверь открылась',payloadSchema:objectSchema({})});
 const methods={
  'set-locked':componentFunction(p,'SetLocked','function main({args,state}) { state.IsLocked = args.value; return state.IsLocked; }',{value:{type:'boolean'}}),
  interact:componentFunction(p,'Interact','async function main({state,engine,self}) { if(state.IsLocked) return false; return await engine.invoke(self,"open",{}); }'),
  open:componentFunction(p,'Open','async function main({engine,owner}) { await engine.operation("sacura.door.write", {object:owner,open:true}); await engine.emit("opened",{}); return true; }'),
 };
 const d=createMechanicDefinition(p,{id:'user.component.locked-door',name:'LockedDoor',componentVersion:1,stateVersion:1,parameters:objectSchema({InitiallyLocked:{type:'boolean',default:true,label:'Изначально заперта'}}),stateSchema:objectSchema({IsLocked:{type:'boolean',default:true}}),stateFromParameters:{IsLocked:'InitiallyLocked'},methods,events:{opened:{name:'Открыта',signalId:signal.id,payloadSchema:signal.payloadSchema}},subscriptions:[{id:'interact',signalId:'interact',methodId:'interact',match:'owner'}]});
 const a=addMechanicInstance(p,d.id,{kind:'object',id:'door-a'},{InitiallyLocked:true}),b=addMechanicInstance(p,d.id,{kind:'object',id:'door-b'},{InitiallyLocked:false});
 return {p,d,a,b};
}
