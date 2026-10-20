import {schemaFromValue} from './valueSchemas.js';
import {schemaTypeLabel} from './referencePresentation.js';
import {parameterSchema} from './authoredMechanics.js';
export function contextFields(project,{parameters={},stateSchema,fn}={}){
 const out=[];const visit=(schema,path,label)=>{if(schema.type==='object')for(const [key,s]of Object.entries(schema.properties))visit(s,path+'.'+key,label+' · '+(s.label||key));else out.push([path,label+' · '+schemaTypeLabel(schema),schema]);};
 for(const [key,value]of Object.entries(project.variables||{}))visit(schemaFromValue(value,project.variableTypes?.[key]),'global.'+key,'Проект · '+key);
 if(fn&&!stateSchema){const ds=project.gameplay?.mechanicDefinitions?.filter(d=>d.logic?.functionId===fn.id||d.rules?.some(r=>r.actions?.some(a=>a.functionId===fn.id))||d.machine?.transitions?.some(t=>t.actions?.some(a=>a.functionId===fn.id)))||[];if(ds.length)stateSchema={type:'object',properties:Object.assign({},...ds.map(d=>d.stateSchema.properties))};}
 const props=fn?Object.fromEntries(fn.parameters.map(p=>[p.name,parameterSchema(p)])):parameters;
 for(const [key,s]of Object.entries(props))visit(s,'args.'+key,'Параметр · '+(s.label||key));
 for(const [key,s]of Object.entries((stateSchema||fn?.localSchema||schemaFromValue(fn?.locals||{})).properties))visit(s,'local.'+key,'Экземпляр / локальная · '+(s.label||key));return out;
}
