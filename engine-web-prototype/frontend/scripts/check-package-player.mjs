import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {createServer as httpServer} from 'node:http';
import {createServer} from 'vite';
import {fileURLToPath} from 'node:url';
import {packagePlayerFixture} from '../tests/fixtures/packagePlayer.js';
import {preparePlayableGame} from '../src/gameExport.js';
const root=fileURLToPath(new URL('../',import.meta.url)),dir=join(tmpdir(),'sacura-package-check');
const {original,variant,project}=await packagePlayerFixture();await mkdir(dir,{recursive:true});
const js=await readFile(root+'public/player-runtime.js','utf8'),css=await readFile(root+'public/player-runtime.css','utf8'),html=await preparePlayableGame(project,{readRuntime:async()=>({js,css})});
const denied=structuredClone(project);denied.variables.permission=false;
const deniedHtml=await preparePlayableGame(denied,{readRuntime:async()=>({js,css})});
for(const [name,value] of Object.entries({'door-a.sacura-package.json':original,'door-b.sacura-package.json':variant,'project-c.sacura.json':project}))await writeFile(join(dir,name),JSON.stringify(value,null,2));
await writeFile(join(dir,'player.html'),html);
await writeFile(join(dir,'player-no-permission.html'),deniedHtml);
console.log('Артефакты проверки:',dir);
if(process.argv.includes('--serve')){
 const vite=await createServer({root,server:{middlewareMode:true,watch:null,hmr:false,ws:false}});
 const play='<!doctype html><html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><body><div id="game"></div><script>globalThis.SACURA_PROJECT='+JSON.stringify(project).replace(/</g,'\\u003c')+';</script><script type="module">import React from "react";import {createRoot} from "react-dom/client";import GamePlayer from "/src/GamePlayer.jsx";import "/src/style.css";createRoot(document.getElementById("game")).render(React.createElement(GamePlayer,{source:globalThis.SACURA_PROJECT}));</script></body></html>';
 const server=httpServer(async(req,res)=>{if(['/player.html','/player-no-permission.html'].includes(req.url)){res.setHeader('Content-Type','text/html; charset=utf-8');res.end(req.url==='/player.html'?html:deniedHtml);}else if(req.url==='/play.html'){res.setHeader('Content-Type','text/html; charset=utf-8');res.end((await vite.transformIndexHtml('/play.html',play)).replace('<script type="module" src="/@vite/client"></script>',''));}else vite.middlewares(req,res);});
 server.listen(4179,'127.0.0.1',()=>console.log('Play: http://127.0.0.1:4179/play.html\nStandalone: http://127.0.0.1:4179/player.html\nNo permission: http://127.0.0.1:4179/player-no-permission.html'));
 const close=async()=>{server.close();await vite.close();process.exit(0);};process.on('SIGINT',close);process.on('SIGTERM',close);
}
