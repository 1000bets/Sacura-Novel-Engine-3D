import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir,writeFile} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {join} from 'node:path';
import {tmpdir} from 'node:os';
import {JSDOM} from 'jsdom';
import React from 'react';
import {createServer} from 'vite';
const root=fileURLToPath(new URL('../',import.meta.url)),dom=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for(const k of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,k,{configurable:true,value:dom.window[k]});globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup}=await import('@testing-library/react');
const server=await createServer({root,server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {transferSource,exportTransfer,importTransfer,transferRoot}=await server.ssrLoadModule('/tests/fixtures/integrationTransfer.js');
const {default:ActionFields}=await server.ssrLoadModule('/src/ActionFields.jsx'),{default:Parameters}=await server.ssrLoadModule('/src/FunctionParameters.jsx'),{default:Workspace}=await server.ssrLoadModule('/src/GameplayWorkspace.jsx');
const {preparePlayableGame}=await server.ssrLoadModule('/src/gameExport.js');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
for(const kind of ['event','function','mechanic','template'])test(kind+' A → editor form in B → C preserves nested editable graphs and resources with no access to A',async()=>{
 let source=transferSource(kind).project;const sourceBytes=JSON.stringify(source),pkgA=await exportTransfer(source,kind,'Автор A'),packageBytes=JSON.stringify(pkgA);let value=await importTransfer(JSON.parse(packageBytes),'Принимающий B');const before=JSON.stringify(value);source=null;
 function Harness(){const [p,setP]=React.useState(value);const change=fn=>setP(p=>{const next=structuredClone(p);fn(next);value=next;return next;});const record=transferRoot(p,kind);
  return kind==='event'?React.createElement(ActionFields,{project:p,action:record.groups[0].actions[0],onChange:patch=>change(p=>Object.assign(transferRoot(p,kind).groups[0].actions[0],patch))}):kind==='function'?React.createElement(Parameters,{project:p,fn:record,onChange:patch=>change(p=>Object.assign(transferRoot(p,kind),patch))}):React.createElement(Workspace,{project:p,sceneId:p.subscenes[0].id,onChange:change});
 }
 render(React.createElement(Harness));const label=kind==='function'?'Значение по умолчанию':'Количество';fireEvent.change(screen.getByRole('spinbutton',{name:label,exact:true}),{target:{value:'2'}});assert.notEqual(JSON.stringify(value),before);assert.equal(JSON.stringify(pkgA),packageBytes);assert.ok(!screen.queryByRole('textbox',{name:/JSON|ID/}));
 const pkgB=await exportTransfer(value,kind),project=await importTransfer(JSON.parse(JSON.stringify(pkgB)),'Независимый C');assert.ok(pkgB.metadata.sources.some(s=>s.author==='Автор A'));assert.equal(project.functions.length,2);if(kind==='template')assert.equal(project.events.length,pkgB.content.events.length);assert.ok(project.functions.every(f=>f.nodes.every(n=>n.position&&Number.isFinite(n.position.x))));const worker=project.functions.find(f=>f.name==='Вложенная функция');assert.equal(worker.nodes[0].comment,'Ресурс и оформление сохраняются при переносе.');assert.deepEqual(project.graphLayout[worker.id],{zoom:.8,pan:[10,20]});assert.match(worker.locals.picture.src,/^data:image\/png;base64,/);
 const dir=join(tmpdir(),'sacura-integration',kind);await mkdir(dir,{recursive:true});await writeFile(join(dir,'A.sacura-package.json'),packageBytes);await writeFile(join(dir,'B-before.sacura.json'),before);await writeFile(join(dir,'B-edited.sacura.json'),JSON.stringify(value));await writeFile(join(dir,'B.sacura-package.json'),JSON.stringify(pkgB));await writeFile(join(dir,'C.sacura.json'),JSON.stringify(project));await writeFile(join(dir,'C.html'),await preparePlayableGame(project,{readRuntime:async()=>({js:await readFile(root+'public/player-runtime.js','utf8'),css:await readFile(root+'public/player-runtime.css','utf8')})}));
 // This fresh process receives C only, blocks network, and cannot use A's fixture,
 // Editor imports, storage or JS realm. Play behavior is asserted independently.
 const program=`import fs from 'node:fs';import assert from 'node:assert/strict';import {PreviewRuntime} from './src/runtime.js';import {SoundDesk} from './src/audio.js';globalThis.fetch=()=>{throw Error('Network/A unavailable');};const p=JSON.parse(fs.readFileSync(process.argv[1],'utf8'));const rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio needed');}));try{await rt.start(p);const w=p.widgets.find(w=>w.id===p.gameplay.hudWidgetId),b=w.elements.find(e=>e.name==='Получить ресурс');await rt.uiAction(b.action,b);for(let i=0;i<300&&!rt.game.ruleRuntime.idle;i++)await new Promise(r=>setTimeout(r,2));assert.equal(rt.snapshot.error,null);assert.equal(rt.snapshot.variables.count,2);assert.equal(rt.snapshot.variables.done,true);if(p.gameplay.mechanics.some(m=>m.definitionId))assert.equal(Object.values(rt.game.state.mechanicStates)[0].uses,1);console.log('C only: count=2, done=true');}finally{rt.stop();}`;
 const output=execFileSync(process.execPath,['--input-type=module','-e',program,join(dir,'C.sacura.json')],{cwd:root,encoding:'utf8'});assert.match(output,/C only: count=2/);assert.equal(JSON.stringify(pkgA),packageBytes);assert.equal(JSON.parse(sourceBytes).functions[0].parameters[0].default,1);
});
