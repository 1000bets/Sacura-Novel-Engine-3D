import {registerOperation} from './operationRegistry.js';
import {variableKey} from './valueSchemas.js';
const contexts=['event','function','rule','mechanic','widget'];
const scalar={type:'union',variants:[{type:'number'},{type:'boolean'},{type:'string'}]};
const install=(id,label,properties,result,execute,extra={},validate)=>registerOperation({id,version:1,source:{path:'src/coreOperations.js',line:1},parameters:{type:'object',properties},result,contexts,editor:{label,category:'Общая логика'},...extra},execute,validate);
install('sacura.variable.set','Изменить переменную',{
 variable:{type:'reference',referenceType:'variable',label:'Переменная'},value:{type:'variableValue',parameter:'variable',label:'Новое значение'}
},{type:'void'},async(p,c)=>{const key=variableKey(c.project,p.variable),oldValue=structuredClone(c.variables[key]);c.variables[key]=structuredClone(p.value);c.emit();await c.signal('sacura.variable.changed',{variableId:p.variable,oldValue,newValue:p.value});});
install('sacura.signal.emit','Отправить сигнал',{
 signal:{type:'reference',referenceType:'signal',label:'Пользовательский сигнал'},payload:{type:'signalPayload',parameter:'signal',label:'Данные сигнала',default:{}}
},{type:'void'},async(p,c)=>{await c.signal(p.signal,p.payload);});
install('sacura.condition.compare','Сравнить значения',{
 left:{...scalar,label:'Первое значение',default:0},operator:{type:'string',enum:['eq','ne','lt','lte','gt','gte'],default:'eq',label:'Сравнение',options:{eq:'Равно',ne:'Не равно',lt:'Меньше',lte:'Не больше',gt:'Больше',gte:'Не меньше'}},right:{...scalar,label:'Второе значение',default:0}
},{type:'boolean'},(p,c)=>{if(typeof p.left!==typeof p.right||!['eq','ne'].includes(p.operator)&&typeof p.left!=='number')throw Error('Для сравнения нужны значения одного типа; порядок сравнивает числа.');return c.evaluate({op:p.operator,a:p.left,b:p.right});},{pure:true},p=>p.left?.op||p.right?.op?[]:typeof p.left!==typeof p.right||!['eq','ne'].includes(p.operator)&&typeof p.left!=='number'?[{path:'left',message:'Сравнивайте значения одного типа; порядок сравнивает числа.'}]:[]);
install('sacura.event.run','Запустить Event',{
 event:{type:'reference',referenceType:'event',label:'Событие'},join:{type:'string',enum:['NONE','STARTED','FLOW_END','EVENT_END'],default:'FLOW_END',label:'Ожидание',options:{NONE:'Не задерживает поток',STARTED:'Ждём запуска',FLOW_END:'Ждём готовности',EVENT_END:'Ждём завершения'}}
},{type:'void'},(p,c)=>c.runEvent(p.event,p.join));

install('sacura.story.enter','Продолжить историю',{beat:{type:'reference',referenceType:'beat',label:'Нода истории'}},{type:'void'},(p,c)=>c.enterStory(p.beat));
install('sacura.scene.enter','Перейти в сцену',{scene:{type:'reference',referenceType:'scene',label:'Сцена'}},{type:'void'},(p,c)=>c.enterScene(p.scene));
install('sacura.mechanic.call','Вызвать механику',{mechanic:{type:'reference',referenceType:'mechanic',label:'Механика'}},{type:'void'},async(p,c)=>{await c.capabilities.callMechanic(p.mechanic,{},c);});
