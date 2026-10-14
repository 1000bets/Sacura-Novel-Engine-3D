import {portableAsset} from './mechanicPackages.js';
import {portableProject} from './serverStorage.js';
import {projectAudioAssets} from './audioAssets.js';
import {validateStudio} from './studioModel.js';
const escapeHtml=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
// A function parameter or operation can select audio without an Event Action.
// Scan authoring data, excluding the catalog itself and the inert migration backup.
export function playableAudioAssets(project){
 const assets=projectAudioAssets(project),used=new Set(),byId=new Set(assets.map(a=>a.id));
 const visit=value=>{if(typeof value==='string'){if(byId.has(value))used.add(value);}else if(value&&typeof value==='object'){
  if(value.type==='music')used.add(value.assetId||'music-main');if(value.type==='resume')used.add('music-main');
  // Legacy labels/files apply to audio Actions, never to an Event's display name.
  if(['sound','music'].includes(value.type))for(const a of assets)if(a.name===value.value||a.file===value.value)used.add(a.id);
  for(const child of Object.values(value))visit(child);
 }};
 for(const key of ['events','functions','gameplay','widgets','input','objects','chapters','variables','actionTemplates','groupTemplates'])visit(project[key]);
 return assets.filter(a=>used.has(a.id));
}
export function playableHtml(project,js,css=''){if(!js)throw new Error('Игровой runtime не собран.');return '<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+escapeHtml(project.title)+'</title><style>html,body,#game{margin:0;height:100%;overflow:hidden}'+css.replace(/@import\s*(?:url\(\s*(?:"[^"]*"|'[^']*'|[^)]*)\s*\)|"[^"]*"|'[^']*')[^;]*;/gi,'').replace(/<\/style/gi,'<\\/style')+'</style><body><div id="game"></div><script>globalThis.SACURA_PROJECT='+JSON.stringify(project).replace(/</g,'\\u003c')+';</script><script>'+js.replace(/<\/script/gi,'<\\/script')+'</script></body></html>';}
export async function preparePlayableGame(project,{readRuntime,readAsset}={}){const issues=validateStudio(project).filter(i=>i.level==='error');if(issues.length)throw new Error('Перед экспортом исправьте проблемы: '+issues.map(i=>i.title).join(' · '));const source=structuredClone(project);delete source.gameplayMigration;const copy=await portableProject(source),assets=playableAudioAssets(copy);copy.audioAssets=assets;const sources=new Map();const read=readAsset||async function(url){const response=await fetch(url,{credentials:'same-origin'});if(!response.ok)throw Error('Не удалось встроить ресурс: '+url);const blob=await response.blob();if(blob.type.includes('text/html'))throw Error('Ресурс отсутствует: '+url);return blob;};
 async function embed(url){if(url.startsWith('data:')&&!/^data:model\/gltf/.test(url))return url;if(!sources.has(url))sources.set(url,(async()=>{const blob=await read(url),{type,data}=await portableAsset(blob,url,read);let binary='';for(let i=0;i<data.length;i+=8192)binary+=String.fromCharCode(...data.subarray(i,i+8192));return 'data:'+type+';base64,'+btoa(binary);})());return sources.get(url);}
 async function visit(value){for(const [key,child]of Object.entries(value)){if(['src','url','map','normalMap'].includes(key)&&typeof child==='string'&&child&&!(key==='src'&&value.format==='obj'))value[key]=await embed(child);else if(child&&typeof child==='object')await visit(child);}}await visit(copy);let js,css;if(readRuntime){({js,css}=await readRuntime());}else{const base=import.meta.env?.BASE_URL||'/',response=await fetch(base+'player-runtime.js'),styles=await fetch(base+'player-runtime.css');if(!response.ok||!styles.ok)throw new Error('Игровой runtime недоступен. Перезапустите движок после сборки.');js=await response.text();css=await styles.text();}return playableHtml(copy,js,css);}
export async function exportPlayableGame(project){const html=await preparePlayableGame(project),url=URL.createObjectURL(new Blob([html],{type:'text/html'})),a=document.createElement('a');a.href=url;a.download=project.title.replace(/[<>:"/\\|?*]/g,'_')+'.html';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);return html;}
