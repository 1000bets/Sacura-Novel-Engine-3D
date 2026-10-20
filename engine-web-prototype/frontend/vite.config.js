import {defineConfig} from 'vite';
import {readFileSync} from 'node:fs';
const {version}=JSON.parse(readFileSync(new URL('./package.json',import.meta.url),'utf8'));
const build={version,commit:process.env.VITE_BUILD_SHA||'local',environment:process.env.VITE_DEPLOY_ENV||'local'};
export default defineConfig({
 define:{__SACURA_BUILD__:JSON.stringify(build)},
 plugins:[{name:'sacura-version',generateBundle(){this.emitFile({type:'asset',fileName:'version.json',source:JSON.stringify(build)});}}],
 server:{proxy:{'/api':{target:'http://127.0.0.1:3000',changeOrigin:false}},watch:{usePolling:process.platform==='win32',interval:400}},
});
