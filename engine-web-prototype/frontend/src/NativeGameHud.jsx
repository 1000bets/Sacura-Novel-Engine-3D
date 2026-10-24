import React from 'react';
import './nativeGameHud.css';
import WidgetRenderer from './WidgetRenderer.jsx';
import {Flower2} from 'lucide-react';
import {t as Translate, useLocale} from './i18n.jsx';
import './gameplay.css';

export default function NativeGameHud({State, Execute}) {
  useLocale();
  const Views = State?.gameViews || [];
  const HasHud = State?.widgetSlots?.includes('hud') || State?.widgetInstances?.some(Instance => Instance.kind === 'hud');
  const Disabled = State?.gamePaused || State?.play === 'paused';
  return <>{Views.map(View => <React.Fragment key={View.componentId}>
    {State.dialogue?.phase === 'end' && View.ending && <div className="game-ending">
      <Flower2 size={29}/>
      <span>{Translate('Конец истории')}</span>
      <strong>{View.ending}</strong>
      <button disabled={Disabled} onClick={() => Execute({objectId:View.objectId,componentId:View.componentId,action:'restart'})}>{Translate('Сыграть ещё раз')}</button>
    </div>}
    {View.reticle && <span className="native-game-reticle" aria-hidden="true">+</span>}
    {View.message&&<div className="gameplay-task" role="status">{View.message}</div>}
    {!HasHud && (View.widget ? <div className="gameplay-hud">
      <WidgetRenderer widget={View.widget} context={{paused:Disabled,variables:View.variables||{},game:View.game||{}}} onAction={(Action,Element)=>{
        if (!Disabled && Element.nativeAction) {
          Execute({objectId:View.objectId,componentId:View.componentId,action:Element.nativeAction});
        }
      }}/>
    </div> : <section className="native-game-hud" aria-label={View.title || 'Game'}>
      <strong>{View.title}</strong>
      <div className="native-game-values">{(View.values || []).map(Value => <span key={Value.label}>{Value.label}: {Value.value}</span>)}</div>
      {View.text && <p aria-live="polite">{View.text}</p>}
      {View.objective && <p>{View.objective}</p>}
      <div className="native-game-actions">{(View.actions || []).map(Action => <button key={Action.id}
        disabled={Disabled || Action.enabled === false} onClick={() => Execute({objectId:View.objectId,componentId:View.componentId,action:Action.id})}>{Action.label}</button>)}</div>
      {View.controls && <small>{View.controls}</small>}
    </section>)}
  </React.Fragment>)}</>;
}
