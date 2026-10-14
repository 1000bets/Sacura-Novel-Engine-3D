import {createServer} from 'node:http';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {join,extname} from 'node:path';
import {tmpdir} from 'node:os';
const root=fileURLToPath(new URL('../',import.meta.url)),artifacts=join(tmpdir(),'sacura-integration');
const kinds=new Set(['event','function','mechanic','template']);
// Only built Editor assets and final C players are served. There is no A project,
// endpoint, account, store or source-project asset server in this verification.
const server=createServer(async(req,res)=>{try{
 const path=new URL(req.url,'http://localhost').pathname;let file;
 if(path==='/')file=root+'dist/index.html';
 else if(/^\/c\/[^/]+\.html$/.test(path)&&kinds.has(path.slice(3,-5)))file=join(artifacts,path.slice(3,-5),'C.html');
 else if(/^\/(assets|game-templates)\/[a-zA-Z0-9_.\/-]+$/.test(path)&&!path.includes('..'))file=root+'dist'+path;
 else if(['/player-runtime.js','/player-runtime.css'].includes(path))file=root+'dist'+path;
 if(!file){res.writeHead(404);res.end('Unavailable: source project A is not served');return;}
 const mime={'.html':'text/html; charset=utf-8','.js':'text/javascript','.css':'text/css','.json':'application/json','.jpg':'image/jpeg','.png':'image/png'};res.setHeader('Content-Type',mime[extname(file)]||'application/octet-stream');res.setHeader('Cache-Control','no-store');res.end(await readFile(file));
 }catch{res.writeHead(404);res.end('Not found');}});
server.listen(5190,'127.0.0.1',()=>console.log('Editor: http://127.0.0.1:5190/\nC-only players: http://127.0.0.1:5190/c/{event,function,mechanic,template}.html'));
process.on('SIGINT',()=>server.close(()=>process.exit()));
