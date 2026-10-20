import {descriptionFor} from './mechanicDescriptions.js';
import {componentOwnerName,componentInstanceName} from './componentCatalog.js';

// Keep the innermost failed receiver. A caller can have the same method key.
export function componentError(error,project,instanceId,methodId){
 if(['INSTANCE_STOPPED','CANCELLED'].includes(error.message))return error;
 const m=project.gameplay?.mechanics.find(m=>m.id===instanceId),d=m&&descriptionFor(project,m.definitionId,m.definitionVersion);
 if(!m||!d)return error;
 const method=d.methods?.[methodId],frame={instanceId:m.id,definitionId:d.id,definitionVersion:d.version,resourceId:d.logicResourceId||'',functionId:method?.logic?.functionId||'',methodId:methodId||'',owner:{...m.scope},ownerName:componentOwnerName(project,m.scope),instanceName:componentInstanceName(project,m),componentName:d.name,methodName:method?.name||methodId||'Подготовка компонента'};
 error.componentTrace??=[];
 if(!error.componentTrace.some(f=>f.instanceId===m.id&&f.methodId===frame.methodId))error.componentTrace.push(frame);
 if(!error.componentDiagnostic){
  const reason=error.message;
  error.message=frame.ownerName+' → '+frame.instanceName+' ('+d.name+') → '+frame.methodName+' · '+error.message;
  error.componentId=d.id;error.instanceId=m.id;error.methodId=frame.methodId;
  error.componentDiagnostic={...frame,message:error.message,reason,...(error.nodeId?{nodeId:error.nodeId}:{}),...(error.line?{line:error.line,column:error.column||1}:{})};
 }
 return error;
}
