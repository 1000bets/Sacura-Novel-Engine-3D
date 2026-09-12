import {defineConfig} from 'vite';
export default defineConfig({server:{watch:{usePolling:process.platform==='win32',interval:400}}});
