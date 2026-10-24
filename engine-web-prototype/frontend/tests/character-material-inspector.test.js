import Test from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer as CreateServer} from 'vite';
import {fileURLToPath as FileURLToPath} from 'node:url';
import React from 'react';

Test('Character uses shared material pickers and commands instead of reflected JSON', async () => {
  const Document = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'});
  for (const Name of ['window','document','navigator','HTMLElement','SVGElement','localStorage']) {
    Object.defineProperty(globalThis,Name,{configurable:true,value:Document.window[Name]});
  }
  globalThis.IS_REACT_ACT_ENVIRONMENT = true;
  const {render:Render,fireEvent:FireEvent,cleanup:Cleanup,waitFor:WaitFor,screen:Screen} = await import('@testing-library/react');
  const Server = await CreateServer({root:FileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
  try {
    const {ReflectedComponents} = await Server.ssrLoadModule('/src/ReflectedFields.jsx');
    const {NativeAssetsContext} = await Server.ssrLoadModule('/src/NativeAssets.jsx');
    const {setLanguage:SetLanguage} = await Server.ssrLoadModule('/src/i18n.js');
    SetLanguage('en');
    const Assets = [{id:'first',path:'/Game/First.material',typeId:'Material'}, {id:'second',path:'/Game/Second.materialinstance',typeId:'MaterialInstance'}];
    const Calls = [];
    const Command = async (Method,Parameters) => {
      Calls.push([Method,Parameters]);
      return {slots:[{slot:0,assetId:'first',inherited:false},{slot:1,assetId:'second',inherited:true}],collision:false,simulate:false};
    };
    for (const Type of ['engine.CharacterComponent','engine.MeshRendererComponent']) {
      const Reflection = {objectId:'hero',revision:1,types:[],command:Command,onRemove(){},onAdd(){},onProperty(){},
        components:[{id:'mesh',typeId:Type,displayName:'Character',properties:[
          {id:'material_slots',typeId:'engine.MaterialSlots',label:'Material Slots',editable:true,value:{entries:[]}},
          {id:'material_asset_id',typeId:'engine.string',label:'Material',editable:true,value:''},
          {id:'description',typeId:'engine.string',label:'Description',editable:true,value:'Humanoid'}
        ]}]};
      const View = Render(React.createElement(NativeAssetsContext.Provider,{value:{assets:Assets}},React.createElement(ReflectedComponents,{reflection:Reflection,running:false})));
      await WaitFor(()=>Assert.equal(View.container.querySelectorAll('.native-mesh-slot').length,2));
      Assert.equal(View.container.querySelectorAll('textarea').length,0);
      Assert.equal(View.container.querySelectorAll('.native-mesh-slot .native-asset-preview-button').length,2);
      const Slot = Screen.getByLabelText('Материал слота 0').closest('details');
      Slot.open = true;
      FireEvent.click(Slot.querySelectorAll('.native-asset-options button')[1]);
      Assert.deepEqual(Calls.at(-1),['mesh.material',{objectId:'hero',componentId:'mesh',slot:0,assetId:'second'}]);
      const Resets = Screen.getAllByRole('button',{name:'From Mesh'});
      Assert.equal(Resets[1].disabled,true);
      FireEvent.click(Resets[0]);
      Assert.deepEqual(Calls.at(-1),['mesh.material',{objectId:'hero',componentId:'mesh',slot:0,assetId:null}]);
      const AllMaterial = Screen.getByLabelText('Material for All Slots').closest('details');
      AllMaterial.open = true;
      FireEvent.click(AllMaterial.querySelector('.native-asset-options button'));
      Assert.deepEqual(Calls.at(-1),['mesh.material',{objectId:'hero',componentId:'mesh',slot:-1,assetId:'first'}]);
      View.rerender(React.createElement(NativeAssetsContext.Provider,{value:{assets:Assets}},React.createElement(ReflectedComponents,{reflection:Reflection,running:true})));
      Assert.ok(View.container.querySelector('.native-mesh-slot .native-asset-options button').disabled);
      Assert.ok(Screen.getAllByRole('button',{name:'From Mesh'}).every(Button=>Button.disabled));
      Cleanup();
    }
  } finally {
    Cleanup();
    await Server.close();
    Document.window.close();
  }
});
