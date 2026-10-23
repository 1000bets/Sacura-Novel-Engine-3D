import React, {useEffect, useRef, useState} from 'react';

const modifiers = e => ({ctrl: !!(e.ctrlKey || e.metaKey), alt: !!e.altKey, shift: !!e.shiftKey});
export default function NativeViewport({blocked = false, editTool, editSpace, snap, showGrid, focusRequest, onActivate, onCreateProject}) {
  const surface = useRef(null);
  const [status, setStatus] = useState({phase: 'starting'});
  const [hidden, setHidden] = useState(false);
  const hiddenRef = useRef(hidden); hiddenRef.current = hidden;
  const api = window.sacuraDesktop;
  const ready = status.phase === 'ready';
  const fail = error => setStatus(s => ({...s, error: error.message}));
  const input = event => api.nativeInput(event).catch(fail);
  const reset = () => input({kind: 'reset'});
  useEffect(() => {api.nativeTools({tool: editTool || 'translate', space: editSpace || 'local', snap: !!snap, grid: !!showGrid}).catch(fail);}, [editTool, editSpace, snap, showGrid]);
  useEffect(() => {if (focusRequest) {input({kind:'key',code:'KeyF',down:true}); input({kind:'key',code:'KeyF',down:false});}}, [focusRequest]);
  useEffect(() => {if (blocked || hidden) reset();}, [blocked, hidden]);
  useEffect(() => {
    let disposed = false, frame, previous = '', pending = false, extent = '', lastResize = 0;
    function update() {
      if (disposed) return;
      const element = surface.current;
      if (element) {
        const r = element.getBoundingClientRect(), scale = window.devicePixelRatio;
        const nextExtent = `${r.width}:${r.height}:${scale}`;
        if (nextExtent !== extent) {extent=nextExtent;lastResize=performance.now();}
        // Chromium clips the image. Do not distort projection when partially clipped.
        const bounds = {x: Math.max(0, Math.round(r.left * scale)), y: Math.max(0, Math.round(r.top * scale)),
          width: Math.max(0, Math.round(r.width * scale)), height: Math.max(0, Math.round(r.height * scale)), scale, interactive:performance.now()-lastResize<120,
          visible: !hiddenRef.current && !document.hidden && r.width > 0 && r.height > 0 && element.checkVisibility({visibilityProperty: true})};
        const key = JSON.stringify(bounds);
        if (!pending && key !== previous) {
          previous = key; pending = true;
          api.nativeBounds(bounds).catch(fail).finally(() => {pending = false;});
        }
      }
      frame = requestAnimationFrame(update);
    }
    update();
    const poll = setInterval(async () => {
      try {const value = await api.nativeStatus(); if (!disposed) setStatus(value); await api.nativeHeartbeat();}
      catch (error) {if (!disposed) fail(error);}
    }, 500);
    const blur = () => reset(); window.addEventListener('blur', blur);
    const untest = api.onNativeTest?.(command => {
      if (command.action === 'visibility') setHidden(!command.value);
      if (command.action === 'input') input(command.event);
    });
    return () => {
      disposed = true; cancelAnimationFrame(frame); clearInterval(poll); window.removeEventListener('blur', blur); untest?.();
      reset(); api.nativeDetach().catch(() => {});
    };
  }, []);
  function pointer(e, phase) {
    if (blocked || hidden || !ready) return;
    e.stopPropagation(); e.preventDefault();
    if (phase === 'down') {onActivate?.();e.currentTarget.focus(); e.currentTarget.setPointerCapture(e.pointerId);}
    const r = e.currentTarget.getBoundingClientRect(), scale = window.devicePixelRatio;
    input({kind: 'pointer', phase, x: (e.clientX-r.left)*scale, y: (e.clientY-r.top)*scale,
      button: e.button, buttons: e.buttons, ...modifiers(e)});
    if (phase === 'up' && e.currentTarget.hasPointerCapture(e.pointerId)) e.currentTarget.releasePointerCapture(e.pointerId);
  }
  function key(e, down) {
    if (blocked || hidden || !ready) return;
    // Saving belongs to the focused editor domain, not the camera's S key.
    if ((e.ctrlKey || e.metaKey) && e.code === 'KeyS') return;
    if (!['KeyW','KeyA','KeyS','KeyD','KeyQ','KeyE','KeyR','KeyF','KeyZ','KeyY','Escape','ShiftLeft','ShiftRight','AltLeft','AltRight','ControlLeft','ControlRight'].includes(e.code)) return;
    e.preventDefault(); e.stopPropagation(); input({kind:'key', code:e.code, down, repeat:e.repeat, ...modifiers(e)});
  }
  const metrics = status.presentation || {};
  return <div style={{position:'absolute',inset:0,background:'#171b24'}}>

    <div ref={surface} tabIndex={0} style={{position:'absolute',inset:0,outline:'none',touchAction:'none'}}
      onFocus={onActivate} onPointerDown={e=>pointer(e,'down')} onPointerMove={e=>pointer(e,'move')} onPointerUp={e=>pointer(e,'up')}
      onPointerCancel={reset} onLostPointerCapture={reset} onBlur={reset} onKeyDown={e=>key(e,true)} onKeyUp={e=>key(e,false)}
      onContextMenu={e=>e.preventDefault()} onWheel={e=>{if (ready&&!blocked&&!hidden) {e.stopPropagation();input({kind:'wheel',delta:e.deltaY*(e.deltaMode===1?16:1),...modifiers(e)});}}}>
      <canvas data-native-surface style={{width:'100%',height:'100%',display:hidden?'none':'block'}}/>
    </div>
    {ready && <div data-native-overlay style={{position:'absolute',left:10,top:10,padding:'5px 8px',background:'#174e38',color:'#fff',fontSize:11,pointerEvents:'none',borderRadius:4}}>
      C++ · Diligent D3D12 · {Number(metrics.fps||0).toFixed(1)} FPS · {Number(metrics.frameMs||0).toFixed(1)} ms
      <br/>{status.viewport?.data?.selection?.name || 'Нет выделения'} · {status.stats?.width||0}×{status.stats?.height||0}
    </div>}
    {status.phase === 'idle' && <div data-native-project-prompt style={{position:'absolute',inset:0,display:'flex',flexDirection:'column',alignItems:'center',justifyContent:'center',gap:12,padding:24,textAlign:'center'}}>
      <h2 style={{margin:0}}>Создайте или откройте проект</h2>
      <p style={{margin:0,color:'#b7bec9'}}>Выберите файл .project, чтобы загрузить сцену в C++ Engine.</p>
      <div style={{display:'flex',gap:12}}>
        <button onClick={onCreateProject}>Новый проект…</button>
        <button onClick={()=>api.nativeOpenProject().catch(fail)}>Открыть проект…</button>
      </div>
    </div>}
    <div style={{position:'absolute',right:10,top:10,display:'flex',gap:8,fontSize:11}}>
      <button onClick={()=>api.nativeOpenProject().catch(fail)}>Открыть C++ проект…</button>
      {ready && <button onClick={()=>setHidden(v=>!v)}>{hidden?'Показать':'Скрыть'}</button>}
    </div>
    {ready && <div style={{position:'absolute',bottom:8,left:10,pointerEvents:'none',fontSize:11,color:'#ddd',background:'#171b24b0'}}>ЛКМ: выбор · Alt+ЛКМ: орбита · СКМ: панорама · ПКМ+WASD/QE: полёт · F: фокус · W/E/R: gizmo · Ctrl+Z/Y</div>}
    {(status.error||metrics.error) && <p role="alert" style={{position:'absolute',top:65,left:12,color:'#ff8d8d',background:'#20151d'}}>Engine: {status.error||metrics.error}</p>}
  </div>;
}
