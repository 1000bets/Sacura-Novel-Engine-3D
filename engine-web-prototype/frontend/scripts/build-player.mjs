import {build} from 'vite';
import {readFile,writeFile,mkdir,rm} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url)),out=root+'.player-build';
let modules=[];const audit={name:'sacura-player-boundary',generateBundle(_,bundle){modules=[...new Set(Object.values(bundle).flatMap(chunk=>Object.keys(chunk.modules||{})).filter(id=>id.startsWith(root+'src/')).map(id=>id.slice(root.length)))].sort();const forbidden=modules.filter(id=>/(Editor|Workspace|ProjectHome|SchemaFields)\.jsx$/.test(id));if(forbidden.length)throw Error('Player импортирует редактор: '+forbidden.join(', '));}};
await build({plugins:[audit],configFile:false,root,publicDir:false,define:{'process.env.NODE_ENV':JSON.stringify('production'),__SACURA_BUILD__:JSON.stringify({version:'1.0.0',environment:'player',commit:'local'})},build:{outDir:out,emptyOutDir:true,lib:{entry:root+'src/play.jsx',name:'SacuraPlayer',formats:['iife'],fileName:()=> 'player-runtime.js',cssFileName:'player-runtime'},assetsInlineLimit:Infinity,cssCodeSplit:false,rollupOptions:{output:{inlineDynamicImports:true}}}});
await mkdir(root+'public',{recursive:true});for(const name of ['player-runtime.js','player-runtime.css'])await writeFile(root+'public/'+name,await readFile(out+'/'+name));await rm(out,{recursive:true,force:true});

await writeFile(root+'public/player-runtime.manifest.json',JSON.stringify({version:1,entry:'src/play.jsx',modules},null,2)+'\n');
