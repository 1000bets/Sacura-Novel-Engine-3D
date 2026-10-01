import {initializeTheme} from './themes.js';
import React from 'react';
import {createRoot} from 'react-dom/client';
import AuthGate from './AuthGate.jsx';
import './editor.css';
initializeTheme();
const root=createRoot(document.getElementById('root'));
root.render(<AuthGate/>);

import "./editorTheme.css";
