import {t} from './i18n.js';
import {defaultInputSettings} from './inputModel.js';
const defaults=defaultInputSettings();
export function inputLabel(item) {
 const original=[...defaults.actions,...defaults.contexts].find(entry=>entry.id===item.id);
 return original?.name===item.name?t(item.name):item.name;
}
