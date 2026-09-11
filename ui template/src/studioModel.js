import {createProject,allBeats,makeAction,uid,nextNode,routeTo,resolvedActions,TYPES,validate as legacyValidate} from './model.js';
export {allBeats,makeAction,uid,TYPES};
export const PHASES=[{id:'BEFORE',label:'До реплики',hint:'Подготовить сцену, затем показать текст',color:'blue'},{id:'ON_START',label:'Во время реплики',hint:'Текст уже виден · постановка продолжается',color:'violet'},{id:'AFTER',label:'После реплики',hint:'Игрок продолжил · завершаем постановку',color:'amber'}];
export const AUDIO_ASSETS=[
 {id:'music-main',name:'Главная тема',file:'MainMenuSound.mp3',kind:'music',speaker:'Музыка',caption:'Основная музыкальная тема'},
 {id:'voice-narrator',name:'Дом встретил её…',file:'The_house_greeted_her_with_the_scent_of_wet_wood.mp3',kind:'voice',speaker:'Рассказчик',caption:'Дом встретил её запахом мокрого дерева.'},
 {id:'voice-bob',name:'Ты всё-таки вернулась',file:'You_came_back_after_all.mp3',kind:'voice',speaker:'Боб',caption:'Ты всё-таки вернулась.'},
 {id:'voice-alice',name:'Я обещала прийти…',file:'I_promised_return_before_rain_goes.mp3',kind:'voice',speaker:'Алиса',caption:'Я обещала прийти до дождя.'},
].map(a=>({...a,url:`/sound/${encodeURIComponent(a.file)}`}));
export const JOIN_LABELS={NONE:'Не задерживает поток',STARTED:'Ждём запуска',FLOW_END:'Ждём готовности · фон продолжится',EVENT_END:'Ждём завершения'};
export function bindingActions(p,b){const e=p.events.find(e=>e.id===b.eventId);return e?.groups.flatMap(g=>g.actions.map((a,i)=>({...a,...(a.id===e.groups[0]?.actions[0]?.id?b.overrides:{}),...(b.actionOverrides?.[a.id]||{}),groupId:g.id})))||[];}
export function batchesFor(b,phase){return (b.batches?.[phase]||[]).map(batch=>({...batch,bindings:batch.bindingIds.map(id=>b.bindings.find(x=>x.id===id)).filter(Boolean)}));}
export function normalizeBatches(b){
 b.batches ||= {};for(const phase of PHASES){let claimed=new Set();b.batches[phase.id]=(b.batches[phase.id]||[]).map(g=>({...g,bindingIds:g.bindingIds.filter(id=>{const x=b.bindings.find(x=>x.id===id&&x.hook===phase.id);if(!x||claimed.has(id))return false;claimed.add(id);return true;})})).filter(g=>g.bindingIds.length);
 const rest=b.bindings.filter(x=>x.hook===phase.id&&!claimed.has(x.id));if(rest.length)b.batches[phase.id].push({id:uid('batch'),mode:b.mode||'SEQUENTIAL',bindingIds:rest.map(x=>x.id)});}
 return b;
}
const newBinding=(eventId,hook='ON_START',join='EVENT_END')=>({id:uid('bind'),eventId,hook,join,overrides:{},actionOverrides:{}});
export function addToBatch(p,beatId,phase,batchId,eventId){const b=allBeats(p).find(x=>x.id===beatId),e=p.events.find(x=>x.id===eventId);if(!b||!e)return;normalizeBatches(b);const binding=newBinding(eventId,phase,e.retention==='AUTO_CLOSE_ON_FLOW_END'?'EVENT_END':'FLOW_END');b.bindings.push(binding);let g=b.batches[phase].find(x=>x.id===batchId);if(!g){g={id:uid('batch'),mode:'SEQUENTIAL',bindingIds:[]};b.batches[phase].push(g);}g.bindingIds.push(binding.id);return binding;}
export function newEvent(type='move',target='alice',value='окно',name){return {id:uid('event'),name:name||TYPES[type].label,description:'',retention:TYPES[type].completion==='CONTINUOUS'?'HOLD_UNTIL_STOPPED':'AUTO_CLOSE_ON_FLOW_END',owner:'SubScene',groups:[{id:uid('group'),name:'Основное действие',actions:[makeAction(type,target,value)]}]};}
export function upgradeProject(source){
 const p=structuredClone(source||createProject());if(p.version===2){allBeats(p).forEach(normalizeBatches);return p;}
 const original=structuredClone(p);p.version=2;p.revision=2;p.audioAssets=AUDIO_ASSETS;p.subscenes=[
 {id:'living',name:'Вечер в гостиной',location:'Гостиная',kind:'living',entry:'a1',sceneId:'chapter1',weather:'Дождь',time:'Закат',color:'#afa1e2'},
 {id:'garden',name:'Следы в саду',location:'Старый сад',kind:'garden',entry:'garden-entry',sceneId:'chapter1',weather:'Гроза',time:'Ночь',color:'#8ec2ab'},
 {id:'station',name:'Последний поезд',location:'Платформа станции',kind:'station',entry:'station-entry',sceneId:'chapter1',weather:'Ясно',time:'Рассвет',color:'#c8ae85'},
 ];
 p.chapters.forEach(c=>{c.subsceneId='living';c.beats.forEach(b=>{if(!('next'in b))b.next=b.kind==='end'?null:nextNode(original,b.id,routeTo(original,b.id))?.id||null;if(b.kind==='choice')b.choices.forEach(c=>{if(!c.next)c.next=nextNode(original,b.id,routeTo(original,b.id),c.id)?.id||null;});normalizeBatches(b);});});
 const ev=(id,name,type,target,value,extra={})=>{const e=newEvent(type,target,value,name);e.id=id;e.groups[0].id=id+'-g';Object.assign(e,extra);p.events.push(e);return e;};
 const music=ev('studio-music','Главная тема','music','audio','Главная тема',{retention:'HOLD_UNTIL_REPLACED',channel:'Audio.BGM',owner:'Scene'});Object.assign(music.groups[0].actions[0],{assetId:'music-main',volume:.42,loop:true,fade:1.2});
 ev('studio-rain','Дождь за окном','weather','world','Дождь',{retention:'HOLD_UNTIL_REPLACED',channel:'World.Weather'});
 ev('studio-night','Ночь наступает','time','world','Ночь',{retention:'HOLD_UNTIL_REPLACED',channel:'World.Time'});
 ev('studio-storm','Гроза в саду','weather','world','Гроза',{retention:'HOLD_UNTIL_REPLACED',channel:'World.Weather'});
 ev('studio-camera','Камера · общий план','camera','camera','Общий план');
 ev('studio-pose','Алиса останавливается','pose','alice','задумчивость');
 for(const asset of AUDIO_ASSETS.filter(a=>a.kind==='voice')){const e=ev(asset.id,`${asset.speaker} · озвучка`,'sound',asset.id==='voice-bob'?'bob':asset.id==='voice-alice'?'alice':'world',asset.caption);Object.assign(e.groups[0].actions[0],{assetId:asset.id,volume:1,duck:true,duckDb:-12,duration:3});}
 const b1=allBeats(p).find(b=>b.id==='a1');if(b1){b1.legacyBindings=structuredClone(b1.bindings);const extras=b1.bindings.filter(b=>!['bgm','rain','arrive'].includes(b.eventId));b1.bindings=[newBinding('studio-music','BEFORE','FLOW_END'),newBinding('studio-rain','BEFORE','FLOW_END'),newBinding('studio-camera','BEFORE'),newBinding('voice-narrator'),newBinding('arrive'),newBinding('studio-pose','AFTER'),newBinding('studio-night','AFTER','FLOW_END'),...extras];const bs=b1.bindings;for(const [id,index]of[['bgm',0],['rain',1],['arrive',4]]){const old=b1.legacyBindings.find(b=>b.eventId===id);if(old){bs[index].overrides=structuredClone(old.overrides||{});bs[index].actionOverrides=structuredClone(old.actionOverrides||{});if(old.condition)bs[index].condition=old.condition;}}b1.batches={BEFORE:[{id:'intro-background',mode:'PARALLEL',bindingIds:bs.slice(0,2).map(x=>x.id)},{id:'intro-camera',mode:'SEQUENTIAL',bindingIds:[bs[2].id]}],ON_START:[{id:'intro-performance',mode:'PARALLEL',bindingIds:bs.slice(3,5).map(x=>x.id)}],AFTER:[{id:'intro-close',mode:'SEQUENTIAL',bindingIds:bs.slice(5,7).map(x=>x.id)}]};normalizeBatches(b1);}
 for(const [id,voice]of[['a2','voice-bob'],['a3','voice-alice']]){const b=allBeats(p).find(b=>b.id===id);if(!b)continue;b.bindings=b.bindings.filter(x=>x.eventId!=='voice');normalizeBatches(b);const group=b.batches.ON_START[0];if(group)group.mode='PARALLEL';addToBatch(p,id,'ON_START',group?.id,voice);}
 // Real alternative routes; the archive remains available and all IDs are retained.
 const setNext=(id,next)=>{const b=allBeats(p).find(b=>b.id===id);if(b&&!('next' in (allBeats(original).find(x=>x.id===id)||{})))b.next=next;};
 setNext('lh3','s1');setNext('ls3','station-entry');setNext('la3','garden-entry');setNext('cl2','station-entry');setNext('tg2','garden-entry');setNext('ch2','d1');
 const mk=(id,speaker,text,bindings=[],extra={})=>normalizeBatches({id,speaker,text,kind:'dialogue',mode:'SEQUENTIAL',bindings:bindings.map(e=>newBinding(e)),...extra});
 const garden=[
 mk('garden-entry','Рассказчик','Мокрые ветки заслоняют дорожку. Дом остался позади.',[ 'studio-storm','studio-night'],{next:'garden-walk'}),
 mk('garden-walk','Алиса','На земле чьи-то свежие следы.',['window'],{next:'garden-choice'}),
 mk('garden-choice','Алиса','Что делать со следами?',[],{kind:'choice',choices:[{id:'follow-tracks',label:'Пойти к старой скамье',condition:'always',next:'garden-gate'},{id:'go-home',label:'Вернуться и всё рассказать',condition:'trust',threshold:2,next:'c1'},{id:'go-station',label:'Уехать до рассвета',condition:'always',next:'station-entry'}]}),
 mk('garden-gate','Рассказчик','Осмотрите записку на скамье.',[],{kind:'gate',signal:'garden-note',timeout:20,next:'garden-letter'}),
 mk('garden-letter','Алиса','«Я ждала тебя здесь каждое утро».',['music-pause'],{next:'garden-reaction'}),
 mk('garden-reaction','Боб','Я думал, ты её никогда не найдёшь.',['look'],{next:'garden-decision'}),
 mk('garden-decision','Алиса','У меня ещё есть время.',[],{kind:'choice',choices:[{id:'garden-stay',label:'Остаться до рассвета',condition:'always',next:'garden-dawn'},{id:'garden-leave',label:'Проводить Боба на поезд',condition:'always',next:'station-entry'}]}),
 mk('garden-dawn','Рассказчик','Дождь стихает. Сквозь ветви проходит первый свет.',['clear','music-resume'],{next:'garden-end'}),
 mk('garden-end','Алиса','Я остаюсь. На этот раз — по-настоящему.',[],{kind:'end',ending:'Вместе до рассвета',next:null}),
 ];
 const station=[
 mk('station-entry','Рассказчик','Вместо гостиной — пустая платформа и огни последнего поезда.',['music-pause','clear'],{next:'station-voice'}),
 mk('station-voice','Алиса','Билет в один конец. Как и в прошлый раз.',[],{next:'station-choice'}),
 mk('station-choice','Алиса','Поезд отправится через несколько минут.',[],{kind:'choice',choices:[{id:'depart',label:'Сесть в поезд',condition:'always',next:'station-ticket'},{id:'return',label:'Вернуться домой',condition:'letter',next:'c1'}]}),
 mk('station-ticket','Рассказчик','Предъявите билет у вагона.',[],{kind:'gate',signal:'ticket',timeout:20,next:'station-move'}),
 mk('station-move','Рассказчик','Двери закрываются. Дом растворяется в тумане.',['music-resume'],{next:'station-final'}),
 mk('station-final','Алиса','Некоторые письма нужно прочитать в дороге.',[],{next:'station-end'}),
 mk('station-end','Рассказчик','Впереди — другая история.',['music-stop'],{kind:'end',ending:'Дорога на восток',next:null}),
 ];
 for(const nodes of[garden,station])for(const b of nodes)for(const binding of b.bindings){const e=p.events.find(e=>e.id===binding.eventId);if(e?.retention!=='AUTO_CLOSE_ON_FLOW_END')binding.join='FLOW_END';}
 p.chapters.push({id:'garden-chapter',subsceneId:'garden',name:'Следы под дождём',beats:garden},{id:'station-chapter',subsceneId:'station',name:'Билет в один конец',beats:station});
 p.objects.push({id:'garden-note',name:'Записка на скамье',type:'Активный меш',color:'#e6d4a2',active:true,position:'стол',subsceneId:'garden',interaction:'Прочитать записку'},{id:'ticket',name:'Билет',type:'Активный меш',color:'#9ebed2',active:true,position:'стол',subsceneId:'station',interaction:'Предъявить билет'});
 const end=allBeats(p).find(b=>b.id==='d6');if(end)end.ending='Дом, в котором ждут';return p;
}
export function sceneFor(p,beatId){const c=p.chapters.find(c=>c.beats.some(b=>b.id===beatId));return p.subscenes.find(s=>s.id===c?.subsceneId)||p.subscenes[0];}
export function edgesFor(p,b){if(b.kind==='choice')return b.choices.map(c=>({from:b.id,to:c.next,label:c.label,condition:c.condition,choiceId:c.id}));return b.next?[{from:b.id,to:b.next,label:b.kind==='gate'?'После взаимодействия':'Продолжить'}]:[];}
export function chooseNext(p,id,choiceId,vars){const b=allBeats(p).find(b=>b.id===id);if(!b)return null;if(b.kind==='choice'){const c=b.choices.find(c=>c.id===choiceId);if(!c||!conditionPass(c,vars))return null;return allBeats(p).find(n=>n.id===c.next)||null;}return allBeats(p).find(n=>n.id===b.next)||null;}
export function conditionPass(c,vars){return !c.condition||c.condition==='always'||(c.condition==='trust'?Number(vars.trust)>=Number(c.threshold??3):!!vars[c.condition]);}
export function validateStudio(p){
 const clone=structuredClone(p);allBeats(clone).forEach(b=>{b.mode='SEQUENTIAL';b.bindings.forEach(x=>x.join=x.join==='NONE'?'FLOW_END':x.join);});
 const issues=legacyValidate(clone).filter(i=>!i.id.startsWith('parallel-'));
 for(const b of allBeats(p)){
  for(const phase of PHASES)for(const batch of batchesFor(b,phase.id)){
   const resources=new Map();for(const binding of batch.bindings){for(const a of bindingActions(p,binding)){const domain=TYPES[a.type]?.domain;if(!domain||['sound','pause','resume','stop','duck'].includes(a.type))continue;const key=`${a.target}/${domain}`;const prev=resources.get(key);if(prev&&prev!==binding.id)issues.push({id:`overlap-${b.id}-${batch.id}-${key}`,beatId:b.id,eventId:binding.eventId,batchId:batch.id,phase:phase.id,level:'error',title:'Два события управляют одним ресурсом',detail:`${p.objects.find(o=>o.id===a.target)?.name||a.target} · ${domain}. Две команды пересекаются. Разнесите их по шагам или измените объект.`,fix:'sequence-batch'});if(batch.mode==='PARALLEL'||['NONE','STARTED'].includes(binding.join))resources.set(key,binding.id);}}
  }
  // Unjoined finite work can overlap later batches and even later dialogue phases.
  const pending=new Map();
  for(const phase of PHASES)for(const batch of batchesFor(b,phase.id))for(const binding of batch.bindings){
   for(const a of bindingActions(p,binding)){
    if(!p.objects.some(o=>o.id===a.target)&&!['world','audio','camera','trust'].includes(a.target))issues.push({id:`override-target-${binding.id}-${a.id}`,beatId:b.id,eventId:binding.eventId,level:'error',title:'В размещении выбран удалённый объект',detail:a.target,fix:'reset-overrides'});
    if(TYPES[a.type]?.completion!=='FINITE'||!TYPES[a.type]?.domain||['sound','duck'].includes(a.type))continue;
    const key=`${a.target}/${TYPES[a.type].domain}`,previous=pending.get(key);
    if(previous&&previous.binding.id!==binding.id&&previous.batch!==batch.id)issues.push({id:`unjoined-${b.id}-${key}`,beatId:b.id,eventId:previous.binding.eventId,phase:previous.phase,batchId:previous.batch,level:'error',title:'Предыдущий шаг ещё управляет объектом',detail:`${key}: поток не дождался завершения в «${PHASES.find(p=>p.id===previous.phase).label}». Поэтому стрелка «затем» не гарантирует свободный ресурс.`,fix:'wait-previous',bindingId:previous.binding.id});
    if(['NONE','STARTED'].includes(binding.join))pending.set(key,{binding,batch:batch.id,phase:phase.id});
   }
   if(Object.keys(binding.actionOverrides||{}).length){const claims=new Set();for(const a of bindingActions(p,binding)){const key=`${a.groupId}/${a.target}/${TYPES[a.type]?.domain}`;if(TYPES[a.type]?.domain&&a.type!=='sound'&&claims.has(key))issues.push({id:`local-${binding.id}-${key}`,beatId:b.id,eventId:binding.eventId,level:'error',title:'Локальные параметры создают конфликт',detail:'Два параллельных действия управляют одним объектом. Сбросьте локальные параметры или выберите другую цель.',fix:'reset-overrides'});claims.add(key);}}
  }
  if(b.kind==='choice')for(const c of b.choices)if(!c.next)issues.push({id:`missing-choice-${b.id}-${c.id}`,beatId:b.id,level:'error',title:'У ответа нет продолжения',detail:'Откройте карту истории и выберите, куда ведёт этот ответ.'});
  for(const edge of edgesFor(p,b))if(edge.to&&!allBeats(p).some(n=>n.id===edge.to))issues.push({id:`edge-${b.id}-${edge.to}`,beatId:b.id,level:'error',title:'Переход ведёт в удалённую реплику',detail:edge.to});
 }
 return [...new Map(issues.map(i=>[i.id,i])).values()];
}
export function fixStudio(p,i){const n=structuredClone(p);if(i.fix==='wait-previous'){allBeats(n).find(b=>b.id===i.beatId).bindings.find(b=>b.id===i.bindingId).join='EVENT_END';return n;}if(i.fix==='reset-overrides'){allBeats(n).find(b=>b.id===i.beatId).bindings.filter(b=>b.eventId===i.eventId).forEach(b=>{b.overrides={};b.actionOverrides={};});return n;}if(i.fix==='sequence-batch'){const b=allBeats(n).find(b=>b.id===i.beatId),g=b.batches[i.phase].find(g=>g.id===i.batchId);g.mode='SEQUENTIAL';g.bindingIds.forEach(id=>{const binding=b.bindings.find(x=>x.id===id);binding.join=n.events.find(e=>e.id===binding.eventId)?.retention==='AUTO_CLOSE_ON_FLOW_END'?'EVENT_END':'FLOW_END';});return n;}return null;}
