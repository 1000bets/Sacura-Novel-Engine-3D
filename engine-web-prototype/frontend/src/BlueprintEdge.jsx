import React,{useState} from 'react';
import {BaseEdge,EdgeLabelRenderer,getBezierPath,getSmoothStepPath} from '@xyflow/react';
import './blueprintEdges.css';
export default function BlueprintEdge(props){
 const {id,sourceX,sourceY,targetX,targetY,sourcePosition,targetPosition,label,data={},selected}=props;
 const [hover,setHover]=useState(false),active=selected||data.highlighted||hover;
 const backward=targetX-sourceX<70;
 const [path,labelX,labelY]=backward?getSmoothStepPath({sourceX,sourceY,targetX,targetY,sourcePosition,targetPosition,borderRadius:24,offset:36}):getBezierPath({sourceX,sourceY,targetX,targetY,sourcePosition,targetPosition,curvature:.3});
 const color=active?'var(--accent)':data.broken?'var(--warning)':data.valueWire?({boolean:'var(--error)',integer:'var(--info)',number:'var(--success)',string:'var(--accent)'})[data.valueType]||'var(--info)':data.branch==='true'?'var(--success)':data.branch==='false'?'var(--warning)':'var(--text-secondary)';
 const width=active?3:2;
 const marker='wire-tip-'+id.replace(/[^a-zA-Z0-9_-]/g,'_');
 const arrowX=backward?sourceX+22:labelX,arrowY=backward?sourceY:labelY;
 const angle=backward?0:Math.atan2(1.5*(targetY-sourceY),.75*(targetX-sourceX))*180/Math.PI;
 return <g className={'blueprint-wire'+(active?' active':'')+(data.valueWire?' data-wire':'')} style={{color}} onMouseEnter={()=>setHover(true)} onMouseLeave={()=>setHover(false)}>
  <defs><marker id={marker} viewBox="0 0 12 12" markerWidth="12" markerHeight="12" refX="10" refY="6" markerUnits="userSpaceOnUse" orient="auto-start-reverse"><path d="M 2 2 L 10 6 L 2 10" fill="none" stroke={color} strokeWidth="2" strokeLinejoin="round" strokeLinecap="round"/></marker></defs>
  <path className="blueprint-wire-halo" d={path} fill="none" stroke="var(--bg-app)" strokeWidth={width+5} vectorEffect="non-scaling-stroke"/>
  <BaseEdge id={id} path={path} interactionWidth={24} markerEnd={data.valueWire?undefined:'url(#'+marker+')'} style={{stroke:color,strokeWidth:width,strokeDasharray:data.valueWire?'3 5':data.broken?'7 5':undefined,strokeLinecap:'round',strokeLinejoin:'round',vectorEffect:'non-scaling-stroke'}}/>
  {data.valueWire?<><circle cx={sourceX+5} cy={sourceY} r="3" fill={color}/><circle cx={targetX-5} cy={targetY} r="3" fill={color}/></>:active&&<path d="M -5 -4 L 1 0 L -5 4" transform={'translate('+arrowX+' '+arrowY+') rotate('+angle+')'} fill="none" stroke={color} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>}
  {label&&(active||data.showLabel)&&<EdgeLabelRenderer><div className="blueprint-wire-label" title={String(label)} style={{transform:'translate(-50%, -100%) translate('+labelX+'px,'+(labelY-8)+'px)',color}}>{label}</div></EdgeLabelRenderer>}
 </g>;
}
export const blueprintEdgeTypes={blueprint:BlueprintEdge};
