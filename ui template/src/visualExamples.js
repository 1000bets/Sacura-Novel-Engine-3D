import {makeAction} from './model.js';

export function extendVisualExamples(p){
 if(p.visualExamplesVersion===1)return p;
 const presets=[
 ['rain','Дождь','Окружение','weather','world','Дождь'],['storm','Гроза и молнии','Окружение','weather','world','Гроза'],['fog','Утренний туман','Окружение','weather','world','Туман'],['snow','Снег','Окружение','weather','world','Снег'],['clear','Ясное небо','Окружение','weather','world','Ясно'],
 ['night','Ночь','Свет','time','world','Ночь'],['dawn','Рассвет','Свет','time','world','Рассвет'],['day','Дневной свет','Свет','time','world','День'],['sunset','Закат','Свет','time','world','Закат'],['dim','Приглушить свет','Свет','lighting','world','Приглушить'],['warm','Тёплый свет','Свет','lighting','world','Тёплый свет'],['cold','Холодный свет','Свет','lighting','world','Холодный свет'],
 ['walk','Алиса идёт к окну','Персонажи','move','alice','окно'],['return','Алиса возвращается','Персонажи','move','alice','стол'],['dance','Алиса танцует','Персонажи','pose','alice','танец'],['sad','Боб грустит','Персонажи','pose','bob','грусть'],['smile','Алиса улыбается','Персонажи','pose','alice','улыбка'],['think','Боб задумался','Персонажи','pose','bob','задумчивость'],
 ['close','Крупный план','Камера','camera','camera','Крупный план'],['wide','Общий план','Камера','camera','camera','Общий план'],['item','План предмета','Камера','camera','camera','План предмета'],
 ['open','Открыть дверь','Предметы','door','door','Открыть'],['shut','Закрыть дверь','Предметы','door','door','Закрыть'],['hint','Подсветить письмо','Предметы','highlight','letter','Подсветить'],['hide','Спрятать письмо','Предметы','visibility','letter','Скрыть'],['show','Показать письмо','Предметы','visibility','letter','Показать'],
 ['fireflies','Светлячки','Частицы','particles','world','Светлячки'],['petals','Лепестки на ветру','Частицы','particles','world','Лепестки'],['particles-off','Убрать частицы','Частицы','particles','world','Выключить'],
 ];
 for(const [id,name,category,type,target,value]of presets){
  const eventId='visual-'+id;if(p.events.some(e=>e.id===eventId))continue;
  const action={...makeAction(type,target,value),id:eventId+'-a',duration:3};
  p.events.push({id:eventId,name,category,description:'Визуальный пример · можно изменить и добавить в любую реплику',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[{id:eventId+'-g',name:'Применить к сцене',actions:[action]}]});
 }
 if(!p.events.some(e=>e.id==='visual-performance')){
  const group=(id,name,actions)=>({id:'visual-performance-'+id,name,actions:actions.map((a,i)=>({...makeAction(...a),id:'visual-performance-'+id+'-'+i,duration:2.5}))});
  p.events.push({id:'visual-performance',name:'Небольшая постановка',category:'Постановка',description:'Погода и свет вместе → движение → танец и лепестки → общий план',owner:'SubScene',retention:'AUTO_CLOSE_ON_FLOW_END',groups:[
   group('1','Дождь и вечерний свет',[['weather','world','Дождь'],['time','world','Закат'],['wait','world','Пауза']]),
   group('2','Алиса подходит к окну',[['move','alice','окно']]),
   group('3','Танец и лепестки вместе',[['pose','alice','танец'],['particles','world','Лепестки'],['camera','camera','Крупный план'],['wait','world','Пауза']]),
   group('4','Снова общий план',[['camera','camera','Общий план'],['weather','world','Ясно']]),
  ]});
 }
 p.visualExamplesVersion=1;return p;
}
