import React,{useEffect,useRef} from 'react';
import {basicSetup,EditorView} from 'codemirror';
import {javascript} from '@codemirror/lang-javascript';
import {HighlightStyle,syntaxHighlighting} from '@codemirror/language';
import {tags} from '@lezer/highlight';
import {linter,setDiagnostics} from '@codemirror/lint';
import {parseMechanicScript} from './mechanicScriptCompiler.js';

const highlights=HighlightStyle.define([
 {tag:tags.keyword,color:'#d6a8ef'},{tag:tags.comment,color:'#9896ac',fontStyle:'italic'},
 {tag:tags.string,color:'#b5d8ad'},{tag:[tags.number,tags.bool,tags.null],color:'#ebc48b'},
 {tag:[tags.variableName,tags.propertyName],color:'#dcdde9'},{tag:tags.function(tags.variableName),color:'#a8d3f2'},
 {tag:[tags.operator,tags.punctuation],color:'#c4bacf'},{tag:tags.definition(tags.variableName),color:'#edc2dc'}
]);

export default function MechanicScriptEditor({source,onChange,onCompile,diagnostic,disabled=false,parseSource=parseMechanicScript,label="JavaScript механики"}){
 const host=useRef(null),view=useRef(null),callbacks=useRef({onChange,onCompile});callbacks.current={onChange,onCompile};
 useEffect(()=>{
  const editor=new EditorView({parent:host.current,doc:source,extensions:[basicSetup,EditorView.editable.of(!disabled),javascript(),syntaxHighlighting(highlights),EditorView.contentAttributes.of({'aria-label':label,spellcheck:'false'}),EditorView.theme({
   '&':{height:'100%',backgroundColor:'#15151e',color:'#dddce7',fontSize:'13px'},'.cm-scroller':{fontFamily:'"SFMono-Regular",Consolas,"Liberation Mono",monospace',lineHeight:'1.7',overflow:'auto'},'.cm-content':{padding:'16px 0'},'.cm-line':{padding:'0 16px'},'.cm-gutters':{backgroundColor:'#15151e',color:'#716c82',borderRight:'1px solid #302c3b'},'.cm-activeLine,.cm-activeLineGutter':{backgroundColor:'#22202e'},'.cm-cursor':{borderLeftColor:'#e7aecb'},'&.cm-focused .cm-selectionBackground,.cm-selectionBackground':{backgroundColor:'#493449'},'.cm-tooltip':{backgroundColor:'#272431',color:'#eee5ed',borderColor:'#514452'},'.cm-panels':{backgroundColor:'#23212c',color:'#eee'},'.cm-searchMatch':{backgroundColor:'#735a35'},'.cm-foldPlaceholder':{backgroundColor:'#302b3c',border:'0',color:'#c8afca'}
  },{dark:true}),EditorView.updateListener.of(update=>{if(update.docChanged)callbacks.current.onChange(update.state.doc.toString());}),EditorView.domEventHandlers({keydown(event){if((event.metaKey||event.ctrlKey)&&event.key.toLowerCase()==='s'){event.preventDefault();callbacks.current.onCompile();return true;}event.stopPropagation();return false;}}),linter(v=>{try{parseSource(v.state.doc.toString());return [];}catch(e){const line=v.state.doc.line(Math.min(e.line||1,v.state.doc.lines)),from=Math.min(line.to,line.from+(e.column||1)-1);return [{from,to:Math.min(line.to,from+1),severity:'error',message:e.message}];}},{delay:500})]});view.current=editor;
  return()=>{view.current=null;editor.destroy();};
 },[disabled]);
 useEffect(()=>{const editor=view.current;if(editor&&editor.state.doc.toString()!==source)editor.dispatch({changes:{from:0,to:editor.state.doc.length,insert:source}});},[source]);
 useEffect(()=>{const editor=view.current;if(!editor||!diagnostic)return;const line=editor.state.doc.line(Math.min(diagnostic.line||1,editor.state.doc.lines)),from=Math.min(line.to,line.from+(diagnostic.column||1)-1);editor.dispatch(setDiagnostics(editor.state,[{from,to:Math.min(line.to,from+1),message:diagnostic.message,severity:'error'}]));editor.dispatch({selection:{anchor:from},scrollIntoView:true});editor.focus();},[diagnostic]);
 return <div className="mechanic-script-codemirror" ref={host} onKeyDown={e=>e.stopPropagation()}/>;
}
