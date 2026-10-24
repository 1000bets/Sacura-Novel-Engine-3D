import Test from 'node:test';
import Assert from 'node:assert/strict';
import {JSDOM} from 'jsdom';
import {createServer as CreateServer} from 'vite';
import {fileURLToPath as FileURLToPath} from 'node:url';
import React from 'react';

Test('native Player sends fire under pointer lock and physical keys without browser code', async () => {
  const Document = new JSDOM('<!doctype html><html><body></body></html>', {url:'http://localhost/'});
  for (const Name of ['window','document','navigator','HTMLElement','SVGElement','localStorage','getComputedStyle']) {
    Object.defineProperty(globalThis, Name, {configurable:true,value:Document.window[Name]});
  }
  globalThis.IS_REACT_ACT_ENVIRONMENT = true;
  globalThis.ResizeObserver = class { observe() {} disconnect() {} };
  const {render:Render,fireEvent:FireEvent,cleanup:Cleanup,act:Act,screen:Screen} = await import('@testing-library/react');
  const Server = await CreateServer({root:FileURLToPath(new URL('..',import.meta.url)),server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
  const Events = [];
  const State = {gameRunning:true,gamePaused:false,dialogue:{phase:'idle'},gameViews:[{pointerLock:true,projectTitle:'Input regression'}]};
  Document.window.navigator.getGamepads = () => [];
  Document.window.sacuraDesktop = {
    bridge:{connect:async()=>({connected:true,context:{}}),request:async()=>({ok:true,context:{},value:State})},
    nativeStatus:async()=>({}),nativeHeartbeat:async()=>{},nativeBounds:async()=>{},
    nativeInput:async Event=>{Events.push(Event);}
  };
  try {
    const {default:Player} = await Server.ssrLoadModule('/src/NativeGamePlayer.jsx');
    let Rendered;
    await Act(async()=>{Rendered=Render(React.createElement(Player));});
    const Canvas = Rendered.container.querySelector('canvas');
    Canvas.requestPointerLock = async()=>{Object.defineProperty(Document.window.document,'pointerLockElement',{configurable:true,value:Canvas});};
    Canvas.setPointerCapture = ()=>{throw new Error('Pointer capture is invalid while locked');};
    await Act(async()=>{
      FireEvent(Canvas,new Document.window.MouseEvent('pointerdown',{bubbles:true,button:0,clientX:10,clientY:10}));
      FireEvent(Canvas,new Document.window.MouseEvent('pointerup',{bubbles:true,button:0,clientX:10,clientY:10}));
    });
    Assert.deepEqual(Events.filter(Event=>Event.control==='Primary').map(Event=>Event.value[0]),[1,0]);
    for (const [Key,KeyCode,Expected] of [['к',82,'KeyR'],['ц',87,'KeyW'],['2',50,'Digit2'],[' ',32,'Space'],['Escape',27,'Escape']]) {
      await Act(async()=>{
        FireEvent.keyDown(Canvas,{key:Key,keyCode:KeyCode,code:''});
        FireEvent.keyUp(Canvas,{key:Key,keyCode:KeyCode,code:''});
      });
      Assert.deepEqual(Events.filter(Event=>Event.code===Expected).map(Event=>Event.down),[true,false]);
    }
    Assert.equal(Document.window.document.title,'Input regression');
    Cleanup();
    Object.defineProperty(Document.window.document,'pointerLockElement',{configurable:true,value:null});
    const {default:Hud} = await Server.ssrLoadModule('/src/NativeGameHud.jsx');
    const Actions = [];
    Render(React.createElement(Hud,{State:{dialogue:{phase:'end'},gameViews:[{componentId:'controller',objectId:'camera',ending:'Game Over'}]},Execute:Action=>Actions.push(Action)}));
    FireEvent.click(Screen.getByRole('button'));
    Assert.deepEqual(Actions,[{componentId:'controller',objectId:'camera',action:'restart'}]);
    Assert.ok(Screen.getByText('Game Over'));
  } finally {
    Cleanup();
    await Server.close();
    Document.window.close();
  }
});
