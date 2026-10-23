import type {Bridge, Context} from '../bridge/contract';
declare const bridge:Bridge;
declare const context:Context;
bridge.request({method:'object.create',params:{name:'Example'},context}).then(result=>{
  if(result.ok){const id:string=result.value.id;void id;}
  else {const code:string=result.error.code;void code;}
});
bridge.request({method:'transform.set',params:{objectId:'id',transform:{position:[1,2,3],rotation:[0,0,0,1],scale:[1,1,1]}},context});
// @ts-expect-error Unknown command cannot be sent by a typed caller.
bridge.request({method:'executeCpp',params:{source:'...'},context});
// @ts-expect-error Required stable ID is missing.
bridge.request({method:'object.destroy',params:{},context});
// @ts-expect-error Transform vectors have fixed length.
bridge.request({method:'transform.set',params:{objectId:'id',transform:{position:[1],rotation:[0,0,0,1],scale:[1,1,1]}},context});
