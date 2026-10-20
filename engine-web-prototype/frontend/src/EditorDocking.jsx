import React, {createContext, useCallback, useContext, useEffect, useLayoutEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {t as tr, useLocale} from './i18n.jsx';
import {DOCK_AREAS, activeDockWindow, dockWindow, isWindowOpen, windowLabel} from './editorWindows.js';

export const TAB_DRAG_TYPE = 'application/sacura-editor-window';
const DockingContext = createContext(null);
export const useDocking = () => useContext(DockingContext);
export const acceptsWindowDrag = event => Array.from(event.dataTransfer?.types || []).includes(TAB_DRAG_TYPE);
export function dockDestinationAt(x, y) {
  const element = document.elementFromPoint?.(x, y);
  const target = element?.closest('[data-dock-target], [data-dock-strip], [data-dock-root]');
  if (!target) return null;
  return {area: target.dataset.dockTarget || target.dataset.dockStrip || target.dataset.dockRoot, beforeId: element.closest('[data-dock-window]')?.dataset.dockWindow};
}

export function EditorDockProvider({windows, onChange, onSelect, onClose, onPlace, children}) {
  const hosts = useRef({}), [drag, setDrag] = useState(null);
  const place = (id, area, beforeId) => {
    onChange(current => dockWindow(current, id, beforeId, area));
    onPlace?.(area); setDrag(null);
    queueMicrotask(() => document.getElementById('editor-tab-' + id)?.focus());
  };
  useEffect(() => {
    if (!drag) return;
    const cancel = () => setDrag(null);
    const escape = e => {if (e.key === 'Escape') {e.preventDefault(); e.stopPropagation(); cancel();}};
    window.addEventListener('dragend', cancel); window.addEventListener('drop', cancel); if (drag.kind === 'float') window.addEventListener('blur', cancel); window.addEventListener('keydown', escape, true);
    return () => {window.removeEventListener('dragend', cancel); window.removeEventListener('drop', cancel); window.removeEventListener('blur', cancel); window.removeEventListener('keydown', escape, true);};
  }, [drag]);
  return <DockingContext.Provider value={{windows, onChange, onSelect, onClose, hosts, drag, setDrag, place}}>{children}
    {drag?.point && <div className="dock-drag-preview" aria-hidden="true" style={{left:drag.point.x + 14, top:drag.point.y + 14}}>{tr(windowLabel(drag.id))}</div>}
  </DockingContext.Provider>;
}

export function EditorDockBody({area}) {
  const docking = useDocking();
  const register = useCallback(element => {docking.hosts.current[area] = element;}, [docking.hosts, area]);
  return <div className="dock-region-content" hidden={!activeDockWindow(docking.windows, area)} ref={register}/>;
}

export function EditorDockStrip({area, className, children}) {
  const docking = useDocking();
  return <div className={className} data-dock-strip={area} onDragOver={e => {if (acceptsWindowDrag(e)) {e.preventDefault(); e.dataTransfer.dropEffect = 'move';}}} onDrop={e => {
    if (!acceptsWindowDrag(e)) return;
    e.preventDefault(); e.stopPropagation();
    const id = e.dataTransfer.getData(TAB_DRAG_TYPE);
    if (isWindowOpen(docking.windows, id)) docking.place(id, area);
  }}>{children}</div>;
}

export function DockDropTarget({area}) {
  useLocale();
  const docking = useDocking(), [over, setOver] = useState(false);
  useEffect(() => {if (!docking.drag) setOver(false);}, [docking.drag]);
  if (!docking.drag) return null;
  const label = tr(DOCK_AREAS.find(([id]) => id === area)[1]);
  return <div className={'dock-drop-target' + (over || docking.drag.area === area ? ' drag-over' : '') + (docking.drag.kind === 'float' ? ' pointer-dock-target' : '')}
    data-dock-target={area} role="region" aria-label={tr('Закрепить в области «{0}»', [label])}
    onDragOver={e => {if (acceptsWindowDrag(e)) {e.preventDefault(); e.stopPropagation(); e.dataTransfer.dropEffect = 'move'; setOver(true);}}}
    onDragLeave={e => {if (!e.currentTarget.contains(e.relatedTarget)) setOver(false);}}
    onDrop={e => {
      if (!acceptsWindowDrag(e)) return;
      e.preventDefault(); e.stopPropagation(); const id = e.dataTransfer.getData(TAB_DRAG_TYPE);
      if (id === docking.drag.id) docking.place(id, area);
      setOver(false);
    }}>
    <span data-dock-target={area}>{tr('Закрепить здесь')}<strong>{label}</strong></span>
  </div>;
}

export function DockPanelMount({area, floating, visible, children}) {
  const {hosts} = useDocking();
  const [container] = useState(() => {const element = document.createElement('div'); element.className = 'editor-panel-mount'; return element;});
  useLayoutEffect(() => {
    let canceled = false;
    const attach = () => {
      const target = hosts.current[floating ? 'floating' : area];
      if (target && container.parentNode !== target) target.appendChild(container);
      container.hidden = !floating && !visible;
      return !!target;
    };
    // A region later in the React tree (the inspector) registers its ref after this effect.
    if (!attach()) queueMicrotask(() => {if (!canceled) attach();});
    return () => {canceled = true;};
  }, [hosts, area, floating, visible, container]);
  useLayoutEffect(() => () => container.remove(), [container]);
  // The portal's container stays the same when its host changes, preserving drafts and selection.
  return createPortal(children, container);
}
