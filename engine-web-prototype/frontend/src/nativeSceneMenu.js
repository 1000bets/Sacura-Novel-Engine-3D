export function nativeSceneMenuItems({context,selection,objects,editing,editSpace,showGrid,command,select,focus,setEditSpace,setShowGrid,copy,paste,canPaste}) {
  const selected=selection&&objects.some(object=>object.id===selection);
  const unavailable=reason=>({disabled:true,reason});
  const placement=context?.worldPosition&&{position:context.worldPosition,rotation:[0,0,0,1],scale:[1,1,1]};
  const create=async(name,componentType,transform=placement,cameraAspect,primitive)=>{
    const params={name};
    if(componentType)params.componentType=componentType;
    if(transform)params.transform=transform;
    if(cameraAspect)params.cameraAspect=cameraAspect;
    if(primitive)params.primitive=primitive;
    const result=await command('object.create',params);
    if(result?.id)await select(result.id);
  };
  const createCharacter=async()=>{
    const result=await command('character.create',{name:'Новый персонаж',...(placement?{transform:placement}:{})});
    if(result?.id)await select(result.id);
  };
  return [
    {id:'scene.character',label:'Персонаж',icon:'PersonStanding',group:'Добавить объект',action:createCharacter},
    {id:'scene.interactive',label:'Интерактивный предмет',icon:'MousePointer2',group:'Добавить объект',...unavailable('Шаблон интерактивного предмета пока не поддерживается C++ проектом.')},
    {id:'scene.objects',label:'Объект',icon:'Box',group:'Добавить объект',children:[
      ...[['cube','Куб','Box'],['sphere','Сфера','Circle'],['cylinder','Цилиндр','Cylinder']].map(([id,label,icon])=>({
        id:`scene.${id}`,label,icon,action:()=>create(label,'engine.MeshRendererComponent',
          placement&&{...placement,position:[placement.position[0],placement.position[1]+0.5,placement.position[2]]},undefined,id)}))
    ]},
    {id:'scene.light',label:'Источник освещения',icon:'Lightbulb',group:'Добавить объект',action:()=>create('Источник освещения','engine.LightComponent')},
    {id:'scene.point',label:'Точка постановки здесь',icon:'MapPin',group:'Добавить объект',action:()=>create('Точка постановки','engine.ArrowComponent')},
    {id:'scene.sound',label:'Звуковое событие',icon:'Volume2',group:'Добавить объект',...unavailable('Звуковые события пока не поддерживаются C++ исполнением.')},
    {id:'scene.camera',label:'Камера из текущего вида',icon:'Video',group:'Добавить объект',disabled:!context?.cameraTransform,reason:'Положение камеры ещё не получено от viewport.',action:()=>create('Камера','engine.CameraComponent',context.cameraTransform,context.cameraAspect)},
    {id:'scene.frame',label:'Приблизить выделение',icon:'Focus',group:'Выделение',shortcut:'F',disabled:!selected,action:focus},
    {id:'scene.group',label:'Сгруппировать выделение',icon:'FolderPlus',group:'Выделение',...unavailable('В C++ редакторе пока доступно одиночное выделение.')},
    {id:'scene.copy',label:'Копировать',icon:'Copy',group:'Выделение',shortcut:'Ctrl+C',disabled:!selected||!copy,action:copy},
    {id:'scene.paste',label:'Вставить',icon:'ClipboardPaste',group:'Выделение',shortcut:'Ctrl+V',disabled:!canPaste,action:paste},
    {id:'scene.duplicate',label:'Дублировать выделение',icon:'Copy',group:'Выделение',disabled:!selected,action:async()=>{const result=await command('object.duplicate',{objectId:selection});if(result?.id)await select(result.id);}},
    {id:'scene.delete',label:'Удалить выделение',icon:'Trash2',group:'Выделение',shortcut:'Del',disabled:!selected,action:()=>command('object.destroy',{objectId:selection})},
    {id:'scene.axes',label:'Оси трансформации',icon:'Axis3d',group:'Выделение',disabled:!selected,children:[
      {id:'scene.axes.world',label:'Мировые оси',icon:editSpace==='world'?'Check':'Globe',action:()=>setEditSpace('world')},
      {id:'scene.axes.local',label:'Оси объекта',icon:editSpace==='local'?'Check':'Box',action:()=>setEditSpace('local')}
    ]},
    {id:'scene.grid',label:showGrid?'Скрыть сетку':'Показать сетку',icon:'Grid2X2',group:'Вид',action:()=>setShowGrid(value=>!value)}
  ].map(item=>({...item,disabled:item.disabled||!editing,
    children:item.children?.map(child=>({...child,disabled:child.disabled||item.disabled||!editing}))}));
}
