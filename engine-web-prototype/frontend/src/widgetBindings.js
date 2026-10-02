import {pathValue} from './blueprintFunctions.js';
export function widgetValue(context,binding){if(!binding)return undefined;const sources={variable:context.variables,game:context.game};return pathValue(sources[binding.source],binding.path);}
export function boundText(element,context,fallback){const value=widgetValue(context,element.binding);if(value===undefined)return fallback;const text=typeof value==='object'?JSON.stringify(value):String(value);return element.binding?.format?element.binding.format.replaceAll('{value}',text):text;}
export function widgetVisible(element,context){if(!element.showIf)return true;const v=widgetValue(context,element.showIf);return element.showIf.negate?!v:!!v;}
