import {writeFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
// Generate the synchronous starter's bundle before loading any project modules.
await import('./build-component-sources.mjs');
const {createStandardProject}=await import('../src/projectLifecycle.js');
const {templateBuilders,novel,shooter,quest,clicker}=await import('../templates/projects.mjs');
const {exportAuthoringPackage}=await import('../src/mechanicPackages.js');
const {validateStudio}=await import('../src/studioModel.js');
export {shooter,quest,novel,clicker};
const directory=new URL('../public/game-templates/',import.meta.url),metadata={author:'Sacura contributors',license:'MIT',version:'3.0.0'};
export async function buildTemplates(){
 const assetUrls={};
 for(const [key,build]of [...templateBuilders,['letters-after-rain',()=>createStandardProject()]]){const p=await build(),issues=validateStudio(p);if(issues.length)throw Error(key+': '+JSON.stringify(issues));const pkg=await exportAuthoringPackage(p,p,{...metadata,kind:'template',name:p.title,description:p.description,readAsset:async url=>{const {readFile}=await import('node:fs/promises');const bytes=await readFile(new URL('../public/'+decodeURIComponent(url.replace(/^\//,'')),import.meta.url));return new Blob([bytes]);}});pkg.metadata.id='sacura.template.'+key;for(const asset of pkg.assets){const audio=p.audioAssets.find(a=>a.name===asset.name);if(audio?.url?.startsWith('/sound/'))assetUrls[asset.sha256]=audio.url.slice(1);}await writeFile(new URL(key+'.sacura-template.json',directory),JSON.stringify(pkg,null,2)+'\n');}
 await writeFile(new URL('asset-manifest.json',directory),JSON.stringify({version:1,assets:assetUrls},null,2)+'\n');
 const p=await novel(),definition=p.gameplay.mechanicDefinitions.find(d=>d.name==='Reward'),counter=p.gameplay.mechanicDefinitions.find(d=>d.name==='Counter');
 const pkg=await exportAuthoringPackage(p,definition,{...metadata,kind:'component',definitionIds:[definition.id,counter.id],name:'Награда · Reward + Counter',description:'Открываемая формула награды; Counter — явная зависимость. Создайте подключения и назначьте Counter в ссылках Reward. Подходит для очков, отношения и монет.'});pkg.metadata.id='sacura.recipe.reward';await writeFile(new URL('reward.sacura-package.json',directory),JSON.stringify(pkg,null,2)+'\n');
}
if(process.argv[1]===fileURLToPath(import.meta.url))await buildTemplates();
