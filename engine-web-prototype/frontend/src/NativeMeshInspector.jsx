import {t as tr, useLocale} from './i18n.jsx';
import React, {useEffect, useState} from 'react';
import {AssetReferenceField} from './NativeAssets.jsx';

export default function NativeMeshInspector({component: Component, objectId: ObjectId, command: Command, revision: Revision, disabled: Disabled}) {
  useLocale();
  const [Snapshot, SetSnapshot] = useState(null);
  useEffect(() => {
    let Cancelled = false;
    let Timer;
    async function Refresh() {
      const Result = await Command('mesh.inspect', {objectId: ObjectId, componentId: Component.id});
      if (Cancelled) { return; }
      if (Result) { SetSnapshot(Result); }
      if (Result && Result.slots.length === 0) { Timer = setTimeout(Refresh, 500); }
    }
    Refresh();
    return () => { Cancelled = true; clearTimeout(Timer); };
  }, [ObjectId, Component.id, Revision]);
  const Assign = (Slot, Identity) => Command('mesh.material', {objectId: ObjectId, componentId: Component.id, slot: Slot, assetId: Identity.assetId});
  const Physics = (Enabled, Simulate) => Command('mesh.physics', {objectId: ObjectId, componentId: Component.id, enabled: Enabled, simulate: Simulate});
  return <div className="native-mesh-section">
    <h4>{tr("Материалы")}</h4>
    {Snapshot?.slots.map(Slot => <div key={Slot.slot} className="native-mesh-slot"><span>{tr("Слот")}{Slot.slot}</span><AssetReferenceField label={'Материал слота ' + Slot.slot} assetId={Slot.assetId} allowedTypes={['Material', 'MaterialInstance']} disabled={Disabled} onChange={Identity => Assign(Slot.slot, Identity)}/><button disabled={Disabled || Slot.inherited} onClick={() => Assign(Slot.slot, {assetId: null})}>{tr("Из меша")}</button></div>)}
    <details><summary>{tr("Назначить всем слотам")}</summary><AssetReferenceField label={tr("Материал всех слотов")} allowedTypes={['Material', 'MaterialInstance']} disabled={Disabled} onChange={Identity => Assign(-1, Identity)}/><button disabled={Disabled} onClick={() => Assign(-1, {assetId: null})}>{tr("Вернуть материалы меша")}</button></details>
    {!Snapshot?.slots.length && <p>{tr("Материальные слоты появятся после загрузки меша.")}</p>}
    <h4>{tr("Физика объекта")}</h4>
    {Snapshot && <><label><input type="checkbox" disabled={Disabled} checked={Snapshot.collision} onChange={Event => Physics(Event.target.checked, Snapshot.simulate)}/>{tr("Коллизия")}</label><label><input type="checkbox" disabled={Disabled} checked={Snapshot.simulate} onChange={Event => Physics(Snapshot.collision, Event.target.checked)}/>{tr("Симулировать физику")}</label></>}
  </div>;
}
