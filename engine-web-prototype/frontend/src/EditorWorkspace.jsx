import {t as tr, useLocale} from './i18n.jsx';
import React, {useEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import {RegisterWorkspaceDocument} from './workspaceDocuments.js';
import {Icon, Button} from './StudioParts.jsx';
import {EDITOR_WINDOWS, DOCK_AREAS, BaseTabs, windowLabel as OriginalWindowLabel, windowArea, dockTabs, activeDockWindow, isWindowOpen, openWindow, moveWindowTab, updateWindowRect} from './editorWindows.js';
import {EditorDockBody, EditorDockStrip, DockPanelMount, useDocking} from './EditorDocking.jsx';

function LocalizedWindowLabel(Identifier) {
  const Caption = OriginalWindowLabel(Identifier);
  if (EDITOR_WINDOWS.some(([Builtin]) => Builtin === Identifier)) {
    return tr(Caption);
  }
  return Caption;
}

export function EditorWindowsMenu({windows:Windows, onSelect:Select, onClose:Close, onReset:Reset}) {
  useLocale();
  const [Query, SetQuery] = useState('');
  return <div className="editor-windows-menu" role="menu" aria-label={tr("Вкладки редактора")}>
    <label className="window-menu-search"><Icon name="Search" size={14}/><input autoFocus type="search" aria-label={tr("Найти вкладку")} placeholder={tr("Найти вкладку…")} value={Query} onChange={Event => SetQuery(Event.target.value)}/></label>
    {[...EDITOR_WINDOWS.map(([Identifier, Caption, Icon]) => [Identifier, tr(Caption), Icon]), ...Object.entries(Windows.documents || {}).map(([Identifier, Definition]) => [Identifier, Definition.label, 'File'])].filter(([,Label]) => Label.toLocaleLowerCase().includes(Query.toLocaleLowerCase())).map(([Identifier, Label, IconName]) => <div className="window-menu-row" key={Identifier}>
      <button role="menuitemcheckbox" aria-checked={isWindowOpen(Windows, Identifier)} onClick={() => Select(Identifier)}><Icon name={IconName} size={15}/><span>{Label}</span></button>
      {isWindowOpen(Windows, Identifier) && <button className="window-reveal" aria-label={'Закрыть вкладку «' + Label + '»'} onClick={() => Close(Identifier)}><Icon name="X" size={14}/></button>}
    </div>)}
    <button role="menuitem" onClick={Reset}><Icon name="RotateCcw" size={15}/>{tr("Восстановить раскладку")}</button>
  </div>;
}
export function EditorDockTabs({area:Area = 'workspace', maximized:Maximized, onMaximize:Maximize}) {
  useLocale();
  const Docking = useDocking();
  const {windows:Windows} = Docking;
  const Group = dockTabs(Windows, Area);
  const Active = activeDockWindow(Windows, Area);
  const Destinations = [...DOCK_AREAS, ...Object.entries(Windows.containers || {}).filter(([Identifier]) => GroupHasTabs(Windows, Identifier)).map(([Identifier, Container]) => [Identifier, Container.label])];
  function Select(Identifier) {
    Docking.onSelect(Identifier);
    Docking.popups.current.get(Area)?.focus();
  }
  return <EditorDockStrip area={Area} className="panel-tabs dock-tabs managed-dock-tabs">
    <div className="workspace-tab-list" role="tablist" aria-label={tr("Вкладки")}>
      {Group.map(Identifier => <div key={Identifier} className={'workspace-tab' + (Active === Identifier ? ' active' : '')} data-dock-window={Identifier}>
        <button role="tab" id={'editor-tab-' + Identifier} aria-selected={Active === Identifier} tabIndex={Active === Identifier ? 0 : -1} draggable={false}
          onPointerDown={Event => Docking.startDrag(Event, Identifier)} onClick={() => Select(Identifier)} onDoubleClick={() => Docking.detach(Identifier)}
          onKeyDown={Event => {
            if (Event.key === 'Delete') {
              Event.preventDefault();
              Docking.onClose(Identifier);
            }
            if (['ArrowLeft','ArrowRight'].includes(Event.key)) {
              Event.preventDefault();
              let Offset = 1;
              if (Event.key === 'ArrowLeft') {
                Offset = -1;
              }
              if (Event.altKey) {
                Docking.onChange(Current => moveWindowTab(Current, Identifier, Offset));
              } else {
                const Next = Group[(Group.indexOf(Identifier) + Offset + Group.length) % Group.length];
                Select(Next);
                Event.currentTarget.ownerDocument.getElementById('editor-tab-' + Next)?.focus();
              }
            }
          }}>{LocalizedWindowLabel(Identifier)}</button>
        <button className="workspace-tab-close" aria-label={'Закрыть вкладку «' + LocalizedWindowLabel(Identifier) + '»'} onClick={() => Docking.onClose(Identifier)}><Icon name="X" size={12}/></button>
      </div>)}
    </div>
    <div className="workspace-tab-actions">
      <details className="workspace-tab-menu"><summary aria-label={tr("Переместить вкладку")} title={tr("Переместить вкладку")} onClick={Event => {
        const Bounds = Event.currentTarget.getBoundingClientRect();
        const Owner = Event.currentTarget.ownerDocument.defaultView;
        const Menu = Event.currentTarget.nextElementSibling;
        Menu.style.left = Math.max(4, Math.min(Bounds.right - 230, Owner.innerWidth - 234)) + 'px';
        Menu.style.top = Math.min(Bounds.bottom, Math.max(4, Owner.innerHeight - 310)) + 'px';
        Menu.style.maxHeight = Math.min(300, Owner.innerHeight - 12) + 'px';
      }}><Icon name="PanelsTopLeft" size={15}/></summary><div role="menu">
        {Destinations.filter(([Identifier]) => Identifier !== Area).map(([Identifier, Label]) => <button key={Identifier} disabled={!Active} role="menuitem" onClick={Event => {
          Event.currentTarget.closest('details').open = false;
          Docking.place(Active, Identifier);
        }}>{tr("В")}{Label}</button>)}
        <button disabled={!Active || Group.length < 2} onClick={() => Docking.split(Active, Area, 'right')}>{tr("Новая группа справа")}</button>
        <button disabled={!Active || Group.length < 2} onClick={() => Docking.split(Active, Area, 'bottom')}>{tr("Новая группа снизу")}</button>
      </div></details>
      <Button icon="AppWindow" disabled={!Active} aria-label={tr("Открепить окно")} title={tr("Открепить вкладку в отдельное окно")} onClick={() => Docking.detach(Active)}/>
      {Maximize && <Button icon={Maximized ? 'Minimize2' : 'Maximize2'} title={tr("Развернуть рабочую область")} onClick={Maximize}/>}
    </div>
  </EditorDockStrip>;
}
function GroupHasTabs(Windows, Area, Seen = new Set()) {
  if (Seen.has(Area)) {
    return false;
  }
  Seen.add(Area);
  return dockTabs(Windows, Area).length > 0 || Object.entries(Windows.containers || {}).some(([Identifier, Container]) => Container.parent === Area && GroupHasTabs(Windows, Identifier, Seen));
}
export function EditorDockArea({area:Area, onMaximize:Maximize, maximized:Maximized}) {
  useLocale();
  const Docking = useDocking();
  const Children = Object.entries(Docking.windows.containers || {}).filter(([Identifier, Container]) => Container.parent === Area && GroupHasTabs(Docking.windows, Identifier));
  let Over = null;
  if (Docking.drag?.target?.area === Area) {
    Over = Docking.drag.target.direction;
  }
  let Content = <section className="universal-dock-group" data-dock-root={Area}>
    <EditorDockTabs area={Area} onMaximize={Maximize} maximized={Maximized}/>
    <EditorDockBody area={Area}/>
    {dockTabs(Docking.windows, Area).length === 0 && <div className="workspace-empty">{tr("Перетащите вкладку сюда")}</div>}
    {Over && Docking.drag && <div className={'workspace-drop-preview ' + Over}>{tr("Разместить вкладку")}</div>}
  </section>;
  for (const [Identifier, Container] of Children) {
    Content = <SplitGroup key={Identifier} identifier={Identifier} direction={Container.direction} first={Content} second={<EditorDockArea area={Identifier}/>}/>;
  }
  return Content;
}
function SplitGroup({identifier:Identifier, direction:Direction, first:First, second:Second}) {
  useLocale();
  const Docking = useDocking();
  const Ratio = Docking.windows.containers[Identifier]?.ratio || 50;
  const SetRatio = Value => Docking.onChange(Current => {
    let Next = Value;
    if (typeof Value === 'function') {
      Next = Value(Current.containers[Identifier]?.ratio || 50);
    }
    return updateWindowRect(Current, Identifier, {ratio:Next});
  });
  function Resize(Event) {
    const Bounds = Event.currentTarget.parentElement.getBoundingClientRect();
    let Value = (Event.clientX - Bounds.left) / Bounds.width;
    if (Direction === 'bottom') {
      Value = (Event.clientY - Bounds.top) / Bounds.height;
    }
    SetRatio(Math.max(15, Math.min(85, Value * 100)));
  }
  return <div className={'workspace-split ' + Direction} style={{'--split-ratio':Ratio + '%'}}><div>{First}</div><div role="separator" tabIndex={0} aria-label={tr("Размер групп вкладок")} className="workspace-split-divider" onPointerDown={Event => {
    Event.currentTarget.setPointerCapture(Event.pointerId);
    Resize(Event);
  }} onPointerMove={Event => {
    if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
      Resize(Event);
    }
  }} onPointerUp={Event => Event.currentTarget.releasePointerCapture(Event.pointerId)} onKeyDown={Event => {
    if (['ArrowLeft','ArrowUp'].includes(Event.key)) {
      Event.preventDefault();
      SetRatio(Value => Math.max(15, Value - 5));
    }
    if (['ArrowRight','ArrowDown'].includes(Event.key)) {
      Event.preventDefault();
      SetRatio(Value => Math.min(85, Value + 5));
    }
  }}/><div>{Second}</div></div>;
}
function DetachedWorkspace({area:Area, descriptor:Descriptor}) {
  useLocale();
  const Docking = useDocking();
  const [Surface, SetSurface] = useState(null);
  useEffect(() => {
    if (Surface) {
      Surface.ownerDocument.title = (LocalizedWindowLabel(activeDockWindow(Docking.windows, Area)) || Descriptor.label) + ' — Sacura';
    }
  }, [Surface, Docking.windows.selected[Area]]);
  useEffect(() => {
    const Popup = window.open('about:blank', 'sacura-' + Area, `popup,width=${Descriptor.width},height=${Descriptor.height},left=${Descriptor.x},top=${Descriptor.y}`);
    if (!Popup) {
      for (const Identifier of dockTabs(Docking.windows, Area)) {
        Docking.place(Identifier, 'workspace');
      }
      return;
    }
    Docking.popups.current.set(Area, Popup);
    const Unregister = RegisterWorkspaceDocument(Popup);
    const OwnerDocument = Popup.document;
    OwnerDocument.title = Descriptor.label + ' — Sacura';
    for (const Style of document.querySelectorAll('link[rel="stylesheet"], style')) {
      const Clone = Style.cloneNode(true);
      if (Style.href) {
        Clone.href = Style.href;
      }
      OwnerDocument.head.appendChild(Clone);
    }
    OwnerDocument.body.className = 'detached-workspace-body';
    const Root = OwnerDocument.createElement('main');
    Root.className = 'editor-app detached-workspace';
    OwnerDocument.body.appendChild(Root);
    const SynchronizeTheme = () => {
      for (const Attribute of document.documentElement.attributes) {
        OwnerDocument.documentElement.setAttribute(Attribute.name, Attribute.value);
      }
      OwnerDocument.body.style.cssText = document.body.style.cssText;
    };
    SynchronizeTheme();
    const Observer = new MutationObserver(SynchronizeTheme);
    Observer.observe(document.documentElement, {attributes:true});
    window.sacuraDesktop?.registerWorkspaceWindow?.('sacura-' + Area);
    SetSurface(Root);
    const SaveBounds = () => Docking.onChange(Current => updateWindowRect(Current, Area, {x:Popup.screenX,y:Popup.screenY,width:Popup.innerWidth,height:Popup.innerHeight}));
    const Timer = setInterval(SaveBounds, 1500);
    Popup.addEventListener('resize', SaveBounds);
    return () => {
      clearInterval(Timer);
      Popup.removeEventListener('resize', SaveBounds);
      Unregister();
      Observer.disconnect();
      for (const Mount of Root.querySelectorAll('.editor-panel-mount')) {
        Mount.hidden = true;
        document.body.appendChild(Mount);
      }
      Docking.popups.current.delete(Area);
      if (window.sacuraDesktop?.closeWorkspaceWindow) {
        window.sacuraDesktop.closeWorkspaceWindow('sacura-' + Area);
      } else {
        Popup.close();
      }
    };
  }, [Area]);
  if (!Surface) {
    return null;
  }
  return createPortal(<EditorDockArea area={Area}/>, Surface);
}
export default function EditorWorkspace({renderPanel:RenderPanel, maximized:Maximized, onMaximize:Maximize}) {
  useLocale();
  const Docking = useDocking();
  const {windows:Windows} = Docking;
  const Visited = useRef(new Set());
  for (const Identifier of Windows.tabs) {
    if (!Object.values(BaseTabs).includes(Identifier)) {
      Visited.current.add(Identifier);
    }
  }
  return <>
    <section className="graph-dock managed-workspace"><EditorDockArea area="workspace" onMaximize={Maximize} maximized={Maximized}/></section>
    {[...Visited.current].filter(Identifier => !Identifier.startsWith('asset:') || Windows.tabs.includes(Identifier)).map(Identifier => <DockPanelMount key={Identifier} id={Identifier}>
      <section className="editor-window-panel is-docked" id={'editor-window-' + Identifier} role="tabpanel" aria-label={LocalizedWindowLabel(Identifier)} onFocusCapture={() => Docking.onChange(Current => openWindow(Current, Identifier))}>
        <div className="editor-window-body">{RenderPanel(Identifier, isWindowOpen(Windows, Identifier) && activeDockWindow(Windows, windowArea(Windows, Identifier)) === Identifier)}</div>
      </section>
    </DockPanelMount>)}
    {Object.entries(Windows.containers || {}).filter(([Identifier, Container]) => Container.kind === 'window' && GroupHasTabs(Windows, Identifier)).map(([Identifier, Container]) => <DetachedWorkspace key={Identifier} area={Identifier} descriptor={Container}/>)}
  </>;
}
