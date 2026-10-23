import {EDITOR_WINDOWS} from './editorWindows.js';

export const DOCK_AREAS = [['workspace', 'Рабочая область'], ['scene', 'Локация и игра'], ['hierarchy', 'Иерархия'], ['inspector', 'Инспектор']];
export const BaseTabs = {scene:'scene-view', hierarchy:'hierarchy-view', inspector:'inspector-view'};
const Definitions = new Map();
export function RegisterEditorTab(Identifier, Definition) {
  Definitions.set(Identifier, Definition);
}
function HasDefinition(Identifier) {
  return Definitions.has(Identifier) || EDITOR_WINDOWS.some(([Key]) => Key === Identifier);
}
export const windowLabel = Identifier => Definitions.get(Identifier)?.label || EDITOR_WINDOWS.find(([Key]) => Key === Identifier)?.[1] || Identifier;
export const windowId = Identifier => ['timeline', 'staging'].includes(Identifier) ? 'story' : Identifier;
export const windowArea = (State, Identifier) => State.placements?.[Identifier] || 'workspace';
export const dockTabs = (State, Area = 'workspace') => State.tabs.filter(Identifier => windowArea(State, Identifier) === Area);
export const isWindowOpen = (State, Identifier) => State.tabs.includes(Identifier);
export function activeDockWindow(State, Area = 'workspace') {
  const Tabs = dockTabs(State, Area);
  if (Tabs.includes(State.selected?.[Area])) {
    return State.selected[Area];
  }
  return Tabs[0] || null;
}
export function defaultWindows(Identifier = 'story') {
  return {version:2, tabs:[...Object.values(BaseTabs), Identifier], active:Identifier, focused:Identifier, floating:{}, position:'bottom', width:480,
    placements:{'scene-view':'scene', 'hierarchy-view':'hierarchy', 'inspector-view':'inspector'},
    selected:{workspace:Identifier, ...BaseTabs}, containers:{}, documents:{}};
}
export function clampWindowRect(Rect = {}, Viewport = {width:1280,height:720}) {
  const Width = Math.max(360, Math.min(Number(Rect.width) || 1000, Viewport.width));
  const Height = Math.max(240, Math.min(Number(Rect.height) || 720, Viewport.height));
  return {x:Number(Rect.x) || 100, y:Number(Rect.y) || 80, width:Width, height:Height};
}
export function restoreWindows(Saved, Initial = 'story') {
  if (!Saved || !Array.isArray(Saved.tabs)) {
    return defaultWindows(Initial);
  }
  const State = {...defaultWindows(Initial), ...Saved, version:2, floating:{}, containers:{}, documents:{}};
  for (const [Identifier, Definition] of Object.entries(Saved.documents || {})) {
    if (Identifier.startsWith('asset:') && typeof Definition?.identity?.assetId === 'string' && typeof Definition.label === 'string') {
      State.documents[Identifier] = Definition;
    }
  }
  for (const [Identifier, Container] of Object.entries(Saved.containers || {})) {
    if (/^window-[a-f0-9-]+$/.test(Identifier) && Container?.kind === 'window') {
      State.containers[Identifier] = {...Container,...clampWindowRect(Container)};
    }
    if (/^group-[a-f0-9-]+$/.test(Identifier) && Container?.kind === 'group') {
      State.containers[Identifier] = {...Container};
    }
  }
  for (const Identifier of Object.keys(State.containers)) {
    const Seen = new Set();
    let Ancestor = Identifier;
    while (State.containers[Ancestor]?.kind === 'group' && !Seen.has(Ancestor)) {
      Seen.add(Ancestor);
      Ancestor = State.containers[Ancestor].parent;
    }
    if (Seen.has(Ancestor) || !State.containers[Ancestor] && !DOCK_AREAS.some(([Key]) => Key === Ancestor)) {
      delete State.containers[Identifier];
    }
  }
  if (!['bottom','left','right'].includes(State.position)) {
    State.position = 'bottom';
  }
  if (!Number.isFinite(State.width)) {
    State.width = 480;
  }
  for (const [Identifier, Definition] of Object.entries(State.documents)) {
    RegisterEditorTab(Identifier, Definition);
  }
  State.tabs = [...new Set(Saved.tabs.filter(HasDefinition))];
  State.placements = {...Saved.placements};
  State.selected = {...Saved.selected};
  if (Saved.version !== 2) {
    for (const [Area, Identifier] of Object.entries(BaseTabs)) {
      State.tabs.push(Identifier);
      State.placements[Identifier] = Area;
      if (!State.selected[Area]) {
        State.selected[Area] = Identifier;
      }
    }
    for (const Identifier of Object.keys(Saved.floating || {})) {
      if (HasDefinition(Identifier)) {
        State.tabs.push(Identifier);
        State.placements[Identifier] = 'workspace';
      }
    }
  }
  for (const Identifier of State.tabs) {
    const Area = windowArea(State, Identifier);
    if (!DOCK_AREAS.some(([Key]) => Key === Area) && !State.containers[Area]) {
      State.placements[Identifier] = 'workspace';
    }
  }
  return State;
}
export function openWindow(State, Requested) {
  const Identifier = windowId(Requested);
  if (!HasDefinition(Identifier)) {
    return State;
  }
  const Area = windowArea(State, Identifier);
  if (State.tabs.includes(Identifier) && State.focused === Identifier && State.selected[Area] === Identifier) {
    return State;
  }
  return {...State, tabs:[...new Set([...State.tabs, Identifier])], active:Identifier, focused:Identifier, selected:{...State.selected, [Area]:Identifier}};
}
export function closeWindow(State, Identifier) {
  if (!isWindowOpen(State, Identifier)) {
    return State;
  }
  const Area = windowArea(State, Identifier);
  const Tabs = State.tabs.filter(Entry => Entry !== Identifier);
  const Remaining = Tabs.filter(Entry => windowArea(State, Entry) === Area);
  const Selected = {...State.selected};
  if (Selected[Area] === Identifier) {
    Selected[Area] = Remaining[0] || null;
  }
  let Focused = State.focused;
  let Active = State.active;
  if (Focused === Identifier) {
    Focused = Remaining[0] || Tabs[0] || null;
  }
  if (Active === Identifier) {
    Active = Remaining[0] || Tabs[0] || null;
  }
  return {...State, tabs:Tabs, selected:Selected, focused:Focused, active:Active};
}
export function dockWindow(State, Identifier, BeforeIdentifier, Area = 'workspace') {
  if (!HasDefinition(Identifier) || !State.containers?.[Area] && !DOCK_AREAS.some(([Key]) => Key === Area)) {
    return State;
  }
  const Closed = closeWindow(State, Identifier);
  const Tabs = [...Closed.tabs];
  let Index = Tabs.indexOf(BeforeIdentifier);
  if (Index < 0) {
    Index = Tabs.length;
  }
  Tabs.splice(Index, 0, Identifier);
  return {...Closed, tabs:Tabs, active:Identifier, focused:Identifier, placements:{...State.placements, [Identifier]:Area}, selected:{...Closed.selected,[Area]:Identifier}};
}
export function floatWindow(State, Identifier, Viewport) {
  if (!HasDefinition(Identifier)) {
    return State;
  }
  const Area = 'window-' + crypto.randomUUID();
  return dockWindow({...State, containers:{...State.containers,[Area]:{kind:'window',label:windowLabel(Identifier),...clampWindowRect({x:Viewport?.x,y:Viewport?.y}, Viewport)}}}, Identifier, null, Area);
}
export function SplitDockGroup(State, Identifier, Area, Direction = 'right') {
  const Group = 'group-' + crypto.randomUUID();
  return dockWindow({...State, containers:{...State.containers,[Group]:{kind:'group',parent:Area,direction:Direction,label:'Группа вкладок'}}}, Identifier, null, Group);
}
export function moveWindowTab(State, Identifier, Offset) {
  const Group = dockTabs(State, windowArea(State, Identifier));
  const Position = Group.indexOf(Identifier);
  const Target = Group[Position + Offset];
  if (!Target) {
    return State;
  }
  const Tabs = [...State.tabs];
  const First = Tabs.indexOf(Identifier);
  const Second = Tabs.indexOf(Target);
  [Tabs[First],Tabs[Second]] = [Tabs[Second],Tabs[First]];
  return {...State,tabs:Tabs};
}
export function selectDockBase(State, Area) {
  return openWindow(State, BaseTabs[Area]);
}
export function updateWindowRect(State, Identifier, Rect) {
  return {...State,containers:{...State.containers,[Identifier]:{...State.containers[Identifier],...Rect}}};
}
