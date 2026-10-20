// Workspace preferences are local editor settings, never part of an authored project.
export const EDITOR_WINDOWS = [
  ['story', 'Поток истории', 'Workflow'],
  ['subscenes', 'Сабсцены', 'PanelsTopLeft'],
  ['characters', 'Персонажи', 'Users'],
  ['widgets', 'Виджеты UI', 'PanelsTopLeft'],
  ['gameplay', 'Игровые механики', 'Gamepad2'],
  ['scripting', 'Скриптинг', 'Code2'],
  ['cameras', 'Камеры', 'Video'],
  ['input', 'Ввод', 'Keyboard'],
  ['files', 'Проводник', 'FolderOpen'],
  ['active', 'Звук и окружение', 'CloudSun'],
  ['event', 'Событие', 'Layers'],
  ['create', 'Создание', 'Plus'],
  ['assets', 'Проект', 'Library'],
  ['sound', 'Звук', 'Music2'],
  ['samples', 'Эффекты', 'Sparkles'],
  ['issues', 'Проблемы', 'TriangleAlert'],
];
const validId = id => EDITOR_WINDOWS.some(([key]) => key === id);
export const DOCK_AREAS = [['workspace', 'Рабочая область'], ['scene', 'Локация и игра'], ['hierarchy', 'Иерархия'], ['inspector', 'Инспектор']];
const validArea = area => DOCK_AREAS.some(([id]) => id === area);
export const windowArea = (state, id) => validArea(state.placements?.[id]) ? state.placements[id] : 'workspace';
export const dockTabs = (state, area = 'workspace') => state.tabs.filter(id => windowArea(state, id) === area);
export function activeDockWindow(state, area = 'workspace') {
  const tabs = dockTabs(state, area), selected = state.selected?.[area];
  if (area !== 'workspace' && selected === null) return null;
  return tabs.includes(selected) ? selected : tabs.includes(state.active) ? state.active : area === 'workspace' ? tabs[0] || null : null;
}
export const selectDockBase = (state, area) => ({...state, selected: {...state.selected, [area]: null}});
export const windowId = id => ['timeline', 'staging'].includes(id) ? 'story' : id;
export const windowLabel = id => EDITOR_WINDOWS.find(([key]) => key === id)?.[1] || id;
export const isWindowOpen = (state, id) => state.tabs.includes(id) || Object.hasOwn(state.floating, id);

export function defaultWindows(id = 'story') {
  return {tabs: [id], active: id, focused: id, floating: {}, position: 'bottom', width: 480, placements: {}, selected: {workspace: id, scene: null, hierarchy: null, inspector: null}};
}

const bounded = (value, fallback, min, max) => Number.isFinite(value) ? Math.max(min, Math.min(max, value)) : fallback;
export function clampWindowRect(rect = {}, viewport = {width: 1280, height: 720}) {
  const maxWidth = Math.max(240, viewport.width - 16), maxHeight = Math.max(160, viewport.height - 16);
  const width = bounded(rect.width, Math.min(900, maxWidth), Math.min(360, maxWidth), maxWidth);
  const height = bounded(rect.height, Math.min(560, maxHeight), Math.min(240, maxHeight), maxHeight);
  return {
    x: bounded(rect.x, Math.min(120, maxWidth - width), 0, viewport.width - width),
    y: bounded(rect.y, Math.min(80, maxHeight - height), 0, viewport.height - height),
    width, height,
  };
}

export function restoreWindows(saved, initialId = 'story', viewport) {
  if (!saved || !Array.isArray(saved.tabs) || typeof saved.floating !== 'object' || !saved.floating) return defaultWindows(initialId);
  const floating = Object.fromEntries(Object.entries(saved.floating).filter(([id]) => validId(id)).map(([id, rect]) => [id, clampWindowRect(rect || {}, viewport)]));
  const tabs = [...new Set(saved.tabs.filter(id => validId(id) && !Object.hasOwn(floating, id)))];
  const active = tabs.includes(saved.active) ? saved.active : tabs[0] || null;
  const placements = Object.fromEntries(Object.entries(saved.placements || {}).filter(([id, area]) => validId(id) && validArea(area)));
  const selected = Object.fromEntries(DOCK_AREAS.map(([area]) => {
    const areaTabs = tabs.filter(id => (placements[id] || 'workspace') === area), value = saved.selected?.[area];
    return [area, areaTabs.includes(value) ? value : area !== 'workspace' && value === null ? null : areaTabs.includes(active) ? active : areaTabs[0] || null];
  }));
  const state = {tabs, active, floating, focused: active, position: ['bottom', 'left', 'right'].includes(saved.position) ? saved.position : 'bottom', width: bounded(saved.width, 480, 260, 900), placements, selected};
  if (isWindowOpen(state, saved.focused)) state.focused = saved.focused;
  return state;
}

export function openWindow(state, requestedId) {
  const id = windowId(requestedId);
  if (!validId(id)) return state;
  if (Object.hasOwn(state.floating, id)) {
    if (state.focused === id && Object.keys(state.floating).at(-1) === id) return state;
    const floating = {...state.floating}, rect = floating[id];
    delete floating[id]; floating[id] = rect;
    return {...state, floating, focused: id};
  }
  const area = windowArea(state, id);
  if (state.active === id && state.focused === id && activeDockWindow(state, area) === id) return state;
  return {...state, tabs: state.tabs.includes(id) ? state.tabs : [...state.tabs, id], active: id, focused: id, selected: {...state.selected, [area]: id}};
}

export function closeWindow(state, id) {
  if (!isWindowOpen(state, id)) return state;
  const area = windowArea(state, id), areaTabs = dockTabs(state, area), index = areaTabs.indexOf(id), tabs = state.tabs.filter(key => key !== id), floating = {...state.floating};
  delete floating[id];
  const remaining = areaTabs.filter(key => key !== id), neighbour = remaining[Math.min(index, remaining.length - 1)] || null;
  const active = state.active === id ? neighbour || tabs[0] || null : state.active;
  const selected = activeDockWindow(state, area) === id ? {...state.selected, [area]: neighbour} : state.selected;
  return {...state, tabs, floating, active, selected, focused: state.focused === id ? active || Object.keys(floating).at(-1) || null : state.focused};
}

export function floatWindow(state, id, viewport) {
  if (!validId(id)) return state;
  const opened = openWindow(state, id);
  if (Object.hasOwn(opened.floating, id)) return opened;
  const closed = closeWindow(opened, id);
  return {...closed, focused: id,
    floating: {...opened.floating, [id]: clampWindowRect({x: 100 + Object.keys(opened.floating).length * 28, y: 70 + Object.keys(opened.floating).length * 28}, viewport)}};
}

export function dockWindow(state, id, beforeId, area = beforeId ? windowArea(state, beforeId) : 'workspace') {
  if (!validId(id) || !validArea(area)) return state;
  const opened = openWindow(state, id), floating = {...opened.floating};
  delete floating[id];
  const tabs = opened.tabs.filter(key => key !== id), index = tabs.indexOf(beforeId);
  tabs.splice(index < 0 ? tabs.length : index, 0, id);
  const origin = windowArea(state, id), previous = dockTabs(state, origin).filter(key => key !== id);
  const selected = {...state.selected};
  if (origin !== area && activeDockWindow(state, origin) === id) selected[origin] = previous[0] || null;
  selected[area] = id;
  return {...opened, tabs, floating, active: id, focused: id, placements: {...state.placements, [id]: area}, selected};
}

export function moveWindowTab(state, id, offset) {
  const group = dockTabs(state, windowArea(state, id)), index = group.indexOf(id), target = index + offset;
  if (index < 0 || target < 0 || target >= group.length) return state;
  const tabs = [...state.tabs], first = tabs.indexOf(id), second = tabs.indexOf(group[target]);
  [tabs[first], tabs[second]] = [tabs[second], tabs[first]];
  return {...state, tabs};
}

export function updateWindowRect(state, id, rect, viewport) {
  if (!Object.hasOwn(state.floating, id)) return state;
  return {...state, floating: {...state.floating, [id]: clampWindowRect(rect, viewport)}};
}
