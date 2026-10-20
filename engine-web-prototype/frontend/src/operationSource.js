// Source metadata belongs to installed engine code. Packages never supply source files.
export function operationSource(definition) {
 const source=definition?.source;
 if(!source||typeof source.path!=='string'||!/^src\/[a-zA-Z0-9_./-]+\.(?:js|jsx|ts|tsx)$/.test(source.path)||source.path.split('/').includes('..'))return null;
 return {path:source.path,line:Number.isInteger(source.line)&&source.line>0?source.line:1};
}
export function operationSourceUrl(definition){const source=operationSource(definition);return source?'/__open-in-editor?file='+encodeURIComponent(source.path+':'+source.line):null;}
export async function openOperationSource(definition,{development=import.meta.env?.DEV,request=globalThis.fetch}={}){
 const url=operationSourceUrl(definition);
 if(!url)throw Error('Расширение не указало доступный исходный файл.');
 if(!development)throw Error('Откройте проект через npm run dev: исходники расширений редактируются в установленном редакторе, затем подключаются обычной сборкой.');
 const response=await request(url);
 if(!response.ok)throw Error('Не удалось открыть исходник. Укажите LAUNCH_EDITOR при запуске npm run dev.');
 return operationSource(definition);
}
