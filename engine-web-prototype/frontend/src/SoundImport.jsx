import {t as tr, useLocale, message} from './i18n.jsx';
import React,{useRef,useState} from 'react';
import {Button,Field,Select} from './StudioParts.jsx';
import {AUDIO_FILE_ACCEPT} from './audioAssets.js';
import {fileKind,importAssetFiles} from './assetFiles.js';
import './soundImport.css';

export default function SoundImport({onImport,disabled}){
 useLocale();
 const input=useRef(),[busy,setBusy]=useState(false),[error,setError]=useState(''),[kind,setKind]=useState('sound'),[status,setStatus]=useState('');
 const upload=async list=>{
  if(disabled||busy||!onImport||!list?.length)return;setBusy(true);setError('');setStatus('Проверяем аудиофайлы…');
  try{const files=Array.from(list);if(files.some(file=>fileKind(file.name)!=='audio'))throw new Error('Выберите WAV, MP3, OGG, M4A, AAC, FLAC или WebM.');
   const imported=await importAssetFiles(files,'Звук');await onImport(imported.map(file=>({...file,audioKind:kind})));setStatus(`Загружено файлов: ${imported.length}. Можно прослушать и назначить реплике.`);
  }catch(cause){setError(cause.message);setStatus('');}finally{setBusy(false);}
 };
 return <section className="sound-import" aria-label={tr("Загрузка звуков")} aria-busy={busy} onDragOver={e=>e.preventDefault()} onDrop={e=>{e.preventDefault();upload(e.dataTransfer.files);}}>
  <div><strong>{tr("Загрузить свои звуки")}</strong><p>{tr("Перетащите аудиофайлы сюда или выберите их на компьютере. До 100 МБ на файл.")}</p></div>
  <Field label={tr("Тип звука")}><Select value={kind} disabled={disabled||busy} options={[["sound","Звуковой эффект"],["music","Музыка"],["voice","Озвучка"]]} onChange={setKind}/></Field>
  <Button icon="Upload" disabled={disabled||busy||!onImport} onClick={()=>input.current.click()}>{busy?tr("Загрузка…"):tr("Выбрать звуки")}</Button>
  <input ref={input} type="file" accept={AUDIO_FILE_ACCEPT} multiple hidden onChange={e=>{upload(e.target.files);e.target.value='';}}/>
  {status&&<p className="sound-import-message" role="status">{status}</p>}{error&&<p className="sound-import-message error-box" role="alert">{message(error)}</p>}
 </section>;
}
