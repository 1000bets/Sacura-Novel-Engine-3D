import {AUDIO_ASSETS} from './studioModel.js';

export const AUDIO_FILE_ACCEPT='.wav,.mp3,.ogg,.m4a,.aac,.flac,.webm';
export function projectAudioAssets(project){
 const assets=new Map(AUDIO_ASSETS.map(asset=>[asset.id,asset]));
 for(const asset of project?.audioAssets||[])if(asset?.id&&asset.url)assets.set(asset.id,asset);
 for(const file of project?.assetFiles||[]){
  if(file.kind!=='audio'||!file.src)continue;
  const kind=['music','voice','sound'].includes(file.audioKind)?file.audioKind:'sound';
  assets.set(file.id,{id:file.id,name:file.name.replace(/\.[^.]+$/,''),file:file.name,url:file.src,kind,speaker:kind==='music'?'Музыка':kind==='voice'?'Озвучка':'Эффект',caption:file.path||file.name,imported:true});
 }
 return [...assets.values()];
}
export async function validateImportedAudio(files,output){
 for(const file of files){if(file.kind!=='audio')continue;
  try{const {buffer}=await output.load(file.src);if(!Number.isFinite(buffer.duration)||buffer.duration<=0)throw new Error('Пустая запись');file.duration=buffer.duration;}
  catch{throw new Error(`${file.name}: не удалось прочитать звук. Выберите исправный аудиофайл в формате, поддерживаемом браузером.`);}
 }
 return files;
}
