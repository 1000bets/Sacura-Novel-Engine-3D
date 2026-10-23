const {test}=require('node:test');
const assert=require('node:assert/strict');
const {preserveDraft}=require('../native-project.cjs');
test('close/switch guards preserve drafts on cancel, save failure and invalid decisions',async()=>{
 for(const choice of ['cancel','discard','save']){
  const calls=[];const bridge={connected:true,request:async(method)=>{calls.push(method);return {ok:true,value:{dirty:true,storyDirty:true,play:'stopped'}};}};
  assert.equal(await preserveDraft(bridge,async()=>choice),choice!=='cancel');
  assert.deepEqual(calls,choice==='save'?['bridge.describe','project.save']:['bridge.describe']);
 }
 const bridge={connected:true,request:async m=>m==='bridge.describe'?{ok:true,value:{dirty:true,play:'stopped'}}:{ok:false,error:{message:'External file changed'}}};
 await assert.rejects(()=>preserveDraft(bridge,async()=>'save'),/External file changed/);
 await assert.rejects(()=>preserveDraft(bridge,async()=>'unexpected'),/Invalid close decision/);
});
test('clean and disconnected sessions do not show a discard prompt',async()=>{
 const unexpected=async()=>{throw Error('Unexpected prompt');};
 assert.equal(await preserveDraft({connected:false},unexpected),true);
 assert.equal(await preserveDraft({connected:true,request:async()=>({ok:true,value:{dirty:false,storyDirty:false}})},unexpected),true);
});
