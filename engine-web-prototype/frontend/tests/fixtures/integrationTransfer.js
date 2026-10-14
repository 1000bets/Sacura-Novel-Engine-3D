import {createMechanicsProject} from '../../src/projectLifecycle.js';
import {createMechanicDefinition,addMechanicInstance,descriptionFor} from '../../src/mechanicDescriptions.js';
import {createRule} from '../../src/ruleModel.js';
import {makeAction} from '../../src/model.js';
import {createWidget,createWidgetElement,rectTransform} from '../../src/widgetModel.js';
import {reference} from '../../src/valueSchemas.js';
import {exportAuthoringPackage,planPackageImport} from '../../src/mechanicPackages.js';
import {instantiateAuthoringTemplate} from '../../src/authoringTemplates.js';
import {inputRecords} from '../../src/packageReferences.js';
export const transferMeta={name:'Интеграционная награда',author:'Автор A',license:'CC0-1.0'};
export const png='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==';
export function receivingProject(name){const p=createMechanicsProject(name);p.variables={count:0,done:false,permission:true};p.variableTypes={count:'integer',done:'boolean',permission:'boolean'};return p;}
export function transferSource(kind){
 const p=receivingProject('Источник A');p.events=[];p.functions=[];p.actionTemplates=[];p.groupTemplates=[];
 const widget=createWidget('transfer-widget','hud','Проверка переноса');widget.elements=[];const layout=(x,y,w=400)=>rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x,y,width:w,height:64});
 widget.elements.push(createWidgetElement('transfer-count','text',{name:'Полученный ресурс',binding:{source:'variable',path:'count',format:'Ресурс: {value}'},layout:layout(30,30)}),createWidgetElement('transfer-button','button',{name:'Получить ресурс',text:'Получить ресурс',action:['event','function'].includes(kind)?'event':'none',eventId:'transfer-entry',layout:layout(30,110)}),createWidgetElement('transfer-image','image',{name:'Локальная картинка',src:png,layout:layout(450,30,70)}));p.widgets=p.widgets.filter(w=>w.kind!=='hud');p.widgets.push(widget);p.gameplay.hudWidgetId=widget.id;p.ui.hud=widget.id;
 const fn={id:'transfer-function',name:'Начислить ресурс',parameters:[{name:'amount',label:'Количество',schema:{type:'integer',min:1},default:1},{name:'controls',label:'Интерфейс',schema:{type:'reference',referenceType:'widget'},default:reference('widget',widget.id)}],locals:{uses:0},localSchema:{type:'object',properties:{uses:{type:'integer',default:0}}},resultSchema:{type:'void'},entry:'call-worker',nodes:[{id:'call-worker',kind:'call',functionId:'transfer-worker',args:{amount:{op:'get',path:'args.amount'}},position:{x:120,y:80},comment:'Вызывает вложенную редактируемую функцию.',next:'use-count'},{id:'use-count',kind:'set',path:'local.uses',value:{op:'add',a:{op:'get',path:'local.uses'},b:1},position:{x:360,y:80}}]};
 const worker={id:'transfer-worker',name:'Вложенная функция',parameters:[{name:'amount',schema:{type:'integer'},default:1}],locals:{picture:{src:png}},resultSchema:{type:'void'},entry:'write-count',nodes:[{id:'write-count',kind:'set',path:'global.count',value:{op:'add',a:{op:'get',path:'global.count'},b:{op:'get',path:'args.amount'}},next:'leaf',position:{x:160,y:90},comment:'Ресурс и оформление сохраняются при переносе.'},{id:'leaf',kind:'event',eventId:'transfer-leaf',position:{x:360,y:90}}]};
 const event={id:'transfer-entry',name:'Переносимое действие',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'transfer-group',name:'Вызвать функцию',actions:[{...makeAction('gameplay'),id:'transfer-call',command:'callFunction',value:{functionId:fn.id,args:{}},duration:0}]}]};const leaf={id:'transfer-leaf',name:'Подтвердить результат',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:'transfer-leaf-group',name:'Подтверждение',actions:[{...makeAction('variable','done',true),id:'transfer-done',duration:0}]}]};p.functions.push(fn,worker);p.events.push(event,leaf);p.graphLayout={[worker.id]:{zoom:.8,pan:[10,20]}};
 const d=createMechanicDefinition(p,{id:'user.integration.reward',name:'Награда интеграции',parameters:{type:'object',properties:{amount:{type:'integer',label:'Количество',min:1,default:1}}},exposed:['amount'],stateSchema:{type:'object',properties:{uses:{type:'integer',default:0}}},initialState:{uses:0},logic:{functionId:fn.id},rules:[createRule({id:'reward',scope:{kind:'project',id:p.id},signal:'button',filter:{source:{...reference('widget',widget.id),elementId:'transfer-button'}},condition:{op:'get',path:'global.permission'},actions:[{kind:'function',functionId:fn.id,args:{amount:{op:'get',path:'args.amount'}}}]})]});const m=addMechanicInstance(p,d.id,{kind:'project',id:p.id});
 if(['event','function'].includes(kind)){p.gameplay.mechanics=[];p.gameplay.mechanicDefinitions=[];}
 return {project:p,root:kind==='event'?event:kind==='function'?fn:kind==='mechanic'?m:p};
}
export async function importTransfer(pkg,name){
 if(pkg.roots.kind==='template')return instantiateAuthoringTemplate(pkg,name);
 const p=receivingProject(name),bindings=Object.fromEntries(pkg.inputs.map(i=>[i.id,inputRecords(p,i).find(r=>r.name===i.name)?.id||inputRecords(p,i)[0]?.id])),plan=await planPackageImport(p,pkg,bindings),next=plan.apply(p);next.gameplay.hudWidgetId=next.widgets.find(w=>w.name==='Проверка переноса').id;return next;
}
export function transferRoot(p,kind){return kind==='template'?p:kind==='mechanic'?p.gameplay.mechanics.find(m=>descriptionFor(p,m.definitionId,m.definitionVersion)?.name==='Награда интеграции'):kind==='function'?p.functions.find(f=>f.name==='Начислить ресурс'):p.events.find(e=>e.name==='Переносимое действие');}
export async function exportTransfer(p,kind,author='Автор B'){return exportAuthoringPackage(p,transferRoot(p,kind),{...transferMeta,kind,author});}
