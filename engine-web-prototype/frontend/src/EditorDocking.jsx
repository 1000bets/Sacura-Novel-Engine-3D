import React, {createContext, useContext, useEffect, useLayoutEffect, useRef, useState} from 'react';
import {RegisterWorkspaceDocument} from './workspaceDocuments.js';
import {createPortal} from 'react-dom';
import {activeDockWindow, dockWindow, dockTabs, isWindowOpen, windowArea, floatWindow, SplitDockGroup} from './editorWindows.js';

const DockingContext = createContext(null);
export const useDocking = () => useContext(DockingContext);
export function dockDestinationAt(Horizontal, Vertical, OwnerDocument = document) {
  const Element = OwnerDocument.elementFromPoint?.(Horizontal, Vertical);
  const Target = Element?.closest('[data-dock-target], [data-dock-strip], [data-dock-root]');
  if (!Target) {
    return null;
  }
  return {area:Target.dataset.dockTarget || Target.dataset.dockStrip || Target.dataset.dockRoot, beforeId:Element.closest('[data-dock-window]')?.dataset.dockWindow};
}
export function EditorDockProvider({windows:Windows, onChange:Change, onSelect:Select, onClose:Close, onPlace:Placed, children:Children}) {
  useEffect(() => RegisterWorkspaceDocument(window), []);
  const Hosts = useRef({});
  const Popups = useRef(new Map());
  const [Drag, SetDrag] = useState(null);
  const [Revision, SetRevision] = useState(0);
  const Latest = useRef(Windows);
  const CancelDrag = useRef(null);
  useEffect(() => () => CancelDrag.current?.(), []);
  Latest.current = Windows;
  function Place(Identifier, Area, BeforeIdentifier) {
    Change(Current => dockWindow(Current, Identifier, BeforeIdentifier, Area));
    Placed?.(Area);
    SetDrag(null);
  }
  function Detach(Identifier, Position = {}) {
    Change(Current => floatWindow(Current, Identifier, {width:1200,height:820,...Position}));
    SetDrag(null);
  }
  function Split(Identifier, Area, Direction) {
    Change(Current => SplitDockGroup(Current, Identifier, Area, Direction));
    SetDrag(null);
  }
  function RegisterHost(Area, Element) {
    if (Hosts.current[Area] !== Element) {
      Hosts.current[Area] = Element;
      SetRevision(Value => Value + 1);
    }
  }
  function StartDrag(Event, Identifier) {
    if (Event.button !== 0) {
      return;
    }
    CancelDrag.current?.();
    const Button = Event.currentTarget;
    const Owner = Button.ownerDocument.defaultView;
    const PointerIdentifier = Event.pointerId;
    const Start = {x:Event.screenX,y:Event.screenY};
    const Origin = {x:Event.screenX - Event.clientX,y:Event.screenY - Event.clientY};
    let Surfaces = null;
    let Started = false;
    let Finished = false;
    window.sacuraDesktop?.workspaceDragSurfaces?.().then(Value => {Surfaces = Value;});
    function Destination(Pointer) {
      const Owners = new Set([Owner, ...Popups.current.values(), window]);
      for (const Candidate of Owners) {
        if (Candidate.closed) {
          continue;
        }
        const Surface = Surfaces?.find(Entry => Entry.name === Candidate.name);
        if (Surface && !Surface.visible) {
          continue;
        }
        const Border = (Candidate.outerWidth - Candidate.innerWidth) / 2;
        let Horizontal = Candidate.screenX + Border;
        let Vertical = Candidate.screenY + Candidate.outerHeight - Candidate.innerHeight - Border;
        if (Candidate === Owner) {
          Horizontal = Origin.x;
          Vertical = Origin.y;
        }
        if (Surface) {
          Horizontal = Surface.bounds.x;
          Vertical = Surface.bounds.y;
        }
        const ClientHorizontal = Pointer.screenX - Horizontal;
        const ClientVertical = Pointer.screenY - Vertical;
        const Target = dockDestinationAt(ClientHorizontal, ClientVertical, Candidate.document);
        if (Target) {
          const Group = Hosts.current[Target.area]?.closest('[data-dock-root]');
          const Bounds = Group?.getBoundingClientRect();
          Target.direction = 'center';
          if (Bounds && !Candidate.document.elementFromPoint(ClientHorizontal, ClientVertical)?.closest('[data-dock-strip]')) {
            if (ClientHorizontal > Bounds.right - Math.min(80, Bounds.width * .2)) {
              Target.direction = 'right';
            } else if (ClientVertical > Bounds.bottom - Math.min(65, Bounds.height * .2)) {
              Target.direction = 'bottom';
            }
          }
          return Target;
        }
      }
      return null;
    }
    function Move(Pointer) {
      if (Pointer.pointerId !== PointerIdentifier) {
        return;
      }
      if (!Started && Math.hypot(Pointer.screenX - Start.x, Pointer.screenY - Start.y) < 5) {
        return;
      }
      Started = true;
      Pointer.preventDefault();
      SetDrag({id:Identifier,target:Destination(Pointer)});
    }
    function Finish(Pointer) {
      if (Pointer.pointerId !== PointerIdentifier) {
        return;
      }
      const Target = Destination(Pointer);
      Cleanup();
      if (!Started) {
        return;
      }
      const SuppressClick = Click => {Click.preventDefault();Click.stopPropagation();};
      Owner.document.addEventListener('click', SuppressClick, {capture:true,once:true});
      Owner.setTimeout(() => Owner.document.removeEventListener('click', SuppressClick, true), 0);
      if (Target) {
        if (Target.direction !== 'center' && dockTabs(Latest.current, Target.area).some(Entry => Entry !== Identifier)) {
          Split(Identifier, Target.area, Target.direction);
        } else {
          Place(Identifier, Target.area, Target.beforeId);
        }
      } else {
        Detach(Identifier, {x:Pointer.screenX - 100,y:Pointer.screenY - 20});
      }
    }
    function KeyDown(Key) {
      if (Key.key === 'Escape') {
        Key.preventDefault();
        Key.stopPropagation();
        Cleanup();
      }
    }
    function Cleanup() {
      if (Finished) {
        return;
      }
      Finished = true;
      Button.removeEventListener('pointermove', Move);
      Button.removeEventListener('pointerup', Finish);
      Button.removeEventListener('pointercancel', Cleanup);
      Button.removeEventListener('lostpointercapture', Cleanup);
      Owner.document.removeEventListener('keydown', KeyDown, true);
      if (Button.hasPointerCapture(PointerIdentifier)) {
        Button.releasePointerCapture(PointerIdentifier);
      }
      Button.style.cursor = '';
      SetDrag(null);
      CancelDrag.current = null;
    }
    Button.setPointerCapture(PointerIdentifier);
    Button.style.cursor = 'grabbing';
    Button.addEventListener('pointermove', Move);
    Button.addEventListener('pointerup', Finish);
    Button.addEventListener('pointercancel', Cleanup);
    Button.addEventListener('lostpointercapture', Cleanup);
    Owner.document.addEventListener('keydown', KeyDown, true);
    CancelDrag.current = Cleanup;
  }
  useEffect(() => {
    return window.sacuraDesktop?.onWorkspaceClose?.(async Area => {
      const DescendsFrom = Candidate => {
        if (Candidate === Area) {
          return true;
        }
        const Parent = Latest.current.containers[Candidate]?.parent;
        return Boolean(Parent && DescendsFrom(Parent));
      };
      for (const Identifier of Latest.current.tabs.filter(Entry => DescendsFrom(windowArea(Latest.current, Entry)))) {
        const Result = await Close(Identifier);
        if (Result === false) {
          return;
        }
      }
    });
  }, [Close]);
  return <DockingContext.Provider value={{windows:Windows,onChange:Change,onSelect:Select,onClose:Close,hosts:Hosts,popups:Popups,revision:Revision,registerHost:RegisterHost,drag:Drag,setDrag:SetDrag,startDrag:StartDrag,place:Place,detach:Detach,split:Split}}>{Children}</DockingContext.Provider>;
}
export function EditorDockBody({area:Area}) {
  const Docking = useDocking();
  const Element = useRef(null);
  useLayoutEffect(() => {
    Docking.registerHost(Area, Element.current);
    return () => Docking.registerHost(Area, null);
  }, [Area]);
  return <div className="dock-region-content" ref={Element}/>;
}
export function EditorDockStrip({area:Area, className:ClassName, children:Children}) {
  return <div className={ClassName} data-dock-strip={Area}>{Children}</div>;
}
export function DockDropTarget() {
  return null;
}
export function DockPanelMount({id:Identifier, area:Area, visible:Visible, children:Children}) {
  const Docking = useDocking();
  const [Container] = useState(() => {
    const Element = document.createElement('div');
    Element.className = 'editor-panel-mount';
    return Element;
  });
  if (Identifier) {
    Area = windowArea(Docking.windows, Identifier);
    Visible = isWindowOpen(Docking.windows, Identifier) && activeDockWindow(Docking.windows, Area) === Identifier;
  }
  useLayoutEffect(() => {
    const Target = Docking.hosts.current[Area];
    if (Target && Container.parentNode !== Target) {
      Target.appendChild(Container);
    }
    Container.hidden = !Visible;
  }, [Area, Visible, Container, Docking.revision]);
  useLayoutEffect(() => () => Container.remove(), [Container]);
  return createPortal(Children, Container);
}
