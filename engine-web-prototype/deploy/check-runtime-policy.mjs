import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';

// Read-only smoke check: unlike smoke.mjs it never creates accounts or projects.
export async function checkRuntimePolicy(base){
 const response=await fetch(new URL('/',base));assert.equal(response.status,200,'Editor entry is unavailable');
 const header=response.headers.get('content-security-policy');assert.ok(header,'Editor must serve Content-Security-Policy');
 for(const policy of header.split(',')){
  const directives=new Map(policy.trim().split(';').filter(Boolean).map(d=>{const [name,...values]=d.trim().split(/\s+/);return [name,values];}));
  const script=directives.get('script-src')||directives.get('default-src')||[];
  assert.ok(script.includes("'wasm-unsafe-eval'"),'CSP blocks the QuickJS WebAssembly runtime: add wasm-unsafe-eval to script-src');
  assert.deepEqual([...script].sort(),["'self'","'wasm-unsafe-eval'"].sort(),'Keep host JS eval, inline scripts and external script origins blocked');
 }
 await response.arrayBuffer();return {status:'ok',policy:header};
}
if(process.argv[1]===fileURLToPath(import.meta.url)){await checkRuntimePolicy(process.argv[2]||process.env.SACURA_SMOKE_URL||'http://127.0.0.1:8080');console.log('Runtime CSP: WASM allowed; host JS eval and inline scripts blocked.');}
