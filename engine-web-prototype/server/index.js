import {connectStorage} from './storage.js';
import {createApp} from './app.js';
import {createAuth} from './auth.js';
const storage=await connectStorage();
await createAuth(storage.db).migrateOwner(process.env.APP_USER,process.env.APP_PASSWORD);
const app=createApp(storage,{secureCookies:process.env.COOKIE_SECURE==='true',trustProxy:process.env.TRUST_PROXY==='1'?1:process.env.TRUST_PROXY||false,frontendBaseUrl:process.env.FRONTEND_BASE_URL||''});
const server=app.listen(Number(process.env.PORT||3000),'0.0.0.0',()=>console.log('Sacura API listening'));
for(const signal of ['SIGINT','SIGTERM'])process.on(signal,()=>{server.close(()=>{storage.db.close();process.exit(0);});setTimeout(()=>process.exit(1),10000).unref();});
