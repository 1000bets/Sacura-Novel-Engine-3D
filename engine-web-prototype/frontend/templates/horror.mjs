import {reference as ref} from '../src/valueSchemas.js';
import {rectTransform} from '../src/widgetModel.js';

// Ordinary project data: geometry, typed references, shared source resources and story.
export async function buildHorror({empty,object,source,instance,scope,beat,call,hud,element,input,finalize}) {
 const p=empty('Последний обход · Point & click хоррор','horror'),s=p.subscenes[0],g=p.gameplay;
 s.name='Корпус № 6 · ночная смена';s.entry='horror-intro';s.time='Ночь';s.weather='Ясно';
 s.description='Открытый сверху игровой макет заброшенной проходной. Все цветные предметы осматриваются щелчком с близкого расстояния. Камера следует за смотрителем.';
 s.cameras[0]={id:'horror-overview',name:'Обзор проходной',mode:'fixed',position:[8,7,12],target:[0,0,-1],fov:60,smoothing:0};s.defaultCameraId='horror-overview';
 const solid=(id,name,pos,size,color,collide=true)=>{
  const o=object(p,id,name,pos);o.color=color;o.transforms[s.id].scale=size.map(x=>x*2);
  // Geometry supplies the collider. A custom size is LOCAL and would be scaled twice.
  o.collision={enabled:collide};return o;
 };
 for(const [i,z]of [[0,5],[1,-5]]){
  solid('horror-floor-'+i,'Плитка проходной '+(i+1),[0,-.16,z],[9,.3,10],'#384146');
  solid('horror-west-'+i,'Западная стена '+(i+1),[-4.45,1.4,z],[.3,2.8,10],'#303b40');
  solid('horror-east-'+i,'Восточная стена '+(i+1),[4.45,1.4,z],[.3,2.8,10],'#303b40');
 }
 solid('horror-end','Стена дальней смены',[0,1.4,-9.8],[9,2.8,.3],'#272f35');
 for(let n=0;n<8;n++)solid('floor-seam-'+n,'Шов плитки '+(n+1),[0,.002,8-n*2.3],[8.6,.012,.025],'#77807c',false);
 const hero=object(p,'horror-player','Смотритель · игрок',[0,0,6.5],'Персонаж');hero.color='#b89b62';hero.walkAnimation='ходит';hero.idleAnimation='стоит';hero.collision={enabled:true,custom:true,size:[.48,1.6,.48],offset:[0,.8,0]};
 hero.transforms[s.id].rotation=[0,180,0];
 hero.dialogueWidgetId=null;
 for(const [i,z]of [[0,5],[1,-2],[2,-7]]){const light=object(p,'horror-wall-light-'+i,'Дежурный свет '+(i+1),[i%2?3.8:-3.8,2.25,z],'Источник света');light.color='#d0b58c';light.light={intensity:16,distance:9,decay:2};solid('horror-light-cover-'+i,'Плафон '+(i+1),[i%2?4.1:-4.1,2.25,z],[.18,.2,.8],'#c5ae7b',false);}
 g.controller={...g.controller,ownerId:hero.id,mode:'wasd',speed:2.8,jumpSpeed:0,gravity:16,eyeHeight:1.6};
 g.camera={mode:'thirdPerson',targetId:hero.id,pointerLock:false,crosshair:false};
 g.cameraSettings={...g.cameraSettings,distance:4.5,eyeHeight:1.6,fov:64,sensitivity:.004};g.storyPolicy.dialogueControl='suspend';g.storyPolicy.ending='stop';
 const desk=solid('horror-desk','Стол смотрителя',[-2.8,.55,4.4],[1.6,1.1,1.8],'#635a49');
 const fuse=solid('horror-fuse','Керамический предохранитель',[-2.7,1.22,4.1],[.4,.28,.65],'#e5cc85',false);
 const journal=solid('horror-journal','Журнал смен',[-2.9,1.14,4.9],[.7,.08,.55],'#b29473',false);
 const cabinet=solid('horror-cabinet','Шкаф пропавшей смены',[3,.9,1.6],[1.1,1.8,1.5],'#4f625d');
 const mirror=solid('horror-mirror','Тусклое зеркало',[-4.15,1.4,-.7],[.1,1.7,1.5],'#829b9f',false);
 solid('horror-tape-table','Тумба магнитофона',[2.7,.45,-2.8],[1.6,.9,1.4],'#3e5963');
 const tape=solid('horror-tape','Магнитофон последней смены',[2.7,1.08,-2.8],[1.2,.35,.75],'#d3a367',false);
 for(const [i,x]of [[0,2.35],[1,3.05]]){const r=solid('tape-reel-'+i,'Катушка '+(i+1),[x,1.31,-2.8],[.28,.1,.28],'#252932',false);r.primitive='cylinder';}
 const box=solid('horror-power','Красный электрощит',[-4.05,1.1,-5.2],[.5,1.3,1.2],'#92514d');
 const door=solid('horror-exit','Зелёная дверь выхода',[0,1,-9.55],[1.5,2,.18],'#4e8877',false);
 const shadow=object(p,'horror-shadow','Чужой силуэт',[0,0,-7],'Персонаж');shadow.color='#14151c';shadow.description='Силуэт появляется по правилам HauntedRoom. Его можно осмотреть.';
 shadow.dialogueWidgetId=null;
 const lamp=object(p,'horror-alarm-light','Аварийный свет',[0,2.5,-5],'Источник света');lamp.color='#e27059';lamp.light={intensity:24,distance:12,decay:2};
 const bell=solid('horror-bell','Звонок охраны',[3,1.1,6],[.5,.3,.5],'#b9a36a',false);bell.primitive='sphere';
 const roomDef=await source(p,'HauntedRoom','Что открыть → HauntedRoom.escape / mirror. Измените условие requireRecord или реакцию зеркала. Результат → другое условие финала / появление силуэта.');
 const propDef=await source(p,'HauntedProp','Что открыть → HauntedProp.interact. Измените расстояние или добавьте действие перед вызовом HauntedRoom. Один исходник обслуживает все предметы; текст и ссылки задаются в инспекторе.');
 const doorDef=await source(p,'LockedDoor','Выход использует общий LockedDoor. HauntedRoom отдельно снимает замок и ожидает Open.');
 const lock=instance(p,doorDef,'horror-lock',scope('object',door.id),'Замок выхода',{OpenSeconds:1});
 const room=instance(p,roomDef,'horror-room',scope('scene',s.id),'Правила ночного обхода',{}, {shadow:ref('object',shadow.id),lamp:ref('object',lamp.id),exit:ref('mechanic',lock.id),ending:ref('beat','horror-ending')});
 room.inputContexts=['game-mechanics'];
 const props=[
  [fuse,'fuse',''],[journal,'examine','Журнал: «Питание — щит в конце коридора. После включения прослушать запись на магнитофоне. Зелёная дверь — единственный выход». Последняя подпись — ваша. Дата: 1996.'],
  [desk,'examine','На столе лежит предохранитель. Под ним свежий след ладони, хотя пыль здесь не трогали годами.'],
  [cabinet,'examine','Внутри пять одинаковых пальто. В кармане каждого — ваше удостоверение. Из закрытой секции доносится один короткий стук.'],
  [mirror,'mirror',''],[tape,'tape',''],[box,'power',''],[door,'exit',''],
  [shadow,'examine','Силуэт беззвучно произносит ваше имя. На его груди номер следующей смены.'],
  [bell,'examine','Звонок не издал звука. Но в дальнем конце коридора кто-то ответил: «Смена принята».'],
 ];
 for(const [o,kind,text]of props){const m=instance(p,propDef,'horror-prop-'+o.id,scope('object',o.id),o.name+' · осмотр',{kind,text},{room:ref('mechanic',room.id),viewer:ref('object',hero.id)});p.objects.find(current=>current.id===o.id).interactionCall=call(m,'interact');}
 // Reel hits must address the same tape instance, not swallow the point-and-click ray.
 for(const o of p.objects.filter(o=>o.id.startsWith('tape-reel-')))o.interactionCall=structuredClone(p.objects.find(current=>current.id===tape.id).interactionCall);
 p.chapters[0].beats=[beat('horror-intro','dialogue','Корпус № 6 закрыли тридцать лет назад. Сегодня вам пришло сообщение с собственного номера: «Не оставляй меня на вторую смену». Вы пришли на последний обход.','horror-walk'),beat('horror-walk','gameplay','',null),beat('horror-ending','end','Вы вышли в рассвет. В телефоне одно новое сообщение: «Спасибо. Теперь моя очередь уйти». В журнале за вашей подписью больше нет пустой строки.',null,{ending:'Рассвет'})];
 input(p,'horror-look','Осмотреться правой кнопкой',{id:'sacura.camera.look',version:1,parameters:{delta:{op:'get',path:'args.payload.delta'},device:{op:'get',path:'args.device'}}},'delta','pointer');
 p.input.actions.find(a=>a.id==='horror-look').valueType='axis2d';p.input.actions.find(a=>a.id==='horror-look').trigger='continuous';
 p.input.contexts.find(c=>c.id==='game-mechanics').bindings.find(b=>b.actionId==='horror-look').scale=[1,1];
 const w=hud(p,'horror-hud');
 element(w,'horror-title','text','ПОСЛЕДНИЙ ОБХОД',20,{style:{color:'#e5c38f',fontSize:24,fontWeight:600,padding:0}});
 element(w,'horror-objective','text','Задача',55,{layout:rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:24,y:55,width:650,height:70}),binding:{source:'component',instance:ref('mechanic',room.id),field:'objective',valueType:'string'},style:{color:'#f5eee0',fontSize:21,padding:0}});
 element(w,'horror-controls','text','WASD — идти · ЛКМ — предмет · ПКМ + мышь — осмотр',130,{layout:rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:24,y:135,width:740,height:34}),style:{color:'#d1c6b5',fontSize:18,padding:0}});
 p.description='Что открыть → «Корпус № 6» → HauntedRoom, любой предмет → HauntedProp, выход → LockedDoor. Что изменить → условие выхода в escape, реакцию mirror или правило interact. Результат → загадка и события действительно меняются.\nМаршрут: журнал и предохранитель на столе → красный щит → магнитофон → зелёная дверь. Зеркало, шкаф и звонок содержат необязательные страхи. WASD, щелчок рядом с предметом, ПКМ + мышь для осмотра. Сохранение хранит прогресс обхода и положение героя; стартовая подготовка при загрузке не повторяется. Все ссылки выбираются в инспекторе, исходники не содержат ID сцены или предметов.';
 return finalize(p);
}
