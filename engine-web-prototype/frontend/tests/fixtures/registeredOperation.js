import {registerOperation} from '../../src/operationRegistry.js';
// A complete trusted extension module: no central dispatcher/catalog edits.
export const operation=registerOperation({id:'test.objects.count',version:1,
 parameters:{type:'object',properties:{objects:{type:'collection',items:{type:'reference',referenceType:'object'},default:[],label:'Объекты'},factor:{type:'integer',min:1,default:1,label:'Множитель'}}},
 result:{type:'integer'},contexts:['event','function','rule','mechanic'],editor:{label:'Посчитать объекты',category:'Проверка'}
},p=>p.objects.length*p.factor);
