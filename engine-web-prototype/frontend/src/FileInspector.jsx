import React,{useState} from 'react';
import {Field,Button,Icon} from './StudioParts.jsx';
export default function FileInspector({file,disabled,onChange,onPlaceModel,onPreviewAudio}){
 const [name,setName]=useState(file.name),[error,setError]=useState(''),[imageSize,setImageSize]=useState(null);
 const save=()=>{try{onChange({name});setError('');}catch(e){setError(e.message);}};
 const types={model:'3D-модель',audio:'Звук',image:'Изображение',file:'Файл'};
 return <><div className="inspector-identity"><Icon name={file.kind==='model'?'FileBox':file.kind==='audio'?'FileAudio':file.kind==='image'?'Image':'File'} size={25}/><strong>{types[file.kind]}</strong></div>
 <Field label="Имя файла"><input aria-label="Имя файла" disabled={disabled} value={name} onChange={e=>setName(e.target.value)} onKeyDown={e=>{if(e.key==='Enter')save();}}/></Field>
 <Button icon="Check" disabled={disabled||name===file.name} onClick={save}>Переименовать</Button>{error&&<p role="alert" className="error-box">{error}</p>}
 <Field label="Тип"><span>{types[file.kind]} · {file.model?.format?.toUpperCase()||file.name.split('.').at(-1).toUpperCase()}</span></Field>
 <Field label="Путь"><span>{file.path}</span></Field><Field label="Размер"><span>{file.bytes!=null?(file.bytes/1024/1024).toFixed(2)+' МБ':'Встроенный ресурс'}</span></Field>
 {file.kind==='model'&&<><Field label="Материалы"><span>{file.model?.slots?.map(slot=>slot.name).join(', ')||'Не указаны'}</span></Field><Field label="Анимации"><span>{file.model?.animations?.length||0}</span></Field><Button icon="Plus" disabled={disabled} onClick={()=>onPlaceModel(file)}>Добавить в сцену</Button></>}
 {file.kind==='audio'&&<><Field label="Назначение звука"><select aria-label="Назначение звука" disabled={disabled} value={file.audioKind||'sound'} onChange={e=>onChange({audioKind:e.target.value})}><option value="sound">Звуковой эффект</option><option value="music">Музыка</option><option value="voice">Озвучка</option></select></Field><Field label="Длительность"><span>{file.duration!=null?file.duration.toFixed(2)+' сек':'Определяется при воспроизведении'}</span></Field><Button icon="Play" onClick={()=>onPreviewAudio(file)}>Прослушать / остановить</Button></>}
 {file.kind==='image'&&<><img className="file-inspector-preview" src={file.src} alt={file.name} onLoad={e=>setImageSize([e.currentTarget.naturalWidth,e.currentTarget.naturalHeight])}/>{imageSize&&<Field label="Разрешение"><span>{imageSize.join(' × ')} пикселей</span></Field>}</>}
 <Field label="Используется объектами"><span>{file.users?.map(user=>user.name).join(', ')||'Не размещён в сцене'}</span></Field>
 </>;
}
