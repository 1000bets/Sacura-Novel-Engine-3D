import {operationDefinition} from './operationRegistry.js';
import {GAME_COMMANDS} from './gameplayModel.js';
export function activityValue(project,activity){
 const value=activity.value;
 if(activity.type==='gameplay')return project.functions.find(f=>f.id===value?.functionId)?.name||project.events.find(e=>e.id===value?.eventId)?.name||operationDefinition(activity.command,activity.operationVersion??1)?.editor.label||GAME_COMMANDS[activity.command]||'Действие механики';
 return (activity.type==='variable'?activity.target+' · ':'')+(value&&typeof value==='object'?Array.isArray(value)?value.join(', '):'Составное значение':String(value??''));
}
