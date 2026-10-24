import React, {useState} from 'react';
import {ReflectedValueField} from './ReflectedFields.jsx';
import {LocalizeEditorText, useLocale} from './i18n.jsx';
import {Icon} from './StudioParts.jsx';

const Text = (Name, Fallback) => LocalizeEditorText('Editor.Properties.' + Name, Fallback);

function ApplyLocalChange(Value, Change) {
  const Result = structuredClone(Value);
  let Target = Result;
  for (const Segment of Change.path.slice(0, -1)) {
    Target = Target[Segment];
  }
  const Key = Change.path.at(-1);
  if (Change.operation === 'set') {
    Target[Key] = Change.value;
  } else if (Change.operation === 'insert') {
    Target[Key].push(Change.value);
  } else if (Change.operation === 'remove') {
    Target[Key].splice(Change.index, 1);
  } else if (Change.operation === 'move') {
    const [Element] = Target[Key].splice(Change.index, 1);
    Target[Key].splice(Change.destination, 0, Element);
  }
  return Result;
}

function ReflectedCollection({Schema, Value, Path, Disabled, Change, Label}) {
  const [Draft, SetDraft] = useState(null);
  const [Adding, SetAdding] = useState(false);
  return <details className="reflected-property-group" open>
    <summary>{Label} ({Value.length})</summary>
    {Value.map((Element, Index) => <section className="reflected-collection-element" key={Index}>
      <header><span>{Index + 1}</span>{!Schema.fixedSize && <>
        <button type="button" title={Text('MoveUp', 'Move up')} aria-label={Text('MoveUp', 'Move up')} disabled={Disabled || Index === 0} onClick={() => Change({path:Path, operation:'move', index:Index, destination:Index - 1})}><Icon name="ArrowUp" size={14}/></button>
        <button type="button" title={Text('MoveDown', 'Move down')} aria-label={Text('MoveDown', 'Move down')} disabled={Disabled || Index === Value.length - 1} onClick={() => Change({path:Path, operation:'move', index:Index, destination:Index + 1})}><Icon name="ArrowDown" size={14}/></button>
        <button type="button" title={Text('Remove', 'Remove element')} aria-label={Text('Remove', 'Remove element')} disabled={Disabled} onClick={() => Change({path:Path, operation:'remove', index:Index})}><Icon name="Trash2" size={14}/></button>
      </>}</header>
      <ReflectedPropertyTree Schema={{...Schema.element, label:String(Index + 1)}} Value={Element} Path={[...Path, Index]} Disabled={Disabled} Change={Change}/>
    </section>)}
    {!Schema.fixedSize && !Adding && Object.hasOwn(Schema, 'elementDefault') && <button disabled={Disabled} onClick={() => {SetDraft(structuredClone(Schema.elementDefault)); SetAdding(true);}}><Icon name="Plus" size={14}/>{Text('Add', 'Add element')}</button>}
    {Adding && <section className="reflected-collection-element">
      <strong>{Text('NewElement', 'New element')}</strong>
      <ReflectedPropertyTree Schema={Schema.element} Value={Draft} Path={['element']} Disabled={Disabled} Change={ChangeValue => SetDraft(Current => ApplyLocalChange({element:Current}, ChangeValue).element)}/>
      <div className="reflected-collection-actions"><button disabled={Disabled} onClick={async () => {
        const Result = await Change({path:Path, operation:'insert', value:Draft});
        if (Result) {
          SetAdding(false);
        }
      }}>{Text('Apply', 'Add')}</button><button onClick={() => SetAdding(false)}>{Text('Cancel', 'Cancel')}</button></div>
    </section>}
  </details>;
}

export default function ReflectedPropertyTree({Schema, Value, Path = [], Disabled = false, Change}) {
  useLocale();
  const Label = LocalizeEditorText(Schema.localizationKey, Schema.label || Schema.typeId);
  const ReadOnly = Disabled || Schema.editable === false || !!Schema.error;
  if (Schema.error) {
    return <p role="status">{Label}: {Schema.error}</p>;
  }
  if (Schema.kind === 'struct' && Schema.typeId !== 'engine.Color') {
    const Fields = <>{Schema.fields.map(Field => <ReflectedPropertyTree key={Field.id} Schema={Field} Value={Value?.[Field.id]} Path={[...Path, Field.id]} Disabled={ReadOnly} Change={Change}/>)}</>;
    if (!Path.length) {
      return Fields;
    }
    return <details className="reflected-property-group" open><summary title={Schema.description}>{Label}</summary>{Fields}</details>;
  }
  if (Schema.kind === 'array' && Array.isArray(Value)) {
    return <ReflectedCollection Schema={Schema} Value={Value} Path={Path} Disabled={ReadOnly} Change={Change} Label={Label}/>;
  }
  const Commit = Next => Change({path:Path, operation:'set', value:Next});
  const Assign = Identity => {
    if (Schema.typeId === 'engine.string') {
      Commit(Identity.assetId || '');
    } else {
      Commit(Identity);
    }
  };
  return <div data-reflected-path={JSON.stringify(Path)}><ReflectedValueField property={{...Schema, label:Label, value:Value, editable:!ReadOnly}} disabled={ReadOnly} onChange={Commit} onAsset={Assign}/></div>;
}
