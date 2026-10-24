import {mkdir, writeFile, readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';

const Destination = new URL('../Root/Verification/WebTemplateOracle/', import.meta.url);
await mkdir(Destination, {recursive:true});
for (const [Name, Package] of Object.entries({quest:'point-and-click',shooter:'shooter',novel:'novel',horror:'horror'})) {
  const Document = JSON.parse(await readFile(new URL('../engine-web-prototype/frontend/public/game-templates/'+Package+'.sacura-template.json',import.meta.url),'utf8'));
  const Project = Document.content;
  const Oracle = {
    name:Document.metadata.name, objects:Project.objects, scenes:Project.subscenes,
    gameplay:Project.gameplay, chapters:Project.chapters, widgets:Project.widgets,
    ui:Project.ui, input:Project.input
  };
  delete Oracle.gameplay.mechanicDefinitions;
  await writeFile(new URL(Name+'.json',Destination), JSON.stringify(Oracle,null,2)+'\n');
}
const Sources = ['projects.mjs','horror.mjs','scenes.json', ...['Battle','Counter','DeathReaction','EnemyAgent','HauntedProp','HauntedRoom','Health','Inventory','Loadout','LockedDoor','Medkit','Objective','Pickup','Reward','Weapon'].filter(Name=>Name!=='Inventory').map(Name=>'components/'+Name+'.js')];
const Manifest = {};
for (const Source of Sources) {
  Manifest[Source] = createHash('sha256').update(await readFile(new URL('../engine-web-prototype/frontend/templates/'+Source,import.meta.url))).digest('hex');
}
await writeFile(new URL('sources.json',Destination),JSON.stringify(Manifest,null,2)+'\n');
