import React from 'react';
import {createRoot} from 'react-dom/client';
import GamePlayer from './GamePlayer.jsx';
import './style.css';
class GameError extends React.Component{constructor(p){super(p);this.state={error:null};}static getDerivedStateFromError(error){return {error};}render(){return this.state.error?<pre role="alert">{this.state.error.message}</pre>:this.props.children;}}
createRoot(document.getElementById('game')).render(<GameError><GamePlayer source={globalThis.SACURA_PROJECT}/></GameError>);
