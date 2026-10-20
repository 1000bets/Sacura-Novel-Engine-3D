import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {checkRuntimePolicy} from '../../deploy/check-runtime-policy.mjs';
async function serve(policy,run){const server=createServer((_,res)=>{if(policy)res.setHeader('Content-Security-Policy',policy);res.end('<!doctype html><title>Editor</title>');});await new Promise(r=>server.listen(0,'127.0.0.1',r));try{await run('http://127.0.0.1:'+server.address().port);}finally{await new Promise(r=>server.close(r));}}
test('deployment HTTP smoke accepts the shipped Nginx policy needed by the isolated JS runtime',async()=>{
 const config=await readFile('nginx.conf','utf8'),policy=config.match(/add_header Content-Security-Policy "([^"]+)" always/)[1];
 await serve(policy,async base=>assert.equal((await checkRuntimePolicy(base)).status,'ok'));
});
test('deployment refuses blocked WASM and broad host eval permissions, including a second conflicting CSP',async()=>{
 for(const policy of [null,"script-src 'self'","script-src 'self' 'wasm-unsafe-eval' 'unsafe-eval'","script-src 'self' 'wasm-unsafe-eval' 'unsafe-inline'",["script-src 'self' 'wasm-unsafe-eval'","script-src 'self'"]])await serve(policy,base=>assert.rejects(checkRuntimePolicy(base),/Content-Security-Policy|blocks.*WebAssembly|host JS eval/));
});
