import React,{useEffect,useRef,useState} from 'react';
import {Button,Icon} from './StudioParts.jsx';
import {t as tr,useLocale} from './i18n.jsx';
import './nativeCharacterWorkspace.css';

const characterType='engine.CharacterComponent';
const meshType='engine.MeshRendererComponent';
const utilityTypes=new Set(['engine.CameraComponent','engine.LightComponent']);
export const nativeCharacters=objects=>objects.filter(object=>object.components?.some(component=>component.typeId===characterType));
export const nativeCharacterModels=assets=>assets.flatMap(asset=>asset.typeId==='Model'
  ?(asset.subassets||[]).filter(subasset=>subasset.typeId==='StaticMesh')
    .map(subasset=>({key:`${asset.id}|${subasset.id}`,assetId:asset.id,subAssetId:subasset.id,
      label:`${asset.name} · ${subasset.name}`,builtin:asset.builtin})):[]);

export default function NativeCharacterWorkspace({objects=[],components=[],assets=[],selected,disabled=false,playing=false,
  dirty=false,command,select,onFocus,onImport,onActivate}){
  useLocale();
  const characters=nativeCharacters(objects),character=characters.find(item=>item.id===selected);
  const available=objects.filter(item=>!item.components?.some(component=>component.typeId===characterType||utilityTypes.has(component.typeId)));
  const models=nativeCharacterModels(assets);
  const [query,setQuery]=useState(''),[name,setName]=useState(''),[adoptId,setAdoptId]=useState(''),[busy,setBusy]=useState(false),[error,setError]=useState('');
  const initialSelection=useRef(false);
  useEffect(()=>{
    if(initialSelection.current||!characters.length)return;
    initialSelection.current=true;
    if(!character)select(characters[0].id);
  },[characters.length,character?.id,select]);
  const marker=character&&components.find(item=>item.typeId===characterType);
  const renderer=character&&components.find(item=>item.typeId===meshType);
  const sceneUtility=character?.components?.some(component=>utilityTypes.has(component.typeId));
  const description=marker?.properties?.find(item=>item.id==='description')?.value||'';
  const mesh=renderer?.properties?.find(item=>item.id==='mesh_asset_id');
  const modelKey=mesh?.value&&mesh?.subAssetId?`${mesh.value}|${mesh.subAssetId}`:'';
  const create=async event=>{
    event.preventDefault();if(!name.trim()||busy)return;
    setBusy(true);setError('');
    try{const result=await command('character.create',{name:name.trim()});if(result?.id){setName('');await select(result.id);}}
    finally{setBusy(false);}
  };
  const importModel=async()=>{
    setBusy(true);setError('');
    try{
      const model=await onImport();
      if(model&&character){
        let objectId=character.id;
        let componentId=renderer?.id;
        if(sceneUtility){
          const created=await command('character.create',{name:model.name||tr('Новый персонаж')});
          if(!created?.id)throw Error(tr('Не удалось создать персонажа для модели.'));
          objectId=created.id;
          const hierarchy=await command('scene.getHierarchy');
          componentId=hierarchy?.find(item=>item.id===objectId)?.components
            ?.find(component=>component.typeId===meshType)?.id;
        }else if(!componentId){
          componentId=(await command('object.addComponent',{objectId,typeId:meshType}))?.id;
        }
        if(!componentId)throw Error(tr('Не удалось добавить меш персонажу.'));
        await command('component.setAsset',{objectId,componentId,
          property:'mesh_asset_id',assetId:model.assetId,subAssetId:model.subAssetId});
        if(objectId!==character.id)await select(objectId);
      }
    }catch(cause){setError(cause.message);}finally{setBusy(false);}
  };
  const adopt=async event=>{
    event.preventDefault();const objectId=adoptId||available[0]?.id;
    if(!objectId||busy)return;
    setBusy(true);setError('');
    try{const result=await command('object.addComponent',{objectId,typeId:characterType});if(result?.id){await select(objectId);setAdoptId('');}}
    finally{setBusy(false);}
  };
  const duplicate=async()=>{const result=await command('object.duplicate',{objectId:character.id});if(result?.id)await select(result.id);};
  const assignModel=key=>{
    const model=models.find(item=>item.key===key);
    if(renderer)command('component.setAsset',{objectId:character.id,componentId:renderer.id,
      property:'mesh_asset_id',assetId:model?.assetId||null,subAssetId:model?.subAssetId||null});
  };
  return <div className="character-workspace native-character-workspace" onPointerDown={onActivate}>
    <header><div><strong><Icon name="Users"/>{tr('Персонажи')}</strong><small>{characters.length} {tr('в сцене · модели и размещение')}</small></div></header>
    {playing&&<div className="camera-running">{tr('Для изменения персонажей остановите воспроизведение.')}<Button icon="Square" onClick={()=>command('play.stop')}>{tr('Остановить')}</Button></div>}
    {error&&<p className="subscene-form-error" role="alert">{error}</p>}
    <div className="character-workspace-body"><aside>
      <label className="subscene-search"><Icon name="Search" size={14}/><input aria-label={tr('Найти персонажа')} placeholder={tr('Найти персонажа…')} value={query} onChange={event=>setQuery(event.target.value)}/></label>
      <form onSubmit={create}><label>{tr('Новый персонаж')}<input aria-label={tr('Имя нового персонажа')} required placeholder={tr('Например, Алиса')} value={name} onChange={event=>setName(event.target.value)}/></label><Button icon="UserPlus" type="submit" disabled={disabled||busy}>{tr('Создать персонажа')}</Button></form>
      {available.length>0&&<form onSubmit={adopt} className="native-character-adopt"><label>{tr('Объект сцены')}<select aria-label={tr('Объект для персонажа')} value={available.some(item=>item.id===adoptId)?adoptId:available[0].id} onChange={event=>setAdoptId(event.target.value)}>{available.map(item=><option key={item.id} value={item.id}>{item.name}</option>)}</select></label><Button icon="UserPlus" type="submit" disabled={disabled||busy}>{tr('Сделать персонажем')}</Button></form>}
      <div className="character-list">{characters.filter(item=>item.name.toLowerCase().includes(query.toLowerCase())).map(item=><button key={item.id} type="button" className={character?.id===item.id?'active':''} onClick={()=>select(item.id)}><Icon name="PersonStanding"/><span>{item.name}<small>{tr(item.active?'В сцене':'Выключен')} · {tr(item.components.some(component=>component.typeId===meshType)?'3D модель':'Без модели')}</small></span></button>)}</div>
      {!characters.length&&<p className="native-character-note">{tr('Создайте первого персонажа. Он появится в сцене со стартовой 3D моделью.')}</p>}
    </aside><main>{character?<>
      <div className="subscene-edit-heading"><div><small>{tr('ПЕРСОНАЖ СЦЕНЫ')}</small><h2>{character.name}</h2></div><div className="character-buttons"><Button icon="Focus" onClick={()=>onFocus(character.id)}>{tr('Показать в сцене')}</Button><Button icon="Copy" disabled={disabled||busy} onClick={duplicate}>{tr('Дублировать')}</Button><Button icon="Trash2" disabled={disabled||busy} onClick={()=>command('object.destroy',{objectId:character.id})}>{tr('Удалить')}</Button></div></div>
      {sceneUtility&&<p className="native-character-note">{tr('Это камера или источник света. Импорт модели создаст отдельного персонажа в сцене.')}</p>}
      <fieldset className="character-details" disabled={disabled||busy}>
        <div className="subscene-form-grid two"><label>{tr('Имя')}<input key={character.id+character.name} aria-label={tr('Имя персонажа')} defaultValue={character.name} onBlur={event=>{const value=event.target.value.trim();if(value&&value!==character.name)command('object.rename',{objectId:character.id,name:value});}} onKeyDown={event=>{if(event.key==='Enter')event.currentTarget.blur();}}/></label><label className="check"><input type="checkbox" checked={character.active} onChange={event=>command('object.setActive',{objectId:character.id,active:event.target.checked})}/>{tr('Персонаж включён')}</label></div>
        <label>{tr('Описание')}<textarea key={character.id+description} aria-label={tr('Описание персонажа')} rows={3} defaultValue={description} placeholder={tr('Кто этот персонаж?')} onBlur={event=>{if(marker&&event.target.value!==description)command('component.setProperty',{objectId:character.id,componentId:marker.id,property:'description',value:event.target.value});}}/></label>
        <section className="native-character-model"><h3>{tr('3D модель')}</h3><p>{tr('Выберите статический меш проекта или импортируйте GLB/FBX. Стартовая модель позволяет видеть персонажа сразу после создания.')}</p>
          {renderer?<label>{tr('Меш персонажа')}<select aria-label={tr('Меш персонажа')} value={modelKey} onChange={event=>assignModel(event.target.value)}><option value="">{tr('Без модели')}</option>{models.map(model=><option key={model.key} value={model.key}>{model.builtin?'Engine · ':'Game · '}{model.label}</option>)}</select></label>
            :<Button icon="Box" onClick={()=>command('object.addComponent',{objectId:character.id,typeId:meshType})}>{tr('Добавить меш')}</Button>}
          <Button icon="Upload" onClick={importModel} disabled={disabled||busy}>{tr('Импортировать модель')}</Button>
          <small>{tr('FBX с ригом загружается как статическая поза. Риг и анимации пока не воспроизводятся в C++ viewport.')}</small>
          <Button icon="UserMinus" disabled={disabled||busy} onClick={()=>command('object.removeComponent',{objectId:character.id,componentId:marker.id})}>{tr('Убрать признак персонажа')}</Button>
        </section>
        <div className="native-character-actions"><Button icon="Save" onClick={()=>command('scene.save')} disabled={!dirty||disabled}>{tr('Сохранить сцену')}</Button><span>{tr('Позиция и поворот редактируются в сцене и Inspector.')}</span></div>
      </fieldset>
    </>:<div className="character-empty"><Icon name="Users" size={40}/><h2>{tr('Персонажи вашей истории')}</h2><p>{tr('Выберите персонажа слева или создайте нового.')}</p></div>}</main></div>
  </div>;
}
