import NativeGamePlayer from './NativeGamePlayer.jsx';
import {initializeLanguage,InitializeEditorPreferences} from './i18n.js';
import {initializeTheme,applyTheme,saveTheme} from './themes.js';
import React from 'react';
import {createRoot} from 'react-dom/client';
import AuthGate from './AuthGate.jsx';
import ProjectHome from './ProjectHome.jsx';
import NativeProjectHome from './NativeProjectHome.jsx';
import NativeAssetEditorRoot from './NativeAssetEditorRoot.jsx';
import './editor.css';
initializeTheme();
// Match the requested web reference once; later theme choices remain user-owned.
if(window.sacuraDesktop?.nativeViewport){try{
  if(!localStorage.getItem('sacura-native-web-home-v1')){
    applyTheme('sakura');saveTheme('sakura',localStorage);localStorage.setItem('sacura-native-web-home-v1','1');
  }
}catch{}}
initializeLanguage();
const root=createRoot(document.getElementById('root'),{onUncaughtError:error=>console.error('React render failed:',error.stack||error.message)});
// Native panels read exclusively from the Bridge. The empty legacy shape supplies
// existing shell/layout defaults; it is never saved or simulated in desktop.
(window.sacuraDesktop?.nativePlayer ? Promise.resolve() : InitializeEditorPreferences()).catch(Error => console.error('Editor preferences failed:', Error.message)).finally(() => root.render(window.sacuraDesktop?.nativePlayer ? <NativeGamePlayer/> : window.sacuraDesktop?.nativeViewport
  ? (window.sacuraDesktop.assetEditorIdentity ? <NativeAssetEditorRoot/> : <NativeProjectHome/>)
  : window.sacuraDesktop?.isDesktop===true?<ProjectHome/>:<AuthGate/>));

import "./editorTheme.css";
import "./editorWindows.css";
