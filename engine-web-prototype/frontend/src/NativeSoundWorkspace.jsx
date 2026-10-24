import './nativeSoundWorkspace.css';
import React, {useEffect, useMemo, useState} from 'react';
import {Button, Field, Icon, Waveform} from './StudioParts.jsx';
import {soundDesk as SoundDesk} from './audio.js';
import AudioTimingEditor from './AudioTimingEditor.jsx';
import AudioActionControls from './AudioActionControls.jsx';
import {normalizeSidechain as NormalizeSidechain} from './audioSettings.js';
import {t as Translate, useLocale as UseLocale} from './i18n.jsx';

const SettingsKey = 'sacura.native.sound.preview.v1';
const SoundKinds = {music:{label:'Музыка',icon:'Music2',className:'green'},voice:{label:'Озвучка',icon:'Mic',className:'violet'},sound:{label:'Звуковой эффект',icon:'Volume2',className:'violet'}};

function ReadSettings() {
  try {
    const Settings = JSON.parse(window.localStorage.getItem(SettingsKey) || '{}');
    if (Settings && typeof Settings === 'object') {
      return Settings;
    }
  } catch {}
  return {};
}

export function NativeAudioAssets(Assets = [], Session = '') {
  return Assets.filter(Asset => Asset.typeId === 'Audio' && /\.wav$/i.test(Asset.name || '')).map(Asset => {
    let Kind = 'sound';
    if (/music|bgm/i.test(Asset.relativePath || '')) {
      Kind = 'music';
    } else if (/voice|dialog/i.test(Asset.relativePath || '')) {
      Kind = 'voice';
    }
    return {...Asset, name:Asset.name.replace(/\.wav$/i, ''),file:Asset.name,
      url:`sacura://native-asset/${encodeURIComponent(Session)}/${encodeURIComponent(Asset.id)}?v=${encodeURIComponent(Asset.version || '')}`,
      kind:Kind,caption:Asset.path};
  });
}

export default function NativeSoundWorkspace({assets:Assets = [],session:Session = '',startupAudioPath:StartupAudioPath = null,disabled:Disabled = false,onImport:Import,onSetStartup:SetStartup}) {
  UseLocale();
  const Sounds = useMemo(() => NativeAudioAssets(Assets, Session), [Assets, Session]);
  const [Filter, SetFilter] = useState('all');
  const [Settings, SetSettings] = useState(() => ReadSettings().assets || {});
  const [Sidechain, SetSidechain] = useState(() => NormalizeSidechain(ReadSettings().sidechain));
  const [Busy, SetBusy] = useState(false);
  const [Error, SetError] = useState('');
  const [, Refresh] = useState(0);
  useEffect(() => SoundDesk.subscribe(() => Refresh(Revision => Revision + 1)), []);
  useEffect(() => { SoundDesk.setAssets(Sounds.map(Asset => ({...Asset,...Settings[Asset.id]}))); }, [Sounds, Settings]);
  useEffect(() => {
    try {
      window.localStorage.setItem(SettingsKey, JSON.stringify({assets:Settings,sidechain:Sidechain}));
    } catch {
      SetError(Translate('Не удалось сохранить параметры прослушивания.'));
    }
  }, [Settings, Sidechain]);
  useEffect(() => { SoundDesk.setSidechain(Sidechain); }, [Sidechain]);
  useEffect(() => () => SoundDesk.stopAll(), [Session]);
  useEffect(() => {
    if (Disabled) {
      SoundDesk.stopAll();
    }
  }, [Disabled]);
  const Update = (Identifier, Patch) => SetSettings(Current => ({...Current,[Identifier]:{...Current[Identifier],...Patch}}));
  const Run = async Action => {
    SetBusy(true);
    SetError('');
    try {
      await Action();
    } catch (Failure) {
      SetError(Failure.message || String(Failure));
    } finally {
      SetBusy(false);
    }
  };
  const Visible = Sounds.filter(Asset => Filter === 'all' || (Settings[Asset.id]?.kind || Asset.kind) === Filter);
  let ImportLabel = Translate('Импортировать WAV');
  let SidechainLabel = Translate('Сайдчейн готов');
  let SidechainClass = 'status-pill';
  if (Busy) {
    ImportLabel = Translate('Обработка…');
  }
  if (SoundDesk.ducks.size) {
    SidechainLabel = Translate('Сейчас приглушена');
    SidechainClass += ' active';
  }
  return <div className="workspace-content sound-workspace native-sound-workspace">
    <div className="page-heading"><div><h1>{Translate('Звуковые файлы')}</h1>
      <p>{Translate('Импортируйте WAV, прослушайте файл и выберите стартовый звук проекта.')}</p></div>
      <Button icon="Square" onClick={() => SoundDesk.stopAll()}>{Translate('Остановить всё')}</Button></div>
    <div className="sound-import"><div><strong>{Translate('Звук проекта')}</strong>
      <p>{Translate('Импортированные WAV сохраняются в Content/Audio. Параметры прослушивания сохраняются только в этом редакторе и не меняют звук игры.')}</p></div>
      <Button icon="Upload" disabled={Disabled || Busy} onClick={() => Run(Import)}>{ImportLabel}</Button></div>
    <div className="dock-toolbar" role="group" aria-label={Translate('Фильтр звуков')}>
      {[['all','Все'],['music','Музыка'],['voice','Озвучка'],['sound','Эффекты']].map(([Identifier, Label]) => {
        let ClassName = '';
        if (Filter === Identifier) {
          ClassName = 'active';
        }
        return <Button key={Identifier} className={ClassName} onClick={() => SetFilter(Identifier)}>{Translate(Label)}</Button>;
      })}
    </div>
    {Error && <div className="error-box" role="alert">{Error}</div>}
    <div className="audio-routing"><div><Icon name="Mic" size={23}/><strong>{Translate('Голос')}</strong></div>
      <span className="route-line">attack {Sidechain.attack} {Translate('с →')}</span>
      <div><Icon name="AudioLines" size={23}/><strong>{Translate('Музыка −')}{Sidechain.reductionDb} dB</strong></div>
      <span className="route-line">release {Sidechain.release} {Translate('с →')}</span>
      <div><Icon name="Volume2" size={23}/><strong>{Translate('Исходная громкость')}</strong></div>
      <span className={SidechainClass}>{SidechainLabel}</span></div>
    <details className="sidechain-settings"><summary>{Translate('Сайдчейн прослушивания')}</summary><div className="sidechain-fields">
      {[['attack','Атака, с',0,10,.01],['release','Восстановление, с',0,10,.01],['reductionDb','Снижение громкости, dB',0,60,1]].map(([Identifier, Label, Minimum, Maximum, Step]) =>
        <Field key={Identifier} label={Translate(Label)}><input type="number" min={Minimum} max={Maximum} step={Step} value={Sidechain[Identifier]}
          onChange={Event => SetSidechain(NormalizeSidechain({...Sidechain,[Identifier]:Number(Event.target.value)}))}/></Field>)}
    </div></details>
    {!Sounds.length && <div className="hint-box" role="status"><Icon name="Info"/><span>{Translate('В проекте пока нет WAV-файлов. Импортируйте звук, чтобы увидеть его здесь.')}</span></div>}
    {Sounds.length > 0 && !Visible.length && <p role="status">{Translate('Нет звуков выбранного типа.')}</p>}
    <div className="sound-grid">{Visible.map((Asset, Index) => {
      const Kind = Settings[Asset.id]?.kind || Asset.kind;
      const KindInfo = SoundKinds[Kind] || SoundKinds.sound;
      let Volume = 1;
      if (Kind === 'music') {
        Volume = .42;
      }
      const Configuration = {volume:Volume,loop:Kind === 'music',...Settings[Asset.id]};
      const Track = SoundDesk.get(Asset.id);
      const Active = Boolean(StartupAudioPath && StartupAudioPath.replaceAll('\\','/').toLowerCase() === Asset.relativePath.replaceAll('\\','/').toLowerCase());
      let PlaybackIcon = 'Play';
      let PlaybackLabel = Translate('Слушать');
      let StartupIcon = 'Plus';
      let StartupLabel = Translate('Сделать стартовым');
      let FadeIn = Configuration.fadeIn;
      let FadeOut = Configuration.fadeOut || 0;
      let ActionType = 'sound';
      if (Kind === 'music') {
        ActionType = 'music';
      }
      if (Configuration.fadeInEnabled === false) {
        FadeIn = 0;
      }
      if (Configuration.fadeOutEnabled === false) {
        FadeOut = 0;
      }
      if (Track?.status === 'playing') {
        PlaybackIcon = 'Pause';
        PlaybackLabel = Translate('Пауза');
      } else if (Track?.status === 'paused') {
        PlaybackLabel = Translate('Продолжить');
      }
      if (Active) {
        StartupIcon = 'Check';
        StartupLabel = Translate('Стартовый звук');
      }
      const Preview = {...Asset,...Settings[Asset.id],kind:Kind};
      return <article className="sound-card" key={Asset.id}>
        <div className="between"><span className={'type-tag ' + KindInfo.className}><Icon name={KindInfo.icon}/>{Translate(KindInfo.label)}</span><span className="micro">{String(Index + 1).padStart(2,'0')}</span></div>
        <h2>{Asset.name}</h2><p>{Asset.caption}</p>
        <Field label={Translate('Тип звука')}><select aria-label={Translate('Тип звука') + ' ' + Asset.name} value={Kind} onChange={Event => Update(Asset.id,{kind:Event.target.value})}>
          {Object.entries(SoundKinds).map(([Identifier, Description]) => <option key={Identifier} value={Identifier}>{Translate(Description.label)}</option>)}
        </select></Field>
        <Waveform key={Asset.url} asset={Preview}/>
        <AudioTimingEditor asset={Preview} onChange={Patch => Update(Asset.id,Patch)}/>
        <div className="sound-controls"><Button className="primary" icon={PlaybackIcon} disabled={Disabled} onClick={() => {
          if (Track?.status === 'playing') {
            SoundDesk.pause(Asset.id);
          } else if (Track?.status === 'paused') {
            SoundDesk.resume(Asset.id);
          } else {
            SoundDesk.play(Asset.id,{...Configuration,key:Asset.id,duck:Configuration.duck !== false,fadeIn:FadeIn,fadeOut:FadeOut});
          }
        }}>{PlaybackLabel}</Button>
          <Button icon="Square" title={Translate('Стоп')} onClick={() => SoundDesk.stop(Asset.id,{endMode:Configuration.endMode || 'immediate',fadeOut:FadeOut})}/>
          <Field label={Translate('Громкость {0}%', [Math.round(Configuration.volume * 100)])}><input type="range" min="0" max="1" step=".01" value={Configuration.volume} onChange={Event => {
            const UpdatedVolume = Number(Event.target.value);
            Update(Asset.id,{volume:UpdatedVolume});
            SoundDesk.setVolume(Asset.id,UpdatedVolume);
          }}/></Field></div>
        <div className="audio-options"><label><input type="checkbox" checked={Configuration.loop} onChange={Event => {
          Update(Asset.id,{loop:Event.target.checked});
          if (Track) {
            Track.audio.loop = Event.target.checked;
          }
        }}/> {Translate('Повторять')}</label></div>
        <AudioActionControls action={{...Configuration,type:ActionType}} onChange={Patch => Update(Asset.id,Patch)}/>
        {Track?.error && <div className="error-box" role="alert">{Track.error}</div>}
        <div className="sound-footer"><span title={Asset.file}>{Asset.file}</span><Button icon={StartupIcon} disabled={Disabled || Busy || Asset.builtin || Active} onClick={() => Run(() => SetStartup(Asset.id))}>{StartupLabel}</Button></div>
      </article>;
    })}</div>
    {StartupAudioPath && <div className="hint-box"><Icon name="Info"/><span>{Translate('Стартовый звук сохраняется в файле проекта и воспроизводится при запуске истории.')}</span><Button disabled={Disabled || Busy} onClick={() => Run(() => SetStartup(null))}>{Translate('Убрать стартовый звук')}</Button></div>}
  </div>;
}
