import {t as tr, useLocale} from './i18n.jsx';
import React, {useId, useRef, useState} from 'react';
import {Icon} from './StudioParts.jsx';
import {THEMES, applyTheme, resolveTheme, saveTheme} from './themes.js';
import AnchoredPopover from './AnchoredPopover.jsx';
import './themePicker.css';

export default function ThemePicker({onOpen}) {
 useLocale();
  const trigger = useRef(), id = useId();
  const [open, setOpen] = useState(false);
  const [theme, setTheme] = useState(() => resolveTheme(document.documentElement.dataset.theme).id);
  const [saved, setSaved] = useState(true);
  const choose = id => {
    setTheme(applyTheme(id));
    try { setSaved(saveTheme(id, window.localStorage)); } catch { setSaved(false); }
  };
  return <div className="theme-picker">
    <button ref={trigger} title={tr("Оформление")} aria-label={tr("Оформление")} aria-haspopup="dialog" aria-expanded={open} aria-controls={open ? id : undefined}
      onClick={() => { if (!open) onOpen?.(); setOpen(v => !v); }}><Icon name="Palette" size={16}/></button>
    {open && <AnchoredPopover anchor={trigger.current} onClose={()=>setOpen(false)} width={340} align="end" className="theme-popover" id={id} label={tr("Оформление")}>
      <fieldset><legend>{tr("Оформление")}</legend><p>{tr("Цветовая тема редактора")}</p>
        {THEMES.map(item => <label key={item.id} className="theme-option">
          <input type="radio" aria-label={tr(item.name)} name={id} value={item.id} checked={theme === item.id} onChange={() => choose(item.id)}/>
          <span className="theme-swatch" aria-hidden="true" style={{'--preview-app':item.tokens['--bg-app'],'--preview-panel':item.tokens['--bg-panel-elevated'],'--preview-accent':item.tokens['--accent']}}><i/><i/><i/></span>
          <span className="theme-option-copy"><strong>{tr(item.name)}</strong><small>{tr(item.description)}</small></span>
          <Icon name="Check" size={15} className="theme-check"/>
        </label>)}
      </fieldset>
      <p className="theme-persistence" role="status">{saved ? tr("Применяется сразу · сохраняется на этом устройстве") : tr("Тема применена. Браузер не разрешил сохранить выбор.")}</p>
    </AnchoredPopover>}
  </div>;
}
