import React, {useEffect, useId, useRef, useState} from 'react';
import {createPortal} from 'react-dom';
import './colorField.css';

function Clamp(Value) {
  return Math.min(1, Math.max(0, Value));
}

function LinearToSrgb(Value) {
  if (Value <= 0.0031308) {
    return Value * 12.92;
  }
  return 1.055 * Math.pow(Value, 1 / 2.4) - 0.055;
}

function SrgbToLinear(Value) {
  if (Value <= 0.04045) {
    return Value / 12.92;
  }
  return Math.pow((Value + 0.055) / 1.055, 2.4);
}

function Channels(Value) {
  if (Array.isArray(Value)) {
    return [...Value];
  }
  return [Value.r, Value.g, Value.b, Value.a];
}

function ToHsv(Color) {
  const [Red, Green, Blue] = Color;
  const Maximum = Math.max(Red, Green, Blue);
  const Difference = Maximum - Math.min(Red, Green, Blue);
  let Hue = 0;
  let Saturation = 0;
  if (Difference > 0) {
    if (Maximum === Red) {
      Hue = (Green - Blue) / Difference;
    } else if (Maximum === Green) {
      Hue = (Blue - Red) / Difference + 2;
    } else {
      Hue = (Red - Green) / Difference + 4;
    }
    Hue = (Hue * 60 + 360) % 360;
  }
  if (Maximum > 0) {
    Saturation = Difference / Maximum;
  }
  return [Hue, Saturation, Maximum];
}

function FromHsv([Hue, Saturation, Brightness], Alpha) {
  const Channel = Offset => {
    const Position = (Offset + Hue / 60) % 6;
    return Brightness * (1 - Saturation * Math.max(0, Math.min(Position, 4 - Position, 1)));
  };
  return [Channel(5), Channel(3), Channel(1), Alpha];
}

function Preview(Color, Srgb) {
  const Display = Color.slice(0, 3).map(Channel => {
    if (Srgb) {
      return Clamp(LinearToSrgb(Channel)) * 255;
    }
    return Clamp(Channel) * 255;
  });
  return `rgba(${Display.join(',')},${Clamp(Color[3])})`;
}

function Hex(Color, Srgb) {
  return Color.map((Channel, Index) => {
    let Value = Channel;
    if (Srgb && Index < 3) {
      Value = LinearToSrgb(Value);
    }
    return Math.round(Clamp(Value) * 255).toString(16).padStart(2, '0');
  }).join('').toUpperCase();
}

function HexField({label: Label, color: Color, srgb: Srgb, onChange: Change}) {
  const Serialized = Hex(Color, Srgb);
  const [Text, SetText] = useState(Serialized);
  useEffect(() => {
    SetText(Serialized);
  }, [Serialized]);
  return <label>{Label}<input aria-label={Label + ' RRGGBBAA'} value={Text} pattern="#?[\da-fA-F]{8}" required onChange={Event => {
    SetText(Event.target.value);
    const Value = Event.target.value.trim().replace(/^#/, '');
    if (!/^[\da-f]{8}$/i.test(Value)) {
      return;
    }
    Change(Value.match(/../g).map((Byte, Index) => {
      const Channel = parseInt(Byte, 16) / 255;
      if (Srgb && Index < 3) {
        return SrgbToLinear(Channel);
      }
      return Channel;
    }));
  }}/></label>;
}

function ColorPanel({label: Label, value: Value, onApply: Apply, onClose: Close}) {
  const Original = useRef(Channels(Value)).current;
  const [Draft, SetDraft] = useState(Original);
  const [Hsv, SetHsv] = useState(() => ToHsv(Original));
  const [BrightnessMaximum, SetBrightnessMaximum] = useState(() => Math.max(1, ...Original.slice(0, 3)));
  const [Srgb, SetSrgb] = useState(true);
  const Dialog = useRef(null);
  const Title = useId();
  useEffect(() => {
    Dialog.current.showModal();
  }, []);
  function ChangeColor(Color) {
    SetDraft(Color);
    SetHsv(ToHsv(Color));
    SetBrightnessMaximum(Previous => Math.max(Previous, ...Color.slice(0, 3)));
  }
  function ChangeHsv(Color) {
    SetHsv(Color);
    SetDraft(FromHsv(Color, Draft[3]));
    SetBrightnessMaximum(Previous => Math.max(Previous, Color[2]));
  }
  function ChangeChannel(Index, Value) {
    const Color = [...Draft];
    Color[Index] = Value;
    ChangeColor(Color);
  }
  function PickWheel(Event) {
    const Bounds = Event.currentTarget.getBoundingClientRect();
    const Horizontal = (Event.clientX - Bounds.left) / Bounds.width * 2 - 1;
    const Vertical = (Event.clientY - Bounds.top) / Bounds.height * 2 - 1;
    ChangeHsv([(Math.atan2(Vertical, Horizontal) * 180 / Math.PI + 360) % 360, Math.min(1, Math.hypot(Horizontal, Vertical)), Hsv[2]]);
  }
  return createPortal(<dialog className="color-panel" ref={Dialog} aria-labelledby={Title}
    onCancel={Event => {Event.preventDefault(); Close();}}
    onKeyDown={Event => Event.stopPropagation()}
    onPointerDown={Event => Event.stopPropagation()}>
    <form onSubmit={Event => {Event.preventDefault(); Event.stopPropagation(); Apply(Draft);}}>
      <header><strong id={Title}>Color Panel — {Label}</strong><button type="button" aria-label="Закрыть выбор цвета" onClick={Close}>×</button></header>
      <div className="color-panel-content">
        <div className="color-panel-picker">
          <div className="color-panel-wheel" role="group" aria-label="Цветовой круг" tabIndex={0}
            onPointerDown={Event => {Event.currentTarget.setPointerCapture(Event.pointerId); PickWheel(Event);}}
            onPointerMove={Event => {
              if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
                PickWheel(Event);
              }
            }}
            onPointerUp={Event => {
              if (Event.currentTarget.hasPointerCapture(Event.pointerId)) {
                Event.currentTarget.releasePointerCapture(Event.pointerId);
              }
            }}
            onKeyDown={Event => {
              const Next = [...Hsv];
              if (Event.key === 'ArrowLeft') {
                Next[0] = (Next[0] + 359) % 360;
              } else if (Event.key === 'ArrowRight') {
                Next[0] = (Next[0] + 1) % 360;
              } else if (Event.key === 'ArrowUp') {
                Next[1] = Clamp(Next[1] + .01);
              } else if (Event.key === 'ArrowDown') {
                Next[1] = Clamp(Next[1] - .01);
              } else {
                return;
              }
              Event.preventDefault(); ChangeHsv(Next);
            }}>
            <span style={{left: `${50 + Math.cos(Hsv[0] * Math.PI / 180) * Hsv[1] * 50}%`, top: `${50 + Math.sin(Hsv[0] * Math.PI / 180) * Hsv[1] * 50}%`}}/>
          </div>
          <label>Яркость<input type="range" min={0} max={BrightnessMaximum} step="0.001" value={Math.max(0, Hsv[2])} onChange={Event => ChangeHsv([Hsv[0], Hsv[1], Number(Event.target.value)])}/></label>
          <label>Альфа<input type="range" min={0} max={1} step="0.001" value={Clamp(Draft[3])} onChange={Event => ChangeChannel(3, Number(Event.target.value))}/></label>
        </div>
        <div className="color-panel-values">
          <div className="color-panel-previews">{[['Исходный', Original], ['Новый', Draft]].map(([Caption, Color]) => <div key={Caption}><span>{Caption}</span><div className="color-swatch"><span style={{background: Preview(Color, Srgb)}}/></div></div>)}</div>
          <label className="color-panel-srgb"><input type="checkbox" checked={Srgb} onChange={Event => SetSrgb(Event.target.checked)}/>Предпросмотр sRGB</label>
          <div className="color-panel-channels">
            <fieldset><legend>RGB Linear</legend>{['R', 'G', 'B', 'A'].map((Name, Index) => <label key={Name}>{Name}<input aria-label={Name} type="number" step="any" required value={Draft[Index]} onChange={Event => {
              if (Event.target.value !== '' && Number.isFinite(Event.target.valueAsNumber)) {
                ChangeChannel(Index, Event.target.valueAsNumber);
              }
            }}/></label>)}</fieldset>
            <fieldset><legend>HSV</legend>{['H', 'S', 'V'].map((Name, Index) => <label key={Name}>{Name}<input aria-label={Name} type="number" step="any" min={0} required value={Hsv[Index]} onChange={Event => {
              if (Event.target.value === '' || !Number.isFinite(Event.target.valueAsNumber)) {
                return;
              }
              const Next = [...Hsv];
              Next[Index] = Math.max(0, Event.target.valueAsNumber);
              if (Index === 0) {
                Next[Index] %= 360;
              }
              if (Index === 1) {
                Next[Index] = Clamp(Next[Index]);
              }
              ChangeHsv(Next);
            }}/></label>)}</fieldset>
          </div>
          <HexField label="Hex Linear" color={Draft} srgb={false} onChange={ChangeColor}/>
          <HexField label="Hex sRGB" color={Draft} srgb={true} onChange={ChangeColor}/>
          <small>HEX: RRGGBBAA, диапазон 0–1. RGB и V поддерживают HDR.</small>
        </div>
      </div>
      <footer><button type="button" onClick={Close}>Отмена</button><button type="submit" autoFocus>Применить</button></footer>
    </form>
  </dialog>, document.body);
}

export default function ColorField({label: Label, value: Value, disabled: Disabled, onChange: Change}) {
  const [Open, SetOpen] = useState(false);
  useEffect(() => {
    if (Disabled) {
      SetOpen(false);
    }
  }, [Disabled]);
  return <div className="color-field"><span>{Label}</span>
    <button type="button" className="color-swatch" disabled={Disabled} aria-label={'Выбрать цвет: ' + Label} aria-haspopup="dialog" onClick={() => SetOpen(true)}>
      <span style={{background: Preview(Channels(Value), true)}}/>
    </button>
    {Open && !Disabled && <ColorPanel label={Label} value={Value} onClose={() => SetOpen(false)} onApply={Color => {
      SetOpen(false);
      if (Color.every((Channel, Index) => Channel === Channels(Value)[Index])) {
        return;
      }
      if (Array.isArray(Value)) {
        Change(Color);
      } else {
        Change({...Value, r: Color[0], g: Color[1], b: Color[2], a: Color[3]});
      }
    }}/>}
  </div>;
}
