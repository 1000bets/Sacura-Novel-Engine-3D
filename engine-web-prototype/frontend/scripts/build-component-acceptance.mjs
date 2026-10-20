// Reproducible, independently edited acceptance variants; public templates stay intact.
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {instantiateAuthoringTemplate} from '../src/authoringTemplates.js';
import {editComponentSource,publishComponentSource} from '../src/componentResources.js';
import {preparePlayableGame} from '../src/gameExport.js';
import {templateMutations} from '../tests/fixtures/templateMutations.js';
const root=new URL('../',import.meta.url),out=new URL('dist/acceptance/',root);await mkdir(out,{recursive:true});
const js=await readFile(new URL('public/player-runtime.js',root),'utf8'),css=await readFile(new URL('public/player-runtime.css',root),'utf8');
for(const edit of templateMutations){
 const raw=JSON.parse(await readFile(new URL('public/game-templates/'+edit.path+'.sacura-template.json',root),'utf8')),project=await instantiateAuthoringTemplate(raw,undefined,{reviewedSources:true}),r=project.logicResources.find(r=>r.name===edit.component);
 if(!r.source.includes(edit.from))throw Error('Учебная формула изменилась: '+edit.path);
 editComponentSource(project,r.id,r.source.replace(edit.from,edit.to));publishComponentSource(project,r.id);
 project.description+='\nВариант приёмки: '+edit.result;
 await writeFile(new URL(edit.path+'.sacura-project.json',out),JSON.stringify(project,null,2));
 await writeFile(new URL(edit.path+'.html',out),await preparePlayableGame(project,{readRuntime:async()=>({js,css})}));
 console.log(edit.path+': '+edit.result);
}
