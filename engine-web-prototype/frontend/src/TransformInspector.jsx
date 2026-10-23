import {t as tr, useLocale} from './i18n.jsx';
import React, {useEffect, useRef, useState} from 'react';
import {objectTransform} from './sceneEditing.js';
import {Icon, Button} from './StudioParts.jsx';
import NumericInput from './NumericInput.jsx';

export default function TransformInspector({object, scene, onChange, onInteraction, onReset, onFocus, onDuplicate, onDelete, disabled, compact=false, spawn=false}) {
  useLocale();
  const Value = objectTransform(object, scene.id, scene.kind);
  const Serialized = JSON.stringify(Value);
  const [Draft, SetDraft] = useState(Value);
  const [ScaleLocked, SetScaleLocked] = useState(false);
  const Gesture = useRef(null);
  useEffect(() => {
    if (!Gesture.current) {
      SetDraft(JSON.parse(Serialized));
    }
  }, [Serialized, object.id, scene.id]);
  useEffect(() => () => {
    const Active = Gesture.current;
    if (Active && !Active.Ending) {
      Active.Ending = true;
      Active.Pending = null;
      void Active.Running.then(() => Active.Call?.('cancel', Active.Transaction)).catch(() => {});
    }
  }, [object.id, scene.id]);
  function UpdatedVector(Baseline, Key, Axis, NumberValue) {
    const Next = {...Baseline, [Key]:[...Baseline[Key]]};
    Next[Key][Axis] = NumberValue;
    if (Key === 'scale' && ScaleLocked) {
      const Previous = Number(Baseline.scale[Axis]);
      if (Previous === 0) {
        Next.scale = Baseline.scale.map(() => NumberValue);
      } else {
        Next.scale = Baseline.scale.map(Entry => Number(Entry) * NumberValue / Previous);
      }
    }
    return Next;
  }
  function Begin() {
    const Active = {Baseline:Draft, Transaction:crypto.randomUUID(), Call:onInteraction, Pending:null, Ending:false, Failure:null};
    Gesture.current = Active;
    Active.Running = Promise.resolve().then(() => Active.Call?.('begin', Active.Transaction)).catch(Failure => {Active.Failure = Failure;});
  }
  function Preview(Key, Axis, NumberValue) {
    const Active = Gesture.current;
    if (!Active || Active.Ending) {
      return;
    }
    const Next = UpdatedVector(Active.Baseline, Key, Axis, NumberValue);
    SetDraft(Next);
    Active.Pending = Next;
    if (Active.Pumping) {
      return;
    }
    Active.Pumping = true;
    Active.Running = Active.Running.then(async () => {
      while (Active.Pending && !Active.Ending && !Active.Failure) {
        const Pending = Active.Pending;
        Active.Pending = null;
        try {
          await Active.Call?.('update', Active.Transaction, Pending);
        } catch (Failure) {
          if (Failure.code !== 'INVALID_ARGUMENT') {
            Active.Failure = Failure;
          }
        }
      }
      Active.Pumping = false;
    });
  }
  async function Cancel() {
    const Active = Gesture.current;
    if (!Active || Active.Ending) {
      return;
    }
    Active.Ending = true;
    Active.Pending = null;
    await Active.Running;
    try {
      await Active.Call?.('cancel', Active.Transaction);
    } finally {
      Gesture.current = null;
      SetDraft(Active.Baseline);
    }
  }
  async function Commit(Key, Axis, NumberValue, IsDrag) {
    const Active = Gesture.current;
    let Baseline = Draft;
    if (IsDrag && Active) {
      Baseline = Active.Baseline;
    }
    const Next = UpdatedVector(Baseline, Key, Axis, NumberValue);
    try {
      if (IsDrag && Active) {
        Active.Ending = true;
        Active.Pending = null;
        await Active.Running;
        if (Active.Failure) {
          throw Active.Failure;
        }
        if (Active.Call) {
          await Active.Call('commit', Active.Transaction, Next);
        } else {
          await onChange(Next);
        }
      } else {
        await onChange(Next);
      }
      SetDraft(Next);
    } catch (Failure) {
      if (Active?.Call) {
        await Active.Call('cancel', Active.Transaction);
      }
      SetDraft(Baseline);
      throw Failure;
    } finally {
      Gesture.current = null;
    }
  }
  let Heading = 'Положение, поворот, масштаб';
  if (spawn) {
    Heading = 'Старт игрока';
  } else if (compact) {
    Heading = 'Трансформация';
  }
  let LockIcon = 'UnlockKeyhole';
  if (ScaleLocked) {
    LockIcon = 'LockKeyhole';
  }
  return <fieldset className="transform-inspector" aria-label={tr('Положение, поворот, масштаб')} disabled={disabled}>
    <legend><Icon name="Move" size={14}/> {tr(Heading)}</legend>
    {spawn && <small className="player-spawn-note">{tr('Положение — место появления при запуске сцены.')}</small>}
    {[['position','Положение','м',0.01],['rotation','Поворот','°',0.5],['scale','Масштаб','×',0.01]].map(([Key, Label, Unit, Step]) => <div className="transform-vector" key={Key}>
      <span>{tr(Label)}{Key === 'scale' && <button type="button" className="scale-axis-lock" aria-label="Сохранять пропорции масштаба" aria-pressed={ScaleLocked} title="Связать оси масштаба с сохранением пропорций" onClick={() => SetScaleLocked(Current => !Current)}><Icon name={LockIcon} size={13}/></button>}{Key !== 'scale' && <small>{Unit}</small>}</span>
      <div>{['X','Y','Z'].map((AxisName, Axis) => <label key={AxisName}><b className={'axis-' + AxisName}>{AxisName}</b>
        <NumericInput label={Label + ' ' + AxisName} value={Draft[Key][Axis]} disabled={disabled} step={Step} onBegin={Begin} onPreview={Next => Preview(Key, Axis, Next)} onCancel={Cancel} onCommit={(Next, IsDrag) => Commit(Key, Axis, Next, IsDrag)}/>
      </label>)}</div>
    </div>)}
    <div className="transform-actions"><Button icon="Focus" onClick={onFocus}>{tr('В кадр')}</Button><Button icon="RotateCcw" onClick={onReset}>{tr('Сбросить')}</Button><Button icon="Copy" disabled={!onDuplicate} onClick={onDuplicate}>{tr('Дубликат')}</Button><Button icon="Trash2" onClick={onDelete}>{tr('Удалить')}</Button></div>
    {!compact && <p>Тяните число мышью или нажмите для ввода выражения. Shift — точнее, Ctrl — быстрее. Enter — применить, Escape — отменить.</p>}
  </fieldset>;
}
