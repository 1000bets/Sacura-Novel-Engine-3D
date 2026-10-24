import React, {useEffect, useState} from 'react';
import {Handle, Position} from '@xyflow/react';

export function NativeNarrativeNode({data, selected}) {
  const Definition = data.narrative;
  const Node = data.beat;
  return <div className={'moment-node native-narrative-node' + (selected ? ' selected' : '')} style={{width: 360, padding: 16, background: 'var(--panel)', border: '1px solid var(--border)', borderRadius: 8}}>
    <strong className="moment-grab">{Definition?.label || Node.narrativeType}</strong>
    {!Definition && <p role="alert">Narrative type is unavailable. Reopen the project after restoring its Python script.</p>}
    {!Definition?.pure && <div style={{position: 'relative', padding: 8}}><Handle type="target" position={Position.Left} id="in"/>Execute</div>}
    {(Definition?.inputs || []).map(Pin => <div key={Pin.name} className="native-node-fields nodrag nopan nowheel" onClick={Event=>Event.stopPropagation()} onDoubleClick={Event=>Event.stopPropagation()} onKeyDown={Event=>Event.stopPropagation()} style={{position: 'relative', padding: 8}}>
      <Handle type="target" position={Position.Left} id={'data:' + Pin.name}/><label>{Pin.name} <small>{Pin.type}</small></label>
      {Node.sources?.[Pin.name] && <small>{Node.sources[Pin.name].join(' / ')}</small>}
      <NarrativeLiteral Pin={Pin} Value={Node.inputs?.[Pin.name]} Disabled={data.running} Commit={Value=>data.NativeCommand('story.setNarrativeInput',{nodeId:Node.id,input:Pin.name,value:Value})}/>
    </div>)}
    {(Definition?.outputs || []).map(Pin => <div key={Pin.name} style={{position: 'relative', padding: 8, textAlign: 'right'}}>
      {Pin.name} <small>{Pin.type}</small><Handle type="source" position={Position.Right} id={'data:' + Pin.name}/>
    </div>)}
    {(Node.choices || []).map(Exit => <div key={Exit.id} style={{position: 'relative', padding: 8, textAlign: 'right'}}>
      {Exit.label || Exit.id}<Handle type="source" position={Position.Right} id={'choice:' + Exit.id}/>
    </div>)}
  </div>;
}

function NarrativeLiteral({Pin, Value, Disabled, Commit}) {
  const [Draft, SetDraft] = useState('');
  const [Error, SetError] = useState('');
  useEffect(() => {
    SetDraft(JSON.stringify(Value ?? Pin.default ?? null));
    SetError('');
  }, [Value, Pin.default]);
  const Save = async () => {
    if (Disabled || Draft===JSON.stringify(Value ?? Pin.default ?? null)) {
      return;
    }
    try {
      const Parsed = JSON.parse(Draft);
      const Result = await Commit(Parsed);
      if (Result===null) {
        SetError('Value was rejected. Check the editor notification.');
        return;
      }
      SetError('');
    } catch (Failure) {
      SetError(Failure.message);
    }
  };
  if (Pin.type === 'engine.bool') {
    return <input type="checkbox" aria-label={Pin.name} disabled={Disabled} checked={Boolean(Value ?? Pin.default)} onChange={Event => Commit(Event.target.checked)}/>;
  }
  return <><textarea aria-label={Pin.name + ' literal JSON'} disabled={Disabled} value={Draft} onChange={Event => SetDraft(Event.target.value)} onBlur={Save} onKeyDown={Event=>{
    if (Event.key==='Escape') {
      Event.preventDefault();
      SetDraft(JSON.stringify(Value ?? Pin.default ?? null));
      SetError('');
    }
    if (Event.key==='Enter' && (Event.ctrlKey || Event.metaKey)) {
      Event.preventDefault();
      Event.currentTarget.blur();
    }
  }}/>{Error && <p role="alert">{Error}</p>}</>;
}

export function NativeNarrativeInspector({Node, Story, Disabled, Command}) {
  const Definition = Story.narrativeCatalog?.find(Entry => Entry.type === Node.narrativeType);
  const Nodes = Story.document.nodes;
  return <section className="inspector-primary-fields">
    <h3>{Definition?.label || Node.narrativeType}</h3>
    {!Definition && <p role="alert">The Python Narrative definition is unavailable.</p>}
    {(Definition?.inputs || []).map(Pin => {
      const Source = Node.sources?.[Pin.name];
      const Options = Nodes.flatMap(Candidate => {
        const CandidateDefinition = Story.narrativeCatalog.find(Entry => Entry.type === Candidate.narrativeType);
        return (CandidateDefinition?.outputs || []).filter(Output => Output.type === Pin.type).map(Output => ({
          Value: JSON.stringify([Candidate.id, Output.name]), Label: (CandidateDefinition.label || Candidate.narrativeType) + ': ' + Output.name + ' (' + Candidate.id + ')'
        }));
      });
      return <div key={Pin.name}>
        <label>{Pin.name} <small>{Pin.type}</small></label>
        <select aria-label={Pin.name + ' source'} disabled={Disabled} value={Source ? JSON.stringify(Source) : ''} onChange={Event => {
          let Connection = ['', ''];
          if (Event.target.value) { Connection = JSON.parse(Event.target.value); }
          Command('story.connectData', {sourceId: Connection[0], output: Connection[1], targetId: Node.id, input: Pin.name});
        }}><option value="">Literal</option>{Options.map(Option => <option key={Option.Value} value={Option.Value}>{Option.Label}</option>)}</select>
        <NarrativeLiteral Pin={Pin} Value={Node.inputs?.[Pin.name]} Disabled={Disabled} Commit={Value => Command('story.setNarrativeInput', {nodeId: Node.id, input: Pin.name, value: Value})}/>
      </div>;
    })}
    {(Node.choices || []).map((Exit, Index) => <label key={Exit.id}>{Exit.label || Exit.id}
      <select aria-label={'Exit ' + Exit.id} disabled={Disabled} value={Exit.nextNodeId || ''} onChange={Event => Command('story.connect', {sourceId: Node.id, port: Index, targetId: Event.target.value || null})}>
        <option value="">Disconnected</option>{Nodes.map(Candidate => <option key={Candidate.id} value={Candidate.id}>{Candidate.narrativeType || Candidate.text || Candidate.kind} ({Candidate.id})</option>)}
      </select>
    </label>)}
    <button disabled={Disabled} onClick={() => Command('story.validate')}>Validate story</button>
  </section>;
}
