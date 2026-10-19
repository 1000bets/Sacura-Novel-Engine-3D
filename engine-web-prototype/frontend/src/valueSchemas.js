// Serializable schemas and native project references. No editor or executor dependencies.
const unsafe=new Set(['__proto__','prototype','constructor']);
const kinds=new Set(['number','integer','boolean','string','vector','collection','object','union','reference','void','variableValue','signalPayload','componentArguments','componentResult']);
export const referenceTypes=['object','scene','project','variable','event','function','widget','inputAction','signal','beat','item','weapon','recipe','resource','mechanic'];
export const reference=(type,id,mode='direct')=>({kind:'reference',type,mode,id});
export function assertSerializable(value,depth=0){
 if(depth>64)throw Error('Данные слишком глубоки.');
 if(value===undefined||typeof value==='function'||typeof value==='symbol'||typeof value==='bigint'||typeof value==='number'&&!Number.isFinite(value))throw Error('Нужны сериализуемые значения.');
 if(value&&typeof value==='object'){if(!Array.isArray(value)&&Object.getPrototypeOf(value)!==Object.prototype&&Object.getPrototypeOf(value)!==null)throw Error('Нужны обычные данные.');for(const [k,v]of Object.entries(value)){if(unsafe.has(k))throw Error('Недопустимое имя поля.');assertSerializable(v,depth+1);}}
}
export function assertSchema(schema,depth=0){
 if(depth>32||!schema||!kinds.has(schema.type))throw Error('Неизвестный тип схемы.');assertSerializable(schema);
 if(schema.type==='reference'&&!referenceTypes.includes(schema.referenceType))throw Error('Неизвестный тип ссылки.');
 if(schema.type==='collection'){if(schema.minItems!==undefined&&(!Number.isInteger(schema.minItems)||schema.minItems<0)||schema.maxItems!==undefined&&(!Number.isInteger(schema.maxItems)||schema.maxItems<0))throw Error('Некорректный размер коллекции.');assertSchema(schema.items,depth+1);}
 if(schema.type==='vector'&&![2,3,4].includes(schema.size??3))throw Error('Вектор должен иметь 2, 3 или 4 компонента.');
 if(schema.type==='object'){if(!schema.properties||Array.isArray(schema.properties)||typeof schema.properties!=='object')throw Error('Опишите поля структуры.');Object.values(schema.properties).forEach(s=>assertSchema(s,depth+1));}
 if(schema.type==='union'){if(!Array.isArray(schema.variants)||!schema.variants.length)throw Error('Опишите варианты значения.');schema.variants.forEach(s=>assertSchema(s,depth+1));}
 if(['variableValue','signalPayload','componentArguments','componentResult'].includes(schema.type)&&typeof schema.parameter!=='string')throw Error('Выберите параметр зависимого значения.');
 if(['componentArguments','componentResult'].includes(schema.type)&&typeof schema.methodParameter!=='string')throw Error('Выберите параметр метода компонента.');
 if(schema.enum&&(!Array.isArray(schema.enum)||!schema.enum.length))throw Error('Пустой список значений.');
 if(schema.min!==undefined&&!Number.isFinite(schema.min)||schema.max!==undefined&&!Number.isFinite(schema.max)||schema.min>schema.max)throw Error('Некорректные границы значения.');
}
export function referenceRecords(project,type){if(type==='mechanic')return (project.gameplay?.mechanics||[]).map(m=>({...m,name:m.name||project.gameplay.mechanicDefinitions?.find(d=>d.id===m.definitionId)?.name||m.capabilityId||m.type}));
 if(type==='project')return [{id:project.id,name:project.title||'Проект'}];
 if(['item','weapon','recipe','resource'].includes(type))return project.gameplay?.[({item:'items',weapon:'weapons',recipe:'recipes',resource:'resources'})[type]]||[];
 if(type==='variable')return (project.gameplay?.variableBindings||[]).map(x=>({id:x.id,name:x.key}));
 return ({beat:project.chapters?.flatMap(c=>c.beats),object:project.objects,scene:project.subscenes,event:project.events,function:project.functions,widget:project.widgets,inputAction:project.input?.actions,signal:project.gameplay?.signals})[type]||[];
}
export function variableKey(project,id){const key=project.gameplay?.variableBindings?.find(x=>x.id===id)?.key;if(!key||!Object.hasOwn(project.variables||{},key))throw Error('Переменная удалена. Выберите другую.');return key;}
export function resolveReference(value,expected,context={}){
 const {project,bindings={},scope,sourceId=context.source?.type==='object'?context.source.id:undefined,targetId=context.target?.type==='object'?context.target.id:undefined,checkReferences=true}=context;
 if(typeof value==='string'&&context.allowResolvedReferences)value=reference(expected,value);
 if(!value||value.kind!=='reference'||value.type!==expected)throw Error('Выберите ссылку типа «'+expected+'».');
 const mode=value.mode||'direct';
 if(mode==='external'){if(typeof value.parameter!=='string'||!value.parameter)throw Error('У внешней связи нужен параметр.');const bound=bindings[value.parameter];if(bound===undefined){if(context.allowDeferred)return undefined;throw Error('Назначьте параметр «'+value.parameter+'».');}return resolveReference(bound,expected,{...context,bindings:{}});}
 if(mode==='tag'){if(expected!=='object'||typeof value.tag!=='string'||!value.tag.trim())throw Error('Выберите тег объектов.');return checkReferences?(project.objects||[]).filter(o=>o.active!==false&&o.tags?.includes(value.tag)).map(o=>o.id):[];}
 let id=value.id;
 if(mode==='context'){if(expected!=='object'||!['self','source','target','initiator'].includes(value.role))throw Error('Некорректная контекстная ссылка.');id=({self:scope?.kind==='object'?scope.id:undefined,source:sourceId,target:targetId,initiator:context.initiator?.type==='object'?context.initiator.id:undefined})[value.role];if(!id){if(context.allowDeferred)return undefined;throw Error('В этом контексте нет объекта «'+value.role+'».');}}
 else if(!['direct','internal'].includes(mode)||typeof id!=='string'||!id)throw Error('Выберите существующее определение.');
 if(checkReferences&&(!project||!referenceRecords(project,expected).some(x=>x.id===id)))throw Error('Ссылка «'+expected+'» недоступна. Выберите существующее определение.');
 return id;
}
const scalar={type:'union',variants:[{type:'number'},{type:'boolean'},{type:'string'}]};
export function schemaFromValue(value,type){
 if(type==='integer')return {type:'integer'};
 if(type==='array'||Array.isArray(value))return {type:'collection',items:value?.length?schemaFromValue(value[0]):scalar};
 if(type==='struct'||value&&typeof value==='object')return {type:'object',properties:Object.fromEntries(Object.entries(value||{}).map(([k,v])=>[k,schemaFromValue(v)]))};
 return {type:type||typeof value};
}
export function dependentSchema(schema,context){
 // Parameter expressions are resolved by the existing evaluator at execution.
 // Import/project validation must not treat an expression object as a native ID.
 if(context.allowExpressions&&context.parameters?.[schema.parameter]?.op)return null;
 if(['componentArguments','componentResult'].includes(schema.type)){
  const raw=context.parameters?.[schema.parameter],id=raw?.kind==='reference'?resolveReference(raw,'mechanic',context):raw,methodId=context.parameters?.[schema.methodParameter];
  if(!id||!methodId)return null;
  const m=context.project?.gameplay?.mechanics?.find(m=>m.id===id),d=context.project?.gameplay?.mechanicDefinitions?.find(d=>d.id===m?.definitionId&&d.version===m?.definitionVersion);
  if(!d?.componentVersion||!Object.hasOwn(d.methods||{},methodId)){if(context.checkReferences===false||context.allowDeferred)return null;throw Error('Выберите существующий экземпляр и метод компонента.');}
  return d.methods[methodId][schema.type==='componentArguments'?'parameters':'result'];
 }
 if(schema.type==='variableValue'){const raw=context.parameters?.[schema.parameter];let id;if(raw?.kind==='reference')id=resolveReference(raw,'variable',context);else id=raw;if(!id)return null;if(context.checkReferences===false&&!context.project?.gameplay?.variableBindings?.some(x=>x.id===id))return null;const key=variableKey(context.project,id);return schemaFromValue(context.project.variables[key],context.project.variableTypes?.[key]);}
 if(schema.type==='signalPayload'){const raw=context.parameters?.[schema.parameter],id=raw?.kind==='reference'?resolveReference(raw,'signal',context):raw;if(!id)return null;return context.project.gameplay?.signals?.find(x=>x.id===id)?.payloadSchema||null;}
 return schema;
}
export function schemaDefaults(schema){
 if(Object.hasOwn(schema,'default'))return structuredClone(schema.default);
 if(schema.type==='object'){const out={};for(const [k,s]of Object.entries(schema.properties)){const v=schemaDefaults(s);if(v!==undefined)out[k]=v;}return out;}
 return undefined;
}
export function validateValue(schema,value,context={},path=''){
 const errors=[],bad=message=>errors.push({path,message}),label=schema.label||path||'Значение';
 if(['componentArguments','componentResult'].includes(schema.type)){try{const s=dependentSchema(schema,context);if(!s){if(!context.allowDeferred)bad(label+': сначала выберите компонент и метод.');return errors;}return validateValue(s,value,context,path);}catch(e){bad(e.message);return errors;}}
 if(value===undefined){if(!schema.optional&&!Object.hasOwn(schema,'default')&&schema.type!=='void')bad(label+': значение обязательно.');return errors;}
 if(context.allowExpressions&&value?.op){try{context.validateExpression?.(value);}catch(e){bad(e.message);}return errors;}
 try{
  if(['variableValue','signalPayload'].includes(schema.type)){const s=dependentSchema(schema,context);if(!s){if(!context.allowDeferred)bad(label+': сначала выберите связь.');return errors;}return validateValue(s,value,context,path);}
  if(schema.type==='reference'){const id=resolveReference(value,schema.referenceType,context);if(Array.isArray(id))bad(label+': группа нужна в параметре коллекции.');}
  else if(schema.type==='collection'){if(value?.kind==='reference'&&value.mode==='tag'&&schema.items.type==='reference'&&schema.items.referenceType==='object'){resolveReference(value,'object',context);return errors;}if(!Array.isArray(value))bad(label+': нужна коллекция.');else{if(value.length<(schema.minItems??0)||value.length>(schema.maxItems??10000))bad(label+': некорректное количество элементов.');for(let i=0;i<Math.min(value.length,10000);i++)errors.push(...validateValue(schema.items,value[i],context,path+'.'+i));}}
  else if(schema.type==='object'){if(!value||typeof value!=='object'||Array.isArray(value))bad(label+': нужна структура.');else{for(const k of Object.keys(value))if(unsafe.has(k)||!Object.hasOwn(schema.properties,k))bad(label+': неизвестное поле «'+k+'».');for(const [k,s]of Object.entries(schema.properties))errors.push(...validateValue(s,value[k],context,path?path+'.'+k:k));}}
  else if(schema.type==='union'){if(!schema.variants.some(s=>!validateValue(s,value,context,path).length))bad(label+': неверный тип значения.');}
  else if(schema.type==='vector'){if(!Array.isArray(value)||value.length!==(schema.size??3)||!value.every(Number.isFinite))bad(label+': нужен конечный числовой вектор.');}
  else if(schema.type==='void'){if(value!==undefined)bad(label+': операция не возвращает значение.');}
  else if(schema.type==='number'||schema.type==='integer'){if(!Number.isFinite(value)||schema.type==='integer'&&!Number.isInteger(value))bad(label+': нужно '+(schema.type==='integer'?'целое число.':'конечное число.'));else if(value<(schema.min??-Infinity)||value>(schema.max??Infinity))bad(label+': число вне диапазона.');}
  else if(typeof value!==schema.type)bad(label+': неверный тип значения.');
  if(schema.enum&&!schema.enum.includes(value))bad(label+': выберите значение из списка.');
 }catch(e){bad(e.message);}
 return errors;
}
export function materializeValue(schema,value,context){
 if(value===undefined)value=schemaDefaults(schema);
 if(value?.op){value=context.evaluate(value);}
 if(['variableValue','signalPayload','componentArguments','componentResult'].includes(schema.type)){const s=dependentSchema(schema,context);if(!s)throw Error('Не назначена связь значения.');return materializeValue(s,value,context);}
 if(schema.type==='union'){const variant=schema.variants.find(s=>!validateValue(s,value,context).length);if(!variant)throw Error('Неверный тип значения.');return materializeValue(variant,value,context);}
 if(schema.type==='reference')return resolveReference(value,schema.referenceType,context);
 if(schema.type==='collection'&&value?.mode==='tag')return resolveReference(value,'object',context);
 if(schema.type==='collection')return value.map(x=>materializeValue(schema.items,x,context));
 if(schema.type==='object')return Object.fromEntries(Object.entries(schema.properties).filter(([k,s])=>value?.[k]!==undefined||schemaDefaults(s)!==undefined).map(([k,s])=>[k,materializeValue(s,value?.[k],context)]));
 return structuredClone(value);
}
