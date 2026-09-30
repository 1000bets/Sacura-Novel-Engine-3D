import test from 'node:test';
import assert from 'node:assert/strict';
import {createRendererViewport} from '../src/rendererViewport.js';

test('viewport queues resize until draw and never lets the buffer change CSS layout',()=>{
 let notify,disconnected=false,projections=0;const sizes=[];
 class Observer{constructor(callback){notify=callback;}observe(){}disconnect(){disconnected=true;}}
 const renderer={setSize:(...args)=>sizes.push(args)},camera={updateProjectionMatrix(){projections++;}};
 const viewport=createRendererViewport(renderer,camera,{getBoundingClientRect:()=>({width:800,height:450})},Observer);
 assert.deepEqual(sizes,[]);viewport.update();assert.deepEqual(sizes,[[800,450,false]]);
 notify([{contentRect:{width:901.2,height:501.2}}]);notify([{contentRect:{width:902.2,height:502.2}}]);
 assert.equal(sizes.length,1);viewport.update();assert.deepEqual(sizes[1],[902,502,false]);assert.equal(camera.aspect,902/502);
 notify([{contentRect:{width:902.3,height:502.3}}]);assert.equal(viewport.update(),false);assert.equal(sizes.length,2);
 notify([{contentRect:{width:0,height:0}}]);assert.equal(viewport.update(),false);assert.equal(projections,2);
 viewport.dispose();assert.equal(disconnected,true);
});
