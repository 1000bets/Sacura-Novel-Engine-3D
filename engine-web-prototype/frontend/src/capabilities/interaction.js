import {registerCapability} from '../capabilityRegistry.js';
import {operation,ref,ownerSchema,ownerScope,length,sub,session,executionContext} from './shared.js';
registerCapability({id:'sacura.interaction',methods:{
 async pickup(item,id,owner,context={}){const key='pickup:'+owner+':'+id,generation=this.host.generation;this.flights.add(key);const capabilityOwner={kind:'object',id},capabilityOwners=[capabilityOwner,ownerScope(this.host.project,owner)];context={...context,capabilityOwner,capabilityOwners,capabilitySceneId:this.scene.id};try{if(item.pickupEventId)await this.host.binding({id:'input:pickup:'+owner+':'+id,eventId:item.pickupEventId,join:'FLOW_END',overrides:{},actionOverrides:{},operationContext:executionContext({...context,ownerId:owner})},generation);
  this.host.assert(generation);this.assertReaction(context);if(!this.object(id))throw Error('INSTANCE_STOPPED');const scope=ownerScope(this.host.project,owner);await this.trigger('pickup',{objectId:id,itemId:item.id},undefined,{...context,source:{type:'object',id},target:{type:scope.kind,id:scope.id},sceneId:this.scene.id,...(scope.kind==='object'?{initiatorId:owner}:{})});
 }catch(e){if(generation===this.host.generation&&!['CANCELLED','INSTANCE_STOPPED'].includes(e.message))this.error(e);}finally{this.flights.delete(key);}},
 interact(id,owner,context={}){
  const item=this.config.items.find(i=>i.worldObjectId===id);if(!item||this.world.visible[id]===false)return false;
  owner??=item.inventoryOwner?.id||item.inventoryOwnerId||this.config.legacyInventoryOwnerId||this.config.inventory.owner.id;
  const picker=ownerScope(this.host.project,owner).kind==='object'?owner:undefined,p=picker&&this.position(picker),to=this.position(id);if(picker&&(!p||!to))return false;
  if(p&&length(sub(p,to))>2.5){if(picker===this.controlledId&&['point-click','both'].includes(this.config.controller.mode)){const d=sub(to,p);d[1]=0;const dist=length(d);this.setDestination(p.map((v,i)=>v+d[i]*Math.max(0,(dist-1.5)/(dist||1))));this.pendingPickup=id;this.pendingPickupOwner=owner;return true;}this.state.message='Подойдите ближе к предмету.';return false;}
  try{this.addItem(item.id,1,{...context,ownerId:owner,publish:false});this.world.visible[id]=false;this.pickup(item,id,owner,context);this.host.emit();return true;}catch(e){this.error(e);return false;}
 }
},operations:[operation('sacura.interaction.pickup','Подобрать предмет',{owner:ownerSchema,object:ref('object','Предмет на сцене')},{type:'boolean'},(p,c)=>session(c).interact(p.object,p.owner,c)),
 operation('sacura.object.visible','Показать объект',{object:ref('object','Объект'),visible:{type:'boolean',default:false,label:'Показать'}},{type:'void'},(p,c)=>{session(c).world.visible[p.object]=p.visible;c.emit();})]});
