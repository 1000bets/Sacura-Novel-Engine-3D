// Pure presentation projection of StoryGraphDocument. No execution or local document edits.
export function nativeStoryPresentation(snapshot,objects){
 const document=snapshot?.document||{nodes:[],events:[],name:''};
 const scene={id:'native-story',name:document.name,entry:document.startNodeId,cameras:[]};
 const beats=document.nodes.map(n=>({...n,nativeKind:n.kind,kind:n.kind.toLowerCase(),text:n.text||n.prompt||'',next:n.nextNodeId,choices:(n.choices||[]).map(c=>({...c,next:c.nextNodeId,result:{type:'none'}}))}));
 const events=(document.events||[]).map(e=>({...e,groups:e.groups.map(g=>({...g,actions:g.actions.map(a=>({...a,type:a.kind,target:a.targetObjectId,value:a.position,duration:a.durationSeconds}))}))}));
 const project={chapters:[{id:'native-document',subsceneId:scene.id,beats}],subscenes:[scene],events,objects,variables:{},functions:[],gameplay:{enabled:false,rules:[],mechanics:[],mechanicDefinitions:[]}};
 const nodes=beats.map(beat=>{
  const ports=['Choice','Narrative'].includes(beat.nativeKind)?beat.choices.map((c,i)=>({id:'choice:'+c.id,label:c.label,next:c.next,port:i})):['Line','Action'].includes(beat.nativeKind)?[{id:'next',label:'Дальше',next:beat.next,port:-1}]:[];
  const phases=['BEFORE','ON_START','AFTER'].map(id=>({id,label:id,batches:(beat.batches?.[id]||[]).map(batch=>({...batch,events:batch.bindingIds.map(key=>{const binding=beat.bindings.find(b=>b.id===key),event=events.find(e=>e.id===binding.eventId);return {binding,event,actions:event?.groups.flatMap(g=>g.actions)||[]};})}))}));
  return {id:beat.id,type:'moment',position:{x:0,y:0},width:360,height:360+ports.length*95+phases.reduce((n,p)=>n+p.batches.reduce((v,b)=>v+50+b.events.length*90,0),0),data:{NativeNode:document.nodes.find(Node=>Node.id===beat.id),beat,scene,phases,ports,entry:scene.entry===beat.id,nativeMode:true}};
 });
 for (const Node of nodes) {
  if (Node.data.beat.nativeKind === 'Narrative') {
   Node.type = 'narrative';
   Node.data.narrative = snapshot.narrativeCatalog?.find(Definition => Definition.type === Node.data.beat.narrativeType);
  }
 }
 const edges=nodes.flatMap(node=>node.data.ports.filter(p=>p.next).map(p=>({id:node.id+'/'+p.id,source:node.id,target:p.next,sourceHandle:p.id,targetHandle:'in',data:{from:node.id,to:p.next}})));
 for (const Node of document.nodes) {
  for (const [Input, Source] of Object.entries(Node.sources || {})) {
   edges.push({id: Node.id + '/data/' + Input, source: Source[0], target: Node.id, sourceHandle: 'data:' + Source[1], targetHandle: 'data:' + Input, data: {from: Source[0], to: Node.id, valueWire: true}});
  }
 }
 return {project,model:{nodes,edges}};
}
