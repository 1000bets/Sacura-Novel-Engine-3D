import React, {useEffect, useId, useRef, useState} from 'react';
import {EvaluateNumericExpression} from './numericExpression.js';

export default function NumericInput({value:Value, label:Label, step:Step = 0.1, disabled:Disabled, onCommit:Commit, onBegin:Begin, onPreview:Preview, onCancel:Cancel}) {
  const [Text, SetText] = useState(String(Value));
  const [ErrorText, SetErrorText] = useState('');
  const ErrorId = useId();
  const Editing = useRef(false);
  const SkipBlur = useRef(false);
  const DragCleanup = useRef(null);
  const CommitSequence = useRef(0);
  const Current = useRef({Value, Commit, Begin, Preview, Cancel});
  Current.current = {Value, Commit, Begin, Preview, Cancel};
  useEffect(() => {
    if (!Editing.current && !DragCleanup.current) {
      SetText(String(Value));
    }
  }, [Value]);
  useEffect(() => () => DragCleanup.current?.(true), []);
  useEffect(() => {
    if (Disabled) {
      DragCleanup.current?.(true);
    }
  }, [Disabled]);
  async function Apply(Next, IsDrag) {
    const Sequence = ++CommitSequence.current;
    try {
      await Current.current.Commit(Next, IsDrag);
      if (Sequence === CommitSequence.current && !Editing.current) {
        SetText(String(Next));
        SetErrorText('');
      }
    } catch (Failure) {
      if (Sequence === CommitSequence.current && !Editing.current) {
        SetErrorText(Failure.message);
        SetText(String(Current.current.Value));
      }
    }
  }
  function PointerDown(Event) {
    if (Event.button !== 0 || Disabled || Editing.current) {
      return;
    }
    Event.preventDefault();
    const Input = Event.currentTarget;
    const Owner = Input.ownerDocument.defaultView;
    const PointerId = Event.pointerId;
    const Initial = Number(Value);
    let LastHorizontal = Event.screenX;
    let Next = Initial;
    let Started = false;
    let Finished = false;
    function Cleanup(Abort = false) {
      if (Finished) {
        return;
      }
      Finished = true;
      Input.removeEventListener('pointermove', Move);
      Input.removeEventListener('pointerup', Up);
      Input.removeEventListener('pointercancel', AbortDrag);
      Input.removeEventListener('lostpointercapture', AbortDrag);
      Owner.document.removeEventListener('keydown', Key, true);
      Owner.removeEventListener('blur', AbortDrag);
      if (Input.hasPointerCapture(PointerId)) {
        Input.releasePointerCapture(PointerId);
      }
      Input.style.cursor = '';
      DragCleanup.current = null;
      if (Abort && Started) {
        SetText(String(Initial));
        Promise.resolve(Current.current.Cancel?.()).catch(Failure => SetErrorText(Failure.message));
      }
    }
    function AbortDrag() {
      Cleanup(true);
    }
    function Move(Pointer) {
      if (Pointer.pointerId !== PointerId) {
        return;
      }
      const Movement = Pointer.screenX - LastHorizontal;
      if (!Started && Math.abs(Movement) < 4) {
        return;
      }
      if (!Started) {
        Started = true;
        SetErrorText('');
        Current.current.Begin?.();
      }
      let Multiplier = 1;
      if (Pointer.shiftKey) {
        Multiplier = 0.1;
      } else if (Pointer.ctrlKey) {
        Multiplier = 10;
      }
      LastHorizontal = Pointer.screenX;
      Next += Movement * Step * Multiplier;
      Next = Number(Next.toPrecision(12));
      SetText(String(Next));
      Current.current.Preview?.(Next);
    }
    function Up(Pointer) {
      if (Pointer.pointerId !== PointerId) {
        return;
      }
      Cleanup();
      if (Started) {
        void Apply(Next, true);
      } else {
        Input.focus();
        Input.select();
      }
    }
    function Key(Keyboard) {
      if (Keyboard.key === 'Escape') {
        Keyboard.preventDefault();
        Keyboard.stopPropagation();
        Cleanup(true);
      }
    }
    Input.setPointerCapture(PointerId);
    Input.style.cursor = 'ew-resize';
    Input.addEventListener('pointermove', Move);
    Input.addEventListener('pointerup', Up);
    Input.addEventListener('pointercancel', AbortDrag);
    Input.addEventListener('lostpointercapture', AbortDrag);
    Owner.document.addEventListener('keydown', Key, true);
    Owner.addEventListener('blur', AbortDrag);
    DragCleanup.current = Cleanup;
  }
  return <span className="numeric-input-control">
    <input type="text" role="spinbutton" aria-label={Label} aria-valuenow={Number(Value)} aria-invalid={Boolean(ErrorText)} aria-describedby={ErrorText ? ErrorId : undefined} disabled={Disabled} value={Text} spellCheck={false}
      title="Тяните мышью для изменения. Shift — точнее, Ctrl — быстрее. Можно вводить выражения: (2 + 3) / 10."
      onPointerDown={PointerDown} onFocus={() => {Editing.current = true;}}
      onChange={Event => {SetText(Event.target.value);SetErrorText('');}}
      onBlur={() => {
        Editing.current = false;
        if (SkipBlur.current) {
          SkipBlur.current = false;
          return;
        }
        try {
          const Next = EvaluateNumericExpression(Text, Value);
          if (Next !== Number(Value)) {
            void Apply(Next, false);
          } else {
            SetText(String(Value));
          }
        } catch (Failure) {
          SetErrorText(Failure.message);
        }
      }} onKeyDown={Event => {
        if (Event.key === 'Enter') {
          Event.preventDefault();
          Event.currentTarget.blur();
        } else if (Event.key === 'Escape') {
          Event.preventDefault();
          Event.stopPropagation();
          SkipBlur.current = true;
          SetText(String(Value));
          SetErrorText('');
          Event.currentTarget.blur();
        }
      }}/>
    {ErrorText && <span id={ErrorId} className="numeric-input-error" role="alert">{ErrorText}</span>}
  </span>;
}
