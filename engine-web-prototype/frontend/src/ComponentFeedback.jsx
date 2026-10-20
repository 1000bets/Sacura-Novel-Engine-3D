import './componentFeedback.css';
import React from 'react';
export default function ComponentFeedback({game}){
 if(!game?.message||game.messageAt!==undefined&&game.clock-game.messageAt>3)return null;
 return <div className="component-feedback" role="status" aria-live="polite">{game.message}</div>;
}
