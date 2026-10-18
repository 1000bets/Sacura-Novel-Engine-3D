import {createEmptyProject,projectFromTemplate,projectName} from './projectLifecycle.js';
import {readAuthoringPackage,planPackageImport} from './mechanicPackages.js';

// Built-in and local authoring packages use exactly the same preflight and remapper.
export async function instantiateAuthoringTemplate(raw,name){
 if(raw?.authoringPackage)return instantiateAuthoringTemplate(raw.authoringPackage,name);
 if(raw?.format==='sacura-project-template')return projectFromTemplate(raw,name);
 const pkg=await readAuthoringPackage(raw);if(pkg.roots.kind!=='template')throw Error('Для нового проекта нужен пакет шаблона. Отдельную механику импортируйте в проект.');
 const empty=createEmptyProject(projectName(name||pkg.metadata.name));empty.chapters=[];empty.events=[];
 // A newly created recipient has no authored Events or UI to preserve. Keep fallbacks only
 // for kinds the template does not supply; otherwise the library gets two menus.
 const suppliedKinds=new Set((pkg.content.widgets||[]).map(w=>w.kind));
 empty.widgets=empty.widgets.filter(w=>!suppliedKinds.has(w.kind));
 for(const kind of suppliedKinds)delete empty.ui[kind];
 const plan=await planPackageImport(empty,pkg);return plan.apply(empty);
}
export async function parseAuthoringProjectFile(raw){const value=typeof raw==='string'?JSON.parse(raw):raw;return {project:await instantiateAuthoringTemplate(value),template:value};}
