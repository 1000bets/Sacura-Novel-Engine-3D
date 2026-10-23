import {useEffect,useMemo,useSyncExternalStore} from 'react';
import {createDesktopEditorAdapter} from './editorPlatform.js';
const empty={connected:false,state:null,objects:[],components:[],types:[],assets:[],story:null};
const noSubscribe=()=>()=>{},getEmpty=()=>empty;
export function useEditorPlatform(){
  const adapter=useMemo(()=>createDesktopEditorAdapter(),[]);
  const snapshot=useSyncExternalStore(adapter?.subscribe||noSubscribe,adapter?.getSnapshot||getEmpty);
  useEffect(()=>{if(!adapter)return;adapter.start().catch(()=>{});return()=>adapter.dispose();},[adapter]);
  return {adapter,...snapshot};
}
