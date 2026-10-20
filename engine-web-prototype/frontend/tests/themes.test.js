import test from 'node:test';
import assert from 'node:assert/strict';
import {THEMES, DEFAULT_THEME, THEME_STORAGE_KEY, resolveTheme, readTheme, saveTheme, applyTheme} from '../src/themes.js';
const luminance = hex => {
 const channels = [1,3,5].map(i => parseInt(hex.slice(i,i+2),16)/255).map(v => v <= .04045 ? v/12.92 : ((v+.055)/1.055)**2.4);
 return channels.reduce((total,v,i) => total + v * [.2126,.7152,.0722][i],0);
};
const contrast = (a,b) => { const x=luminance(a), y=luminance(b); return (Math.max(x,y)+.05)/(Math.min(x,y)+.05); };
for (const theme of THEMES) {
 test(`${theme.name}: readable text and semantic states on all interactive surfaces`, () => {
  for (const fg of ['--text-primary','--text-secondary','--text-muted','--accent','--success','--warning','--error','--info']) {
   for (const bg of ['--bg-app','--bg-panel','--bg-panel-elevated','--bg-hover','--bg-active','--bg-input','--graph-node']) {
    assert.ok(contrast(theme.tokens[fg],theme.tokens[bg]) >= 4.5, `${fg} on ${bg}: ${contrast(theme.tokens[fg],theme.tokens[bg]).toFixed(2)}`);
   }
  }
  for (const bg of ['--bg-app','--graph-node']) assert.ok(contrast(theme.tokens['--graph-connection'],theme.tokens[bg]) >= 3, `connections on ${bg}`);
 });
}
test('theme preference round-trips, unknown values fall back, denied storage does not crash', () => {
 const data = new Map(), storage = {getItem:key=>data.get(key),setItem:(key,value)=>data.set(key,value)};
 assert.equal(readTheme(storage),DEFAULT_THEME);
 for (const theme of THEMES) { assert.equal(saveTheme(theme.id,storage),true); assert.equal(readTheme(storage),theme.id); }
 data.set(THEME_STORAGE_KEY,'obsolete-theme'); assert.equal(readTheme(storage),DEFAULT_THEME);
 const denied={getItem(){throw Error('Denied');},setItem(){throw Error('Quota');}};
 assert.equal(readTheme(denied),DEFAULT_THEME); assert.equal(saveTheme('sakura',denied),false);
 assert.equal(resolveTheme(null).id,DEFAULT_THEME);
});
test('switching themes replaces the complete token contract without stale colors', () => {
 const properties=new Map(), root={dataset:{},style:{setProperty:(key,value)=>properties.set(key,value)}};
 const keys=Object.keys(THEMES[0].tokens).sort();
 for(const theme of [...THEMES,THEMES[0]]) {
  assert.deepEqual(Object.keys(theme.tokens).sort(),keys);
  applyTheme(theme.id,root);assert.equal(root.dataset.theme,theme.id);
  assert.deepEqual(Object.fromEntries(properties),theme.tokens);
 }
});
