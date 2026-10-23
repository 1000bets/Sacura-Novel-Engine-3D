import React, {createContext, useContext, useEffect, useState} from 'react';
import {Icon} from './StudioParts.jsx';
import './nativeAssets.css';

export const NativeAssetsContext = createContext({assets: []});

export function AssetThumbnail({assetId: AssetId, subAssetId: SubAssetId, type: Type = 'Asset'}) {
  const Context = useContext(NativeAssetsContext);
  const [Source, SetSource] = useState('');
  const Asset = Context.assets.find(Entry => Entry.id === AssetId);
  useEffect(() => {
    let Cancelled = false;
    let Timer;
    let ObjectUrl;
    SetSource('');
    if (!AssetId || !Context.session) { return; }
    let SubassetPath = '';
    if (SubAssetId) { SubassetPath = '/' + encodeURIComponent(SubAssetId); }
    const Address = `sacura://native-thumbnail/${Context.session}/${encodeURIComponent(AssetId)}${SubassetPath}?v=${encodeURIComponent(Asset?.version || '')}`;
    async function Load() {
      try {
        const Response = await fetch(Address);
        if (Cancelled) { return; }
        if (Response.status === 202) { Timer = setTimeout(Load, 250); return; }
        if (!Response.ok) { return; }
        const Blob = await Response.blob();
        if (Cancelled) { return; }
        ObjectUrl = URL.createObjectURL(Blob);
        SetSource(ObjectUrl);
      } catch {
        return;
      }
    }
    Load();
    return () => { Cancelled = true; clearTimeout(Timer); if (ObjectUrl) { URL.revokeObjectURL(ObjectUrl); } };
  }, [AssetId, SubAssetId, Context.session, Asset?.version]);
  if (Source) { return <img className="native-asset-thumbnail" src={Source} alt=""/>; }
  return <span className="native-asset-thumbnail native-asset-type"><Icon name="FileBox" size={24}/><small>{Type}</small></span>;
}

export function AssetReferenceField({assetId: AssetId, subAssetId: SubAssetId, allowedTypes: AllowedTypes = [], onChange: Change, disabled: Disabled, label: Label = 'Ассет'}) {
  const Context = useContext(NativeAssetsContext);
  const [Search, SetSearch] = useState('');
  const [Menu, SetMenu] = useState(false);
  const Asset = Context.assets.find(Entry => Entry.id === AssetId);
  let Caption = 'Не назначено';
  let Type = 'Asset';
  let Available = false;
  if (Asset) {
    Caption = Asset.path;
    Type = Asset.typeId;
    Available = true;
    if (SubAssetId) {
      const Subasset = Asset.subassets?.find(Entry => Entry.id === SubAssetId);
      Available = !!Subasset;
      if (Subasset) { Caption += ' / ' + Subasset.name; Type = Subasset.typeId; }
    }
  }
  if (AssetId && !Available) { Caption = 'Недоступный ресурс'; }
  const Choices = [];
  for (const Entry of Context.assets) {
    if (AllowedTypes.length === 0 || AllowedTypes.includes(Entry.typeId)) {
      Choices.push({assetId: Entry.id, label: Entry.path, type: Entry.typeId});
    }
    for (const Subasset of Entry.subassets || []) {
      if (AllowedTypes.length === 0 || AllowedTypes.includes(Subasset.typeId)) {
        Choices.push({assetId: Entry.id, subAssetId: Subasset.id, label: Entry.path + ' / ' + Subasset.name, type: Subasset.typeId});
      }
    }
  }
  const Identity = {assetId: AssetId, subAssetId: SubAssetId};
  const Open = () => Context.open?.(Identity);
  const Browse = () => Context.reveal?.(Identity);
  const Clear = () => { Change?.({assetId: null, subAssetId: null}); SetMenu(false); };
  return <div className="native-asset-field" onContextMenu={Event => { Event.preventDefault(); SetMenu(true); }}>
    <button type="button" className="native-asset-preview-button" title="Открыть ассет" disabled={!Available} onDoubleClick={Open} onClick={Browse}><AssetThumbnail assetId={AssetId} subAssetId={SubAssetId} type={Type}/></button>
    <details className="native-asset-picker"><summary aria-label={Label} aria-disabled={Disabled}>{Caption}</summary>
      <div className="native-asset-options"><input aria-label="Поиск ассета" value={Search} onChange={Event => SetSearch(Event.target.value)}/>
        {Choices.filter(Entry => Entry.label.toLowerCase().includes(Search.toLowerCase())).map(Entry => <button type="button" key={Entry.assetId + (Entry.subAssetId || '')} disabled={Disabled} onClick={Event => { Change?.(Entry); Event.currentTarget.closest('details').open = false; }}><AssetThumbnail assetId={Entry.assetId} subAssetId={Entry.subAssetId} type={Entry.type}/><span>{Entry.label}</span></button>)}
      </div>
    </details>
    <button type="button" title="Открыть ассет" aria-label="Открыть ассет" disabled={!Available} onClick={Open}><Icon name="ExternalLink" size={15}/></button>
    <button type="button" title="Выделить в Проводнике" aria-label="Выделить в Проводнике" disabled={!Available} onClick={Browse}><Icon name="Search" size={15}/></button>
    <button type="button" title="Очистить ссылку" aria-label="Очистить ссылку" disabled={Disabled || !AssetId} onClick={Clear}><Icon name="Trash2" size={15}/></button>
    {Menu && <div className="native-asset-context" onMouseLeave={() => SetMenu(false)}><button type="button" disabled={!Available} onClick={() => { Open(); SetMenu(false); }}>Открыть ассет</button><button type="button" disabled={!Available} onClick={() => { Browse(); SetMenu(false); }}>Выделить в Проводнике</button><button type="button" disabled={Disabled || !AssetId} onClick={Clear}>Очистить ссылку</button><button type="button" onClick={() => SetMenu(false)}>Закрыть</button></div>}
  </div>;
}
