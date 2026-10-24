import React, {useEffect, useMemo, useState} from 'react';
import {Button, Icon} from './StudioParts.jsx';
import {t as Translate, useLocale} from './i18n.jsx';
import './nativeScriptsWorkspace.css';

export default function NativeScriptsWorkspace({Assets, Adapter, Session, OpenAsset}) {
  useLocale();
  const Scripts = useMemo(() => Assets.filter(Asset => Asset.typeId === 'PythonScript').sort((First, Second) => First.path.localeCompare(Second.path)), [Assets]);
  const [Snapshot, SetSnapshot] = useState({Files:[], Errors:[], bLoading:true});
  const [Refresh, SetRefresh] = useState(0);
  const [Query, SetQuery] = useState('');
  useEffect(() => {
    let bCancelled = false;
    SetSnapshot({Files:[], Errors:[], bLoading:true});
    async function Load() {
      const Results = await Promise.allSettled(Scripts.map(Script => Adapter.command('assetDocument.execute', {assetId:Script.id, operation:'open'})));
      if (bCancelled) {
        return;
      }
      const Files = [];
      const Errors = [];
      for (const [Index, Result] of Results.entries()) {
        if (Result.status === 'fulfilled') {
          Files.push({...Scripts[Index], Source:Result.value.source});
        } else {
          Errors.push(Scripts[Index].path + ': ' + Result.reason.message);
        }
      }
      SetSnapshot({Files, Errors, bLoading:false});
    }
    Load();
    return () => {
      bCancelled = true;
    };
  }, [Scripts, Adapter, Session, Refresh]);
  const Results = useMemo(() => {
    const Search = Query.trim().toLocaleLowerCase();
    return Snapshot.Files.map(File => {
      const Matches = [];
      if (Search) {
        for (const [Index, Line] of File.Source.split(/\r?\n/).entries()) {
          if (Line.toLocaleLowerCase().includes(Search)) {
            Matches.push({Line:Index + 1, Text:Line});
          }
        }
      }
      return {...File, Matches};
    }).filter(File => !Search || File.path.toLocaleLowerCase().includes(Search) || File.Matches.length > 0);
  }, [Snapshot.Files, Query]);
  return <section className="native-scripts-workspace" aria-label={Translate('Все скрипты проекта')}>
    <header className="native-scripts-toolbar">
      <Icon name="Search"/>
      <input type="search" aria-label={Translate('Поиск по именам и содержимому скриптов')} placeholder={Translate('Поиск по именам и содержимому скриптов')} value={Query} onChange={Event => SetQuery(Event.target.value)}/>
      <Button icon="RefreshCw" title={Translate('Обновить')} disabled={Snapshot.bLoading} onClick={() => SetRefresh(Previous => Previous + 1)}/>
    </header>
    <div className="native-scripts-results">
      {Snapshot.bLoading && <p role="status">{Translate('Загрузка скриптов…')}</p>}
      {!Snapshot.bLoading && Scripts.length === 0 && <p role="status">{Translate('Python-скриптов пока нет.')}</p>}
      {!Snapshot.bLoading && Scripts.length > 0 && Results.length === 0 && Snapshot.Errors.length === 0 && <p role="status">{Translate('Скрипты не найдены.')}</p>}
      {Snapshot.Errors.map(ErrorMessage => <p role="alert" key={ErrorMessage}>{ErrorMessage}</p>)}
      <ul>
        {Results.map(File => <li key={File.id}>
          <button className="native-script-file" onClick={() => OpenAsset({assetId:File.id})} title={File.path}>
            <Icon name="FileCode"/><span>{File.path}</span><Icon name="ArrowUpRight"/>
          </button>
          {File.Matches.length > 0 && <div className="native-script-matches">
            {File.Matches.slice(0, 3).map(Match => <button key={Match.Line} title={Match.Text} onClick={() => OpenAsset({assetId:File.id})}><span>{Match.Line}</span><code>{Match.Text}</code></button>)}
            {File.Matches.length > 3 && <small>{Translate('Совпадений')}: {File.Matches.length}</small>}
          </div>}
        </li>)}
      </ul>
    </div>
    <footer>{Translate('Файлов')}: {Results.length} / {Scripts.length}</footer>
  </section>;
}
