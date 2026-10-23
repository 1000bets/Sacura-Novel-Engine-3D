import React, {lazy, Suspense} from 'react';
import NativeViewport from './NativeViewport.jsx';
const WebViewport = lazy(() => import('./LocationScene.jsx'));
// Desktop never mounts Three.js, including host errors.
export default function SceneViewport({blocked, ...props}) {
  return window.sacuraDesktop?.isDesktop
    ? <NativeViewport blocked={blocked} editing={props.editing && props.mode==='scene'} onContext={props.onContext} onAssetDrop={props.onAssetDrop} editTool={props.editTool} editSpace={props.editSpace} snap={props.snap} showGrid={props.showGrid} focusRequest={props.focusRequest} onActivate={props.onActivate} onCreateProject={props.onCreateProject}/>
    : <Suspense fallback={null}><WebViewport {...props}/></Suspense>;
}
