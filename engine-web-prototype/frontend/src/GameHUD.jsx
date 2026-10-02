import React from 'react';
import WidgetRenderer from './WidgetRenderer.jsx';
import {resolveWidget} from './widgetModel.js';
export default function GameHUD({project,preview,onAction,text}){const widget=project.widgets.find(w=>w.id===project.gameplay?.hudWidgetId&&w.kind==='hud')||resolveWidget(project,'hud');return <><div className="gameplay-hud"><WidgetRenderer widget={widget} context={{project,variables:preview.variables,game:preview.world.game,ready:preview.ready,paused:preview.paused}} onAction={onAction}/></div>{['fps','thirdPerson'].includes(project.gameplay.preset)&&<span className="game-crosshair" aria-hidden="true">+</span>}<div className="gameplay-task" role="status">{preview.world.game?.message||preview.hint||text}</div></>;}
