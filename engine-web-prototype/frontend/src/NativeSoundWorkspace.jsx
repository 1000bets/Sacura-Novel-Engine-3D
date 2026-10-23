import React,{useEffect,useMemo,useState} from 'react';
import {Button,Field,Icon,Waveform} from './StudioParts.jsx';
import {soundDesk} from './audio.js';
import AudioTimingEditor from './AudioTimingEditor.jsx';
import AudioActionControls from './AudioActionControls.jsx';
import {normalizeSidechain} from './audioSettings.js';
import {t as tr,useLocale} from './i18n.jsx';

const settingsKey='sacura.native.sound.preview.v1';
const savedSettings=()=>{try{return JSON.parse(window.localStorage.getItem(settingsKey)||'{}');}catch{return {};}};

export function nativeAudioAssets(assets=[],session=''){
  return assets.filter(asset=>asset.typeId==='Audio'&&/\.wav$/i.test(asset.name||'')).map(asset=>({
    id:asset.id,name:asset.name.replace(/\.wav$/i,''),file:asset.name,
    url:`sacura://native-asset/${encodeURIComponent(session)}/${encodeURIComponent(asset.id)}?v=${encodeURIComponent(asset.version||'')}`,
    kind:/music|bgm/i.test(asset.relativePath||'')?'music':/voice|dialog/i.test(asset.relativePath||'')?'voice':'sound',
    caption:asset.path,relativePath:asset.relativePath,builtin:asset.builtin,
  }));
}

export default function NativeSoundWorkspace({assets=[],session='',startupAudioPath=null,disabled=false,onImport,onSetStartup}){
  useLocale();
  const sounds=useMemo(()=>nativeAudioAssets(assets,session),[assets,session]);
  const [filter,setFilter]=useState('all'),[settings,setSettings]=useState(()=>savedSettings().assets||{}),[sidechain,setSidechain]=useState(()=>normalizeSidechain(savedSettings().sidechain)),[busy,setBusy]=useState(false),[error,setError]=useState('');
  const [,rerender]=useState(0);
  useEffect(()=>soundDesk.subscribe(()=>rerender(n=>n+1)),[]);
  useEffect(()=>{soundDesk.setAssets(sounds.map(asset=>({...asset,...settings[asset.id]})));},[sounds,settings]);
  useEffect(()=>{try{window.localStorage.setItem(settingsKey,JSON.stringify({assets:settings,sidechain}));}catch{}},[settings,sidechain]);
  useEffect(()=>{soundDesk.setSidechain(sidechain);},[sidechain]);
  useEffect(()=>()=>soundDesk.stopAll(),[session]);
  const update=(id,patch)=>setSettings(value=>({...value,[id]:{...value[id],...patch}}));
  const run=async action=>{setBusy(true);setError('');try{await action();}catch(e){setError(e.message||String(e));}finally{setBusy(false);}};
  const visible=sounds.filter(asset=>filter==='all'||(settings[asset.id]?.kind||asset.kind)===filter);
  return <div className="workspace-content sound-workspace">
    <div className="page-heading"><div><div className="eyebrow">{tr('МЕДИАТЕКА / ')}{sounds.length} {tr('ФАЙЛОВ')}</div>
      <h1>{tr('Звуковые файлы')}</h1><p>{tr('Импортируйте WAV, прослушайте файл и выберите стартовый звук проекта.')}</p></div>
      <Button icon="Square" onClick={()=>soundDesk.stopAll()}>{tr('Остановить всё')}</Button></div>
    <div className="sound-import"><div><strong>{tr('Звук проекта')}</strong><p>{tr('WAV-файлы хранятся в Content/Audio и доступны через AssetRegistry.')}</p></div>
      <Button icon="Upload" disabled={disabled||busy} onClick={()=>run(onImport)}>{busy?tr('Обработка…'):tr('Импортировать WAV')}</Button></div>
    <div className="dock-toolbar" role="group" aria-label={tr('Фильтр звуков')}>
      { [['all','Все'],['music','Музыка'],['voice','Озвучка'],['sound','Эффекты']].map(([id,label])=><Button key={id} className={filter===id?'active':''} onClick={()=>setFilter(id)}>{tr(label)}</Button>) }
    </div>
    {error&&<div className="error-box" role="alert">{error}</div>}
    <div className="audio-routing"><div><Icon name="Mic" size={23}/><strong>{tr('Голос')}</strong></div>
      <span className="route-line">attack {sidechain.attack} {tr('с →')}</span>
      <div><Icon name="AudioLines" size={23}/><strong>{tr('Музыка −')}{sidechain.reductionDb} dB</strong></div>
      <span className="route-line">release {sidechain.release} {tr('с →')}</span>
      <div><Icon name="Volume2" size={23}/><strong>{tr('Исходная громкость')}</strong></div>
      <span className={'status-pill '+(soundDesk.ducks.size?'active':'')}>{soundDesk.ducks.size?tr('Сейчас приглушена'):tr('Сайдчейн готов')}</span></div>
    <details className="sidechain-settings"><summary>{tr('Сайдчейн прослушивания')}</summary><div className="sidechain-fields">
      {[['attack','Атака, с',0,10,.01],['release','Восстановление, с',0,10,.01],['reductionDb','Снижение громкости, dB',0,60,1]].map(([key,label,min,max,step])=><Field key={key} label={tr(label)}><input type="number" min={min} max={max} step={step} value={sidechain[key]} onChange={event=>setSidechain(normalizeSidechain({...sidechain,[key]:Number(event.target.value)}))}/></Field>)}
    </div></details>
    {!sounds.length&&<div className="hint-box"><Icon name="Info"/><span>{tr('В проекте пока нет WAV-файлов. Импортируйте звук, чтобы увидеть его здесь.')}</span></div>}
    <div className="sound-grid">{visible.map((asset,index)=>{
      const kind=settings[asset.id]?.kind||asset.kind;
      const cfg={volume:kind==='music'?.42:1,loop:kind==='music',...settings[asset.id]};
      const track=soundDesk.get(asset.id);
      const active=Boolean(startupAudioPath&&startupAudioPath.replaceAll('\\','/').toLowerCase()===asset.relativePath.replaceAll('\\','/').toLowerCase());
      return <article className="sound-card" key={asset.id}>
        <div className="between"><span className={'type-tag '+(kind==='music'?'green':'violet')}><Icon name={kind==='music'?'Music2':kind==='voice'?'Mic':'Volume2'}/>{kind==='music'?tr('Музыка'):kind==='voice'?tr('Озвучка'):tr('Звуковой эффект')}</span><span className="micro">{String(index+1).padStart(2,'0')}</span></div>
        <h2>{asset.name}</h2><p>{asset.caption}</p>
        <Field label={tr('Тип звука')}><select aria-label={tr('Тип звука')+' '+asset.name} value={kind} onChange={event=>update(asset.id,{kind:event.target.value})}><option value="sound">{tr('Звуковой эффект')}</option><option value="music">{tr('Музыка')}</option><option value="voice">{tr('Озвучка')}</option></select></Field>
        <Waveform key={asset.url} asset={{...asset,...settings[asset.id],kind}}/>
        <AudioTimingEditor asset={{...asset,...settings[asset.id],kind}} onChange={patch=>update(asset.id,patch)}/>
        <div className="sound-controls"><Button className="primary" icon={track?.status==='playing'?'Pause':'Play'} onClick={()=>{
          if(track?.status==='playing')soundDesk.pause(asset.id);
          else if(track?.status==='paused')soundDesk.resume(asset.id);
          else soundDesk.play(asset.id,{...cfg,key:asset.id,duck:cfg.duck!==false,fadeIn:cfg.fadeInEnabled===false?0:cfg.fadeIn,fadeOut:cfg.fadeOutEnabled===false?0:cfg.fadeOut});
        }}>{track?.status==='playing'?tr('Пауза'):track?.status==='paused'?tr('Продолжить'):tr('Слушать')}</Button>
          <Button icon="Square" title={tr('Стоп')} onClick={()=>soundDesk.stop(asset.id,{endMode:cfg.endMode||'immediate',fadeOut:cfg.fadeOutEnabled===false?0:cfg.fadeOut||0})}/>
          <Field label={tr('Громкость {0}%', [Math.round(cfg.volume*100)])}><input type="range" min="0" max="1" step=".01" value={cfg.volume} onChange={event=>{const volume=Number(event.target.value);update(asset.id,{volume});soundDesk.setVolume(asset.id,volume);}}/></Field></div>
        <div className="audio-options"><label><input type="checkbox" checked={cfg.loop} onChange={event=>{update(asset.id,{loop:event.target.checked});if(track)track.audio.loop=event.target.checked;}}/> {tr('Повторять')}</label></div>
        <AudioActionControls action={{...cfg,type:kind==='music'?'music':'sound'}} onChange={patch=>update(asset.id,patch)}/>
        {track?.error&&<div className="error-box" role="alert">{track.error}</div>}
        <div className="sound-footer"><span title={asset.file}>{asset.file}</span><Button icon={active?'Check':'Plus'} disabled={disabled||busy||asset.builtin||active} onClick={()=>run(()=>onSetStartup(asset.id))}>{active?tr('Стартовый звук'):tr('Сделать стартовым')}</Button></div>
      </article>;
    })}</div>
    {startupAudioPath&&<div className="hint-box"><Icon name="Info"/><span>{tr('Стартовый звук сохраняется в файле проекта и воспроизводится при запуске истории.')}</span><Button disabled={disabled||busy} onClick={()=>run(()=>onSetStartup(null))}>{tr('Убрать стартовый звук')}</Button></div>}
  </div>;
}
