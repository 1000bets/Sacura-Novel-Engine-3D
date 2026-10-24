import React, {useEffect, useState} from 'react';
import AnchoredPopover from './AnchoredPopover.jsx';
import {Icon} from './StudioParts.jsx';
import NativeViewport from './NativeViewport.jsx';

export function NativePrefabControls({ObjectId, Disabled, Command}) {
  const [Anchor, SetAnchor] = useState(null);
  const [State, SetState] = useState(null);
  const [Selected, SetSelected] = useState([]);
  const [Path, SetPath] = useState('Prefabs/NewPrefab.prefab');
  const Refresh = async () => { SetState(await Command('prefab.inspect', {objectId: ObjectId})); SetSelected([]); };
  useEffect(() => { SetState(null); if (Anchor && ObjectId && !Disabled) { Refresh(); } }, [ObjectId, Disabled, Anchor]);
  const Modify = async Operation => {
    await Command('prefab.modify', {objectId: ObjectId, operation: Operation, selection: Selected});
    await Refresh();
  };
  return <>
    <button disabled={Disabled} title="Prefab" aria-label="Prefab" aria-expanded={!!Anchor} onClick={Event => SetAnchor(Event.currentTarget)}><Icon name="Package" size={13}/></button>
    {Anchor && <AnchoredPopover anchor={Anchor} onClose={() => SetAnchor(null)} width={320} label="Prefab" className="hierarchy-prefab-popover">
    <strong>Prefab</strong>
    <fieldset disabled={Disabled}>
      <label>New asset path<input value={Path} onChange={Event => SetPath(Event.target.value)}/></label>
      <button onClick={() => Command('prefab.create', {objectId: ObjectId, path: Path}).then(Refresh)}>Create prefab</button>
      {State?.instance && <>
        <button onClick={() => Command('prefab.create', {objectId: ObjectId, path: Path, variant: true}).then(Refresh)}>Create variant</button>
        <button onClick={Refresh}>Refresh overrides</button>
        {(State.overrides || []).map(Override => <label key={Override.index}><input type="checkbox" checked={Selected.includes(Override.index)} onChange={Event => {
          if (Event.target.checked) { SetSelected([...Selected, Override.index]); }
          else { SetSelected(Selected.filter(Index => Index !== Override.index)); }
        }}/>{Override.label || Override.property}</label>)}
        <p>An empty selection applies to all overrides. Apply writes the shared asset and clears this scene's Undo history.</p>
        <button onClick={() => Modify('apply')}>Apply overrides</button><button onClick={() => Modify('revert')}>Revert overrides</button>
        <button onClick={() => Modify('unpack')}>Unpack root</button><button onClick={() => Modify('unpackAll')}>Unpack completely</button>
      </>}
    </fieldset>
  </AnchoredPopover>}
  </>;
}

function JsonField({Label, Value, Disabled, Commit}) {
  const [Draft, SetDraft] = useState(JSON.stringify(Value));
  const [Error, SetError] = useState('');
  useEffect(() => { SetDraft(JSON.stringify(Value)); }, [JSON.stringify(Value)]);
  if (typeof Value === 'boolean') {
    return <label>{Label}<input type="checkbox" disabled={Disabled} checked={Value} onChange={Event => Commit(Event.target.checked)}/></label>;
  }
  return <label style={{display: 'grid', gap: 4}}>{Label}<input style={{width: '100%', minWidth: 0}} disabled={Disabled} value={Draft} onChange={Event => SetDraft(Event.target.value)} onBlur={() => {
    try { Commit(JSON.parse(Draft)); SetError(''); } catch (Failure) { SetError(Failure.message); }
  }}/>{Error && <span role="alert">{Error}</span>}</label>;
}

export function NativePrefabWorkspace({Snapshot, Execute, Desktop, Assets, Error}) {
  const [Prefab, SetPrefab] = useState('');
  const [Component, SetComponent] = useState('');
  const [Tool, SetTool] = useState('translate');
  const Objects = Snapshot.document || [];
  const Selected = Objects.find(Object => Object.id === Snapshot.selection);
  const Command = (Method, Params) => Execute('command', {method: Method, params: Params});
  return <main className="native-prefab-workspace" data-document-tab={Desktop.documentId} style={{display: 'flex', flexDirection: 'column', height: '100%', minHeight: 0}}>
    <header className="native-document-toolbar"><strong>{Snapshot.name}</strong><span>{Snapshot.dirty && '*'}</span>
      <button disabled={Snapshot.readOnly} onClick={() => Execute('save')}>Save</button>
      <button disabled={!Snapshot.canUndo} onClick={() => Execute('undo')}>Undo</button><button disabled={!Snapshot.canRedo} onClick={() => Execute('redo')}>Redo</button>
      <select aria-label="Transform tool" value={Tool} onChange={Event => SetTool(Event.target.value)}><option value="translate">Move</option><option value="rotate">Rotate</option><option value="scale">Scale</option></select>
    </header>
    <div style={{display: 'grid', gridTemplateColumns: 'minmax(180px, 25%) minmax(160px, 1fr) minmax(160px, 22%)', flex: 1, minHeight: 0}}>
      <aside aria-label="Prefab Inspector" style={{overflow: 'auto', padding: 12}}>
        {Selected && <fieldset disabled={Snapshot.readOnly} style={{border: 0, padding: 0, margin: 0, minWidth: 0, display: 'grid', gap: 8}}>
          <label style={{display: 'grid', gap: 4}}>Name<input key={Selected.id + Selected.name} defaultValue={Selected.name} onBlur={Event => Command('object.rename', {objectId: Selected.id, name: Event.target.value})}/></label>
          <label style={{display: 'grid', gap: 4}}>Parent<select value={Selected.parentId || ''} onChange={Event => Command('object.setParent', {objectId: Selected.id, parentId: Event.target.value || null})}><option value="">Root</option>{Objects.filter(Object => Object.id !== Selected.id).map(Object => <option key={Object.id} value={Object.id}>{Object.name}</option>)}</select></label>
          {['position', 'rotation', 'scale'].map(Field => <JsonField key={Selected.id + Field} Label={Field} Value={Selected.transform[Field]} Commit={Value => Command('transform.set', {objectId: Selected.id, transform: {...Selected.transform, [Field]: Value}})}/>)}
          {(Selected.components || []).map(Item => <details key={Item.id}><summary>{Item.displayName || Item.typeId}</summary>
            {Item.properties.map(Property => <JsonField key={Property.id} Label={Property.label || Property.id} Value={Property.value} Disabled={!Property.editable} Commit={Value => Command('component.setProperty', {objectId: Selected.id, componentId: Item.id, property: Property.id, value: Value})}/>)}
            <button onClick={() => Command('object.removeComponent', {objectId: Selected.id, componentId: Item.id})}>Remove component</button>
          </details>)}
          <label>Component type<input value={Component} onChange={Event => SetComponent(Event.target.value)}/></label><button disabled={!Component} onClick={() => Command('object.addComponent', {objectId: Selected.id, typeId: Component})}>Add component</button>
          <button onClick={() => Command('object.destroy', {objectId: Selected.id})}>Delete object</button>
        </fieldset>}
        {Error && <p role="alert">{Error}</p>}
      </aside>
      <section style={{position: 'relative', minHeight: 0}}><NativeViewport desktop={Desktop} editing={!Snapshot.readOnly} blocked={Snapshot.readOnly} editTool={Tool} showGrid/></section>
      <aside aria-label="Prefab Hierarchy" style={{overflow: 'auto', padding: 12}}>
        <button disabled={Snapshot.readOnly} onClick={() => Command('object.create', {name: 'Object', parentId: Snapshot.selection || null})}>Create object</button>
        {Objects.map(Object => <button key={Object.id} style={{display: 'block', width: '100%'}} aria-pressed={Selected?.id === Object.id} onClick={() => Command('selection.set', {objectId: Object.id})}>{Object.name}</button>)}
        <select aria-label="Nested prefab" value={Prefab} onChange={Event => SetPrefab(Event.target.value)}><option value="">Choose prefab</option>{Assets.filter(Asset => Asset.typeId === 'Prefab').map(Asset => <option key={Asset.id} value={Asset.id}>{Asset.path}</option>)}</select>
        <button disabled={Snapshot.readOnly || !Prefab} onClick={() => Command('prefab.instantiate', {assetId: Prefab, parentId: Snapshot.selection || null})}>Add nested prefab</button>
      </aside>
    </div>
  </main>;
}
