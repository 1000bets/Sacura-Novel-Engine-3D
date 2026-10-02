import english from './translations.en.json' with {type:'json'};

export const LANGUAGE_STORAGE_KEY = 'sacura-language-v1';
export const LANGUAGES = Object.freeze([{id:'ru',name:'Русский'},{id:'en',name:'English'}]);
export const DEFAULT_LANGUAGE = 'ru';
let language = DEFAULT_LANGUAGE;
const listeners = new Set();
export const getLanguage = () => language;
export const localeTag = () => language === 'en' ? 'en-US' : 'ru-RU';
export function subscribeLanguage(listener) { listeners.add(listener); return () => listeners.delete(listener); }
export function readLanguage(storage) {
  try { const value=storage.getItem(LANGUAGE_STORAGE_KEY); return LANGUAGES.some(item=>item.id===value)?value:DEFAULT_LANGUAGE; }
  catch { return DEFAULT_LANGUAGE; }
}
export function setLanguage(id, storage, root = globalThis.document?.documentElement) {
  if (!LANGUAGES.some(item=>item.id===id)) return false;
  language=id;
  if(root)root.lang=id;
  let saved=true;
  try { (storage ?? globalThis.localStorage)?.setItem(LANGUAGE_STORAGE_KEY,id); } catch { saved=false; }
  for(const listener of listeners)listener();
  return saved;
}
export function initializeLanguage(storage) {
  try { language=readLanguage(storage ?? globalThis.localStorage); } catch { language=DEFAULT_LANGUAGE; }
  if(globalThis.document)document.documentElement.lang=language;
  return language;
}
// Translate explicit UI copy only. Never pass authored dialogue or project names here.
export function t(source, values) {
  if(typeof source!=='string')return source;
  const key=source.trim();
  if(!key)return source;
  const translated=language==='en'?(english[key]??key):key;
  const padded=source.slice(0,source.length-source.trimStart().length)+translated+source.slice(source.trimEnd().length);
  return values ? padded.replace(/\{(\d+)\}/g,(match,index)=>String(values[index]??match)) : padded;
}
// Mark option labels coming from authored data so a name matching UI copy stays intact.
export const literalLabel = text => ({text});
export const optionLabel = value => value && typeof value==='object' && 'text' in value ? value.text : t(value);
export function plural(count, one, few, many, englishOne, englishMany=englishOne+'s') {
  if(language==='en')return `${count} ${count===1?englishOne:englishMany}`;
  return `${count} ${{one,few,many,other:few}[new Intl.PluralRules('ru').select(count)]}`;
}
// Diagnostics can outlive a language switch. Match only known UI templates;
// interpolate their captured values verbatim, including filenames and authored names.
function diagnosticPattern(source,target) {
 const slots=[];
 const escaped=source.replace(/[.*+?^${}()|[\]\\]/g,'\\$&').replace(/\\\{(\d+)\\\}/g,(_,index)=>{slots.push(Number(index));return '(.*?)';});
 return {regex:new RegExp('^'+escaped+'$'),slots,target};
}
const entries=Object.entries(english);
const reverse=new Map(entries.map(([ru,en])=>[en,ru]));
const patterns={
 en:entries.filter(([key])=>/\{\d+\}/.test(key)).map(([ru,en])=>diagnosticPattern(ru,en)),
 ru:entries.filter(([key])=>/\{\d+\}/.test(key)).map(([ru,en])=>diagnosticPattern(en,ru)),
};
export function message(source) {
 if(typeof source!=='string')return source;
 const key=source.trim(),exact=language==='en'?english[key]:reverse.get(key);
 if(exact)return source.slice(0,source.length-source.trimStart().length)+exact+source.slice(source.trimEnd().length);
 for(const {regex,slots,target} of patterns[language]){
  const match=regex.exec(source);
  if(match){const values={};slots.forEach((slot,i)=>values[slot]=match[i+1]);return target.replace(/\{(\d+)\}/g,(placeholder,index)=>values[index]??placeholder);}
 }
 return source;
}
