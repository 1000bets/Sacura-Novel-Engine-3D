import {lowerSerializedFields} from './serializedFields.js';
import {newQuickJSWASMModuleFromVariant} from 'quickjs-emscripten-core';
import variant from '@jitl/quickjs-singlefile-browser-release-sync';

let modulePromise,preparedModule;
export const prepareMechanicScripts=()=>modulePromise??=newQuickJSWASMModuleFromVariant(variant).then(module=>preparedModule=module);
export const mechanicScriptsReady=()=>!!preparedModule;

// Value wires are synchronous and read-only. They use the same isolated VM as
// mechanics, but never run host operations or schedule asynchronous work.
export function runFunctionJavaScriptSync(source,input,call,{deadline=Date.now()+100}={}){
 if(!preparedModule)throw Error('JavaScript ещё загружается. Запустите игру после компиляции функции.');
 const runtime=preparedModule.newRuntime();runtime.setMemoryLimit(16*1024*1024);runtime.setMaxStackSize(256*1024);runtime.setInterruptHandler(()=>Date.now()>deadline);
 const vm=runtime.newContext();let handle;
 try{
  const bridge=vm.newFunction('__call',(name,args)=>{try{return vm.newString(JSON.stringify({value:call(vm.getString(name),JSON.parse(vm.getString(args)))}));}catch(error){return {error:vm.newError(error)};}});
  vm.setProp(vm.global,'__call',bridge);bridge.dispose();
  const setup='"use strict";\nconst __freeze = value => { if (value && typeof value === "object") { Object.values(value).forEach(__freeze); Object.freeze(value); } return value; };\nconst __ctx = __freeze('+JSON.stringify(input)+');\nconst engine = Object.freeze({call: (name,args={}) => JSON.parse(__call(name,JSON.stringify(args))).value});\n';
  const result=vm.evalCode(setup+'(()=>{\n"use strict";\n'+source+'\nreturn main({...__ctx,engine});\n})()','function.js');
  if(result.error){const value=vm.dump(result.error);result.error.dispose();const frame=String(value.stack||'').match(/function\.js:(\d+)(?::(\d+))?/);throw Object.assign(Error(value.message==='interrupted'?'Функция превысила лимит вычислений.':value.message||String(value)),{line:frame?Math.max(1,Number(frame[1])-6):undefined,column:frame?.[2]?Number(frame[2]):undefined});}
  handle=result.value;
  const state=vm.getPromiseState(handle);if(!state.notAPromise){if(state.type==='fulfilled')state.value.dispose();else if(state.type==='rejected')state.error.dispose();throw Error('Функция значения должна вернуть результат сразу. async/await и Promise используйте в игровых механиках.');}
  return vm.dump(handle);
 }finally{handle?.dispose();vm.dispose();runtime.dispose();}
}
export async function validateMechanicJavaScript(source){
 source=lowerSerializedFields(source);
 const QuickJS=await prepareMechanicScripts(),vm=QuickJS.newContext();try{const result=vm.evalCode('(function(){\n'+source+'\n})','mechanic.js');if(result.error){const error=vm.dump(result.error);result.error.dispose();throw Error(error.message||String(error));}result.value.dispose();}finally{vm.dispose();}
}
const bootstrap=`
const __decode = value => JSON.parse(value).value;
const __invoke = (method, ...args) => {
  const value = __bridge(method, JSON.stringify(args, (key,value) => {
    if (typeof value === 'number' && !Number.isFinite(value)) throw Error('Скрипт получил неконечное число.');
    if (['function','symbol','bigint'].includes(typeof value)) throw Error('Игровой API принимает только обычные данные.');
    return value;
  }));
  return value && typeof value.then === 'function' ? value.then(__decode) : __decode(value);
};
function __store(root, path = [], array = false) {
  return new Proxy(array ? [] : {}, {
    get(target, key) {
      if (key === 'toJSON') return () => __invoke('get', root, path);
      if (key === Symbol.iterator && array) return Array.prototype[Symbol.iterator];
      if (typeof key === 'symbol') return undefined;
      if (array && typeof Array.prototype[key] === 'function') return Array.prototype[key];
      const value = __invoke('get', root, [...path, key]);
      return value && typeof value === 'object' ? __store(root, [...path, key], Array.isArray(value)) : value;
    },
    set(target, key, value) { if(value === undefined)throw Error('Нельзя записать undefined в игровые данные.'); __invoke('set', root, [...path, String(key)], value); return true; },
    ownKeys() { const keys = Object.keys(__invoke('get', root, path) || {}); return array ? [...new Set([...keys, 'length'])] : keys; },
    getOwnPropertyDescriptor(target, key) { return key === 'length' && array ? Reflect.getOwnPropertyDescriptor(target, key) : {enumerable:true,configurable:true}; }
  });
}
const __engine = new Proxy(__input.engine, {get(target,key) {
  if (key in target) return target[key];
  if (key === 'timeout') return (seconds, body) => Promise.race([body, __invoke('wait',seconds).then(()=>{throw Error('Превышено время ожидания.');})]);
  if (key === 'sequence' || key === 'selector') return async (names, args) => {let result=key==='sequence'?'Success':'Failure';for(const name of names){result=await __invoke('call',name,args);if(!['Running','Success','Failure'].includes(result))throw Error('Задача поведения должна вернуть Running, Success или Failure.');if(result==='Running'||key==='sequence'&&result==='Failure'||key==='selector'&&result==='Success')break;}return result;};
  return (...args) => __invoke(key, ...args);
}});
const __ctx = {...__input, global:__store('global'), state:__store('state'), params:__store('params'), engine:__engine};
const console = {log:(...args)=>__invoke('log', ...args), warn:(...args)=>__invoke('log', ...args), error:(...args)=>__invoke('log', ...args)};
`;

// Real ECMAScript runs in a separate VM: no DOM, network, Node or host eval.
// Each invocation owns its VM and interrupt budget, including promise continuations.
export async function runMechanicJavaScript(source,input,bridge,{assertActive=()=>{},abortSignal,cpuMs=100,memoryBytes=16*1024*1024,phase='main',filename='mechanic.js'}={}){
 source=lowerSerializedFields(source);
 const sourceOffset=(bootstrap+'\n(()=>{\n').split('\n').length-1;
 const scriptError=value=>{const frames=[...(value.stack||'').matchAll(/:(\d+)(?::(\d+))?\)?/g)],frame=frames.find(m=>Number(m[1])>sourceOffset),line=frame?Number(frame[1])-sourceOffset:undefined,column=frame?.[2]?Number(frame[2]):undefined;return Object.assign(Error((filename!=='mechanic.js'?filename+(line?':'+line+(column?':'+column:''):'')+' · ':'')+(value.message||String(value))),{line,column,scriptStack:value.stack});};
 const QuickJS=await prepareMechanicScripts();assertActive();const runtime=QuickJS.newRuntime();runtime.setMemoryLimit(memoryBytes);runtime.setMaxStackSize(256*1024);
 let deadline=Date.now()+cpuMs,closed=false,jobs=0,wake,settledError;const pending=new Set(),notify=()=>{wake?.();wake=undefined;};
 runtime.setInterruptHandler(()=>{try{assertActive();if(abortSignal?.aborted){settledError={message:'INSTANCE_STOPPED'};return true;}if(Date.now()>deadline){settledError={message:'Скрипт превысил лимит вычислений.'};return true;}return false;}catch(error){settledError={message:error.message};return true;}});
 const vm=runtime.newContext(),hostErrors=new Map();let errorSequence=0;
 const json=value=>vm.newString(JSON.stringify({value})),fromError=e=>{const error=e instanceof Error?e:Error(String(e)),handle=vm.newError(error),id=++errorSequence,marker=vm.newNumber(id);hostErrors.set(id,error);vm.setProp(handle,'__sacuraHostErrorId',marker);marker.dispose();return handle;};
 // Forward the exact host error on rethrow, including its innermost receiver.
 // Only tokens created by this invocation map to host objects; VM-authored
 // metadata cannot invent a host diagnostic or gain access to host callbacks.
 const errorFromHandle=handle=>{const marker=vm.getProp(handle,'__sacuraHostErrorId');try{const original=hostErrors.get(vm.getNumber(marker)),value=vm.dump(handle);if(!original)return scriptError(value);if(typeof value.message==='string'&&value.message!==original.message){const forwarded=Object.assign(Error(value.message),original);if(original.componentDiagnostic)forwarded.componentDiagnostic={...original.componentDiagnostic,message:value.message,reason:value.message};return forwarded;}return original;}finally{marker.dispose();}};
 const pump=()=>{if(closed)return;const result=runtime.executePendingJobs(1000);if(result.error){settledError=errorFromHandle(result.error);result.error.dispose();}else result.dispose();notify();};
 const callback=vm.newFunction('__bridge',(methodHandle,argsHandle)=>{
  try{assertActive();const method=vm.getString(methodHandle),args=JSON.parse(vm.getString(argsHandle)),value=bridge(method,args);if(!value||typeof value.then!=='function')return json(value);
   const deferred=vm.newPromise();pending.add(deferred);
   Promise.resolve(value).then(value=>{if(closed)return;const handle=json(value);deferred.resolve(handle);handle.dispose();},error=>{if(closed)return;const handle=fromError(error);deferred.reject(handle);handle.dispose();}).finally(()=>{if(closed)return;pending.delete(deferred);deferred.dispose();deadline=Date.now()+cpuMs;queueMicrotask(pump);});return deferred.handle;
  }catch(error){return {error:fromError(error)};}
 });
 vm.setProp(vm.global,'__bridge',callback);callback.dispose();const inputResult=vm.evalCode('('+JSON.stringify(input)+')');const inputHandle=vm.unwrapResult(inputResult);vm.setProp(vm.global,'__input',inputHandle);inputHandle.dispose();
 let handle;const abort=()=>{settledError={message:'INSTANCE_STOPPED'};notify();};abortSignal?.addEventListener('abort',abort,{once:true});
 try{
  const code=bootstrap+'\n(()=>{\n'+source+'\nreturn typeof '+phase+' === "function" ? '+phase+'(__ctx) : true;\n})()',result=vm.evalCode(code,filename);
  if(result.error){const error=errorFromHandle(result.error);result.error.dispose();throw error;}handle=result.value;
  for(;;){assertActive();if(abortSignal?.aborted)throw Error('INSTANCE_STOPPED');if(settledError)throw settledError instanceof Error?settledError:Error(settledError.message||String(settledError));const state=vm.getPromiseState(handle);
   if(state.type==='fulfilled'){const value=vm.dump(state.value);if(!state.notAPromise)state.value.dispose();return value;}
   if(state.type==='rejected'){const error=errorFromHandle(state.error);state.error.dispose();throw error;}
   if(++jobs>10000)throw Error('Скрипт превысил лимит продолжений.');pump();const next=vm.getPromiseState(handle);if(next.type!=='pending'){if(next.type==='fulfilled'&&!next.notAPromise)next.value.dispose();else next.error.dispose();continue;}
   if(settledError)continue;if(!pending.size&&!runtime.hasPendingJob())throw Error('Скрипт ожидает Promise, который не может завершиться.');
   if(!runtime.hasPendingJob())await new Promise(resolve=>{wake=resolve;});
  }
 }catch(error){if(error.message==='interrupted')error.message='Скрипт превысил лимит вычислений или был остановлен.';throw error;}
 finally{closed=true;abortSignal?.removeEventListener('abort',abort);handle?.dispose();for(const deferred of pending)deferred.dispose();vm.dispose();runtime.dispose();}
}
