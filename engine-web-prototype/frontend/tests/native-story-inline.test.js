import Test,{after,afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const DocumentEnvironment=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for (const Name of ['window','document','navigator','HTMLElement','localStorage']) {
 Object.defineProperty(globalThis,Name,{configurable:true,value:DocumentEnvironment.window[Name]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render:Render,screen:Screen,fireEvent:FireEvent,cleanup:Cleanup,waitFor:WaitFor}=await import('@testing-library/react');
const Server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {NativeStoryNodeFields,NativeStoryActionFields}=await Server.ssrLoadModule('/src/NativeStoryInspector.jsx');
const {NativeNarrativeInspector}=await Server.ssrLoadModule('/src/NativeNarrative.jsx');
afterEach(Cleanup);
after(async()=>{await Server.close();DocumentEnvironment.window.close();});
Test('inline dialogue commits once on blur, cancels Escape and refreshes after undo',()=>{
 const Changes=[];
 const Node={id:'line',kind:'Line',speaker:'Narrator',text:'Original'};
 const Properties={Node,Objects:[],Edit:Patch=>Changes.push(Patch)};
 const View=Render(React.createElement(NativeStoryNodeFields,Properties));
 let Field=Screen.getByLabelText('Текст выбранной реплики');
 FireEvent.change(Field,{target:{value:'Draft'}});
 Assert.equal(Changes.length,0);
 FireEvent.keyDown(Field,{key:'Escape'});
 FireEvent.blur(Field);
 Assert.equal(Field.value,'Original');
 Assert.equal(Changes.length,0);
 FireEvent.change(Field,{target:{value:'Edited'}});
 FireEvent.blur(Field);
 Assert.deepEqual(Changes,[{text:'Edited'}]);
 View.rerender(React.createElement(NativeStoryNodeFields,{...Properties,Node:{...Node,text:'Edited'}}));
 View.rerender(React.createElement(NativeStoryNodeFields,Properties));
 Assert.equal(Screen.getByLabelText('Текст выбранной реплики').value,'Original');
});
Test('choice prompt uses prompt field and disabled dialogue cannot commit',()=>{
 const Changes=[];
 const View=Render(React.createElement(NativeStoryNodeFields,{Node:{kind:'Choice',prompt:'Question'},Edit:Patch=>Changes.push(Patch)}));
 FireEvent.change(Screen.getByLabelText('Текст выбора'),{target:{value:'New question'}});
 FireEvent.blur(Screen.getByLabelText('Текст выбора'));
 Assert.deepEqual(Changes,[{prompt:'New question'}]);
 View.rerender(React.createElement(NativeStoryNodeFields,{Node:{kind:'Line',speaker:'Narrator',text:'Locked'},Disabled:true,Edit:Patch=>Changes.push(Patch)}));
 const Field=Screen.getByLabelText('Текст выбранной реплики');
 Assert.equal(Field.disabled,true);
 FireEvent.change(Field,{target:{value:'Rejected'}});FireEvent.blur(Field);
 Assert.equal(Changes.length,1);
});
Test('inline action edits preserve the other authored actions',()=>{
 const Actions=[{id:'first',kind:'Wait',durationSeconds:1},{id:'second',kind:'Wait',durationSeconds:2}];
 const Changes=[];
 Render(React.createElement(NativeStoryActionFields,{Node:{kind:'Action',actions:Actions},Edit:Patch=>Changes.push(Patch)}));
 FireEvent.change(Screen.getAllByLabelText('Длительность, сек')[0],{target:{value:'3'}});
 Assert.deepEqual(Changes,[{actions:[{...Actions[0],durationSeconds:3},Actions[1]]}]);
});

Test('narrative literals validate JSON, cancel drafts and reuse the typed command',async()=>{
 const Commands=[];
 const Node={id:'narrative',kind:'Narrative',narrativeType:'test',inputs:{Amount:2},choices:[]};
 const Story={document:{nodes:[Node]},narrativeCatalog:[{type:'test',inputs:[{name:'Amount',type:'engine.float',default:0}],outputs:[]}]};
 Render(React.createElement(NativeNarrativeInspector,{Node,Story,Command:async(Method,Parameters)=>{Commands.push([Method,Parameters]);return {};}}));
 const Field=Screen.getByLabelText('Amount literal JSON');
 FireEvent.blur(Field);
 Assert.equal(Commands.length,0);
 FireEvent.change(Field,{target:{value:'invalid'}});FireEvent.blur(Field);
 await WaitFor(()=>Assert.ok(Screen.getByRole('alert')));
 Assert.equal(Commands.length,0);
 FireEvent.keyDown(Field,{key:'Escape'});
 Assert.equal(Field.value,'2');
 FireEvent.change(Field,{target:{value:'3.5'}});FireEvent.blur(Field);
 await WaitFor(()=>Assert.deepEqual(Commands,[['story.setNarrativeInput',{nodeId:'narrative',input:'Amount',value:3.5}]]));
});
