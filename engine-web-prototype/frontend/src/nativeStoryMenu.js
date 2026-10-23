// Menu presentation only: C++ creates IDs, edits documents and owns Undo/Redo.
export function nativeStoryMenu({context,story,selection,disabled,command,onSelect,onEvents}){
  const document=story?.document;
  const node=document?.nodes.find(n=>n.id===(context.nodeId||selection?.nodeId));
  const unavailable=reason=>({disabled:true,reason});
  if(context.kind==='native-event'){
    const add=async params=>{
      const before=new Set(node?.bindings?.map(b=>b.id));
      const result=await command('story.addEvent',{nodeId:context.nodeId,phase:context.phase,...(context.batchId?{batchId:context.batchId}:{}),...params});
      const created=result?.document.nodes.find(n=>n.id===context.nodeId)?.bindings?.find(b=>!before.has(b.id));
      if(created)onSelect({nodeId:context.nodeId,bindingId:created.id,eventId:created.eventId});
    };
    const locked=disabled||!node;
    return [
      ...[['Wait','Ожидание','Clock3'],['MoveTo','Перемещение объекта','Move'],['CameraCut','Переход камеры','Video'],['NavigateTo','Движение по навигации','Route']].map(([actionKind,label,icon])=>({label,icon,group:'Создать событие',disabled:locked,action:()=>add({actionKind})})),
      ...(document?.events||[]).map(event=>({id:event.id,label:event.name,icon:'Layers',group:'События этой истории',disabled:locked,action:()=>add({eventId:event.id})})),
    ];
  }
  const add=async kind=>{
    const before=new Set(document.nodes.map(n=>n.id));
    const result=await command('story.addNode',{kind,...(context.position||{x:0,y:0})});
    const created=result?.document.nodes.find(n=>!before.has(n.id));if(!created)return;
    onSelect({nodeId:created.id});
    const pin=context.pin;
    if(pin){
      const source=pin.handleType==='source'?document.nodes.find(n=>n.id===pin.nodeId):created;
      const port=source.kind==='Choice'?(pin.handleType==='source'?source.choices.findIndex(c=>'choice:'+c.id===pin.handleId):0):-1;
      await command('story.connect',{sourceId:source.id,port,targetId:pin.handleType==='source'?created.id:pin.nodeId});
    }
  };
  const reason='Этот тип ноды пока не поддерживается C++ StoryRuntime.';
  const phases=[['BEFORE','До текста'],['ON_START','Вместе с текстом'],['AFTER','После ответа игрока']];
  return [
    ...[['Line','Реплика','MessageSquare'],['Choice','Выбор игрока','GitFork'],['Action','Действие','Zap'],['End','Концовка','Flag']].map(([kind,label,icon])=>({label,icon,group:'Добавить ноду',disabled:disabled||!document||(context.pin?.handleType==='target'&&kind==='End')||(context.valueType&&context.valueType!=='flow'),action:()=>add(kind)})),
    {label:'Добавить событие к выбранной реплике…',icon:'Layers',group:'События',disabled:disabled||!node||!['Line','Choice'].includes(node.kind),children:phases.map(([phase,label])=>({label,action:()=>onEvents({nodeId:node.id,phase,x:context.x,y:context.y})}))},
    ...['If · Если','Ждать взаимодействие','Скриптинг','Переменные проекта'].map(label=>({label,group:'Недоступно в C++',...unavailable(reason)})),
  ];
}
