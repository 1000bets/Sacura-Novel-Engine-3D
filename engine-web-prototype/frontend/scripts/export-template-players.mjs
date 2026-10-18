import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {preparePlayableGame} from '../src/gameExport.js';
import {builtinTemplates} from '../src/builtinTemplates.js';
const root=fileURLToPath(new URL('../',import.meta.url)),destination=root+'dist/examples';await mkdir(destination,{recursive:true});
const js=await readFile(root+'public/player-runtime.js','utf8'),css=await readFile(root+'public/player-runtime.css','utf8');
for(const t of builtinTemplates){const pkg=JSON.parse(await readFile(root+'public/game-templates/'+t.path+'.sacura-template.json','utf8')),p=await instantiateAuthoringTemplate(pkg);const html=await preparePlayableGame(p,{readRuntime:async()=>({js,css})});await writeFile(destination+'/'+t.path+'.html',html);console.log(t.name+': '+destination+'/'+t.path+'.html');}
