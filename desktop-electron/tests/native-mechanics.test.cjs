const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs/promises');
const os=require('node:os');
const path=require('node:path');
const {NativeMechanics}=require('../native-mechanics.cjs');

test('native mechanic keeps a draft and last compiled version, executes against Play, and imports disabled',async()=>{
  const folder=await fs.mkdtemp(path.join(os.tmpdir(),'sacura-native-mechanics-'));
  try{
    const project=path.join(folder,'Game.project');await fs.writeFile(project,'{}');
    const mechanics=new NativeMechanics(project);await mechanics.load();
    const [created]=await mechanics.create('object-1');
    await mechanics.update(created.id,{source:'async function main({engine}) { await engine.self.translate(1, 2, 3); }'});
    await mechanics.compile(created.id);
    await mechanics.update(created.id,{source:'async function main( {'});
    await assert.rejects(mechanics.compile(created.id),/Строка 1/);
    assert.match(mechanics.list()[0].compiledSource,/translate/);
    const calls=[],bridge={request:async(method,params)=>{calls.push({method,params});return {ok:true,value:[1,2,3],context:{session:'test',epoch:2,revision:1}};}};
    assert.deepEqual(await mechanics.runStart(bridge,{session:'test',epoch:2,revision:0}),[{id:created.id,ok:true}]);
    assert.deepEqual(calls[0].params,{objectId:'object-1',operation:'translate',value:[1,2,3]});
    const reopened=new NativeMechanics(project);await reopened.load();
    assert.equal(reopened.list()[0].source,'async function main( {');
    const exported=path.join(folder,'mechanic.json');await reopened.export(created.id,exported);
    const imported=await reopened.import(exported,'object-2');
    assert.equal(imported[1].enabled,false);
    assert.equal(imported[1].compiledSource,'');
    assert.equal(imported[1].objectId,'object-2');
    assert.equal(imported[1].source,reopened.list()[0].source);
    await reopened.update(imported[1].id,{source:'async function main({engine}) { await engine.self.setActive(false); }'});
    assert.equal(reopened.list()[0].source,'async function main( {');
  }finally{await fs.rm(folder,{recursive:true,force:true});}
});
