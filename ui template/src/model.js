export const uid = (prefix='id') => `${prefix}-${globalThis.crypto?.randomUUID?.() || Math.random().toString(36).slice(2)}`;
export const TYPES = {
 move: {label:'Переместить',icon:'Footprints',domain:'Положение',completion:'FINITE'},
 pose: {label:'Изменить позу',icon:'PersonStanding',domain:'Анимация',completion:'INSTANT'},
 camera:{label:'Сменить план',icon:'Video',domain:'Камера',completion:'INSTANT'},
 music:{label:'Включить музыку',icon:'Music2',domain:'Музыка',completion:'CONTINUOUS'},
 pause:{label:'Приостановить музыку',icon:'Pause',domain:'Управление музыкой',completion:'INSTANT'},
 resume:{label:'Продолжить музыку',icon:'Play',domain:'Управление музыкой',completion:'INSTANT'},
 stop:{label:'Остановить музыку',icon:'Square',domain:'Управление музыкой',completion:'INSTANT'},
 duck:{label:'Приглушить под голос',icon:'AudioLines',domain:'Сайдчейн',completion:'FINITE'},
 weather:{label:'Изменить погоду',icon:'CloudRain',domain:'Погода',completion:'INSTANT'},
 time:{label:'Изменить время суток',icon:'Sun',domain:'Время суток',completion:'INSTANT'},
 sound:{label:'Проиграть звук',icon:'Volume2',domain:'Звук',completion:'FINITE'},
 wait:{label:'Пауза / ожидание',icon:'Hourglass',domain:null,completion:'FINITE'},
 variable:{label:'Изменить условие',icon:'SlidersHorizontal',domain:null,completion:'INSTANT'},
 visibility:{label:'Показать объект',icon:'Box',domain:'Видимость',completion:'INSTANT'},
 lighting:{label:'Настроить свет',icon:'Lamp',domain:'Освещение',completion:'INSTANT'},
 particles:{label:'Частицы в воздухе',icon:'Sparkles',domain:'Частицы',completion:'INSTANT'},
 door:{label:'Открыть / закрыть дверь',icon:'DoorOpen',domain:'Дверь',completion:'INSTANT'},
 highlight:{label:'Подсветить предмет',icon:'Scan',domain:'Подсказка',completion:'INSTANT'},
};
const actionDefaults={move:['alice','окно'],pose:['alice','улыбка'],camera:['camera','Общий план'],music:['audio','Главная тема'],pause:['audio','Пауза'],resume:['audio','Продолжить'],stop:['audio','Остановить'],duck:['audio','Под голос'],weather:['world','Дождь'],time:['world','Ночь'],sound:['world','Звук'],wait:['world','Пауза'],variable:['trust','+1'],visibility:['letter','Показать'],lighting:['world','Тёплый свет'],particles:['world','Светлячки'],door:['door','Открыть'],highlight:['letter','Подсветить']};
export const makeAction = (type='move',target,value) => ({id:uid('action'),type,target:target??actionDefaults[type][0],value:value??actionDefaults[type][1],wait:TYPES[type].completion==='CONTINUOUS'?'STARTED':'COMPLETED',scope:TYPES[type].completion==='CONTINUOUS'?'EVENT':'SELF',conflict:['camera','music','weather'].includes(type)?'REPLACE_CURRENT':'FAIL_NEW',duration:2,queueTimeout:10,startTimeout:5,executionTimeout:30,stopTimeout:3,onFailure:'Остановить событие',finalState:'Сохранить результат',fallback:'Безопасное исходное состояние'});
const event = (id,name,actions,more={})=>({id,name,description:'Готовая постановка для повторного использования',groups:[{id:`${id}-g1`,name:'Основное действие',actions}],retention:'AUTO_CLOSE_ON_FLOW_END',owner:'SubScene',...more});
const bind=(eventId,extra={})=>({id:uid('binding'),eventId,hook:'ON_START',join:'EVENT_END',overrides:{},...extra});
export function createProject(){
 const events=[
 event('arrive','Алиса входит в гостиную',[makeAction('move','alice','камин'),makeAction('sound','world','Шаги по дереву')]),
 event('window','Алиса идёт к окну',[makeAction('move','alice','окно')]),
 event('desk','Алиса подходит к столу',[makeAction('move','alice','стол')]),
 event('look','Боб замечает письмо',[makeAction('pose','bob','задумчивость'),makeAction('camera','camera','Крупный план')]),
 event('bgm','Тихое фортепиано',[makeAction('music','audio','After the rain')],{retention:'HOLD_UNTIL_REPLACED',channel:'Audio.BGM',owner:'Scene'}),
 event('rain','Дождь за окном',[makeAction('weather','world','Дождь'),makeAction('sound','world','Шум дождя')]),
 event('voice','Голос поверх музыки',[makeAction('sound','alice','Реплика Алисы'),makeAction('duck','audio','−12 dB')]),
 event('music-pause','Пауза в музыке',[makeAction('pause','audio','Сохранить позицию')]),
 event('music-resume','Вернуть музыку',[makeAction('resume','audio','С сохранённой позиции')]),
 event('music-stop','Музыка затихает',[makeAction('stop','audio','Плавно за 2 секунды')]),
 event('night','Наступает ночь',[makeAction('time','world','Ночь'),makeAction('weather','world','Гроза')]),
 event('clear','Дождь заканчивается',[makeAction('weather','world','Ясно'),makeAction('time','world','Рассвет')]),
 event('letter','Письмо становится доступным',[makeAction('visibility','letter','Показать')]),
 event('trust','Алиса доверяет Бобу',[makeAction('variable','trust','+1'),makeAction('pose','alice','улыбка')]),
 event('fire','Камин потрескивает',[makeAction('sound','fireplace','Треск поленьев')]),
 event('wait-a','Открыть дверь',[{...makeAction('wait','door','Разрешение Боба'),waitFor:'wait-b'}]),
 event('wait-b','Разрешение Боба',[{...makeAction('wait','bob','Открытая дверь'),waitFor:'wait-a'}]),
 ];
 const objects=[{id:'alice',name:'Алиса',type:'Персонаж',color:'#b4a0dd',position:'камин',active:true},{id:'bob',name:'Боб',type:'Персонаж',color:'#d5b183',position:'диван',active:true},{id:'letter',name:'Письмо',type:'Активный меш',color:'#f1dfb2',position:'стол',active:true,interaction:'Осмотреть письмо'},{id:'fireplace',name:'Камин',type:'Меш',color:'#b57f50',active:true},{id:'door',name:'Дверь в сад',type:'Активный меш',color:'#7fa3a0',active:true,interaction:'Открыть дверь'}];
 const mk=(id,speaker,text,events=[],extra={})=>({id,kind:'dialogue',speaker,text,bindings:events.map(x=>typeof x==='string'?bind(x):x),mode:'SEQUENTIAL',...extra});
 const chapters=[
 {id:'arrival',name:'Возвращение',description:'Гостиная · закат',beats:[
 mk('a1','Рассказчик','Дом встретил её запахом мокрого дерева.',[bind('bgm',{join:'FLOW_END',hook:'BEFORE'}),'rain','arrive']),
 mk('a2','Боб','Ты всё-таки вернулась.',['look']),mk('a3','Алиса','Я обещала прийти до дождя.',['voice']),mk('a4','Боб','Он начался ещё час назад.',['fire']),
 mk('a5','Алиса','Здесь ничего не изменилось.'),mk('a6','Боб','Некоторые вещи просто ждут своего времени.'),mk('a7','Рассказчик','Часы пробили восемь.'),mk('a8','Алиса','А некоторые ждут слишком долго.'),
 ]},
 {id:'letter-scene',name:'Письмо на столе',description:'Разговор, который всё меняет',beats:[
 mk('l1','Боб','Я нашёл кое-что среди её вещей.',['look']),
 mk('l2','Алиса','Ты ведь не открывал его?',['voice','letter']),
 mk('l3','Рассказчик','Алиса отворачивается к окну.',['window','desk'],{mode:'PARALLEL'}),
 mk('choice1','Алиса','Что сказать Бобу?',[],{kind:'choice',choices:[{id:'honest',label:'Рассказать правду',condition:'trust',threshold:2,next:'lh1'},{id:'hide',label:'Спрятать письмо',condition:'always',next:'ls1'},{id:'ask',label:'Спросить о матери',condition:'always',next:'la1'}]}),
 mk('lh1','Алиса','Это последнее письмо от мамы.',['trust'],{branch:'honest'}),mk('lh2','Боб','Ты можешь прочитать его здесь.',[],{branch:'honest'}),mk('lh3','Алиса','Только останься рядом.',[],{branch:'honest'}),
 mk('ls1','Алиса','Это старые бумаги. Ничего важного.',[],{branch:'hide'}),mk('ls2','Боб','Тогда почему у тебя дрожат руки?',[],{branch:'hide'}),mk('ls3','Алиса','Наверное, из-за холода.',[],{branch:'hide'}),
 mk('la1','Алиса','Она говорила обо мне?',['look'],{branch:'ask'}),mk('la2','Боб','Каждый день. Даже когда молчала.',[],{branch:'ask'}),mk('la3','Алиса','Я не знала.',[],{branch:'ask'}),
 mk('merge1','Рассказчик','За окном стало совсем темно.',['night'],{kind:'merge'}),mk('l4','Боб','Письмо всё ещё на столе.'),
 ]},
 {id:'search',name:'Пока тикают часы',description:'Свободное исследование · ожидание игрока',beats:[
 mk('s1','Боб','Я подожду у камина.',['music-pause','fire']),
 mk('s2','Рассказчик','Найди письмо на столе.',[],{kind:'gate',signal:'letter',timeout:45,fallback:'Подсветить письмо'}),
 mk('s3','Алиса','Бумага всё ещё пахнет её духами.',['music-resume','voice']),mk('s4','Рассказчик','На конверте только одно слово: «Домой».',['look']),
 mk('s5','Алиса','Почему она не отправила его?'),mk('s6','Боб','Возможно, хотела сказать всё сама.'),
 mk('choice2','Алиса','Прочитать письмо?',[],{kind:'choice',choices:[{id:'read',label:'Прочитать вслух',condition:'letter',next:'sr1'},{id:'keep',label:'Оставить на потом',condition:'always',next:'sk1'}]}),
 mk('sr1','Алиса','«Если ты читаешь это, значит, нашла дорогу».',['voice'],{branch:'read'}),mk('sr2','Боб','Это очень на неё похоже.',[],{branch:'read'}),mk('sk1','Алиса','Я пока не готова.',[],{branch:'keep'}),mk('sk2','Боб','Мы никуда не торопимся.',[],{branch:'keep'}),mk('merge2','Рассказчик','Гром заставил стёкла задрожать.',['night'],{kind:'merge'}),
 ]},
 {id:'storm',name:'По ту сторону двери',description:'Гроза · параллельная постановка',beats:[
 mk('t1','Боб','Кажется, дверь в сад открыта.',['wait-a','wait-b'],{mode:'PARALLEL'}),
 mk('t2','Алиса','Я посмотрю.',['window','look'],{mode:'PARALLEL'}),mk('t3','Рассказчик','Ветер переворачивает страницу.',['rain','fire']),
 mk('choice3','Алиса','Куда пойти?',[],{kind:'choice',choices:[{id:'garden',label:'Выйти в сад',condition:'trust',threshold:3,next:'tg1'},{id:'stay',label:'Остаться с Бобом',condition:'always',next:'ts1'}]}),
 mk('tg1','Алиса','Она посадила это дерево в год моего рождения.',[],{branch:'garden'}),mk('tg2','Боб','И каждый год отмечала на нём твой рост.',[],{branch:'garden'}),mk('ts1','Алиса','Расскажи мне ещё что-нибудь о ней.',[],{branch:'stay'}),mk('ts2','Боб','Она всегда оставляла свет на крыльце.',[],{branch:'stay'}),mk('merge3','Рассказчик','Дождь постепенно стихает.',['clear'],{kind:'merge'}),
 ]},
 {id:'confession',name:'То, о чём мы молчали',description:'Вложенные условия · общий выход',beats:[
 mk('c1','Алиса','Я думала, что она сердится на меня.',['voice']),mk('c2','Боб','Она скучала. Это совсем другое.',['look']),mk('c3','Алиса','Можно начать сначала?',['trust']),
 mk('choice4','Боб','Как закончится этот разговор?',[],{kind:'choice',choices:[{id:'home',label:'Остаться дома',condition:'trust',threshold:3,next:'ch1'},{id:'leave',label:'Пообещать вернуться',condition:'always',next:'cl1'}]}),
 mk('ch1','Алиса','Наверное, я всё ещё помню, где моя комната.',[],{branch:'home'}),mk('ch2','Боб','Там всё готово.',[],{branch:'home'}),mk('cl1','Алиса','Я приеду на выходных.',[],{branch:'leave'}),mk('cl2','Боб','Ключ можешь оставить себе.',[],{branch:'leave'}),mk('merge4','Рассказчик','Тишина больше не кажется пустой.',[],{kind:'merge'}),
 ]},
 {id:'dawn',name:'До первого света',description:'Рассвет · завершение и очистка',beats:[
 mk('d1','Рассказчик','Над садом светлеет небо.',['clear']),mk('d2','Алиса','Я поставлю чайник.',['desk']),mk('d3','Боб','Как раньше?'),mk('d4','Алиса','Как раньше.',['trust']),mk('d5','Рассказчик','Камин догорает. На столе лежит открытое письмо.',['music-stop']),mk('d6','Рассказчик','Конец главы.',[],{kind:'end'}),
 ]},
 ];
 // Nested branches deliberately reuse sound, camera and motion templates across distant story paths.
 const nested=[
  ['lh2','honest','Спросить, почему письмо осталось здесь?','Спросить прямо','Я боялся, что ты снова уедешь.','Но у тебя было право знать.','Дать ему время','Не отвечай сейчас.','Я тоже долго не решалась вернуться.'],
  ['la2','ask','О каком воспоминании спросить?','О последнем лете','Она каждое утро выходила в сад.','Говорила: яблоня ждёт тебя.','О старом доме','Она не меняла замок.','Хотела, чтобы твой ключ всегда подходил.'],
  ['sr2','read','Продолжить читать?','Прочитать следующую страницу','«Не вини себя за расстояние».','«Некоторые дороги нужны, чтобы вернуться».','Сложить письмо','На сегодня достаточно.','Остальное я прочту завтра.'],
  ['tg2','garden','Что рассмотреть в саду?','Отметки на дереве','Вот эта — когда мне было двенадцать.','А рядом её почерк.','Свет на крыльце','Лампочку меняли недавно.','Кто-то всё это время оставлял свет.'],
  ['ch2','home','Что сказать напоследок?','Попросить прощения','Прости, что меня не было рядом.','Ты рядом сейчас.','Поговорить о завтра','Завтра начнём с сада.','А потом разберём чердак.'],
  ['d2',null,'Утро начинается с маленького выбора.','Заварить мамин чай','На полке нашлась жестяная банка.','Тот самый запах бергамота.','Открыть окно','В комнату вошёл свежий воздух.','Дождь наконец закончился.'],
 ];
 nested.forEach(([before,parent,text,left,l1,l2,right,r1,r2],i)=>{
  const id=`nested-${i}`,l=`${id}-left`,r=`${id}-right`,c=chapters.find(c=>c.beats.some(b=>b.id===before));
  const nodes=[mk(id,'Алиса',text,[],{kind:'choice',...(parent?{branch:parent}:{}),choices:[{id:l,label:left,condition:i===2?'letter':'trust',threshold:2,next:`${l}-1`},{id:r,label:right,condition:'always',next:`${r}-1`}]}),
  mk(`${l}-1`,i%2?'Алиса':'Боб',l1,['voice'],{branch:l}),mk(`${l}-2`,'Алиса',l2,['look'],{branch:l}),mk(`${r}-1`,'Алиса',r1,['fire'],{branch:r}),mk(`${r}-2`,'Боб',r2,[],{branch:r})];
  c.beats.splice(c.beats.findIndex(b=>b.id===before),0,...nodes);
 });
 events.find(e=>e.id==='arrive').groups.push({id:'arrive-g2',name:'Остановиться и посмотреть на Боба',actions:[makeAction('pose','alice','задумчивость'),makeAction('camera','camera','Общий план')]});
 return {version:1,title:'Письма после дождя',events,objects,chapters,variables:{trust:2,letter:false},scene:{weather:'Дождь',time:'Закат',camera:'Общий план',music:'After the rain',musicState:'playing',duck:false}};
}
export const allBeats=p=>p.chapters.flatMap(c=>c.beats);
export const resolvedActions=(p,b)=>{const e=p.events.find(e=>e.id===b.eventId);const first=e?.groups[0]?.actions[0]?.id;return e?.groups.flatMap(g=>g.actions.map(a=>({...a,...(a.id===first?b.overrides:{}),groupId:g.id})))||[]};
export const targetName=(p,id)=>p.objects.find(x=>x.id===id)?.name||({world:'Мир',audio:'Музыка',camera:'Главная камера',trust:'Доверие'}[id]||id);
export const validActionTarget=(p,a)=>a.type==='variable'?Object.hasOwn(p.variables||{},a.target):p.objects.some(o=>o.id===a.target)||['world','audio','camera','trust'].includes(a.target);
export function routeTo(p,id){const result={},seen=new Set();let b=allBeats(p).find(b=>b.id===id);while(b?.branch&&!seen.has(b.branch)){seen.add(b.branch);const owner=allBeats(p).find(n=>n.choices?.some(c=>c.id===b.branch));if(!owner)break;result[owner.id]=b.branch;b=owner;}return result;}
const resource = a=>TYPES[a.type]?.domain?`${a.target}/${TYPES[a.type].domain}`:null;
export function validate(p){
 const issues=[];const add=(id,title,detail,beatId,eventId,fix,level='error')=>issues.push({id,title,detail,beatId,eventId,fix,level});
 const validTargets=[...p.objects.map(o=>o.id),'world','audio','camera','trust'];
 const usedIds=new Set(allBeats(p).flatMap(b=>b.bindings.map(x=>x.eventId)));
 for(const b of allBeats(p)){
  for(const binding of b.bindings){
   const ev=p.events.find(e=>e.id===binding.eventId);
   if(!ev){add(`missing-${binding.id}`,'Событие не найдено','Шаблон удалён, а ссылка осталась.',b.id,binding.eventId,'remove-binding');continue;}
   if(ev.retention!=='AUTO_CLOSE_ON_FLOW_END'&&binding.join==='EVENT_END')add(`held-${binding.id}`,'Сценарий ждёт бесконечное событие',`«${ev.name}» живёт до остановки. Следующая реплика не появится. Дождитесь выполнения шагов (Flow End).`,b.id,ev.id,'flow-end');
  }
  if(b.mode==='PARALLEL'||b.bindings.some(x=>x.join==='NONE'||x.join==='STARTED')){
   const claims=new Map();
   for(const binding of b.bindings)for(const a of resolvedActions(p,binding)){
    const key=`${binding.hook}/${resource(a)}`;if(!resource(a)||['sound','pause','resume','stop'].includes(a.type))continue;
    if(claims.has(key)&&claims.get(key)!==binding.id)add(`parallel-${b.id}-${key}`,'Два события управляют одним объектом',`${targetName(p,a.target)} → ${TYPES[a.type].domain.toLowerCase()}. События запущены вместе; второй запуск будет отклонён. Выполните их по очереди.`,b.id,binding.eventId,'sequential');
    if(b.mode==='PARALLEL'||['NONE','STARTED'].includes(binding.join))claims.set(key,binding.id);
   }
  }
  if(b.kind==='choice'&&!b.choices.some(c=>c.condition==='always'))add(`choice-${b.id}`,'Игрок может остаться без ответа','У каждого ответа есть условие. Добавьте вариант, доступный всегда.',b.id,null,'fallback');
  if(b.kind==='choice')for(const c of b.choices){if(c.next&&!allBeats(p).some(n=>n.id===c.next))add(`edge-${c.id}`,'Ответ ведёт в удалённый блок','Выберите существующий блок или общий путь.',b.id,null,'edge');}
  if(b.kind==='gate'&&!p.objects.some(o=>o.id===b.signal&&o.active&&o.type==='Активный меш'))add(`gate-${b.id}`,'Игрок не может выполнить ожидание','Объект для взаимодействия удалён, выключен или не является активным мешем.',b.id,null,'gate');
  for(const binding of b.bindings)for(const a of resolvedActions(p,binding)){if(!validActionTarget(p,a))add(`binding-target-${binding.id}`,a.type==='variable'?'Переменная не найдена':'В размещении выбран удалённый объект','Измените локальную цель события.',b.id,binding.eventId,'binding-target');}
  for(const binding of b.bindings){if(!Object.keys(binding.overrides).length)continue;const claims=new Set();for(const a of resolvedActions(p,binding)){const key=`${a.groupId}/${resource(a)}`;if(resource(a)&&a.type!=='sound'&&claims.has(key))add(`local-group-${binding.id}-${key}`,'Локальная настройка создаёт конфликт действий','После изменения объекта два действия одного шага управляют одним свойством. Верните параметры шаблона.',b.id,binding.eventId,'reset-overrides');claims.add(key);}}
 }
 for(const ev of p.events){
  const beat=allBeats(p).find(b=>b.bindings.some(x=>x.eventId===ev.id));
  if(ev.retention==='HOLD_UNTIL_REPLACED'&&!ev.channel)add(`channel-${ev.id}`,'Не задана роль для замены','Событию «до замены» нужна общая роль, например «Фоновая музыка».',beat?.id,ev.id,'channel');
  for(const g of ev.groups){const claimed=new Map();for(const a of g.actions){
   if(!validActionTarget(p,a))add(`target-${a.id}`,a.type==='variable'?'Переменная не найдена':'Объект действия не найден',`«${ev.name}»: цель ${a.target} недоступна.`,beat?.id,ev.id,'target');
   if(TYPES[a.type]?.completion==='CONTINUOUS'&&(a.wait==='COMPLETED'||a.scope==='SELF'))add(`continuous-${a.id}`,'Бесконечное действие не может закончиться само','Для музыки выберите «дождаться запуска» и «до закрытия события».',beat?.id,ev.id,'continuous');
   if(a.conflict==='QUEUE'&&!Number(a.queueTimeout))add(`queue-${a.id}`,'Очередь без ограничения ожидания','Укажите время ожидания ресурса и действие при ошибке.',beat?.id,ev.id,'timeout');
   const key=resource(a);if(key&&a.type!=='sound'&&claimed.has(key))add(`group-${g.id}-${key}`,'Два действия одновременно меняют одно свойство',`${targetName(p,a.target)} → ${TYPES[a.type].domain}. Действия внутри одного шага запускаются вместе. Разнесите их по шагам.`,beat?.id,ev.id,'split-group');claimed.set(key,a.id);
  }}
 }
 // Only dependencies reachable from placed events participate in the demo wait graph.
 const graph=new Map(p.events.filter(e=>usedIds.has(e.id)).map(e=>[e.id,e.groups.flatMap(g=>g.actions).filter(a=>a.waitFor).map(a=>a.waitFor)]));
 const visited=new Set(),stack=[];let cycle;
 function walk(id){if(stack.includes(id)){cycle=[...stack.slice(stack.indexOf(id)),id];return;}if(visited.has(id))return;visited.add(id);stack.push(id);for(const next of graph.get(id)||[])walk(next);stack.pop();}
 for(const id of graph.keys())walk(id);
 if(cycle){const ev=cycle[0];const beat=allBeats(p).find(b=>b.bindings.some(x=>x.eventId===ev));add('deadlock','События ждут друг друга',cycle.map(id=>p.events.find(e=>e.id===id)?.name||id).join(' → ')+'. Ни одно не сможет продолжиться.',beat?.id,ev,'break-cycle');}
 return issues;
}
export function fixIssue(p,issue){
 const n=structuredClone(p),beat=allBeats(n).find(b=>b.id===issue.beatId),e=n.events.find(x=>x.id===issue.eventId);
 if(issue.fix==='sequential'){beat.mode='SEQUENTIAL';beat.bindings.forEach(b=>{const ev=n.events.find(e=>e.id===b.eventId);b.join=ev?.retention==='AUTO_CLOSE_ON_FLOW_END'?'EVENT_END':'FLOW_END';});}
 if(issue.fix==='flow-end')beat.bindings.filter(b=>b.eventId===issue.eventId).forEach(b=>b.join='FLOW_END');
 if(issue.fix==='break-cycle'){e.groups.forEach(g=>g.actions.forEach(a=>{delete a.waitFor;a.value='Игрок открывает дверь';}));if(beat){beat.kind='gate';beat.signal='door';beat.timeout=45;beat.fallback='Подсветить дверь';beat.text='Откройте дверь в сад.';}}
 if(issue.fix==='split-group')e.groups=e.groups.flatMap(g=>g.actions.map((a,i)=>({id:uid('group'),name:`${g.name} · ${i+1}`,actions:[a]})));
 if(issue.fix==='continuous')e.groups.forEach(g=>g.actions.forEach(a=>{if(TYPES[a.type].completion==='CONTINUOUS'){a.wait='STARTED';a.scope='EVENT';}}));
 if(issue.fix==='timeout')e.groups.forEach(g=>g.actions.forEach(a=>a.queueTimeout=10));
 if(issue.fix==='target')e.groups.forEach(g=>g.actions.forEach(a=>{if(!n.objects.some(o=>o.id===a.target)&&!['world','audio','camera','trust'].includes(a.target))a.target='alice';}));
 if(issue.fix==='channel')e.channel='Audio.BGM';
 if(issue.fix==='fallback')beat.choices.push({id:uid('choice'),label:'Продолжить',condition:'always'});
 if(issue.fix==='remove-binding')beat.bindings=beat.bindings.filter(b=>b.eventId!==issue.eventId);
 if(issue.fix==='binding-target')beat.bindings.filter(b=>b.eventId===issue.eventId).forEach(b=>delete b.overrides.target);
 if(issue.fix==='gate'){const obj=n.objects.find(o=>o.id===beat.signal);if(obj){obj.active=true;obj.type='Активный меш';}else beat.signal=n.objects.find(o=>o.active&&o.type==='Активный меш')?.id||'letter';}
 if(issue.fix==='edge')beat.choices.forEach(c=>{if(c.next&&!allBeats(n).some(b=>b.id===c.next))delete c.next;});
 if(issue.fix==='reset-overrides')beat.bindings.filter(b=>b.eventId===issue.eventId).forEach(b=>b.overrides={});
 return n;
}
export function available(choice,vars){return choice.condition==='always'||(choice.condition==='trust'?vars.trust>=Number(choice.threshold||0):!!vars[choice.condition]);}
export function nextNode(p,beatId,route={},choiceId){
 const list=allBeats(p),current=list.find(b=>b.id===beatId);if(!current)return null;
 const choice=current.choices?.find(c=>c.id===choiceId);if(choice?.next)return list.find(b=>b.id===choice.next)||null;
 const selected=new Set(Object.values({...route,...(choiceId?{[beatId]:choiceId}:{})}));let i=list.findIndex(b=>b.id===beatId)+1;while(i<list.length&&list[i].branch&&!selected.has(list[i].branch))i++;return list[i]||null;
}
export function applyBeat(p,beat,previous,phases=['BEFORE','ON_START']){
 const state=structuredClone(previous||{...p.scene,positions:{},variables:p.variables,instances:[]});
 for(const b of [...beat.bindings].sort((a,b)=>['BEFORE','ON_START','AFTER'].indexOf(a.hook)-['BEFORE','ON_START','AFTER'].indexOf(b.hook))){if(!phases.includes(b.hook))continue;if(b.condition&&b.condition!=='always'&&!available({condition:b.condition,threshold:3},state.variables))continue;const e=p.events.find(e=>e.id===b.eventId);if(!e)continue;
  if(e.channel)state.instances=state.instances.map(i=>i.channel===e.channel?{...i,state:'FINISHED',reason:'Заменён новым событием'}:i);
  for(const a of resolvedActions(p,b)){switch(a.type){
   case 'move':state.positions[a.target]=a.value;break;case 'camera':state.camera=a.value;break;
   case 'weather':state.weather=a.value;break;case 'time':state.time=a.value;break;
   case 'music':state.music=a.value;state.musicState='playing';break;case 'pause':state.musicState='paused';break;case 'resume':state.musicState='playing';break;case 'stop':state.musicState='stopped';state.instances=state.instances.map(i=>i.channel==='Audio.BGM'?{...i,state:'FINISHED',reason:'Остановлен'}:i);break;
   case 'duck':state.duck=true;break;case 'variable':state.variables[a.target]=(Number(state.variables[a.target])||0)+Number(a.value);break;case 'pose':state.poses={...state.poses,[a.target]:a.value};break;case 'visibility':state.visible={...state.visible,[a.target]:a.value!=='Скрыть'};break;
  }}
  state.instances.push({id:uid('run'),name:e.name,definitionId:e.id,bindingId:b.id,channel:e.channel,owner:e.owner,state:e.retention==='AUTO_CLOSE_ON_FLOW_END'?'FINISHED':'HELD',flow:'ENDED'});
 }return state;
}
