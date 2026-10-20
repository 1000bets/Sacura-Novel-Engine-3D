import {descriptionFor} from './mechanicDescriptions.js';
import {componentStateView} from './componentFieldBindings.js';
import {resolveReference,reference} from './valueSchemas.js';
import {componentOwnerName} from './componentCatalog.js';
export function componentBindingSchema(project,b){const id=resolveReference(b.instance,'mechanic',{project}),m=project.gameplay.mechanics.find(m=>m.id===id),d=descriptionFor(project,m.definitionId,m.definitionVersion),schema=d?.stateSchema.properties[b.field];if(!d?.componentVersion||!schema||schema.type!==b.valueType)throw Error('Привязка состояния компонента недоступна. Выберите подключение и объявленное поле.');return {m,d,schema};}
export function componentBindingValue(project,state,b,variables=project.variables){const {m,d}=componentBindingSchema(project,b);return state?.mechanicStates?.[m.id]?componentStateView(project,m,d,state.mechanicStates[m.id],variables)[b.field]:undefined;}
export function availableComponentBindings(project){return project.gameplay.mechanics.flatMap(m=>{const d=descriptionFor(project,m.definitionId,m.definitionVersion);return d?.componentVersion===1?Object.entries(d.stateSchema.properties).map(([field,schema])=>({name:componentOwnerName(project,m.scope)+' · '+(m.name||d.name)+' · '+(schema.label||field),binding:{source:'component',instance:reference('mechanic',m.id),field,valueType:schema.type}})):[];});}
