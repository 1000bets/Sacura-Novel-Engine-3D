import test,{afterEach} from 'node:test';
import assert from 'node:assert/strict';
import english from '../src/translations.en.json' with {type:'json'};
import {LANGUAGE_STORAGE_KEY,readLanguage,initializeLanguage,setLanguage,getLanguage,localeTag,subscribeLanguage,t,message,literalLabel,optionLabel,plural} from '../src/i18n.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {actionSummary} from '../src/authoringPresentation.js';
import {buildTimelineModel} from '../src/timelineModel.js';
import {flowAriaLabels} from '../src/flowLocalization.js';

const storage=()=>{const values=new Map();return {getItem:key=>values.get(key),setItem:(key,value)=>values.set(key,value)};};
const memory=storage();
afterEach(()=>setLanguage('ru',memory));

test('language preference survives initialization; invalid or inaccessible preferences fall back to Russian',()=>{
 assert.equal(readLanguage(memory),'ru');
 const root={lang:''};let notifications=0;const unsubscribe=subscribeLanguage(()=>notifications++);
 assert.equal(setLanguage('en',memory,root),true);assert.equal(root.lang,'en');assert.equal(memory.getItem(LANGUAGE_STORAGE_KEY),'en');
 assert.equal(initializeLanguage(memory),'en');assert.equal(localeTag(),'en-US');assert.equal(notifications,1);
 assert.equal(setLanguage('fr',memory,root),false);assert.equal(getLanguage(),'en');assert.equal(root.lang,'en');assert.equal(notifications,1);
 unsubscribe();setLanguage('ru',memory,root);assert.equal(notifications,1);
 memory.setItem(LANGUAGE_STORAGE_KEY,'invalid');assert.equal(readLanguage(memory),'ru');
 assert.equal(initializeLanguage({getItem(){throw Error('denied');}}),'ru');
 assert.equal(setLanguage('en',{setItem(){throw Error('denied');}},root),false);assert.equal(root.lang,'en');assert.equal(t('Файл'),'File');
});

test('UI translations preserve whitespace, unknown strings and authored interpolation values',()=>{
 setLanguage('en',memory);
 assert.equal(t(' Файл '),' File ');assert.equal(t('   '),'   ');assert.equal(t(null),null);
 assert.equal(t('An unknown label'),'An unknown label');
 assert.equal(t('Проект сохранён: {0}',['Камера.json']),'Project saved: Камера.json');
 assert.equal(optionLabel('Камера'),'Camera');assert.equal(optionLabel(literalLabel('Камера')),'Камера');
 const options=[['Дождь','Дождь'],['author',literalLabel('Текст')]];
 assert.deepEqual(options.map(([value,label])=>[value,optionLabel(label)]),[['Дождь','Rain'],['author','Текст']]);
 setLanguage('ru',memory);assert.equal(t('Файл'),'Файл');assert.equal(localeTag(),'ru-RU');
});

test('a diagnostic translates in either direction after it was created, retaining its filename',()=>{
 const russian=t('Проект сохранён: {0}',['Текст · Камера.json']);
 setLanguage('en',memory);const translated=message(russian);
 assert.equal(translated,'Project saved: Текст · Камера.json');
 setLanguage('ru',memory);assert.equal(message(translated),russian);
 assert.equal(message('Unknown error: Камера'),'Unknown error: Камера');
});

test('plural labels and graph accessibility instructions follow the selected locale',()=>{
 const count=n=>plural(n,'действие','действия','действий','action');
 assert.deepEqual([1,2,5,21].map(count),['1 действие','2 действия','5 действий','21 действие']);
 assert.equal(flowAriaLabels()['controls.zoomIn.ariaLabel'],'Увеличить масштаб');
 setLanguage('en',memory);assert.deepEqual([1,2,5,21].map(count),['1 action','2 actions','5 actions','21 actions']);
 assert.equal(flowAriaLabels()['controls.zoomIn.ariaLabel'],'Zoom in');
});

test('switching languages preserves the project and running story, including names matching UI terms',async()=>{
 const project=createEmptyProject('Файл');project.objects.push({id:'authored',name:'Камера'});
 const beat=project.chapters[0].beats[0];beat.speaker='Текст';beat.text='Файл';
 const before=JSON.stringify(project),rt=new PreviewRuntime(new SoundDesk(()=>({play:async()=>{},pause(){}})));
 await rt.start(project,beat.id);
 try{
  const phase=rt.snapshot.phase;
  setLanguage('en',memory);
  assert.equal(rt.running,true);assert.equal(rt.snapshot.phase,phase);assert.equal(rt.snapshot.beatId,beat.id);
  assert.equal(actionSummary({type:'weather',target:'authored',value:'Дождь'},project),'Камера → Rain');
  assert.equal(buildTimelineModel(project).nodes[0].text,'Файл');
  assert.equal(JSON.stringify(project),before);
  setLanguage('ru',memory);assert.equal(rt.running,true);assert.equal(JSON.stringify(project),before);
 }finally{rt.stop();}
});

test('catalog entries have a nonempty English translation with the same interpolation slots',()=>{
 const slots=value=>[...value.matchAll(/\{(\d+)\}/g)].map(x=>x[1]).sort();
 for(const [ru,en] of Object.entries(english)){
  assert.equal(ru,ru.trim(),ru);assert.equal(typeof en,'string',ru);assert.ok(en.trim(),ru);
  assert.deepEqual(slots(en),slots(ru),ru);
 }
});

test('built-in animations and input labels localize while authored names remain literal',async()=>{
 const {animationOptions}=await import('../src/characterPresentation.js');
 const {inputLabel}=await import('../src/inputPresentation.js');
 setLanguage('en',memory);
 const character={extraAnimations:[{id:'custom',name:'Камера',basePose:'стоит'}]};
 const options=animationOptions(character,'custom');
 assert.equal(optionLabel(options.find(([id])=>id==='custom')[1]),'Камера');
 assert.equal(inputLabel({id:'interact',name:'Осмотреть'}),'Inspect');
 assert.equal(inputLabel({id:'interact',name:'Текст'}),'Текст');
});

test('3D interface captions update without recreating sprites and unsubscribe on disposal',async()=>{
 const {labelSprite}=await import('../src/sceneEffects.js');
 const previous=globalThis.document,draws=[];
 globalThis.document={createElement:()=>({getContext:()=>({clearRect(){},beginPath(){},roundRect(){},fill(){},stroke(){},fillText:text=>draws.push(text)})})};
 let sprite;
 try{
  sprite=labelSprite(()=>t('Письмо · осмотреть'));
  assert.equal(draws.at(-1),'Письмо · осмотреть');
  setLanguage('en',memory);assert.equal(draws.at(-1),'Letter · inspect');
  sprite.material.dispose();const count=draws.length;
  setLanguage('ru',memory);assert.equal(draws.length,count);
 }finally{sprite?.material.dispose();sprite?.material.map.dispose();globalThis.document=previous;}
});

test('explicit Russian UI translation calls have entries in the English catalog',async()=>{
 const {readdir,readFile}=await import('node:fs/promises');
 const directory=new URL('../src/',import.meta.url);
 for(const name of await readdir(directory)){
  if(!/\.(jsx|js)$/.test(name)||['App.jsx','Studio.jsx'].includes(name))continue;
  const source=await readFile(new URL(name,directory),'utf8');
  for(const match of source.matchAll(/\b(?:t|tr)\(\s*(['"])((?:\\.|(?!\1).)*)\1/g)){
   const key=match[2].replace(/\\(['"\\])/g,'$1').replace(/\\n/g,'\n').trim();
   if(/[а-яё]/i.test(key))assert.ok(Object.hasOwn(english,key),`${name}: ${key}`);
  }
 }
});
