// Palette sources and adaptations are documented in design-system/sacura-novel-studio/THEMES.md.
export const THEME_STORAGE_KEY = 'sacura-appearance-v1';
export const DEFAULT_THEME = 'classic';
const palette = (values) => ({
  '--shadow-popup': '0 8px 24px #0006',
  '--shadow-soft': '0 3px 10px #0003',
  '--overlay-scrim': '#080a10b8',
  '--axis-x': '#e4a3b0', '--axis-y': '#a4c5a3', '--axis-z': '#96b4d9',
  ...values,
});
export const THEMES = [
  {id:'classic', name:'Classic Dark', description:'Нейтральный графит · текущее оформление', tokens:palette({
    '--bg-app':'#181a1d','--bg-panel':'#222427','--bg-panel-elevated':'#2b2e32','--bg-hover':'#34383d','--bg-active':'#38434e','--bg-input':'#2b2e32',
    '--text-primary':'#e8eaed','--text-secondary':'#c1c5cb','--text-muted':'#aab1ba','--text-disabled':'#777e87',
    '--accent':'#a5bdd3','--accent-hover':'#c6d9eb','--accent-muted':'#38434e','--border-subtle':'#33363b','--border-strong':'#727b86',
    '--success':'#a0c8b0','--warning':'#d6be8d','--error':'#eea2a2','--info':'#a5bdd3',
    '--graph-grid':'#303337','--graph-node':'#292c30','--graph-node-selected':'#a5bdd3','--graph-connection':'#858c95',
    '--brand-mark':'#c6aa94','--tab-indicator':'#c1c5cb','--game-dialogue':'#171d26',
  })},
  {id:'sakura', name:'Sakura Night', description:'Ночной графит · мягкий цвет сакуры', tokens:palette({
    '--bg-app':'#11121b','--bg-panel':'#1a1b26','--bg-panel-elevated':'#252633','--bg-hover':'#30313f','--bg-active':'#443342','--bg-input':'#22232f',
    '--text-primary':'#ece8ee','--text-secondary':'#c9c3d1','--text-muted':'#b1aabb','--text-disabled':'#797384',
    '--accent':'#d9a3ba','--accent-hover':'#edbed1','--accent-muted':'#443342','--border-subtle':'#30303e','--border-strong':'#7f778d',
    '--success':'#a1c6b4','--warning':'#d6bc92','--error':'#efa7ad','--info':'#acbbdc',
    '--graph-grid':'#282937','--graph-node':'#262634','--graph-node-selected':'#d9a3ba','--graph-connection':'#8c879e',
    '--brand-mark':'#d9a3ba','--tab-indicator':'#d9a3ba','--game-dialogue':'#1a1b26',
  })},
  {id:'slate', name:'Slate Studio', description:'Холодный сланец · ясный голубой акцент', tokens:palette({
    '--bg-app':'#131b24','--bg-panel':'#1c2733','--bg-panel-elevated':'#273443','--bg-hover':'#314152','--bg-active':'#354b60','--bg-input':'#23313e',
    '--text-primary':'#e6edf3','--text-secondary':'#c0cdd9','--text-muted':'#afbfce','--text-disabled':'#7a8a9b',
    '--accent':'#9dbfe0','--accent-hover':'#bdd7ee','--accent-muted':'#354b60','--border-subtle':'#344352','--border-strong':'#71879b',
    '--success':'#a2cbbd','--warning':'#d9c19a','--error':'#e8a9af','--info':'#9dbfe0',
    '--graph-grid':'#2c3947','--graph-node':'#273442','--graph-node-selected':'#9dbfe0','--graph-connection':'#8c9daf',
    '--brand-mark':'#9dbfe0','--tab-indicator':'#9dbfe0','--game-dialogue':'#1c2733',
  })},
  {id:'copper', name:'Copper Cinema', description:'Тёплый уголь · приглушённая медь', tokens:palette({
    '--bg-app':'#1b1918','--bg-panel':'#252220','--bg-panel-elevated':'#302c28','--bg-hover':'#3b3530','--bg-active':'#4a3b32','--bg-input':'#302b27',
    '--text-primary':'#eee9e2','--text-secondary':'#cec5bb','--text-muted':'#b9aea2','--text-disabled':'#84796e',
    '--accent':'#d0a589','--accent-hover':'#e7bfa2','--accent-muted':'#4a3b32','--border-subtle':'#3b342f','--border-strong':'#8b7b6d',
    '--success':'#b0c4a1','--warning':'#d9bf87','--error':'#e6a49c','--info':'#a9c0d2',
    '--graph-grid':'#35302b','--graph-node':'#302b27','--graph-node-selected':'#d0a589','--graph-connection':'#9c8d7d',
    '--brand-mark':'#d0a589','--tab-indicator':'#d0a589','--game-dialogue':'#252220',
  })},
];
export function resolveTheme(id) { return THEMES.find(t => t.id === id) || THEMES[0]; }
export function readTheme(storage) {
  try { return resolveTheme(storage.getItem(THEME_STORAGE_KEY)).id; }
  catch { return DEFAULT_THEME; }
}
export function applyTheme(id, root = document.documentElement) {
  const theme = resolveTheme(id);
  root.dataset.theme = theme.id;
  for (const [name, value] of Object.entries(theme.tokens)) root.style.setProperty(name, value);
  return theme.id;
}
export function saveTheme(id, storage) {
  try { storage.setItem(THEME_STORAGE_KEY, resolveTheme(id).id); return true; }
  catch { return false; }
}
export function initializeTheme() {
  let id = DEFAULT_THEME;
  try { id = readTheme(window.localStorage); } catch { /* Storage may be unavailable. */ }
  return applyTheme(id);
}
