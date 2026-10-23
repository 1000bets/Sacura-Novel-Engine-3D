import test,{after,afterEach} from 'node:test';
import assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';

const dom=new JSDOM('<html><body></body></html>',{url:'http://localhost/'});
for(const key of ['window','document','navigator','HTMLElement','localStorage'])Object.defineProperty(globalThis,key,{configurable:true,value:dom.window[key]});
globalThis.IS_REACT_ACT_ENVIRONMENT=true;
const {render,screen,fireEvent,cleanup,waitFor}=await import('@testing-library/react');
const server=await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {default:NativeCharacterWorkspace,nativeCharacters,nativeCharacterModels}=await server.ssrLoadModule('/src/NativeCharacterWorkspace.jsx');
afterEach(cleanup);
after(async()=>{await server.close();dom.window.close();});

test('native character panel recognizes persisted character components and static model assets',()=>{
  const objects=[{id:'one',components:[{typeId:'engine.CharacterComponent'}]},{id:'two',components:[{typeId:'engine.MeshRendererComponent'}]}];
  const assets=[{id:'model',typeId:'Model',name:'Hero.glb',subassets:[{id:'mesh',typeId:'StaticMesh',name:'Body'},{id:'skin',typeId:'SkeletalMesh',name:'Rig'}]}];
  assert.deepEqual(nativeCharacters(objects).map(item=>item.id),['one']);
  assert.deepEqual(nativeCharacterModels(assets).map(item=>item.key),['model|mesh']);
});

test('native character panel edits scene identity and assigns a registered mesh',async()=>{
  const calls=[],selected=[];
  const character={id:'alice',name:'Алиса',active:true,components:[{id:'marker',typeId:'engine.CharacterComponent'},{id:'renderer',typeId:'engine.MeshRendererComponent'}]};
  const components=[{id:'marker',typeId:'engine.CharacterComponent',properties:[{id:'description',value:'Героиня'}]},
    {id:'renderer',typeId:'engine.MeshRendererComponent',properties:[{id:'mesh_asset_id',value:'starter',subAssetId:'body'}]}];
  const assets=[{id:'starter',name:'StarterCharacter.glb',typeId:'Model',builtin:true,subassets:[{id:'body',name:'Body',typeId:'StaticMesh'}]},
    {id:'custom',name:'Hero.glb',typeId:'Model',builtin:false,subassets:[{id:'mesh',name:'Body',typeId:'StaticMesh'}]}];
  render(React.createElement(NativeCharacterWorkspace,{objects:[character],components,assets,selected:'alice',dirty:true,
    command:async(method,params)=>{calls.push([method,params]);return {id:'created'};},select:id=>selected.push(id),onFocus(){},onImport:async()=>{}}));
  assert.equal(screen.getAllByText('Алиса').length,2);
  fireEvent.change(screen.getByLabelText('Меш персонажа'),{target:{value:'custom|mesh'}});
  assert.deepEqual(calls.at(-1),['component.setAsset',{objectId:'alice',componentId:'renderer',property:'mesh_asset_id',assetId:'custom',subAssetId:'mesh'}]);
  fireEvent.change(screen.getByLabelText('Описание персонажа'),{target:{value:'Главная героиня'}});
  fireEvent.blur(screen.getByLabelText('Описание персонажа'));
  assert.deepEqual(calls.at(-1),['component.setProperty',{objectId:'alice',componentId:'marker',property:'description',value:'Главная героиня'}]);
  fireEvent.change(screen.getByLabelText('Имя нового персонажа'),{target:{value:'Борис'}});
  fireEvent.click(screen.getByRole('button',{name:'Создать персонажа'}));
  await waitFor(()=>assert.deepEqual(calls.at(-1),['character.create',{name:'Борис'}]));
  await waitFor(()=>assert.deepEqual(selected,['created']));
});

test('existing scene object can be made a character',async()=>{
  const calls=[],selected=[];
  render(React.createElement(NativeCharacterWorkspace,{objects:[{id:'mira',name:'Mira',components:[]}],components:[],assets:[],
    command:async(method,params)=>{calls.push([method,params]);return {id:'marker'};},select:id=>selected.push(id),onFocus(){},onImport:async()=>{}}));
  fireEvent.click(screen.getByRole('button',{name:'Сделать персонажем'}));
  await waitFor(()=>assert.deepEqual(calls,[['object.addComponent',{objectId:'mira',typeId:'engine.CharacterComponent'}]]));
  await waitFor(()=>assert.deepEqual(selected,['mira']));
});

test('imported rigged FBX static mesh is assigned to the selected character',async()=>{
  const calls=[];
  const character={id:'hero',name:'Hero',active:true,components:[{id:'marker',typeId:'engine.CharacterComponent'}]};
  render(React.createElement(NativeCharacterWorkspace,{objects:[character],components:[],assets:[],selected:'hero',
    command:async(method,params)=>{calls.push([method,params]);return {id:'renderer'};},select(){},onFocus(){},
    onImport:async()=>({assetId:'imported',subAssetId:'body'})}));
  fireEvent.click(screen.getByRole('button',{name:'Импортировать модель'}));
  await waitFor(()=>assert.deepEqual(calls,[
    ['object.addComponent',{objectId:'hero',typeId:'engine.MeshRendererComponent'}],
    ['component.setAsset',{objectId:'hero',componentId:'renderer',property:'mesh_asset_id',assetId:'imported',subAssetId:'body'}]
  ]));
});

test('importing while a camera was marked as character creates a separate visible character',async()=>{
  const calls=[],selected=[];
  const camera={id:'camera',name:'Main Camera',active:true,components:[
    {id:'marker',typeId:'engine.CharacterComponent'},
    {id:'camera-component',typeId:'engine.CameraComponent'}]};
  render(React.createElement(NativeCharacterWorkspace,{objects:[camera],components:[{id:'marker',typeId:'engine.CharacterComponent'}],
    assets:[],selected:'camera',command:async(method,params)=>{
      calls.push([method,params]);
      if(method==='character.create')return {id:'actor'};
      if(method==='scene.getHierarchy')return [{id:'actor',components:[{id:'actor-mesh',typeId:'engine.MeshRendererComponent'}]}];
      return {};
    },select:id=>selected.push(id),onFocus(){},onImport:async()=>({assetId:'imported',subAssetId:'body',name:'Actor_rigged (2)'})}));
  fireEvent.click(screen.getByRole('button',{name:'Импортировать модель'}));
  await waitFor(()=>assert.deepEqual(calls,[
    ['character.create',{name:'Actor_rigged (2)'}],['scene.getHierarchy',undefined],
    ['component.setAsset',{objectId:'actor',componentId:'actor-mesh',property:'mesh_asset_id',assetId:'imported',subAssetId:'body'}]
  ]));
  await waitFor(()=>assert.deepEqual(selected,['actor']));
});
