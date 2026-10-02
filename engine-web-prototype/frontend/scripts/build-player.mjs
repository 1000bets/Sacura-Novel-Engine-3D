import {build} from 'vite';
import {readFile,writeFile,mkdir,rm} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url)),out=root+'.player-build';
await build({configFile:false,root,publicDir:false,define:{'process.env.NODE_ENV':JSON.stringify('production'),__SACURA_BUILD__:JSON.stringify({version:'1.0.0',environment:'player',commit:'local'})},build:{outDir:out,emptyOutDir:true,lib:{entry:root+'src/play.jsx',name:'SacuraPlayer',formats:['iife'],fileName:()=> 'player-runtime.js',cssFileName:'player-runtime'},assetsInlineLimit:Infinity,cssCodeSplit:false,rollupOptions:{output:{inlineDynamicImports:true}}}});
await mkdir(root+'public',{recursive:true});for(const name of ['player-runtime.js','player-runtime.css'])await writeFile(root+'public/'+name,await readFile(out+'/'+name));await rm(out,{recursive:true,force:true});
