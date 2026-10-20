import {PACKAGE_LIMITS} from './packageReferences.js';

export const SOURCE_PACKAGE_VERSION=3;
export const SDK_SOURCE_CONTRACT=Object.freeze({version:1,language:'javascript',runtime:'sacura-sdk',execution:'gameplay-only'});
export const FUNCTION_SOURCE_CONTRACT=Object.freeze({version:2,language:'javascript',runtime:'sacura-sdk',execution:'gameplay-and-functions'});
export const sourceContractFor=content=>content.functions?.some(f=>f.functionScript)?FUNCTION_SOURCE_CONTRACT:SDK_SOURCE_CONTRACT;
const forbidden=new Set(['__proto__','constructor','prototype','script','scripts','javascript','install','postinstall','code']);
// V1/V2 retain the data-only key policy. V3 authorizes source text only in
// documented project resources, never arbitrary JSON or native registrations.
export function assertPackageData(value){
 const modern=value?.formatVersion===SOURCE_PACKAGE_VERSION;
 const contract=value?.sourceContract?.version===2?FUNCTION_SOURCE_CONTRACT:SDK_SOURCE_CONTRACT;
 if(modern&&(!value.sourceContract||Object.keys(value.sourceContract).length!==Object.keys(contract).length||Object.entries(contract).some(([k,v])=>value.sourceContract[k]!==v)))throw Error('Неизвестный контракт исходников SDK пакета v3.');
 let budget=PACKAGE_LIMITS.values;
 const visit=(v,path='',depth=0)=>{if(--budget<0||depth>PACKAGE_LIMITS.depth)throw Error('Пакет слишком сложный.');if(['function','symbol','bigint','undefined'].includes(typeof v)||typeof v==='number'&&!Number.isFinite(v))throw Error('Пакету нужны сериализуемые данные.');if(!v||typeof v!=='object')return;
  if(v.kind==='component.code'&&(!modern||!/^\/content\/logicResources\/\d+$/.test(path)))throw Error('Исходникам компонентов нужен явный пакет v3 с контрактом SDK.');
  for(const [key,child]of Object.entries(v)){
   if(forbidden.has(key))throw Error('Код и установочные скрипты запрещены вне исходных ресурсов SDK.');
   if(key==='mechanicScript'&&(!modern||!/^\/content\/functions\/\d+$/.test(path)))throw Error('JavaScript механики требует пакета v3 с контрактом SDK.');
   if(key==='functionScript'&&(!modern||contract.version!==2||!/^\/content\/functions\/\d+$/.test(path)))throw Error('JavaScript функции требует пакета v3 с контрактом SDK v2.');
   visit(child,path+'/'+key,depth+1);
  }
 };
 visit(value);
 if(!modern&&value.sourceContract!==undefined)throw Error('Старый пакет данных не поддерживает контракт исходников.');
}
export const packageHasSources=content=>content.logicResources?.some(r=>r.kind==='component.code')||content.functions?.some(f=>f.mechanicScript||f.functionScript);
