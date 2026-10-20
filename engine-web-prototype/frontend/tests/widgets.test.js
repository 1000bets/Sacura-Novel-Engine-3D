import test from 'node:test';
import assert from 'node:assert/strict';
import {createWidget,createWidgetElement,ensureWidgets,resolveWidget,layoutRect,rectTransform,reanchor,widgetRects,removeWidget,removeWidgetElement,scopeWidgetCss,validateWidgets} from '../src/widgetModel.js';
import {upgradeProject,allBeats} from '../src/studioModel.js';
import {readProject} from '../src/projectFiles.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {createCharacter} from '../src/characterModel.js';

test('legacy migration adds menus, dialogue and HUD and is idempotent',()=>{
 const p=upgradeProject();assert.equal(p.widgets.length,4);assert.deepEqual(validateWidgets(p),[]);
 const saved=JSON.stringify(p.widgets);assert.equal(JSON.stringify(upgradeProject(p).widgets),saved);
 for(const c of p.objects.filter(o=>o.type==='Персонаж'))assert.equal(c.dialogueWidgetId,null);
 const empty=createEmptyProject();assert.equal(empty.widgets.length,4);
 const c=createCharacter(empty,empty.subscenes[0].id,'Сакура');assert.equal(c.dialogueWidgetId,null);
});
test('dialogue overrides beat > character > project and ignores missing or wrong kinds',()=>{
 const p=upgradeProject(),character=p.objects.find(o=>o.type==='Персонаж'),beat={speaker:character.name};
 p.widgets.push(createWidget('character','dialogue'),createWidget('beat','dialogue'));
 const standard=resolveWidget(p,'dialogue',beat);character.dialogueWidgetId='character';assert.equal(resolveWidget(p,'dialogue',beat).id,'character');
 beat.widgetId='beat';assert.equal(resolveWidget(p,'dialogue',beat).id,'beat');
 beat.widgetId=p.ui.mainMenu;assert.equal(resolveWidget(p,'dialogue',beat).id,'character');
 beat.widgetId='deleted';character.dialogueWidgetId='deleted';assert.equal(resolveWidget(p,'dialogue',beat).id,standard.id);
 assert.equal(resolveWidget(p,'mainMenu',beat).id,p.ui.mainMenu);
});
test('bottom-right anchoring preserves edge offsets at several sizes',()=>{
 const l=rectTransform({anchorMin:[1,1],anchorMax:[1,1],pivot:[1,1],x:-20,y:-30,width:200,height:100});
 assert.deepEqual(layoutRect(l,{width:1280,height:720}),{x:1060,y:590,width:200,height:100});
 assert.deepEqual(layoutRect(l,{width:1920,height:1080}),{x:1700,y:950,width:200,height:100});
});
test('stretched anchors resize with margins; reanchoring preserves geometry',()=>{
 const parent={width:1280,height:720},l=rectTransform({x:67,y:44,width:460,height:210});
 for(const [min,max,pivot] of [[[0,1],[1,1],[.5,1]],[[0,0],[1,1],[0,0]],[[1,0],[1,0],[1,0]]]){
  const changed=reanchor(l,parent,min,max,pivot);assert.deepEqual(layoutRect(changed,parent),layoutRect(l,parent));
 }
 const stretch=rectTransform({anchorMin:[0,1],anchorMax:[1,1],pivot:[.5,1],width:-100,height:200,y:-20});
 assert.equal(layoutRect(stretch,parent).x,50);assert.equal(layoutRect(stretch,parent).width,1180);
 assert.equal(layoutRect(stretch,{width:1600,height:720}).width,1500);
});
test('nested RectTransforms resolve relative to parent at reference and portrait sizes',()=>{
 const w=createWidget('d'),root=w.elements[0],name=w.elements[1],rects=widgetRects(w);
 assert.equal(rects[name.id].x,rects[root.id].x+28);assert.equal(rects[name.id].y,rects[root.id].y+18);
 for(const [width,height] of [[1280,720],[1920,1080],[1024,768],[720,1280],[2560,1080]]){
  const r=widgetRects(w,width,height),scale=Math.min(width/1280,height/720);
  assert.equal(Math.round(height-r[root.id].y-r[root.id].height),Math.round(34*scale));
  assert.ok(r[name.id].width>0);assert.ok(r[root.id].x>=0);
 }
});
test('widgets, CSS, character defaults and beat overrides survive JSON export/import',()=>{
 const p=createEmptyProject(),c=createCharacter(p,p.subscenes[0].id,'Алиса'),custom=createWidget('custom','dialogue','Алиса · диалог');p.widgets.push(custom);
 custom.css='[data-role="dialogueText"] {color: pink;}';custom.elements[0].layout.x=64;c.dialogueWidgetId=custom.id;allBeats(p)[0].widgetId=custom.id;
 const loaded=readProject(JSON.stringify(p));assert.deepEqual(loaded.widgets,p.widgets);assert.equal(loaded.objects[0].dialogueWidgetId,'custom');assert.equal(resolveWidget(loaded,'dialogue',allBeats(loaded)[0]).id,'custom');
});
test('deleting a widget repairs all references and protects the last widget of each kind',()=>{
 const p=upgradeProject();assert.throws(()=>removeWidget(p,p.ui.dialogue),/хотя бы один/);
 const w=createWidget('extra');p.widgets.push(w);p.ui.dialogue=w.id;p.objects[0].dialogueWidgetId=w.id;allBeats(p)[0].widgetId=w.id;
 removeWidget(p,w.id);assert.notEqual(p.ui.dialogue,w.id);assert.equal(p.objects[0].dialogueWidgetId,null);assert.equal(allBeats(p)[0].widgetId,null);
});
test('deleting a container recursively deletes only its descendants',()=>{
 const w=createWidget('w'),root=w.elements[0];w.elements.push(createWidgetElement('deep','panel',{parentId:root.id}),createWidgetElement('leaf','text',{parentId:'deep'}));
 removeWidgetElement(w,root.id);assert.deepEqual(w.elements.map(e=>e.id),['w-choices']);
});
test('CSS is scoped to each widget, and incomplete/escaping rules report actionable errors',()=>{
 const css=scopeWidgetCss('[data-role="speaker"], .widget-button:hover {color: pink;} :widget {background: #333;}','.unique');
 assert.equal(css.error,null);assert.match(css.css,/\.unique \[data-role="speaker"\],\.unique \.widget-button:hover/);assert.match(css.css,/\.unique\{background/);
 for(const bad of ['body {color:red}','@import "other.css";','.x {background:url(a)}','.x {color:red;','</style>','.x { .y {color:red} }'])assert.ok(scopeWidgetCss(bad,'.scope').error,bad);
});
test('invalid imports reject corrupted layout, duplicate IDs and cycles before entering editor',()=>{
 for(const corrupt of [w=>w.elements[0].layout.anchorMin=[2,0],w=>w.elements[0].parentId=w.elements[1].id,w=>w.elements.push(structuredClone(w.elements[0])),w=>w.elements=null]){
  const p=createEmptyProject();corrupt(p.widgets[0]);assert.throws(()=>readProject(p),/виджет|виджета/);
 }
 const p=createEmptyProject();p.widgets='invalid';assert.throws(()=>readProject(p),/библиотека виджетов/);
});

test('personal variants are assigned immediately and deeply isolated from character/project defaults',async()=>{
 const {createDialogueVariant}=await import('../src/widgetModel.js');
 const p=createEmptyProject(),c=createCharacter(p,p.subscenes[0].id,'Алиса'),beat=allBeats(p)[0];beat.speaker=c.name;
 const original=resolveWidget(p,'dialogue',beat),personal=createDialogueVariant(p,{characterId:c.id},'personal');
 personal.elements[0].layout.y=-80;assert.equal(resolveWidget(p,'dialogue',beat).id,'personal');assert.equal(original.elements[0].layout.y,-34);
 const local=createDialogueVariant(p,{beatId:beat.id},'local');assert.equal(local.elements[0].layout.y,-80);local.elements[0].style.background='#112233';
 assert.equal(resolveWidget(p,'dialogue',beat).id,'local');assert.notEqual(personal.elements[0].style.background,'#112233');assert.equal(p.ui.dialogue,original.id);
});

test('customizing the default also replaces the built-in interaction overlay',async()=>{
 const {isUnmodifiedDefaultWidget}=await import('../src/widgetModel.js');
 const w=createWidget('widget-default-dialogue');assert.equal(isUnmodifiedDefaultWidget(w),true);
 w.elements[0].layout.y=-80;assert.equal(isUnmodifiedDefaultWidget(w),false);
 assert.equal(isUnmodifiedDefaultWidget(createWidget('personal')),false);
});
