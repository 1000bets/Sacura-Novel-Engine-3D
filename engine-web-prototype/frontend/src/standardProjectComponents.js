import {bundledComponentSources} from './bundledComponentSources.js';
import {createComponentResource,editComponentSource,publishComponentSource} from './componentResources.js';
import {addMechanicInstance} from './mechanicDescriptions.js';
import {reference as ref} from './valueSchemas.js';
// Starter data construction only; Play uses the common component runtime.
export function installStandardProjectComponents(project){
 const music=project.audioAssets.find(a=>a.kind==='music');if(music){music.file='MainMenuSound.mp3';music.url='/sound/MainMenuSound.mp3';for(const e of project.events)for(const g of e.groups)for(const a of g.actions)if(['music','resume'].includes(a.type))a.assetId=music.id;}
 project.variables.gardenNoteFound=false;project.variables.ticketShown=false;
 const definitions={};for(const name of ['Counter','Reward','StoryFlag','LockedDoor']){const r=createComponentResource(project,{name});r.lesson=({Counter:'Откройте add и измените вычисление next: изменится прибавка отношения.',Reward:'Откройте award: измените формулу amount — правдивые ответы дают другую награду.',StoryFlag:'Откройте interact: измените условие или добавьте вызов другого компонента после осмотра.',LockedDoor:'Откройте open: здесь исполняется открытие двери; ожидание принадлежит только анимации.'})[name];editComponentSource(project,r.id,bundledComponentSources[name]);publishComponentSource(project,r.id);definitions[name]=r.definitionId;}
 const add=(name,owner,label,values={},refs={})=>{const m=addMechanicInstance(project,definitions[name],{kind:'object',id:owner},values);m.name=label;m.references=refs;return m;};
 const variable=key=>ref('variable',project.gameplay.variableBindings.find(v=>v.key===key).id),counter=add('Counter','alice','Доверие · сюжетный источник',{storySource:true});counter.fieldBindings=[{target:'state',field:'value',variable:variable('trust')}];const reward=add('Reward','alice','Награда за искренность',{}, {counter:ref('mechanic',counter.id)});
 const asCall=(m,methodId,args={})=>({id:'sacura.component.call',version:1,parameters:{instance:ref('mechanic',m.id),methodId,args}});
 for(const [owner,key]of [['letter','letter'],['rain-keepsake','keepsakeFound'],['garden-note','gardenNoteFound'],['ticket','ticketShown']]){const flag=add('StoryFlag',owner,'Запомнить осмотр',{}, {variable:variable(key)});project.objects.find(o=>o.id===owner).interactionCall=asCall(flag,'interact');}
 const door=add('LockedDoor','door','Дверь в сад',{InitiallyLocked:false});project.objects.find(o=>o.id==='door').interactionCall=asCall(door,'interact');
 const replace=(eventId,actionIndex,call)=>{const action=project.events.find(e=>e.id===eventId).groups[0].actions[actionIndex];Object.assign(action,{type:'gameplay',target:'world',command:call.id,operationVersion:1,value:call.parameters,duration:0});delete action.operation;delete action.valueType;};
 replace('trust',0,asCall(reward,'award'));replace('rain-examine-keepsake',0,{id:'sacura.game.message',parameters:{message:'Вы нашли шкатулку с воспоминаниями.'}});replace('visual-open',0,asCall(door,'open'));
 // Gates call the object's component directly; this Event remains an editable alternative.

 for(const key of Object.keys(project.gameplay))if(key.startsWith('legacy'))delete project.gameplay[key];project.gameplay.enabled=true;project.gameplay.storyPolicy={mode:'story',mechanics:'gameplay-node',dialogueControl:'allow',ending:'continue'};
 project.description='Что открыть → «Алиса» → Reward / Counter; «Письмо» и «Шкатулка» → StoryFlag; «Дверь в сад» → LockedDoor. Что изменить → формулу награды или действие осмотра. Результат → ответы дают другую прибавку доверия, осмотр меняет доступность чтения письма.\nПостановка, движение, камера и аудио остаются обычными редактируемыми Events. Исходники всех составных игровых правил открываются в компонентах.';
 return project;
}
