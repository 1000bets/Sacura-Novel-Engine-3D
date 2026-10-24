import NativeRuntimeWidgets from './NativeRuntimeWidgets.jsx';
import NativeGameHud from './NativeGameHud.jsx';
import NativeDialogue from './NativeDialogue.jsx';
import {WidgetMenu} from './WidgetRenderer.jsx';
import TouchInput from './TouchInput.jsx';
import './gameplay.css';
import React, {useEffect, useRef, useState} from 'react';

export default function NativeGamePlayer() {
  const Desktop = window.sacuraDesktop;
  const Canvas = useRef(null);
  const Context = useRef(null);
  const AudioCue = useRef(null);
  const AudioLoaded = useRef(false);
  const MenuInitialized = useRef(false);
  const [Menu,SetMenu] = useState(true);
  const [State, SetState] = useState(null);
  const CurrentState = useRef(null);
  CurrentState.current = State;
  useEffect(() => {
    const Title = State?.gameViews?.[0]?.projectTitle;
    if (Title && Canvas.current) {
      Canvas.current.ownerDocument.title = Title;
    }
  }, [State?.gameViews?.[0]?.projectTitle]);
  useEffect(() => {
    const OwnerDocument = Canvas.current?.ownerDocument;
    if ((State?.gamePaused || !State?.gameViews?.some(View => View.pointerLock)) && OwnerDocument?.pointerLockElement === Canvas.current) {
      OwnerDocument.exitPointerLock();
    }
  }, [State]);
  const [FailureMessage, SetError] = useState('');
  const Request = async (Method, Parameters = {}) => {
    const Result = await Desktop.bridge.request({method: Method, params: Parameters, context: Context.current});
    Context.current = Result.context;
    if (!Result.ok) { throw new Error(Result.error.message); }
    SetState(Result.value);
    return Result.value;
  };
  const Action = (Method, Parameters) => Request(Method, Parameters).catch(Failure => SetError(Failure.message));
  const GameAction = async Identifier => {
    const View = CurrentState.current?.gameViews?.[0];
    if (View) { return await Request('game.action',{objectId:View.objectId,componentId:View.componentId,action:Identifier}); }
  };
  const MenuAction = async (Identifier, Element) => {
    if (Identifier === 'gameplay' && Element?.nativeAction) {
      Identifier = Element.nativeAction;
    }
    try {
      if (Identifier==='mainMenu') { await Request('game.pause',{paused:true});SetMenu(true); }
      else if (Identifier==='start'||Identifier==='restart') { await GameAction('restart');await Request('game.pause',{paused:false});SetMenu(false); }
      else if (Identifier==='resume') { await Request('game.pause',{paused:false});SetMenu(false); }
      else {
        const Result=await GameAction(Identifier);
        if(Identifier==='load'&&Result?.gameViews?.[0]?.storageStatus==='loaded'){await Request('game.pause',{paused:false});SetMenu(false);}
      }
    } catch (Failure) { SetError(Failure.message); }
  };
  useEffect(() => {
    let Active = true;
    let Busy = false;
    let PreviousBounds = '';
    const GamepadSources = new Set();
    const Feed = (Device, Control, Value, Transient = false, Source = 'ElectronPointer') => Desktop.nativeInput({kind: 'gameControl', device: Device, control: Control, value: Value, transient: Transient, source: Source});
    const OwnerWindow = Canvas.current.ownerDocument.defaultView;
    const Refresh = async () => {
      if (!Active || Busy) { return; }
      Busy = true;
      try {
        const Status = await Desktop.nativeStatus();
        if (Status.error) { throw new Error(Status.error); }
        const Connection = await Desktop.bridge.connect();
        if (Connection.connected) {
          Context.current = Connection.context;
          const CurrentState = await Request('game.state');
          if ((CurrentState.widgetInstances || []).length) { SetMenu(false); MenuInitialized.current = true; }
          if (!MenuInitialized.current&&CurrentState.gameViews?.[0]?.widgets) {
            MenuInitialized.current=true;
            await Request('game.pause',{paused:true});
          }
          if (CurrentState.startupAudio && !AudioLoaded.current) {
            AudioLoaded.current = true;
            const AudioResult = await Desktop.bridge.request({method: 'audio.resolveStartup', params: {}});
            if (!AudioResult.ok) { throw new Error(AudioResult.error.message); }
            const AudioUrl = new URL('file:///');
            AudioUrl.pathname = AudioResult.value.absolutePath.replaceAll('\\', '/');
            AudioCue.current = new Audio(AudioUrl.href);
            AudioCue.current.loop = true;
            await AudioCue.current.play();
          }
          if (AudioCue.current) {
            if (CurrentState.gamePaused) { AudioCue.current.pause(); }
            else if (AudioCue.current.paused) { await AudioCue.current.play(); }
          }
          await Desktop.nativeHeartbeat();
          const Rectangle = Canvas.current.getBoundingClientRect();
          const Scale = OwnerWindow.devicePixelRatio;
          const Bounds = {x: 0, y: 0, width: Math.round(Rectangle.width * Scale), height: Math.round(Rectangle.height * Scale), scale: Scale, visible: !Canvas.current.ownerDocument.hidden};
          const Serialized = JSON.stringify(Bounds);
          if (Serialized !== PreviousBounds) {
            await Desktop.nativeBounds(Bounds);
            PreviousBounds = Serialized;
          }
        }
      } catch (Failure) {
        if (Active) { SetError(Failure.message); }
      } finally { Busy = false; }
    };
    const Key = Event => {
      if (Event.target.closest('button,input,textarea,select') || Event.repeat) { return; }
      let Code = Event.code;
      if (!Code || Code === 'Unidentified') {
        if (Event.keyCode >= 65 && Event.keyCode <= 90) {
          Code = 'Key' + String.fromCharCode(Event.keyCode);
        } else if (Event.keyCode >= 48 && Event.keyCode <= 57) {
          Code = 'Digit' + String.fromCharCode(Event.keyCode);
        } else if (Event.keyCode === 32) {
          Code = 'Space';
        } else if (Event.keyCode === 27) {
          Code = 'Escape';
        } else if (/^[0-9]$/.test(Event.key)) {
          Code = 'Digit' + Event.key;
        } else if (/^[A-Za-z]$/.test(Event.key)) {
          Code = 'Key' + Event.key.toUpperCase();
        } else {
          Code = Event.key;
        }
      }
      if (!/^[A-Za-z][A-Za-z0-9]{0,47}$/.test(Code)) {
        return;
      }
      Desktop.nativeInput({kind: 'key', code: Code, down: Event.type === 'keydown'}).catch(Failure => SetError(Failure.message));
    };
    const Pointer = Event => {
      const bLockPointer = CurrentState.current?.gameViews?.some(View => View.pointerLock);
      if (Event.type === 'pointerdown' && Event.button === 0 && bLockPointer && Canvas.current.ownerDocument.pointerLockElement !== Canvas.current) {
        Canvas.current.requestPointerLock()?.catch(Failure => SetError(Failure.message));
      }
      const Rectangle = Canvas.current.getBoundingClientRect();
      Feed('Mouse', 'Position', [(2 * (Event.clientX - Rectangle.left) / Rectangle.width - 1) * Rectangle.width / Rectangle.height, 1 - 2 * (Event.clientY - Rectangle.top) / Rectangle.height, 0]).catch(Failure => SetError(Failure.message));
      if (Event.type === 'pointermove') {
        Feed('Mouse', 'Delta', [Event.movementX, Event.movementY, 0], true).catch(Failure => SetError(Failure.message));
        return;
      }
      const Controls = ['Primary', 'Middle', 'Secondary'];
      if (Controls[Event.button]) {
        let Value = 0;
        if (Event.type === 'pointerdown') {
          Value = 1;
          if (!bLockPointer && !Canvas.current.ownerDocument.pointerLockElement) {
            Canvas.current.setPointerCapture(Event.pointerId);
          }
        }
        Feed('Mouse', Controls[Event.button], [Value, 0, 0]).catch(Failure => SetError(Failure.message));
      }
    };
    const Wheel = Event => Feed('Mouse', 'Wheel', [-Event.deltaY / 120, 0, 0], true).catch(Failure => SetError(Failure.message));
    const PollGamepads = () => {
      if (!OwnerWindow.document.hasFocus()) { return; }
      const Connected = new Set();
      for (const Gamepad of OwnerWindow.navigator.getGamepads()) {
        if (!Gamepad || Gamepad.mapping !== 'standard') { continue; }
        const Source = 'ElectronGamepad' + Gamepad.index;
        Connected.add(Source); GamepadSources.add(Source);
        ['LeftX', 'LeftY', 'RightX', 'RightY'].forEach((Control, Index) => Feed('Gamepad', Control, [Gamepad.axes[Index] || 0, 0, 0], false, Source).catch(() => {}));
        ['South', 'East', 'West', 'North', 'LeftShoulder', 'RightShoulder', 'LeftTrigger', 'RightTrigger', 'Back', 'Start', 'LeftStick', 'RightStick', 'DpadUp', 'DpadDown', 'DpadLeft', 'DpadRight', 'Guide'].forEach((Control, Index) => Feed('Gamepad', Control, [Gamepad.buttons[Index]?.value || 0, 0, 0], false, Source).catch(() => {}));
      }
      for (const Source of GamepadSources) {
        if (!Connected.has(Source)) { GamepadSources.delete(Source); Desktop.nativeInput({kind: 'removeSource', source: Source}).catch(() => {}); }
      }
    };
    const Surface = Canvas.current;
    for (const Name of ['pointerdown', 'pointerup', 'pointermove']) { Surface.addEventListener(Name, Pointer); }
    Surface.addEventListener('wheel', Wheel);
    const GamepadTimer = OwnerWindow.setInterval(PollGamepads, 16);
    const UnsubscribeTest = Desktop.onNativeTest?.(async Command => {
      try {
        const Result = await Request(Command.method, Command.params || {});
        if(Command.method==='game.pause'&&Command.params?.paused===false){SetMenu(false);}
        Desktop.bridgeTestResult({id: Command.id, ok: true, value: Result});
      } catch (Failure) { Desktop.bridgeTestResult({id: Command.id, ok: false, error: Failure.message}); }
    });
    const Reset = () => { PreviousBounds = ''; Desktop.nativeInput({kind: 'reset'}).catch(() => {}); };
    OwnerWindow.addEventListener('keydown', Key);
    OwnerWindow.addEventListener('keyup', Key);
    OwnerWindow.addEventListener('blur', Reset);
    OwnerWindow.addEventListener('focus', Reset);
    const Timer = OwnerWindow.setInterval(Refresh, 100);
    Refresh();
    return () => {
      Active = false;
      OwnerWindow.clearInterval(Timer);
      OwnerWindow.clearInterval(GamepadTimer);
      for (const Name of ['pointerdown', 'pointerup', 'pointermove']) { Surface.removeEventListener(Name, Pointer); }
      Surface.removeEventListener('wheel', Wheel);
      UnsubscribeTest?.();
      AudioCue.current?.pause();
      OwnerWindow.removeEventListener('keydown', Key);
      OwnerWindow.removeEventListener('keyup', Key);
      OwnerWindow.removeEventListener('blur', Reset);
      OwnerWindow.removeEventListener('focus', Reset);
    };
  }, []);
  const Dialogue = State?.dialogue;
  return <main className="standalone-game scene-viewport native-game-player" style={{position: 'fixed', inset: 0, background: '#17171b'}}>
    <canvas ref={Canvas} data-native-surface data-native-owner="" style={{width: '100%', height: '100%', display: 'block'}}/>
    <NativeRuntimeWidgets State={State} Execute={Parameters => Action('widget.event', Parameters)}/>
    {!Menu&&<NativeGameHud State={State} Execute={Parameters => Action('game.action', Parameters)}/>}
    <div className="standalone-bar"><button aria-label="Пауза / продолжить" disabled={!State} onClick={() => {if(Menu){MenuAction('start');}else{Action('game.pause', {paused: !State.gamePaused});}}}>⏯</button><button onClick={()=>MenuAction('save')}>Сохранить</button><button onClick={()=>MenuAction('load')}>Загрузить</button></div>
    {Menu&&State?.gameViews?.[0]?.widgets?.mainMenu&&<WidgetMenu widget={State.gameViews[0].widgets.mainMenu} label="Главное меню" context={{variables:State.gameViews[0].variables||{},title:State.gameViews[0].projectTitle,paused:true}} onAction={MenuAction}/>}
    {!Menu&&State?.gamePaused&&State.gameViews?.[0]?.widgets?.pauseMenu&&<WidgetMenu widget={State.gameViews[0].widgets.pauseMenu} label="Пауза" context={{variables:State.gameViews[0].variables||{},paused:true}} onAction={MenuAction}/>}
    {!Menu&&!State?.gamePaused&&<NativeDialogue state={State} onGameAction={Parameters=>Action('game.action',Parameters)} onAdvance={()=>Action('game.advance')} onChoice={Index=>Action('game.choose',{index:Index})}/>}
    {State?.gameViews?.[0]?.input&&<TouchInput settings={State.gameViews[0].input} contexts={['system','dialogue','gameplay','game-mechanics']} active={!Menu} paused={State.gamePaused} playerMode={State.gameViews[0].controllerMode} onInput={Event=>{
      let Value=[Event.value,0,0];
      if(Array.isArray(Event.value)){Value=[...Event.value,0];}
      Desktop.nativeInput({kind:'gameControl',device:'Touch',control:Event.control,value:Value,source:'ElectronTouch'+Event.control}).catch(Failure=>SetError(Failure.message));
    }}/>}
    {(FailureMessage || Dialogue?.error) && <p role="alert" style={{position: 'absolute', top: 16, left: 16, background: '#411', padding: 16}}>{FailureMessage || Dialogue.error}</p>}
  </main>;
}
