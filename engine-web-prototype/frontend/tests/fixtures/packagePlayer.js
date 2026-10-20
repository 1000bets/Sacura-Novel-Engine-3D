import {source,fixture,meta,bindings,conditionOf,eventOf,requirePermission} from './packageDoor.js';
import {exportAuthoringPackage,planPackageImport} from '../../src/mechanicPackages.js';
import {createSignalDefinition,descriptionFor} from '../../src/mechanicDescriptions.js';
import {reference} from '../../src/valueSchemas.js';
import {createWidget,createWidgetElement,rectTransform} from '../../src/widgetModel.js';
import {makeAction} from '../../src/model.js';

// A real authoring fixture: controls are native widgets, expressions, Events and rules.
export async function packagePlayerFixture(){
 const {p,d,m}=source(),signal=createSignalDefinition(p,{id:'user.signal.try-door',name:'Попытка открыть дверь'});
 p.variables.opened=false;(p.variableTypes??={}).opened='boolean';
 const e=eventOf(p,d.machine.transitions[0]);e.groups[0].actions.push({...makeAction('variable','opened',true),duration:0});
 d.machine.transitions.push({...structuredClone(d.machine.transitions[0]),id:'open-ui',signal:{type:signal.id},filter:{}});
 const item=p.gameplay.items[0],widget=createWidget('door-controls','hud','Проверка импортированной двери');widget.elements=[];
 const layout=(x,y,width=280)=>rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x,y,width,height:52});
 widget.elements.push(createWidgetElement('take-key','button',{text:'Получить ключ',name:'Получить ключ',action:'gameplay',command:'addItem',value:{itemId:item.id,amount:1},layout:layout(30,90)}),createWidgetElement('attempt','button',{text:'Открыть дверь',name:'Открыть дверь',action:'gameplay',command:'sacura.signal.emit',operationVersion:1,value:{signal:reference('signal',signal.id),payload:{}},layout:layout(330,90)}),createWidgetElement('opened-text','text',{name:'Состояние двери',binding:{source:'variable',path:'opened',format:'Дверь открыта: {value}'},layout:layout(30,30,570)}),createWidgetElement('image','image',{name:'Локальное изображение',src:'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==',layout:layout(630,90,52)}));
 p.widgets.push(widget);d.parameters.properties.controls={type:'reference',referenceType:'widget',label:'Интерфейс',default:reference('widget',widget.id)};
 const a=await exportAuthoringPackage(p,m,meta),b=fixture('B');b.variables.opened=false;(b.variableTypes??={}).opened='boolean';const imported=(await planPackageImport(b,a,bindings(a,b))).apply(b),mb=imported.gameplay.mechanics.find(m=>m.definitionId),db=descriptionFor(imported,mb.definitionId),f=conditionOf(imported,db.machine.transitions[0]);
 imported.variables.permission=true;imported.variableTypes.permission='boolean';requirePermission(imported,f);const event=eventOf(imported,db.machine.transitions[0]);event.groups[0].actions.push({...makeAction('highlight',b.objects[0].id,'Подсветить'),duration:0});
 const variant=await exportAuthoringPackage(imported,mb,{...meta,author:'Автор B'}),c=fixture('C');c.variables.opened=false;c.variables.permission=true;(c.variableTypes??={}).opened='boolean';c.variableTypes.permission='boolean';const project=(await planPackageImport(c,variant,bindings(variant,c))).apply(c);project.gameplay.hudWidgetId=project.widgets.find(w=>w.name===widget.name).id;
 return {original:a,variant,project};
}
