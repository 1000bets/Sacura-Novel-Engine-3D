import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
import {EditorView} from 'codemirror';
import {setupCodeMirrorDom} from './helpers/codeMirrorDom.js';
const dom=new JSDOM('<!doctype html><html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;setupCodeMirrorDom(dom.window);globalThis.ResizeObserver=class{observe(){}unobserve(){}disconnect(){}};globalThis.requestAnimationFrame=cb=>setTimeout(cb,0);globalThis.cancelAnimationFrame=clearTimeout;
const {render,screen,fireEvent,cleanup,waitFor,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:Scripts}=await server.ssrLoadModule('/src/MechanicScriptsWorkspace.jsx'),{default:Inspector}=await server.ssrLoadModule('/src/ComponentInstancesInspector.jsx');
const {createMechanicsProject}=await server.ssrLoadModule('/src/projectLifecycle.js'),{createComponentResource,editComponentSource,publishComponentSource}=await server.ssrLoadModule('/src/componentResources.js'),{addMechanicInstance}=await server.ssrLoadModule('/src/mechanicDescriptions.js');
const {PreviewRuntime}=await server.ssrLoadModule('/src/runtime.js'),{SoundDesk}=await server.ssrLoadModule('/src/audio.js'),{readProject}=await server.ssrLoadModule('/src/projectFiles.js');
afterEach(cleanup);after(async()=>{await server.close();dom.window.close();});
function mount(Component,p,props={}){let value=p;function Harness(){const [project,setProject]=React.useState(p);value=project;return React.createElement(Component,{...props,project,onChange:fn=>setProject(p=>{const next=structuredClone(p);fn(next);value=next;return next;})});}render(React.createElement(Harness));return ()=>value;}
function replace(text){const view=EditorView.findFromDOM(document.querySelector('.cm-editor'));act(()=>view.dispatch({changes:{from:0,to:view.state.doc.length,insert:text}}));}
const code='const amount = SerializeField(3, {label:"Награда",min:0});\nconst allowed = SerializeField(true, {label:"Начислять"});\nfunction main({global}) {if(allowed)global.score += amount;return amount;}';

test('ordinary script is created and compiled through CodeMirror; its source-owned fields appear in the actual object inspector and affect Play',async()=>{
 const p=createMechanicsProject('Поля из кода');p.variables.score=0;(p.variableTypes??={}).score='number';p.objects.push({id:'owner',name:'Получатель',type:'Активный меш',active:true,subsceneId:p.subscenes[0].id});
 let value=mount(Scripts,p,{scope:{kind:'object',id:'owner'}});fireEvent.click(screen.getAllByRole('button',{name:'Добавить скрипт',exact:true})[0]);replace(code);fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.equal(value().functions.find(f=>f.mechanicScript)?.mechanicScript.approved,true));
 fireEvent.click(screen.getByRole('button',{name:'Настройки скрипта'}));assert.equal(screen.getByRole('spinbutton',{name:'Награда',exact:true}).matches(':disabled'),true);assert.equal(screen.getAllByText('Объявлено в коде · SerializeField. Измените объявление и скомпилируйте скрипт.').length,2);
 const compiled=value().functions.find(f=>f.mechanicScript).mechanicScript.compiledSource;replace('const amount=SerializeField(calculate());\nfunction main(){}');fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.match(screen.getByRole('alert').textContent,/SerializeField/));assert.equal(value().functions.find(f=>f.mechanicScript).mechanicScript.compiledSource,compiled);assert.equal(value().functions.find(f=>f.mechanicScript).parameters.length,2);
 replace(code);fireEvent.click(screen.getByRole('button',{name:'Скомпилировать',exact:true}));await waitFor(()=>assert.match(document.querySelector('.mechanic-scripts footer').textContent,/Скомпилировано/));
 const result=value();cleanup();value=mount(Inspector,result,{scope:{kind:'object',id:'owner'}});fireEvent.click(screen.getByRole('button',{name:/Новый скрипт.*Системный/}));assert.equal(screen.getByRole('spinbutton',{name:'Награда',exact:true}).value,'3');fireEvent.change(screen.getByRole('spinbutton',{name:'Награда',exact:true}),{target:{value:'8'}});
 const loaded=readProject(JSON.stringify(value())),rt=new PreviewRuntime(new SoundDesk(()=>{throw Error('No audio expected');}));try{await rt.start(loaded);await waitFor(()=>assert.equal(rt.snapshot.variables.score,8));assert.equal(rt.snapshot.error,null);}finally{rt.stop();}
});

test('a new field on an already attached component displays its runtime default without mutating values; 0, false and empty text remain overrides',async()=>{
 const p=createMechanicsProject('Живые defaults'),r=createComponentResource(p,{name:'LiveDefaults'}),m=addMechanicInstance(p,r.definitionId,{kind:'project',id:p.id});
 editComponentSource(p,r.id,'const component={name:"LiveDefaults",parameters:{type:"object",properties:{speed:{type:"number",default:5,label:"Скорость"},active:{type:"boolean",default:true,label:"Активен"},caption:{type:"string",default:"Привет",label:"Подпись"}}},methods:{read:{result:{type:"collection",items:{type:"union",variants:[{type:"number"},{type:"boolean"},{type:"string"}]}},run({params}){return [params.speed,params.active,params.caption];}}}};');publishComponentSource(p,r.id);
 let value=mount(Inspector,p,{scope:{kind:'project',id:p.id}});fireEvent.click(screen.getByRole('button',{name:'LiveDefaults',exact:true}));assert.equal(screen.getByRole('spinbutton',{name:'Скорость',exact:true}).value,'5');assert.equal(screen.getByRole('combobox',{name:'Активен',exact:true}).value,'true');assert.equal(screen.getByRole('textbox',{name:'Подпись',exact:true}).value,'Привет');assert.deepEqual(value().gameplay.mechanics.find(x=>x.id===m.id).values,{});
 fireEvent.change(screen.getByRole('spinbutton',{name:'Скорость',exact:true}),{target:{value:'0'}});fireEvent.change(screen.getByRole('combobox',{name:'Активен',exact:true}),{target:{value:'false'}});fireEvent.change(screen.getByRole('textbox',{name:'Подпись',exact:true}),{target:{value:''}});const saved=readProject(JSON.stringify(value()));cleanup();value=mount(Inspector,saved,{scope:{kind:'project',id:p.id}});fireEvent.click(screen.getByRole('button',{name:'LiveDefaults',exact:true}));assert.equal(screen.getByRole('spinbutton',{name:'Скорость',exact:true}).value,'0');assert.equal(screen.getByRole('combobox',{name:'Активен',exact:true}).value,'false');assert.equal(screen.getByRole('textbox',{name:'Подпись',exact:true}).value,'');
 const rt=new PreviewRuntime(new SoundDesk(()=>{}));try{await rt.start(value());assert.deepEqual(await rt.game.callComponent(m.id,'read',{}),[0,false,'']);}finally{rt.stop();}
});
