export const MAX_IMPORT_BYTES=100*1024*1024;
export function validateImportSize(file){
 if(file.size>MAX_IMPORT_BYTES)throw new Error(`${file.name}: максимальный размер файла — 100 МБ.`);
}
