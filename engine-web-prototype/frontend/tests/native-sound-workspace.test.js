import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';

const dom=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup,waitFor,act}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:NativeSoundWorkspace,nativeAudioAssets}=await server.ssrLoadModule('/src/NativeSoundWorkspace.jsx');
const {soundDesk}=await server.ssrLoadModule('/src/audio.js');
const waveform=soundDesk.waveform.bind(soundDesk),play=soundDesk.play.bind(soundDesk);
afterEach(()=>{cleanup();soundDesk.waveform=waveform;soundDesk.play=play;});
after(async()=>{await server.close();dom.window.close();});

test('native sound cards use registered WAV assets, import, preview and set startup audio',async()=>{
  const calls=[];
  soundDesk.waveform=async()=>({peaks:Array(100).fill(.25),duration:2});
  soundDesk.play=(...args)=>{calls.push(['play',...args]);return Promise.resolve();};
  const assets=[{id:'rain',name:'Rain.wav',typeId:'Audio',relativePath:'Audio/Rain.wav',path:'/Game/Audio/Rain.wav',version:'1',builtin:false},
    {id:'image',name:'Image.png',typeId:'Texture',relativePath:'Image.png',path:'/Game/Image.png'}];
  assert.equal(nativeAudioAssets(assets,'session').length,1);
  render(React.createElement(NativeSoundWorkspace,{assets,session:'session',
    onImport:async()=>calls.push(['import']),onSetStartup:async id=>calls.push(['startup',id])}));
  assert.ok(screen.getByText('Rain'));
  fireEvent.click(screen.getByRole('button',{name:'Импортировать WAV'}));
  await waitFor(()=>assert.ok(calls.some(call=>call[0]==='import')));
  fireEvent.click(screen.getByRole('button',{name:'Слушать'}));
  assert.equal(calls.find(call=>call[0]==='play')[1],'rain');
  fireEvent.click(screen.getByRole('button',{name:'Сделать стартовым'}));
  await waitFor(()=>assert.ok(calls.some(call=>call[0]==='startup'&&call[1]==='rain')));
  fireEvent.change(screen.getByLabelText('Громкость 100%'),{target:{value:'0.33'}});
  cleanup();
  await act(async()=>render(React.createElement(NativeSoundWorkspace,{assets,session:'session',onImport:async()=>{},onSetStartup:async()=>{}})));
  assert.equal(screen.getByLabelText('Громкость 33%').value,'0.33');
});
