import test from 'node:test';
import assert from 'node:assert/strict';
import {applyConnections,movePinConnections} from '../src/storyConnections.js';
const fixture=()=>({chapters:[{beats:[{id:'a',next:'b'},{id:'b',next:'c'},{id:'c',next:null},{id:'choice',kind:'choice',choices:[{id:'yes',next:'b',condition:'trust',threshold:4},{id:'no',next:'b'}]},{id:'end',kind:'end'}]}]});
const wires=[{source:'a',sourceHandle:'next',target:'b'},{source:'choice',sourceHandle:'choice:yes',target:'b'},{source:'choice',sourceHandle:'choice:no',target:'b'}];
test('moving an input moves every incoming wire and preserves answer conditions',()=>{
 const p=fixture();
 const changes=movePinConnections(wires,{node:'b',type:'target',handle:'in'},{node:'c',type:'target',handle:'in'});
 assert.equal(applyConnections(p,changes),null);
 assert.equal(p.chapters[0].beats[0].next,'c');
 assert.deepEqual(p.chapters[0].beats[3].choices,[{id:'yes',next:'c',condition:'trust',threshold:4},{id:'no',next:'c'}]);
});
test('moving an output detaches the old output and replaces the new output continuation',()=>{
 const p=fixture();
 const changes=movePinConnections(wires,{node:'a',type:'source',handle:'next'},{node:'c',type:'source',handle:'next'});
 assert.equal(applyConnections(p,changes),null);
 assert.equal(p.chapters[0].beats[0].next,null);
 assert.equal(p.chapters[0].beats[2].next,'b');
});
test('invalid reconnection is atomic and leaves the original wire intact',()=>{
 for(const connection of [{source:'b',sourceHandle:'next',target:'b'},{source:'end',sourceHandle:'next',target:'b'},{source:'choice',sourceHandle:'next',target:'b'},{source:'a',sourceHandle:'next',target:'missing'}]){
 const p=fixture(),before=structuredClone(p);
 assert.ok(applyConnections(p,[{source:'a',sourceHandle:'next',target:null},connection]));
 assert.deepEqual(p,before);
 }
});
test('break connections and reconnect either endpoint without modifying unrelated wires',()=>{
 const p=fixture();
 assert.equal(applyConnections(p,[{source:'choice',sourceHandle:'choice:yes',target:null}]),null);
 assert.equal(p.chapters[0].beats[3].choices[1].next,'b');
 assert.equal(applyConnections(p,[{source:'a',sourceHandle:'next',target:null},{source:'a',sourceHandle:'next',target:'c'}]),null);
 assert.equal(p.chapters[0].beats[0].next,'c');
 assert.deepEqual(movePinConnections(wires,{node:'a',type:'source',handle:'next'},{node:'b',type:'target',handle:'in'}),[]);
});
