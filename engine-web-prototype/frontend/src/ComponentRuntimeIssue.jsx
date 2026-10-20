import React from 'react';

export default function ComponentRuntimeIssue({diagnostic,onOpen}){
 if(!diagnostic)return null;
 return <section className="component-runtime-issue" role="alert" aria-label="Ошибка компонента">
  <strong>{diagnostic.ownerName} → {diagnostic.instanceName} ({diagnostic.componentName}) · {diagnostic.methodName}</strong>
  <p>{diagnostic.reason||diagnostic.message}</p>
  {onOpen&&(diagnostic.resourceId||diagnostic.functionId)&&<button type="button" onClick={()=>onOpen(diagnostic)}>Остановить и открыть место ошибки</button>}
 </section>;
}
