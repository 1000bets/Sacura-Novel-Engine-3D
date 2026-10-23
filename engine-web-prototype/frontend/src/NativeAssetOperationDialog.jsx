import React, {useEffect, useRef, useState} from 'react';
import {createPortal} from 'react-dom';

export default function NativeAssetOperationDialog({operation: Operation, submit: Submit, close: Close}) {
  const [Value, SetValue] = useState(Operation.value || '');
  const [Busy, SetBusy] = useState(false);
  const [ErrorMessage, SetErrorMessage] = useState('');
  const Input = useRef(null);
  useEffect(() => {Input.current?.focus();Input.current?.select();}, []);
  async function Apply(Event) {
    Event.preventDefault();
    SetBusy(true);
    SetErrorMessage('');
    try {
      await Submit(Operation, Value.trim());
      Close();
    } catch (Failure) {
      SetErrorMessage(Failure.message);
    } finally {
      SetBusy(false);
    }
  }
  return createPortal(<div className="asset-create-backdrop" onPointerDown={Event => {
    if (!Busy && Event.target === Event.currentTarget) {
      Close();
    }
  }}><form className="asset-create-dialog" role="dialog" aria-modal="true" aria-label={Operation.title} onSubmit={Apply} onContextMenu={Event => Event.stopPropagation()} onKeyDown={Event => {
    Event.stopPropagation();
    if (Event.key === 'Escape' && !Busy) {
      Event.stopPropagation();
      Close();
    }
  }}>
    <strong>{Operation.title}</strong><small>{Operation.path}</small>
    {Operation.deleting && <p>Удалить выбранный элемент? Ссылки на удалённые ассеты перестанут работать.</p>}
    {!Operation.deleting && <label>Имя<input ref={Input} aria-label="Имя" required disabled={Busy} value={Value} onChange={Event => SetValue(Event.target.value)}/></label>}
    {ErrorMessage && <p role="alert">{ErrorMessage}</p>}
    <footer><button type="button" disabled={Busy} onClick={Close}>Отмена</button><button type="submit" disabled={Busy || !Operation.deleting && !Value.trim()}>{Operation.button}</button></footer>
  </form></div>, document.body);
}
