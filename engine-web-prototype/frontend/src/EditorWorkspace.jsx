import React, {useCallback, useEffect, useRef, useState} from 'react';
import {Icon, Button} from './StudioParts.jsx';
import EditorContextMenu from './EditorContextMenu.jsx';
import {t as tr, useLocale} from './i18n.jsx';
import {EDITOR_WINDOWS, DOCK_AREAS, windowLabel, windowArea, dockTabs, activeDockWindow, isWindowOpen, openWindow, closeWindow, floatWindow, moveWindowTab, updateWindowRect} from './editorWindows.js';
import {EditorDockProvider, DockPanelMount, DockDropTarget, useDocking, acceptsWindowDrag, dockDestinationAt, TAB_DRAG_TYPE} from './EditorDocking.jsx';

const viewport = () => ({width: window.innerWidth, height: window.innerHeight});

export function EditorWindowsMenu({windows, onChange, onSelect, onClose, eventAvailable, issueCount, hierarchyVisible, inspectorVisible, historyOpen, onHierarchy, onInspector, onHistory, onReset}) {
  useLocale();
  const search = useRef(), [query, setQuery] = useState('');
  useEffect(() => {search.current?.focus();}, []);
  const check = (label, checked, action, icon, disabled = false, count = 0) => <button role="menuitemcheckbox" aria-checked={checked} disabled={disabled} onClick={action}>
    <span className="window-check">{checked && <Icon name="Check" size={14}/>}</span><Icon name={icon} size={15}/><span>{tr(label)}</span>{count > 0 && <small>{count}</small>}
  </button>;
  return <div className="editor-windows-menu" role="menu" aria-label={tr('Окна редактора')} onKeyDown={e => {
    if (!['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key) || e.target.tagName === 'SELECT') return;
    if (['Home', 'End'].includes(e.key) && e.target.tagName === 'INPUT') return;
    e.preventDefault();
    const controls = [...e.currentTarget.querySelectorAll('input,button:not(:disabled),select')], index = controls.indexOf(document.activeElement);
    controls[e.key === 'Home' ? 0 : e.key === 'End' ? controls.length - 1 : (index + (e.key === 'ArrowDown' ? 1 : -1) + controls.length) % controls.length]?.focus();
  }}>
    <label className="window-menu-search"><Icon name="Search" size={14}/><input ref={search} type="search" aria-label={tr('Найти окно')} placeholder={tr('Найти окно…')} value={query} onChange={e => setQuery(e.target.value)}/></label>
    {!query && <div className="window-menu-group">
      {check('Иерархия', hierarchyVisible, onHierarchy, 'PanelLeft')}
      {check('Инспектор', inspectorVisible, onInspector, 'PanelRight')}
      {check('История · отдельное окно', historyOpen, onHistory, 'BookOpen')}
    </div>}
    <div className="window-menu-group">
      {EDITOR_WINDOWS.filter(([, label]) => tr(label).toLocaleLowerCase().includes(query.toLocaleLowerCase())).map(([id, label, icon]) => <div className="window-menu-row" key={id}>
        {check(label, isWindowOpen(windows, id), () => isWindowOpen(windows, id) ? (onClose ? onClose(id) : onChange(closeWindow(windows, id))) : onSelect(id, false), icon, id === 'event' && !eventAvailable && !isWindowOpen(windows, id), id === 'issues' ? issueCount : 0)}
        {isWindowOpen(windows, id) && <button className="window-reveal" title={tr('Перейти к окну «{0}»', [tr(label)])} aria-label={tr('Перейти к окну «{0}»', [tr(label)])} onClick={() => onSelect(id)}><Icon name={windows.floating[id] ? 'AppWindow' : 'ArrowUpRight'} size={14}/></button>}
      </div>)}
    </div>
    {!query && <>
      <label className="window-menu-position"><span>{tr('Рабочая область')}</span><select aria-label={tr('Расположение рабочей области')} value={windows.position} onChange={e => onChange({...windows, position: e.target.value})}>
        <option value="bottom">{tr('Снизу от сцены')}</option><option value="left">{tr('Слева от сцены')}</option><option value="right">{tr('Справа от сцены')}</option>
      </select></label>
      <p className="window-menu-hint">{tr('Перетаскивайте вкладки между областями. Кнопка рядом с вкладками открепляет окно. Раскладка сохраняется автоматически.')}</p>
      <button role="menuitem" onClick={onReset}><Icon name="RotateCcw" size={15}/><span>{tr('Восстановить раскладку')}</span></button>
    </>}
  </div>;
}

function WindowPanel({id, rect, visible, focused, zIndex, onFocus, onRect, onClose, onDock, children}) {
  const docking = useDocking();
  const drag = useRef(), label = tr(windowLabel(id));
  const move = e => {
    if (!drag.current) return;
    const {startX, startY, rect: start, resize} = drag.current, dx = e.clientX - startX, dy = e.clientY - startY;
    onRect(resize ? {...start, width: start.width + dx, height: start.height + dy} : {...start, x: start.x + dx, y: start.y + dy});
  };
  const begin = (e, resize = false) => {
    if (e.button !== 0 || (!resize && e.target.closest('button'))) return;
    e.preventDefault(); e.currentTarget.focus(); e.currentTarget.setPointerCapture(e.pointerId);
    drag.current = {startX: e.clientX, startY: e.clientY, rect, resize};
    if (!resize) docking.setDrag({id, kind: 'float'});
  };
  const finish = e => {
    if (drag.current && !drag.current.resize) {
      const target = document.elementFromPoint?.(e.clientX, e.clientY)?.closest('[data-dock-target]');
      if (target) docking.place(id, target.dataset.dockTarget);
      docking.setDrag(null);
    }
    drag.current = null;
  };
  const keyMove = (e, resize = false) => {
    if (e.target !== e.currentTarget || !['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) return;
    e.preventDefault(); e.stopPropagation();
    const dx = e.key === 'ArrowLeft' ? -16 : e.key === 'ArrowRight' ? 16 : 0, dy = e.key === 'ArrowUp' ? -16 : e.key === 'ArrowDown' ? 16 : 0;
    onRect(resize ? {...rect, width: rect.width + dx, height: rect.height + dy} : {...rect, x: rect.x + dx, y: rect.y + dy});
  };
  return <section className={'editor-window-panel ' + (rect ? 'is-floating' : 'is-docked') + (focused ? ' is-focused' : '')}
    id={'editor-window-' + id} role={rect ? 'dialog' : 'tabpanel'} aria-label={label} aria-labelledby={rect ? undefined : 'editor-tab-' + id}
    hidden={!rect && !visible} style={rect ? {left: rect.x, top: rect.y, width: rect.width, height: rect.height, zIndex} : undefined}
    onPointerDownCapture={onFocus} onFocusCapture={onFocus}>
    <header className="editor-window-title" hidden={!rect} tabIndex={rect ? 0 : -1} aria-label={tr('Переместить окно «{0}»', [label])}
      onPointerDown={begin} onPointerMove={move} onPointerUp={finish} onLostPointerCapture={() => {drag.current = null; docking.setDrag(null);}} onKeyDown={keyMove}>
      <Icon name="Grip" size={14}/><strong>{label}</strong><div className="flex-space"/>
      <Button icon="PanelBottom" title={tr('Закрепить во вкладках')} aria-label={tr('Закрепить во вкладках')} onClick={onDock}/>
      <Button icon="X" title={tr('Закрыть окно «{0}»', [label])} aria-label={tr('Закрыть окно «{0}»', [label])} onClick={onClose}/>
    </header>
    <div className="editor-window-body">{children}</div>
    {rect && <button className="editor-window-resize" aria-label={tr('Изменить размер окна «{0}»', [label])} title={tr('Перетащите для изменения размера; стрелки — с клавиатуры')}
      onPointerDown={e => begin(e, true)} onPointerMove={move} onPointerUp={() => {drag.current = null;}} onLostPointerCapture={() => {drag.current = null;}} onKeyDown={e => keyMove(e, true)}><Icon name="Grip" size={13}/></button>}
  </section>;
}

export function EditorDockTabs({area = 'workspace', maximized, onMaximize}) {
  useLocale();
  const docking = useDocking(), {windows, onChange, onSelect, onClose} = docking;
  const root = useRef(), tabs = useRef(), pointerDrag = useRef(), suppressClick = useRef(false), [context, setContext] = useState(null);
  const closeContext = useCallback(() => setContext(null), []);
  const group = dockTabs(windows, area), active = activeDockWindow(windows, area);
  const update = fn => onChange(current => fn(current));
  useEffect(() => {if (!docking.drag && pointerDrag.current?.moved) pointerDrag.current.canceled = true;}, [docking.drag]);
  const beginTabDrag = (e, id) => {
    if (e.button !== 0) return;
    e.preventDefault(); e.currentTarget.focus(); e.currentTarget.setPointerCapture(e.pointerId);
    pointerDrag.current = {id, x: e.clientX, y: e.clientY, moved: false};
  };
  const moveTabDrag = e => {
    const start = pointerDrag.current;
    if (!start || start.canceled || (!start.moved && Math.hypot(e.clientX-start.x, e.clientY-start.y) < 6)) return;
    start.moved = true;
    const target = dockDestinationAt(e.clientX, e.clientY);
    docking.setDrag({id: start.id, kind: 'tab', point: {x: e.clientX, y: e.clientY}, area: target?.area});
  };
  const finishTabDrag = e => {
    const start = pointerDrag.current; pointerDrag.current = null;
    if (!start) return;
    if (start.moved) {
      suppressClick.current = true;
      const target = docking.drag && dockDestinationAt(e.clientX, e.clientY);
      if (target && target.beforeId !== start.id) docking.place(start.id, target.area, target.beforeId);
      docking.setDrag(null);
    } else onSelect(start.id);
  };
  const focusTab = id => document.getElementById('editor-tab-' + id)?.focus();
  useEffect(() => {tabs.current?.querySelector('[aria-selected="true"]')?.scrollIntoView?.({block: 'nearest', inline: 'nearest'});}, [active, windows.tabs]);
  const close = id => {
    const next = closeWindow(windows, id); onClose(id);
    queueMicrotask(() => {
      const neighbour = activeDockWindow(next, area);
      if (neighbour) focusTab(neighbour);
      else document.querySelector(area === 'workspace' ? '[data-windows-menu]' : '[data-dock-base="' + area + '"]')?.focus();
    });
  };
  const detach = id => {
    update(s => floatWindow(s, id, viewport()));
    queueMicrotask(() => document.querySelector('#editor-window-' + id + '.is-floating > .editor-window-title')?.focus());
  };
  const actions = id => [
    ...DOCK_AREAS.filter(([target]) => target !== area).map(([target, label]) => ({label: tr('Перенести в область «{0}»', [tr(label)]), icon: 'PanelsTopLeft', action: () => docking.place(id, target)})),
    {label: 'Открепить окно', icon: 'AppWindow', action: () => detach(id)},
    {label: 'Переместить вкладку влево', icon: 'ArrowLeft', disabled: group.indexOf(id) <= 0, action: () => update(s => moveWindowTab(s, id, -1))},
    {label: 'Переместить вкладку вправо', icon: 'ArrowRight', disabled: group.indexOf(id) === group.length - 1, action: () => update(s => moveWindowTab(s, id, 1))},
    {label: 'Закрыть окно', icon: 'X', action: () => close(id)},
  ];
  const openContext = (e, id) => {e.preventDefault(); const box = e.currentTarget.getBoundingClientRect(); setContext({id, x: e.type === 'contextmenu' ? e.clientX : box.left, y: e.type === 'contextmenu' ? e.clientY : box.bottom});};
  if (!group.length) return null;
  return <div className={area === 'workspace' ? 'panel-tabs dock-tabs managed-dock-tabs' : 'embedded-dock-tabs'} ref={root} data-dock-strip={area}
    onDragOver={e => {if (acceptsWindowDrag(e)) {e.preventDefault(); e.dataTransfer.dropEffect = 'move';}}} onDrop={e => {
      const id = e.dataTransfer.getData(TAB_DRAG_TYPE);
      if (id && isWindowOpen(windows, id)) {e.preventDefault(); e.stopPropagation(); docking.place(id, area);}
    }}>
    <div className="workspace-tab-list" role="tablist" aria-label={area === 'workspace' ? tr('Открытые окна') : tr('Вкладки: {0}', [tr(DOCK_AREAS.find(([id]) => id === area)[1])])} ref={tabs}>
      {group.map(id => <div key={id} data-dock-window={id} className={'workspace-tab' + (active === id ? ' active' : '')} onDragOver={e => {if (acceptsWindowDrag(e)) e.preventDefault();}} onDrop={e => {
        const source = e.dataTransfer.getData(TAB_DRAG_TYPE);
        if (source && isWindowOpen(windows, source)) {e.preventDefault(); e.stopPropagation(); if (source !== id) docking.place(source, area, id); docking.setDrag(null);}
      }}>
        <button role="tab" id={'editor-tab-' + id} aria-controls={'editor-window-' + id} aria-selected={active === id} tabIndex={active === id || (!active && group[0] === id) ? 0 : -1}
          draggable={false} onDragStart={e => {e.dataTransfer.setData(TAB_DRAG_TYPE, id); e.dataTransfer.effectAllowed = 'move'; docking.setDrag({id, kind: 'tab'});}} onDragEnd={() => docking.setDrag(null)}
          onPointerDown={e => beginTabDrag(e,id)} onPointerMove={moveTabDrag} onPointerUp={finishTabDrag}
          onPointerCancel={()=>{pointerDrag.current=null;docking.setDrag(null);}} onLostPointerCapture={()=>{pointerDrag.current=null;docking.setDrag(null);}}
          onContextMenu={e => openContext(e, id)} onClick={e => {const ignored=suppressClick.current&&e.detail>0;suppressClick.current=false;if(!ignored)onSelect(id);}} onKeyDown={e => {
            if (e.altKey && ['ArrowLeft', 'ArrowRight'].includes(e.key)) {e.preventDefault(); update(s => moveWindowTab(s, id, e.key === 'ArrowLeft' ? -1 : 1)); return;}
            if (['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(e.key)) {
              e.preventDefault(); const index = group.indexOf(id), next = group[e.key === 'Home' ? 0 : e.key === 'End' ? group.length - 1 : (index + (e.key === 'ArrowLeft' ? -1 : 1) + group.length) % group.length];
              onSelect(next); focusTab(next);
            }
            if (e.key === 'Delete') {e.preventDefault(); close(id);}
            if (e.key === 'F10' && e.shiftKey) openContext(e, id);
          }}>{tr(windowLabel(id))}</button>
        <button className="workspace-tab-close" aria-label={tr('Закрыть окно «{0}»', [tr(windowLabel(id))])} title={tr('Закрыть окно')} onClick={() => close(id)}><Icon name="X" size={12}/></button>
      </div>)}
    </div>
    <div className="workspace-tab-actions">
      <Button icon="AppWindow" disabled={!active} aria-label={tr('Открепить окно')} title={tr('Открепить окно')} onClick={() => detach(active)}/>
      {onMaximize && <Button icon={maximized ? 'Minimize2' : 'Maximize2'} title={maximized ? tr('Восстановить рабочую область') : tr('Развернуть рабочую область')} onClick={onMaximize}/>}
    </div>
    {context && <EditorContextMenu x={context.x} y={context.y} title={tr(windowLabel(context.id))} items={actions(context.id)} onClose={closeContext}/>}
  </div>;
}

function WorkspaceContent({renderPanel, maximized, onMaximize}) {
  useLocale();
  const {windows, onChange, onClose, hosts, place} = useDocking(), visited = useRef(new Set());
  EDITOR_WINDOWS.forEach(([id]) => {if (isWindowOpen(windows, id)) visited.current.add(id);});
  const update = fn => onChange(current => fn(current));
  useEffect(() => {
    const resize = () => onChange(current => {
      let next = current;
      for (const [id, rect] of Object.entries(current.floating)) next = updateWindowRect(next, id, rect, viewport());
      return next;
    });
    resize(); window.addEventListener('resize', resize);
    return () => window.removeEventListener('resize', resize);
  }, [onChange]);
  return <section className="graph-dock managed-workspace" aria-label={tr('Рабочая область')} data-dock-root="workspace">
    <EditorDockTabs maximized={maximized} onMaximize={onMaximize}/>
    <div className="editor-window-stack" ref={element => {hosts.current.workspace = element;}}/>
    <div className="floating-window-layer" ref={element => {hosts.current.floating = element;}}/>
    {EDITOR_WINDOWS.filter(([id]) => visited.current.has(id)).map(([id]) => {
      const area = windowArea(windows, id), rect = windows.floating[id], visible = activeDockWindow(windows, area) === id && isWindowOpen(windows, id);
      return <DockPanelMount key={id} area={area} floating={!!rect} visible={visible}>
        <WindowPanel id={id} rect={rect} visible={visible} focused={windows.focused === id} zIndex={50 + Object.keys(windows.floating).indexOf(id)}
          onFocus={() => update(s => openWindow(s, id))} onRect={rect => update(s => updateWindowRect(s, id, rect, viewport()))} onClose={() => onClose(id)} onDock={() => place(id, area)}>
          {renderPanel(id, visible || !!rect)}
        </WindowPanel>
      </DockPanelMount>;
    })}
    <DockDropTarget area="workspace"/>
  </section>;
}

export default function EditorWorkspace(props) {
  const docking = useDocking();
  return docking ? <WorkspaceContent {...props}/> : <EditorDockProvider {...props}><WorkspaceContent {...props}/></EditorDockProvider>;
}
