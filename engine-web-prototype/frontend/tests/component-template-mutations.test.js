import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {templateMutations} from './fixtures/templateMutations.js';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {editComponentSource,publishComponentSource} from '../src/componentResources.js';
import {PreviewRuntime} from '../src/runtime.js';
import {SoundDesk} from '../src/audio.js';
import {preparePlayableGame} from '../src/gameExport.js';
const mechanic=(p,name)=>p.gameplay.mechanics.find(m=>m.name===name);
const byType=(p,name)=>p.gameplay.mechanics.find(m=>p.gameplay.mechanicDefinitions.find(d=>d.id===m.definitionId)?.name===name);
const state=(rt,m)=>rt.game.state.mechanicStates[m.id];
async function probe(p,path){const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('This check does not request audio');}));try{
 const quiet=path==='letters-after-rain'?p.chapters[0].beats.find(b=>b.kind==='dialogue'&&!b.bindings.length):null;
 await rt.start(p,quiet?.id||p.subscenes[0].entry);assert.equal(rt.snapshot.error,null);
 if(path==='shooter'){const hero=mechanic(p,'Здоровье бойца'),kit=mechanic(p,'Аптечка');await rt.game.damage(hero.scope.id,99);await rt.game.useItem(kit.references.item.id,{ownerId:hero.scope.id});assert.equal(state(rt,hero).hp,71);assert.equal(rt.game.count(kit.references.item.id,hero.scope.id),0);}
 if(path==='point-and-click'){const pickup=mechanic(p,'Подобрать ключ'),door=mechanic(p,'Замок · ключ');await rt.game.callComponent(pickup.id,'pickup');assert.equal(rt.game.count(door.references.key.id,p.gameplay.inventory.owner.id),1);assert.equal(await rt.game.callComponent(door.id,'interact'),false);assert.equal(rt.game.state.message,'Заперто');assert.equal(state(rt,door).IsLocked,true);assert.equal(state(rt,mechanic(p,'Замок · история')).IsLocked,true);}
 if(path==='novel'){const choice=rt.project.chapters[0].beats.find(b=>b.id===rt.snapshot.beatId);await rt.advance(choice.choices.find(c=>c.label==='Поддержать').id);assert.equal(rt.snapshot.variables.relationship,6);assert.equal(state(rt,mechanic(p,'Разовый снимок')).value,6);await rt.advance();const second=rt.project.chapters[0].beats.find(b=>b.id===rt.snapshot.beatId);await rt.advance(second.choices.find(c=>c.label==='Поддержать ещё раз').id);assert.equal(rt.snapshot.variables.relationship,12);assert.equal(state(rt,mechanic(p,'Разовый снимок')).value,6);}
 if(path==='ui-clicker'){await rt.game.callComponent(mechanic(p,'Периодическая награда').id,'stop');const e=p.widgets.flatMap(w=>w.elements).find(e=>e.name==='Получить монету');await rt.uiAction(e.action,e);assert.equal(state(rt,mechanic(p,'Монеты')).value,6);assert.equal(Object.keys(rt.game.state.timers).length,0);}
 if(path==='letters-after-rain'){const flag=p.gameplay.mechanics.find(m=>p.gameplay.variableBindings.find(v=>v.id===m.references?.variable?.id)?.key==='letter');assert.ok(flag);await rt.game.callComponent(flag.id,'interact');assert.equal(rt.snapshot.variables.letter,false);assert.equal(rt.snapshot.variables.keepsakeFound,false);}
 }finally{rt.stop();}}
for(const edit of templateMutations)test(edit.path+': editing the real rule changes behavior in Play and the project built into standalone HTML',async()=>{
 const raw=JSON.parse(await readFile('public/game-templates/'+edit.path+'.sacura-template.json','utf8')),p=await instantiateAuthoringTemplate(raw,undefined,{reviewedSources:true}),r=p.logicResources.find(r=>r.name===edit.component),before=r.source;
 assert.ok(before.includes(edit.from));editComponentSource(p,r.id,before.replace(edit.from,edit.to));publishComponentSource(p,r.id);await probe(p,edit.path);
 const html=await preparePlayableGame(p,{readRuntime:async()=>({js:await readFile('public/player-runtime.js','utf8'),css:await readFile('public/player-runtime.css','utf8')})}),built=JSON.parse(html.split('globalThis.SACURA_PROJECT=')[1].split(';</script>')[0]);assert.match(html,/\.component-feedback\{/,'standalone must include the shared message overlay styles');await probe(built,edit.path);assert.equal(raw.content.logicResources.find(r=>r.name===edit.component).source,before);
});
