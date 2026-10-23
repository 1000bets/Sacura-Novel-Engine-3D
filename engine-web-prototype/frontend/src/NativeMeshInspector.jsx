import React, {useContext, useEffect, useState} from 'react';
import {AssetReferenceField, NativeAssetsContext} from './NativeAssets.jsx';

export default function NativeMeshInspector({component: Component, objectId: ObjectId, command: Command, revision: Revision, disabled: Disabled}) {
  const Context = useContext(NativeAssetsContext);
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
  const Mesh = Component.properties.find(Property => Property.id === 'mesh_asset_id');
  const Assign = (Slot, Identity) => Command('mesh.material', {objectId: ObjectId, componentId: Component.id, slot: Slot, assetId: Identity.assetId});
  const Physics = (Enabled, Simulate) => Command('mesh.physics', {objectId: ObjectId, componentId: Component.id, enabled: Enabled, simulate: Simulate});
  return <div className="native-mesh-section">
    <h4>Статический меш</h4>
    {Mesh && <button disabled={!Mesh.value} onClick={() => Context.open({assetId: Mesh.value, subAssetId: Mesh.subAssetId})}>Открыть меш</button>}
    <h4>Материалы</h4>
    {Snapshot?.slots.map(Slot => <div key={Slot.slot} className="native-mesh-slot"><span>Слот {Slot.slot}</span><AssetReferenceField label={'Материал слота ' + Slot.slot} assetId={Slot.assetId} allowedTypes={['Material', 'MaterialInstance']} disabled={Disabled} onChange={Identity => Assign(Slot.slot, Identity)}/><button disabled={Disabled || Slot.inherited} onClick={() => Assign(Slot.slot, {assetId: null})}>Из меша</button></div>)}
    <details><summary>Назначить всем слотам</summary><AssetReferenceField label="Материал всех слотов" allowedTypes={['Material', 'MaterialInstance']} disabled={Disabled} onChange={Identity => Assign(-1, Identity)}/><button disabled={Disabled} onClick={() => Assign(-1, {assetId: null})}>Вернуть материалы меша</button></details>
    {!Snapshot?.slots.length && <p>Материальные слоты появятся после загрузки меша.</p>}
    <h4>Физика объекта</h4>
    {Snapshot && <><label><input type="checkbox" disabled={Disabled} checked={Snapshot.collision} onChange={Event => Physics(Event.target.checked, Snapshot.simulate)}/>Коллизия</label><label><input type="checkbox" disabled={Disabled} checked={Snapshot.simulate} onChange={Event => Physics(Snapshot.collision, Event.target.checked)}/>Симулировать физику</label></>}
  </div>;
}
