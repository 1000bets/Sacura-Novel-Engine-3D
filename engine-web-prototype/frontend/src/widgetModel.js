// Shared authoring/runtime RectTransform model. Coordinates use the widget's reference resolution.
export const WIDGET_KINDS={dialogue:'Диалог',mainMenu:'Главное меню',pauseMenu:'Меню паузы',hud:'Игровой HUD'};
export const ELEMENT_TYPES={progress:'Шкала',inventory:'Инвентарь',panel:'Панель',text:'Текст',button:'Кнопка',image:'Изображение',choices:'Ответы игрока'};
export const WIDGET_ROLES={none:'Без привязки',speaker:'Имя персонажа',dialogueText:'Текст реплики',choices:'Ответы игрока',status:'Подсказка',title:'Название игры'};
export const MENU_ACTIONS={event:'Запустить событие',gameplay:'Игровая команда',save:'Сохранить прохождение',load:'Загрузить прохождение',none:'Без действия',advance:'Продолжить диалог',start:'Начать игру',resume:'Снять паузу',restart:'Начать заново',mainMenu:'В главное меню'};
const finite=(v,fallback=0)=>Number.isFinite(Number(v))?Number(v):fallback;
export const rectTransform=(patch={})=>({anchorMin:[.5,.5],anchorMax:[.5,.5],pivot:[.5,.5],x:0,y:0,width:360,height:80,...patch});
export function createWidgetElement(id,type='panel',patch={}){
 return {id,name:ELEMENT_TYPES[type]||'Элемент',type,parentId:null,role:'none',text:type==='button'?'Кнопка':type==='text'?'Новый текст':'',action:'none',visible:true,layout:rectTransform(),style:{background:type==='panel'?'#211c29':type==='button'?'#704e7c':'transparent',color:'#fff4f8',fontSize:24,fontFamily:'system-ui',fontWeight:400,borderColor:'#ad849d',borderWidth:0,borderRadius:12,padding:12,opacity:1,textAlign:'left',gap:8},...patch};
}
export function createWidget(id,kind='dialogue',name=WIDGET_KINDS[kind]){
 const element=(key,type,patch)=>createWidgetElement(id+'-'+key,type,patch),root=id+'-panel';
 let elements;
 if(kind==='hud')elements=[
 element('hp','text',{name:'Здоровье',text:'HP',style:{fontSize:24,padding:0},binding:{source:'variable',path:'hp',format:'HP: {value}'},layout:rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:24,y:22,width:200,height:42})}),
 element('health','progress',{name:'Шкала здоровья',binding:{source:'variable',path:'hp'},maxVariable:'maxHp',min:0,max:100,layout:rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:24,y:68,width:220,height:16})}),
 element('ammo','text',{name:'Патроны',style:{fontSize:24,padding:0},binding:{source:'variable',path:'ammo',format:'Ammo: {value}'},layout:rectTransform({anchorMin:[0,0],anchorMax:[0,0],pivot:[0,0],x:24,y:98,width:220,height:40})}),
 element('inventory','inventory',{name:'Инвентарь',layout:rectTransform({anchorMin:[1,1],anchorMax:[1,1],pivot:[1,1],x:-24,y:-24,width:440,height:130})}),
 ];
 else if(kind==='dialogue')elements=[
  element('panel','panel',{name:'Диалоговое окно',layout:rectTransform({anchorMin:[0,1],anchorMax:[1,1],pivot:[.5,1],x:0,y:-34,width:-100,height:205}),style:{background:'#211c29',color:'#fff4f8',opacity:.96,borderRadius:16,borderWidth:1,borderColor:'#ad849d',padding:0}}),
  element('speaker','text',{name:'Имя персонажа',parentId:root,role:'speaker',layout:rectTransform({anchorMin:[0,0],anchorMax:[1,0],pivot:[0,0],x:28,y:18,width:-56,height:32}),style:{background:'transparent',color:'#e2b8d1',fontSize:23,fontWeight:600,padding:0}}),
  element('text','text',{name:'Текст реплики',parentId:root,role:'dialogueText',layout:rectTransform({anchorMin:[0,0],anchorMax:[1,1],pivot:[0,0],x:28,y:60,width:-88,height:-105}),style:{background:'transparent',color:'#fff4f8',fontSize:24,padding:0}}),
  element('status','text',{name:'Подсказка',parentId:root,role:'status',layout:rectTransform({anchorMin:[0,1],anchorMax:[1,1],pivot:[0,1],x:28,y:-16,width:-90,height:25}),style:{background:'transparent',color:'#d1bfd0',fontSize:15,padding:0}}),
  element('next','button',{name:'Продолжить',parentId:root,text:'›',action:'advance',layout:rectTransform({anchorMin:[1,1],anchorMax:[1,1],pivot:[1,1],x:-20,y:-14,width:44,height:44}),style:{background:'#704e7c',color:'#fff4f8',fontSize:28,borderRadius:10,textAlign:'center',padding:0}}),
  element('choices','choices',{name:'Ответы игрока',role:'choices',layout:rectTransform({anchorMin:[.5,1],anchorMax:[.5,1],pivot:[.5,1],y:-255,width:620,height:190}),style:{background:'transparent',color:'#fff4f8',fontSize:22,borderRadius:10,padding:4,gap:8}}),
 ];
 else elements=[
  element('panel','panel',{name:'Панель меню',layout:rectTransform({width:510,height:440}),style:{background:'#211c29',color:'#fff4f8',opacity:.98,borderRadius:20,borderWidth:1,borderColor:'#ad849d',padding:0}}),
  element('title','text',{name:'Заголовок',parentId:root,role:kind==='mainMenu'?'title':'none',text:'Пауза',layout:rectTransform({anchorMin:[0,0],anchorMax:[1,0],pivot:[0,0],x:30,y:35,width:-60,height:90}),style:{background:'transparent',color:'#fff4f8',fontSize:36,fontWeight:600,textAlign:'center',padding:0}}),
  ...((kind==='mainMenu')?[['start','Начать игру'],['restart','Начать заново']]:[['resume','Продолжить'],['restart','Начать заново'],['mainMenu','Главное меню']]).map(([action,text],i)=>element(action,'button',{name:text,parentId:root,text,action,layout:rectTransform({anchorMin:[.5,0],anchorMax:[.5,0],y:160+i*80,width:380,height:60}),style:{background:'#704e7c',color:'#fff4f8',fontSize:24,textAlign:'center',borderRadius:10,padding:8}})),
 ];
 return {id,name,kind,referenceSize:[1280,720],elements,css:''};
}
export function isUnmodifiedDefaultWidget(widget){
 if(widget.id!=='widget-default-'+widget.kind||widget.css?.trim())return false;
 const standard=createWidget(widget.id,widget.kind);
 return JSON.stringify(widget.referenceSize)===JSON.stringify(standard.referenceSize)&&JSON.stringify(widget.elements)===JSON.stringify(standard.elements);
}
export function ensureWidgets(project){
 project.widgets??=[];project.ui??={};
 for(const kind of Object.keys(WIDGET_KINDS)){
  if(!project.widgets.some(w=>w.kind===kind))project.widgets.push(createWidget('widget-default-'+kind,kind));
  if(!project.widgets.some(w=>w.id===project.ui[kind]&&w.kind===kind))project.ui[kind]=project.widgets.find(w=>w.kind===kind).id;
 }
 for(const object of project.objects||[])if(object.type==='Персонаж'&&object.dialogueWidgetId===undefined)object.dialogueWidgetId=null;
 return project;
}
export function resolveWidget(project,kind='dialogue',beat){
 const widgets=project.widgets||[],valid=id=>widgets.find(w=>w.id===id&&w.kind===kind);
 const character=kind==='dialogue'?project.objects?.find(o=>o.type==='Персонаж'&&(beat?.speakerId?o.id===beat.speakerId:o.name===beat?.speaker)):null;
 return (kind==='dialogue'&&(valid(beat?.widgetId)||valid(character?.dialogueWidgetId)))||valid(project.ui?.[kind])||widgets.find(w=>w.kind===kind)||createWidget('widget-default-'+kind,kind);
}
export function createDialogueVariant(project,{characterId,beatId},id){
 const owner=characterId?project.objects.find(o=>o.id===characterId&&o.type==='Персонаж'):project.chapters.flatMap(c=>c.beats).find(b=>b.id===beatId);
 if(!owner)throw new Error('Персонаж или реплика не найдены.');
 const source=resolveWidget(project,'dialogue',characterId?{speakerId:characterId}:owner);
 const copy={...structuredClone(source),id,name:source.name+' · '+(owner.name||owner.speaker||'реплика')};
 project.widgets.push(copy);owner[characterId?'dialogueWidgetId':'widgetId']=id;return copy;
}
export function layoutRect(layout,parent,scale=1){
 const l=rectTransform(layout),min=l.anchorMin,max=l.anchorMax,pivot=l.pivot;
 const width=Math.max(1,parent.width*(max[0]-min[0])+finite(l.width)*scale),height=Math.max(1,parent.height*(max[1]-min[1])+finite(l.height)*scale);
 return {x:parent.width*(min[0]+(max[0]-min[0])*pivot[0])+finite(l.x)*scale-width*pivot[0],y:parent.height*(min[1]+(max[1]-min[1])*pivot[1])+finite(l.y)*scale-height*pivot[1],width,height};
}
export function widgetRects(widget,width=widget.referenceSize[0],height=widget.referenceSize[1]){
 const scale=Math.min(width/widget.referenceSize[0],height/widget.referenceSize[1]),result={},visiting=new Set();
 const resolve=element=>{
  if(result[element.id])return result[element.id];
  if(visiting.has(element.id))return null;
  visiting.add(element.id);
  const parent=widget.elements.find(e=>e.id===element.parentId),parentRect=parent?resolve(parent):{x:0,y:0,width,height};
  visiting.delete(element.id);if(!parentRect)return null;
  const local=layoutRect(element.layout,parentRect,scale);
  return result[element.id]={...local,x:local.x+parentRect.x,y:local.y+parentRect.y};
 };
 widget.elements.forEach(resolve);return result;
}
export function reanchor(layout,parent,anchorMin,anchorMax,pivot=layout.pivot){
 const r=layoutRect(layout,parent);
 return {...layout,anchorMin,anchorMax,pivot,width:r.width-parent.width*(anchorMax[0]-anchorMin[0]),height:r.height-parent.height*(anchorMax[1]-anchorMin[1]),x:r.x+r.width*pivot[0]-parent.width*(anchorMin[0]+(anchorMax[0]-anchorMin[0])*pivot[0]),y:r.y+r.height*pivot[1]-parent.height*(anchorMin[1]+(anchorMax[1]-anchorMin[1])*pivot[1])};
}
export function removeWidget(project,id){
 const widget=project.widgets.find(w=>w.id===id);if(!widget)return;
 if(project.widgets.filter(w=>w.kind===widget.kind).length<2)throw new Error('Нужен хотя бы один виджет этого типа.');
 project.widgets=project.widgets.filter(w=>w.id!==id);
 for(const kind of Object.keys(WIDGET_KINDS))if(project.ui[kind]===id)project.ui[kind]=project.widgets.find(w=>w.kind===kind).id;
 for(const o of project.objects||[])if(o.dialogueWidgetId===id)o.dialogueWidgetId=null;
 for(const chapter of project.chapters||[])for(const beat of chapter.beats)if(beat.widgetId===id)beat.widgetId=null;
}
export function removeWidgetElement(widget,id){
 const removed=new Set([id]);let previous;
 do{previous=removed.size;for(const e of widget.elements)if(removed.has(e.parentId))removed.add(e.id);}while(previous!==removed.size);
 widget.elements=widget.elements.filter(e=>!removed.has(e.id));
}
// CSS is deliberately local to a single widget; imports, nested rules and remote resources are unsupported.
export function scopeWidgetCss(css,scope){
 const clean=String(css||'').replace(/\/\*[\s\S]*?\*\//g,'');
 if(!clean.trim())return {css:'',error:null};
 if(/@|url\s*\(|expression\s*\(|[<>]|\\/i.test(clean))return {css:'',error:'Используйте обычные CSS-правила без @-директив, URL и вложенности.'};
 let output='',cursor=0;const pattern=/([^{}]+)\{([^{}]*)\}/g;let match;
 while((match=pattern.exec(clean))){
  if(clean.slice(cursor,match.index).trim())return {css:'',error:'Проверьте фигурные скобки CSS.'};
  const selectors=match[1].split(',').map(s=>s.trim());
  if(selectors.some(s=>!s||/\b(?:html|body)\b|:root|:has\(/i.test(s)))return {css:'',error:'Стили действуют внутри виджета. Используйте .widget-element, [data-role="dialogueText"] или [data-element="ID"].'};
  output+=selectors.map(s=>s===':widget'?scope:`${scope} ${s}`).join(',')+'{'+match[2]+'}\n';cursor=pattern.lastIndex;
 }
 return clean.slice(cursor).trim()?{css:'',error:'Проверьте синтаксис CSS: селектор { свойство: значение; }.'}:{css:output,error:null};
}
export function validateWidgets(project){
 const issues=[],ids=new Set();
 for(const widget of project.widgets||[]){
  if(!widget||typeof widget!=='object'){issues.push({id:'widget-invalid',level:'error',title:'Повреждён виджет',detail:'Некорректная запись в библиотеке.'});continue;}
  const bad=detail=>issues.push({id:'widget-'+widget.id+'-'+issues.length,level:'error',title:'Проверьте виджет «'+widget.name+'»',detail});
  if(typeof widget.id!=='string'||!widget.id)bad('Отсутствует ID виджета.');
  if(!Array.isArray(widget.elements)){bad('Отсутствует список элементов.');continue;}
  if(ids.has(widget.id))bad('Повторяющийся ID виджета.');ids.add(widget.id);
  if(!WIDGET_KINDS[widget.kind])bad('Неизвестный тип виджета.');
  if(!Array.isArray(widget.referenceSize)||widget.referenceSize.length!==2||widget.referenceSize.some(v=>!Number.isFinite(v)||v<=0))bad('Некорректное разрешение холста.');
  const elementIds=new Set();
  for(const e of widget.elements||[]){
   if(!e||typeof e!=='object'){bad('Некорректный элемент.');continue;}
   if(typeof e.id!=='string'||!e.id)bad('Отсутствует ID элемента.');
   if(elementIds.has(e.id))bad('Повторяющийся ID элемента.');elementIds.add(e.id);
   if(e.binding&&(!['variable','game'].includes(e.binding.source)||typeof e.binding.path!=='string'))bad('Повреждена привязка значения.');if(e.action==='event'&&e.eventId&&!project.events?.some(x=>x.id===e.eventId))bad('Событие кнопки удалено.');
   if(!ELEMENT_TYPES[e.type])bad('Неизвестный тип элемента.');
   const l=e.layout;
   if(!l||['anchorMin','anchorMax','pivot'].some(k=>!Array.isArray(l[k])||l[k].length!==2||l[k].some(v=>!Number.isFinite(v)||v<0||v>1))||['x','y','width','height'].some(k=>!Number.isFinite(l[k]))||l.anchorMin.some((v,i)=>v>l.anchorMax[i]))bad('Некорректные анкеры или размер элемента «'+e.name+'».');
   let parent=e,seen=new Set([e.id]);while(parent.parentId){parent=widget.elements.find(x=>x?.id===parent.parentId);if(!parent||seen.has(parent.id)){bad('Повреждена иерархия элемента «'+e.name+'».');break;}seen.add(parent.id);}
  }
  const cssError=scopeWidgetCss(widget.css,'.widget').error;if(cssError)bad(cssError);
 }
 return issues;
}
