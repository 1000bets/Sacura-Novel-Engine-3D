import {t} from './i18n.js';
import {defaultInputSettings} from './inputModel.js';
const defaults=defaultInputSettings();
export function inputLabel(item) {
 const original=[...defaults.actions,...defaults.contexts].find(entry=>entry.id===item.id);
 return original?.name===item.name?t(item.name):item.name;
}

export const POINTER_CONTROLS={primary:'Щелчок / касание',button0:'Левая кнопка',button1:'Средняя кнопка',button2:'Правая кнопка',delta:'Движение мыши'};
export const GAMEPAD_AXES={'0':'Левый стик · X','1':'Левый стик · Y','2':'Правый стик · X','3':'Правый стик · Y'};
export const GAMEPAD_BUTTONS={'0':'A / Cross','1':'B / Circle','2':'X / Square','3':'Y / Triangle','4':'LB / L1','5':'RB / R1','6':'LT / L2','7':'RT / R2','8':'Back / Select','9':'Start / Options','10':'L3','11':'R3','12':'Крестовина ↑','13':'Крестовина ↓','14':'Крестовина ←','15':'Крестовина →','16':'Home'};
export const INPUT_DIRECTIONS={up:[0,1],down:[0,-1],left:[-1,0],right:[1,0],both:[1,1]};
export function keyboardLabel(code){
 const names={ArrowUp:'↑',ArrowDown:'↓',ArrowLeft:'←',ArrowRight:'→',Space:t('Пробел'),Escape:'Esc',Enter:'Enter',ShiftLeft:'Shift L',ShiftRight:'Shift R',ControlLeft:'Ctrl L',ControlRight:'Ctrl R',AltLeft:'Alt L',AltRight:'Alt R'};
 return names[code]||code.replace(/^(Key|Digit)/,'');
}
export function bindingControlLabel(binding){
 if(binding.device==='keyboard')return keyboardLabel(binding.control);
 const labels=binding.device==='pointer'?POINTER_CONTROLS:binding.device==='gamepad-axis'?GAMEPAD_AXES:binding.device==='gamepad-button'?GAMEPAD_BUTTONS:null;
 return labels?.[binding.control]?t(labels[binding.control]):binding.device==='touch'?t('На экране'):binding.control;
}
export function bindingDirection(binding){
 const scale=Array.isArray(binding.scale)?binding.scale:[binding.scale??1,0];
 return Object.keys(INPUT_DIRECTIONS).find(key=>INPUT_DIRECTIONS[key].every((value,index)=>value===scale[index]))||'custom';
}
