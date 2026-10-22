import {initializeLanguage} from './i18n.js';
import {initializeTheme} from './themes.js';
import React from 'react';
import {createRoot} from 'react-dom/client';
import AuthGate from './AuthGate.jsx';
import ProjectHome from './ProjectHome.jsx';
import './editor.css';
initializeTheme();
initializeLanguage();
const root=createRoot(document.getElementById('root'));
root.render(window.sacuraDesktop?.isDesktop===true?<ProjectHome/>:<AuthGate/>);

import "./editorTheme.css";
import "./editorWindows.css";
