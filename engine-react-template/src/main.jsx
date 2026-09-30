import {initializeTheme} from './themes.js';
import React from 'react';
import {createRoot} from 'react-dom/client';
import App from './Editor.jsx';
import './editor.css';
initializeTheme();
createRoot(document.getElementById('root')).render(<App/>);

import "./editorTheme.css";
