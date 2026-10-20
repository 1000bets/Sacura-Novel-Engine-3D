import React,{createContext,useContext} from 'react';
import {t} from './i18n.js';
export const AuthoringNavigation=createContext(null);
export const useAuthoringNavigation=()=>useContext(AuthoringNavigation);
export function AuthoringReferenceLink({type,id}){const navigation=useAuthoringNavigation(),open=type==='function'?navigation?.onFunction:type==='event'?navigation?.onEvent:null;return open&&id?<button type="button" onClick={()=>open(id)}>{t(type==='function'?'Открыть реализацию':'Открыть Event')}</button>:null;}
