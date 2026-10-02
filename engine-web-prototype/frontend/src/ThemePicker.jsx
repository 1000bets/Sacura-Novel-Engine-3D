import {t as tr, useLocale} from './i18n.jsx';
import React, {useEffect, useRef, useState} from 'react';
import {Icon} from './StudioParts.jsx';
import {THEMES, applyTheme, resolveTheme, saveTheme} from './themes.js';
import './themePicker.css';

export default function ThemePicker({onOpen}) {
 useLocale();
  const container = useRef(), trigger = useRef();
  const [open, setOpen] = useState(false);
  const [theme, setTheme] = useState(() => resolveTheme(document.documentElement.dataset.theme).id);
  const [saved, setSaved] = useState(true);
  useEffect(() => {
    if (!open) return;
    const outside = e => { if (!container.current?.contains(e.target)) setOpen(false); };
    document.addEventListener('pointerdown', outside);
    return () => document.removeEventListener('pointerdown', outside);
  }, [open]);
  const choose = id => {
    setTheme(applyTheme(id));
    try { setSaved(saveTheme(id, window.localStorage)); } catch { setSaved(false); }
  };
  return <div className="theme-picker" ref={container}
    onBlur={e => { if (e.relatedTarget && !e.currentTarget.contains(e.relatedTarget)) setOpen(false); }}
    onKeyDown={e => { if (e.key === 'Escape' && open) { e.stopPropagation(); setOpen(false); trigger.current?.focus(); } }}>
    <button ref={trigger} title={tr("Оформление")} aria-label={tr("Оформление")} aria-expanded={open} aria-controls={open ? 'appearance-popover' : undefined}
      onClick={() => { if (!open) onOpen?.(); setOpen(v => !v); }}><Icon name="Palette" size={16}/></button>
    {open && <div className="theme-popover" id="appearance-popover">
      <fieldset><legend>{tr("Оформление")}</legend><p>{tr("Цветовая тема редактора")}</p>
        {THEMES.map(item => <label key={item.id} className="theme-option">
          <input type="radio" aria-label={tr(item.name)} name="editor-theme" value={item.id} checked={theme === item.id} onChange={() => choose(item.id)}/>
          <span className="theme-swatch" aria-hidden="true" style={{'--preview-app':item.tokens['--bg-app'],'--preview-panel':item.tokens['--bg-panel-elevated'],'--preview-accent':item.tokens['--accent']}}><i/><i/><i/></span>
          <span className="theme-option-copy"><strong>{tr(item.name)}</strong><small>{tr(item.description)}</small></span>
          <Icon name="Check" size={15} className="theme-check"/>
        </label>)}
      </fieldset>
      <p className="theme-persistence" role="status">{saved ? tr("Применяется сразу · сохраняется на этом устройстве") : tr("Тема применена. Браузер не разрешил сохранить выбор.")}</p>
    </div>}
  </div>;
}
