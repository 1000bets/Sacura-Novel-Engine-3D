// Unmeasured graphs need enough room for the controls rendered inside a card.
// Actual ReactFlow dimensions take precedence once the canvas has rendered.
export function graphNodeSize(node,description={},rendered){
 const measured=rendered?.measured||rendered;
 if(measured?.width>0&&measured?.height>0)return {width:measured.width,height:measured.height};
 let inputs=description.inputs?.filter(p=>!p.control).length;
 if(inputs===undefined){
  inputs=node.kind==='operation'?Object.keys(node.call?.parameters||{}).length:node.kind==='command'?Object.keys(node.command?.value||{}).length:['call','start'].includes(node.kind)?Object.keys(node.args||{}).length:node.kind==='value'?node.value?.op==='get'?0:node.value?.op==='struct'?Object.keys(node.value.fields||{}).length:node.value?.op==='array'?node.value.values?.length||0:['constant','field','length','not'].includes(node.value?.op)?1:2:1;
 }
 const special=['call','start','event','operation','command'].includes(node.kind)?50:node.kind==='value'&&['get','field'].includes(node.value?.op)?85:['set','array','signal','waitSignal','timeout'].includes(node.kind)?85:0;
 const children=(node.children?.length||0)+(node.branches?.length||0);
 return {width:300,height:160+inputs*110+special+children*100+(node.comment?Math.ceil(node.comment.length/35)*18+25:0)};
}

// Only newly generated cards use this grid. Saved author positions are retained.
export function graphGridPositions(nodes,sizeOf=node=>graphNodeSize(node),{columns=5,gap=80,origin={x:50,y:50}}={}){
 const positions={};let y=origin.y;
 for(let i=0;i<nodes.length;i+=columns){
  const row=nodes.slice(i,i+columns);let x=origin.x,height=0;
  for(const node of row){const size=sizeOf(node);positions[node.id]={x,y};x+=size.width+gap;height=Math.max(height,size.height);}
  y+=height+gap;
 }
 return positions;
}
