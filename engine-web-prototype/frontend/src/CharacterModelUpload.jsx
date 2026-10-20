import {t as tr, useLocale} from './i18n.jsx';
import React,{useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';

export default function CharacterModelUpload({model,disabled,busy,onChoose,onFile,onReset}){
 useLocale();
 const [dragging,setDragging]=useState(false);
 return <section className="character-model-controls"><h3>{tr("3D-модель персонажа")}</h3>
  <div className={'character-model-dropzone'+(dragging?' dragging':'')+(disabled?' disabled':'')} onDragOver={event=>{event.preventDefault();if(!disabled)setDragging(true);}} onDragLeave={()=>setDragging(false)} onDrop={event=>{event.preventDefault();setDragging(false);if(!disabled&&event.dataTransfer.files[0])onFile(event.dataTransfer.files[0]);}}>
   <span className="character-model-upload-icon"><Icon name={model?'FileBox':'UploadCloud'} size={34}/></span>
   <div className="character-model-upload-copy"><strong>{busy?tr("Загружаем модель…"):model?model.name:tr("Перетащите модель персонажа сюда")}</strong><p>{model?tr("{0} МБ · модель сохранена в проекте", [(model.bytes/1024/1024).toFixed(2)]):tr("Или выберите файл на компьютере. Меш и встроенные анимации появятся в предпросмотре и карточках.")}</p><small>{tr("GLB · до 100 МБ · встроенные текстуры")}</small></div>
   <Button className="primary" icon="FolderOpen" disabled={disabled} onClick={onChoose}>{busy?tr("Загрузка…"):model?tr("Заменить модель"):tr("Выбрать модель")}</Button>
  </div>
  <div className="character-model-upload-footer"><p>{model?tr("Модель приведена к высоте 1,6 м. Позицию, поворот и размер настраивайте в редакторе локации."):tr("Сейчас используется стандартная фигура. Можно продолжить работу без загрузки файла.")}</p>{model&&<Button icon="RotateCcw" disabled={disabled} onClick={onReset}>{tr("Стандартная модель")}</Button>}</div>
 </section>;
}
