import test, {after, afterEach} from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import React from 'react';
const Browser = new JSDOM('<html><body></body></html>', {url:'http://localhost/'});
for (const Name of ['window','document','navigator','HTMLElement']) {
  Object.defineProperty(globalThis,Name,{configurable:true,value:Browser.window[Name]});
}
globalThis.IS_REACT_ACT_ENVIRONMENT = true;
const {render:Render,screen:Screen,fireEvent:FireEvent,cleanup:Cleanup} = await import('@testing-library/react');
const Server = await createServer({root:fileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
const {AssetReferenceField,NativeAssetsContext} = await Server.ssrLoadModule('/src/NativeAssets.jsx');
afterEach(Cleanup);
after(async () => {await Server.close();Browser.window.close();});
const Assets = [{id:'model',path:'/Game/Ship.glb',typeId:'Model',subassets:[{id:'mesh',name:'Ship',typeId:'StaticMesh'}]}, {id:'audio',path:'/Game/Sound.wav',typeId:'Audio'}];
test('inline Open preserves asset and subasset identity and remains available for read-only fields', () => {
  const Calls = [];
  Render(React.createElement(NativeAssetsContext.Provider,{value:{assets:Assets,open:Identity=>Calls.push(Identity)}},React.createElement(AssetReferenceField,{assetId:'model',subAssetId:'mesh',disabled:true})));
  Assert.equal(Screen.getByLabelText('Ассет').textContent,'/Game/Ship.glb / Ship');
  FireEvent.click(Screen.getByRole('button',{name:'Открыть ассет'}));
  Assert.deepEqual(Calls,[{assetId:'model',subAssetId:'mesh'}]);
  Assert.equal(Screen.getByRole('button',{name:'Очистить ссылку'}).disabled,true);
});
test('empty, missing and unsupported references have distinct states and cannot open', () => {
  for (const Entry of [{assetId:null,caption:'Не назначено'},{assetId:'missing',caption:'Недоступный ресурс'},{assetId:'model',subAssetId:'removed',caption:'Недоступный ресурс'},{assetId:'audio',caption:'/Game/Sound.wav'}]) {
    Render(React.createElement(NativeAssetsContext.Provider,{value:{assets:Assets,open:()=>Assert.fail('Unavailable asset opened')}},React.createElement(AssetReferenceField,Entry)));
    Assert.equal(Screen.getByLabelText('Ассет').textContent,Entry.caption);
    const Open = Screen.getByRole('button',{name:'Открыть ассет'});
    Assert.equal(Open.disabled,true);
    FireEvent.click(Open);
    Cleanup();
  }
});
