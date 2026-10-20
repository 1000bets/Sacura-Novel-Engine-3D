import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {ledgerSource,relaySource} from './fixtures/componentAcceptance.js';
import {createMechanicsProject} from '../src/projectLifecycle.js';
import {createComponentResource,editComponentSource,publishComponentSource} from '../src/componentResources.js';
import {addMechanicInstance} from '../src/mechanicDescriptions.js';
import {duplicateInstance} from '../src/authoredMechanics.js';
import {reference} from '../src/valueSchemas.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {gameSave,restoreGameSave} from '../src/gameSave.js';
import {addStoryComponentCall} from '../src/componentStory.js';
import {preparePlayableGame} from '../src/gameExport.js';
const ref=reference,turn=()=>new Promise(r=>setTimeout(r,2));
const rt=()=>new PreviewRuntime(new SoundDesk(()=>{throw Error('Audio is not part of relay behavior');}));
const call=(id,args={amount:3})=>({id:'sacura.component.call',version:1,parameters:{instance:ref('mechanic',id),methodId:'apply',args}});
async function idle(run){for(let n=0;n<500;n++){if(run.game.ruleRuntime.idle&&!run.game.flights.size)return;await turn();}throw Error('Runtime did not settle');}
function source(p,name,text){const r=createComponentResource(p,{name});editComponentSource(p,r.id,text);publishComponentSource(p,r.id);return r;}
function setup(){const p=createMechanicsProject('Author integration');for(const id of ['a','b'])p.objects.push({id,name:'Реле '+id.toUpperCase(),type:'Активный меш',active:true,subsceneId:p.subscenes[0].id});const ledger=source(p,'AcceptanceLedger',ledgerSource),relay=source(p,'ThresholdRelay',relaySource),instances=['a','b'].map((id,i)=>{const l=addMechanicInstance(p,ledger.definitionId,{kind:'object',id});const m=addMechanicInstance(p,relay.definitionId,{kind:'object',id},{factor:i?5:3});m.name='Подключение '+id.toUpperCase();m.references.ledger=ref('mechanic',l.id);return {m,l};});return {p,ledger,relay,a:instances[0].m,b:instances[1].m,la:instances[0].l,lb:instances[1].l};}

test('new authored type executes the same methods through story, input and UI; rename, shared/private source and save preserve identity and independence',async()=>{
 const {p,relay,a,b,la,lb}=setup();const privateInstance=duplicateInstance(p,a,{independent:true}),privateResource=p.logicResources.find(r=>r.definitionId===privateInstance.definitionId);assert.notEqual(privateResource.id,relay.id);const privateSource=privateResource.source;
 p.objects.find(o=>o.id==='a').name='Переименованное реле';p.gameplay.storyPolicy.mode='story';const scene=p.subscenes[0],entry={id:'entry',kind:'dialogue',speaker:'Рассказчик',text:'Первый вызов',next:'next',bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]}},next={id:'next',kind:'dialogue',speaker:'Рассказчик',text:'Состояние сохранено',next:null,bindings:[],batches:{BEFORE:[],ON_START:[],AFTER:[]}};scene.entry=entry.id;p.chapters=[{id:'chapter',name:'История',subsceneId:scene.id,beats:[entry,next]}];addStoryComponentCall(p,entry.id,'BEFORE',call(a.id));
 p.input.contexts.find(c=>c.id==='system').bindings.push({id:'apply-key',actionId:'apply-b',device:'keyboard',control:'KeyP',scale:1,deadZone:0});p.input.actions.push({id:'apply-b',name:'Вызов B',valueType:'boolean',behavior:'operation',operation:call(b.id),trigger:'pressed',holdSeconds:.4,consume:true});
 const run=rt();try{await run.start(p,entry.id);assert.equal(run.snapshot.error,null);const state=id=>run.game.state.mechanicStates[id];assert.equal(state(a.id).total,9);assert.equal(state(b.id).total,0);assert.equal(state(la.id).total,9);assert.equal(state(lb.id).total,0);
 await run.inputAction({actionId:'apply-b',phase:'triggered',value:1,device:'keyboard'});assert.equal(state(b.id).total,15);assert.equal(state(a.id).total,9);
 await run.uiAction('gameplay',{id:'button',command:'sacura.component.call',value:call(a.id,{amount:2}).parameters});assert.equal(state(a.id).total,15);assert.equal(state(b.id).total,15);await run.advance();await idle(run);const saved=JSON.parse(JSON.stringify(gameSave(run)));await run.game.callComponent(a.id,'apply',{amount:5});await restoreGameSave(run,saved);assert.equal(run.snapshot.beatId,'next');assert.equal(state(a.id).total,15);assert.equal(state(b.id).total,15);assert.equal(state(a.id).initializations,1);assert.equal(state(la.id).total,24);
 run.stop();editComponentSource(p,relay.id,relay.source.replace('args.amount * params.factor','args.amount * params.factor * 2'));publishComponentSource(p,relay.id);await run.start(p,entry.id);assert.equal(state(a.id).total,18);assert.equal(await run.game.callComponent(b.id,'apply',{amount:3}),30);assert.equal(await run.game.callComponent(privateInstance.id,'apply',{amount:3}),9);assert.equal(p.logicResources.find(r=>r.id===privateResource.id).source,privateSource);
 }finally{run.stop();}
});

test('nested failure identifies the innermost owner/instance/method; input and UI retain a navigable diagnostic and restarting clears it',async()=>{
 const {p,ledger,relay,a,la}=setup();editComponentSource(p,ledger.id,ledger.source.replace('state.total += args.amount','throw Error("Ошибка журнала"); state.total += args.amount'));publishComponentSource(p,ledger.id);const run=rt();try{await run.start(p);let failure;try{await run.game.callComponent(a.id,'apply',{amount:3});}catch(e){failure=e;}assert.ok(failure);assert.equal(failure.instanceId,la.id);assert.equal(failure.componentDiagnostic.resourceId,ledger.id);assert.equal(failure.componentDiagnostic.methodId,'record');assert.match(failure.message,/Реле A.*AcceptanceLedger.*Записать.*Ошибка журнала/);assert.ok(failure.componentTrace.some(f=>f.instanceId===a.id));assert.ok(failure.message.startsWith('Реле A → AcceptanceLedger'));assert.ok(!failure.message.includes(relay.path));
 await run.uiAction('gameplay',{command:'sacura.component.call',value:call(a.id).parameters});assert.equal(run.snapshot.componentError.instanceId,la.id);assert.match(run.snapshot.hint,/Ошибка журнала/);await run.start(p);assert.equal(run.snapshot.componentError,null);
 editComponentSource(p,relay.id,'const component = { methods: ;');await run.start(p);assert.equal(run.snapshot.phase,'ERROR');assert.equal(run.snapshot.componentError.resourceId,relay.id);assert.equal(run.snapshot.componentError.instanceId,a.id);
 }finally{run.stop();}
});

test('duplicating an object copies configured component instances and internal links, reuses sources and never interacts with the original receiver',async()=>{
 const {copyObjectComponents}=await import('../src/componentObjectCopies.js');const {p,a,la}=setup(),original=p.objects.find(o=>o.id==='a');original.interactionCall=call(a.id);const copied={...structuredClone(original),id:'copy',name:'Копия'};const ids=copyObjectComponents(p,[original],[copied]);p.objects.push(copied);assert.equal(p.logicResources.length,2);const copiedRelay=p.gameplay.mechanics.find(m=>m.id===ids.get(a.id));assert.equal(copiedRelay.references.ledger.id,ids.get(la.id));assert.equal(copied.interactionCall.parameters.instance.id,copiedRelay.id);
 const run=rt();try{await run.start(p);await run.game.execute(copied.interactionCall);assert.equal(run.game.state.mechanicStates[copiedRelay.id].total,9);assert.equal(run.game.state.mechanicStates[a.id].total,0);assert.equal(run.game.state.mechanicStates[la.id].total,0);}finally{run.stop();}
});

test('duplicated shooter adapters, inventory and weapon selection point at copied instances with one canonical state',async()=>{
 const {copyObjectComponents}=await import('../src/componentObjectCopies.js'),{instantiateAuthoringTemplate}=await import('../src/authoringTemplates.js');const p=await instantiateAuthoringTemplate(JSON.parse(await readFile('public/game-templates/shooter.sacura-template.json','utf8')),undefined,{reviewedSources:true}),hero=p.objects.find(o=>o.id===p.gameplay.controller.ownerId),copy={...structuredClone(hero),id:'second-hero',name:'Второй боец'},ids=copyObjectComponents(p,[hero],[copy]);p.objects.push(copy);const hp=p.gameplay.actors[hero.id].componentHealth.instance.id,copiedHP=ids.get(hp),weapons=p.gameplay.weapons.filter(w=>w.ownerId===copy.id);assert.equal(weapons.length,2);const run=rt();try{await run.start(p,p.subscenes[0].entry);assert.equal(run.snapshot.error,null);await run.game.damage(copy.id,10);assert.equal(run.game.state.mechanicStates[copiedHP].hp,90);assert.equal(run.game.state.mechanicStates[hp].hp,100);assert.strictEqual(run.game.state.actors[copy.id],run.game.state.mechanicStates[copiedHP]);const medkit=p.gameplay.items.find(i=>[...ids.values()].includes(i.id)&&i.componentUse);assert.ok(medkit);assert.equal(run.game.count(medkit.id,copy.id),1);assert.equal(run.game.state.selectedWeapons[copy.id],weapons[0].id);await run.game.useItem(medkit.id,{ownerId:copy.id});assert.equal(run.game.state.mechanicStates[copiedHP].hp,100);assert.equal(run.game.state.mechanicStates[hp].hp,100);}finally{run.stop();}
});

test('object paste uses copied configuration and source identities rather than whichever object is currently selected',async()=>{
 const {copyObjectComponents}=await import('../src/componentObjectCopies.js'),{p,a,la,b}=setup(),source=p.objects.find(o=>o.id==='a'),snapshot=structuredClone({id:p.id,gameplay:p.gameplay}),copy={...structuredClone(source),id:'pasted'};a.values.factor=11;const ids=copyObjectComponents(p,[source],[copy],snapshot);p.objects.push(copy);const pasted=p.gameplay.mechanics.find(m=>m.id===ids.get(a.id));assert.equal(pasted.values.factor,3);assert.equal(a.values.factor,11);assert.equal(pasted.references.ledger.id,ids.get(la.id));assert.equal(ids.has(b.id),false);assert.equal(p.logicResources.length,2);
});
