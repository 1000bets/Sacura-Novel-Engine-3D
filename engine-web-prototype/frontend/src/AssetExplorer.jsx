import {NativeAssetDragMimeType, GetNativeAssetDrop} from './nativeScenePlacement.js';
import NativeAssetOperationDialog from './NativeAssetOperationDialog.jsx';
import EditorContextMenu from './EditorContextMenu.jsx';
import NativeAssetCreateDialog from './NativeAssetCreateDialog.jsx';
import {AssetThumbnail} from './NativeAssets.jsx';
import {t as tr, useLocale, message} from './i18n.jsx';
import {hasDroppedFiles,dropAssetFiles} from './assetDrop.js';
import React,{useCallback,useEffect,useMemo,useRef,useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
import {collectAssetFiles,assetFolders,folderContents,cleanAssetPath,importAssetFiles} from './assetFiles.js';
import './assetExplorer.css';
import {createWebEditorAdapter} from './editorPlatform.js';

let AssetClipboard = null;
const icons={model:'FileBox',image:'Image',audio:'FileAudio',file:'File'};
export default function AssetExplorer({project,audioAssets,onImport,onCreateFolder,onSelectObject,onSelectFile,onPlaceModel,onAudio,onLogic,disabled,adapter,revealRequest}){
 useLocale();
 const [CreationMenu, SetCreationMenu] = useState(null);
 const [CreationDialog, SetCreationDialog] = useState(null);
 const [OperationDialog, SetOperationDialog] = useState(null);
 const [SelectedFolder, SetSelectedFolder] = useState(null);
 const CloseOperationDialog = useCallback(() => SetOperationDialog(null), []);

 const CloseCreationMenu = useCallback(() => SetCreationMenu(null), []);
 const CloseCreationDialog = useCallback(() => SetCreationDialog(null), []);
 const [dragging,setDragging]=useState(false),[folder,setFolder]=useState(''),[search,setSearch]=useState(''),[selected,setSelected]=useState(null),[busy,setBusy]=useState(false),[error,setError]=useState(''),[newFolder,setNewFolder]=useState(null),[expanded,setExpanded]=useState(new Set(['']));
 const input=useRef(),directory=useRef(),uploading=useRef(false),dragDepth=useRef(0);
 const data=useMemo(()=>adapter||createWebEditorAdapter({files:collectAssetFiles(project,audioAssets),folders:project.assetFolders}),[adapter,project,audioAssets]);
 const files=data.files;
 const folders=useMemo(()=>assetFolders(files,data.folders||[]),[files,data.folders]);
 const path=folders.includes(folder)?folder:'',contents=folderContents(files,folders,path,search),file=files.find(item=>item.id===selected);
 useEffect(()=>{
  if(!revealRequest){return;}
  const Asset=files.find(Entry=>Entry.id===revealRequest.assetId);
  if(!Asset){return;}
  const Folder=Asset.path.split('/').slice(0,-1).join('/');
  setFolder(Folder);setSearch('');setSelected(Asset.id);
  const Parts=Folder.split('/');const Parents=[''];
  for(let Index=1;Index<=Parts.length;Index++){Parents.push(Parts.slice(0,Index).join('/'));}
  setExpanded(Previous=>new Set([...Previous,...Parents]));
  const Timer=setTimeout(()=>document.querySelector('[data-asset-id="'+Asset.id+'"]')?.scrollIntoView({block:'nearest'}),0);
  return()=>clearTimeout(Timer);
 },[revealRequest,files]);
 const navigate=next=>{
  SetSelectedFolder(null);setFolder(next);setSearch('');setSelected(null);
  const Parts = next.split('/');
  const Parents = [''];
  for (let Index=1;Index<=Parts.length;Index++) {Parents.push(Parts.slice(0,Index).join('/'));}
  setExpanded(Previous=>new Set([...Previous,...Parents]));
 };
 const upload=async list=>{if(adapter){setError("Перетаскивание и импорт папки пока не подключены. Используйте «Импорт файлов».");return;}
  if(disabled||uploading.current||!list?.length)return;uploading.current=true;setBusy(true);setError('');
  try{const imported=await importAssetFiles(Array.from(list),path);const stored=await onImport(imported);setSelected(stored?.[0]?.id||imported[0]?.id);onSelectFile?.(stored?.[0]?.id||imported[0]?.id);}catch(cause){setError(cause.message);}finally{uploading.current=false;setBusy(false);}
 };
 const pickDesktop=async()=>{if(adapter){setBusy(true);try{await adapter.import(path);setError("");}catch(e){setError(e.message);}finally{setBusy(false);}return;}try{const files=await window.sacuraDesktop.pickAssets();await upload(files.map(item=>new File([item.bytes],item.name,{type:item.type})));}catch(cause){setError(cause.message);}};
 const createFolder=async event=>{event.preventDefault();const name=cleanAssetPath(newFolder);if(!name)return;const next=[path,name].filter(Boolean).join('/');try{if (adapter) {await adapter.manage('assets.createFolder',{folder:'/'+(path||'Game'),name});} else {await onCreateFolder(next);}navigate(next);setNewFolder(null);setError('');}catch(cause){setError(cause.message);}};
 const tree=(parent='',depth=0)=>folders.filter(item=>item&&item.split('/').slice(0,-1).join('/')===parent).map(item=>{const children=folders.some(child=>child.startsWith(item+'/'));return <React.Fragment key={item}><div className="asset-folder-row" data-asset-folder={item} style={{paddingLeft:depth*14}}>
  <button className="asset-folder-toggle" aria-label={`${tr(expanded.has(item)?'Свернуть':'Развернуть')} ${item}`} disabled={!children} aria-expanded={children?expanded.has(item):undefined} onClick={()=>setExpanded(previous=>{const next=new Set(previous);next.has(item)?next.delete(item):next.add(item);return next;})}><Icon name={expanded.has(item)?'ChevronDown':'ChevronRight'} size={12}/></button>
  <button className={path===item?'selected':''} aria-current={path===item?'location':undefined} onClick={()=>{navigate(item);SetSelectedFolder(item);}}><Icon name="Folder" size={15}/><span>{item.split('/').at(-1)}</span></button>
 </div>{expanded.has(item)&&tree(item,depth+1)}</React.Fragment>;});
 const OpenCreationMenu = async Event => {
  if (!adapter?.creationTypes) {return;}
  Event.preventDefault(); Event.stopPropagation();
  const FolderRow = Event.target.closest('[data-asset-folder]');
  let TargetFolder = path;
  if (FolderRow) {TargetFolder = FolderRow.dataset.assetFolder;}
  const TargetAsset = files.find(Asset => Asset.id === Event.target.closest('[data-asset-id]')?.dataset.assetId);
  if (TargetAsset) {setSelected(TargetAsset.id);onSelectFile?.(TargetAsset.id);}
  if (FolderRow) {SetSelectedFolder(TargetFolder);setSelected(null);}
  const Request = {x:Event.clientX,y:Event.clientY,folder:TargetFolder,selectedFolder:FolderRow?.dataset.assetFolder,selectedAsset:TargetAsset,types:[],nonce:Date.now()};
  SetCreationMenu(Request);
  try {
   const Types = await adapter.creationTypes();
   SetCreationMenu(Previous => {if (Previous?.nonce === Request.nonce) {return {...Previous,types:Types};}return Previous;});
  } catch (ErrorValue) {setError(ErrorValue.message);SetCreationMenu(null);}
 };
 const CreateAsset = async Params => {
  const Created = await adapter.create(Params);
  const AssetPath = Created.path.replace(/^\//,'');
  navigate(AssetPath.split('/').slice(0,-1).join('/'));
  setSelected(Created.id);onSelectFile?.(Created.id);
 };
 const StartNativeAssetDrag = (Event, Asset) => {
  const Placement = GetNativeAssetDrop(Asset);
  if (!adapter || disabled || busy || !Placement) {
   Event.preventDefault();
   return;
  }
  Event.stopPropagation();
  Event.dataTransfer.effectAllowed = 'copy';
  Event.dataTransfer.setData(NativeAssetDragMimeType, JSON.stringify(Placement));
  SetSelectedFolder(null);setSelected(Asset.id);onSelectFile?.(Asset.id);
 };
 const RunAssetOperation = async (Method, Params) => {
  setBusy(true);setError('');
  try {
   const Result = await adapter.manage(Method,Params);
   if (Result.path) {
    navigate(Result.path.replace(/^\//,'').split('/').slice(0,-1).join('/'));
    setSelected(Result.id);onSelectFile?.(Result.id);
   } else if (Result.folder) {
    navigate(Result.folder.replace(/^\//,''));
   } else if (Method === 'assets.delete') {
    setSelected(null);onSelectFile?.(null);
   }
   return Result;
  } finally {setBusy(false);}
 };
 const ApplyFileOperation = async (Operation, Name) => {
  const Params = {...Operation.params};
  if (!Operation.deleting) {Params.name = Name;}
  return RunAssetOperation(Operation.method,Params);
 };
 const BeginFileOperation = (Kind, Asset, Folder) => {
  if (Kind === 'folder') {
   SetOperationDialog({title:'Новая папка',path:'/'+(Folder||'Game'),method:'assets.createFolder',params:{folder:'/'+(Folder||'Game')},button:'Создать'});
   return;
  }
  const Deleting = Kind === 'delete';
  let Title = 'Переименовать';
  let Button = 'Переименовать';
  if (Deleting) {Title = 'Удалить';Button = 'Удалить';}
  let Method = 'assets.renameFolder';
  let Params = {folder:'/'+Folder};
  let Path = '/'+Folder;
  let Value = Folder?.split('/').at(-1) || '';
  if (Asset) {
   Method = 'assets.rename';Params = {assetId:Asset.id};Path = '/'+Asset.path;Value = Asset.name.replace(/\.[^.]+$/,'');
  }
  if (Deleting && Asset) {Method = 'assets.delete';}
  if (Deleting && !Asset) {Method = 'assets.deleteFolder';}
  SetOperationDialog({title:Title,button:Button,path:Path,value:Value,method:Method,params:Params,deleting:Deleting});
 };
 const PasteAsset = async Folder => {
  if (!AssetClipboard) {return;}
  const Clipboard = AssetClipboard;
  let Method = 'assets.duplicate';
  if (Clipboard.cut) {Method = 'assets.move';}
  try {
   await RunAssetOperation(Method,{assetId:Clipboard.assetId,folder:'/'+(Folder||'Game')});
   if (Clipboard.cut && AssetClipboard === Clipboard) {AssetClipboard = null;}
  } catch (Failure) {setError(Failure.message);}
 };
 const BuildFileMenu = Menu => {
  const Asset = Menu.selectedAsset;
  const Folder = Menu.selectedFolder;
  const Writable = !disabled && !busy && !Menu.folder.startsWith('Engine');
  const MutableAsset = Asset && !Asset.builtin && !disabled && !busy;
  const MutableFolder = Folder?.startsWith('Game/') && !disabled && !busy;
  const Items = [{id:'asset.new-folder',label:'Новая папка',icon:'FolderPlus',disabled:!Writable,action:()=>BeginFileOperation('folder',null,Menu.folder)},
   {id:'asset.create',label:'Создать ассет',icon:'FilePlus',disabled:!Writable,children:Menu.types.map(Type=>({id:'asset.create.'+Type.id,label:Type.label,icon:'FilePlus',action:()=>SetCreationDialog({...Menu,type:Type})}))}];
  if (Asset) {
   Items.push({id:'asset.open',label:'Открыть',icon:'ExternalLink',action:()=>adapter.open(Asset)},
    {id:'asset.copy',label:'Копировать',shortcut:'Ctrl+C',icon:'Copy',action:()=>{AssetClipboard={assetId:Asset.id,cut:false};}},
    {id:'asset.cut',label:'Вырезать',shortcut:'Ctrl+X',icon:'Scissors',disabled:!MutableAsset,action:()=>{AssetClipboard={assetId:Asset.id,cut:true};}},
    {id:'asset.duplicate',label:'Дублировать',shortcut:'Ctrl+D',icon:'CopyPlus',disabled:!Writable,action:()=>{AssetClipboard={assetId:Asset.id,cut:false};PasteAsset(Menu.folder);}},
    {id:'asset.copy-path',label:'Копировать путь',icon:'Link',action:()=>window.sacuraDesktop.nativeCopyText('/'+Asset.path)});
  }
  Items.push({id:'asset.paste',label:'Вставить',shortcut:'Ctrl+V',icon:'ClipboardPaste',disabled:!Writable||!AssetClipboard,action:()=>PasteAsset(Menu.folder)});
  if (Asset || Folder) {
   Items.push({id:'asset.rename',label:'Переименовать',shortcut:'F2',icon:'Pencil',disabled:!MutableAsset&&!MutableFolder,action:()=>BeginFileOperation('rename',Asset,Folder)},
    {id:'asset.delete',label:'Удалить',shortcut:'Delete',icon:'Trash2',disabled:!MutableAsset&&!MutableFolder,action:()=>BeginFileOperation('delete',Asset,Folder)});
  }
  Items.push({id:'asset.refresh',label:'Обновить',shortcut:'F5',icon:'RefreshCw',action:()=>adapter.refresh().catch(Failure=>setError(Failure.message))});
  return Items;
 };
 const FileKeyDown = Event => {
  if (!adapter || Event.target.closest('input,textarea,select,[contenteditable=true]')) {return;}
  const Asset = files.find(Entry=>Entry.id===selected);
  const Control = Event.ctrlKey || Event.metaKey;
  if (Control && Event.key.toLowerCase() === 'c' && Asset) {Event.preventDefault();Event.stopPropagation();AssetClipboard={assetId:Asset.id,cut:false};}
  else if (Control && Event.key.toLowerCase() === 'x' && Asset && !Asset.builtin && !disabled) {Event.preventDefault();Event.stopPropagation();AssetClipboard={assetId:Asset.id,cut:true};}
  else if (Control && Event.key.toLowerCase() === 'v' && !disabled && !path.startsWith('Engine')) {Event.preventDefault();Event.stopPropagation();PasteAsset(SelectedFolder||path);}
  else if (Control && Event.key.toLowerCase() === 'd' && Asset && !disabled && !path.startsWith('Engine')) {Event.preventDefault();Event.stopPropagation();AssetClipboard={assetId:Asset.id,cut:false};PasteAsset(path);}
  else if ((Event.key === 'F2' || Event.key === 'Delete') && !disabled && (Asset && !Asset.builtin || SelectedFolder?.startsWith('Game/'))) {
   Event.preventDefault();Event.stopPropagation();let Kind='rename';if(Event.key==='Delete'){Kind='delete';}BeginFileOperation(Kind,Asset,SelectedFolder);
  } else if (Event.key === 'F5') {Event.preventDefault();Event.stopPropagation();adapter.refresh().catch(Failure=>setError(Failure.message));}
  else if (Control && Event.shiftKey && Event.key.toLowerCase() === 'n' && !disabled && !path.startsWith('Engine')) {Event.preventDefault();Event.stopPropagation();BeginFileOperation('folder',null,SelectedFolder||path);}
 };
 const open=item=>{if(adapter){adapter.open(item);return;}if(item.logicResourceId)onLogic?.(item.logicResourceId);else if(item.kind==='model')onPlaceModel(item);else if(item.audioId)onAudio();};
 const hasFiles=hasDroppedFiles;
 const dragOver=event=>{if(!hasFiles(event))return;event.preventDefault();event.stopPropagation();event.dataTransfer.dropEffect=disabled||busy?'none':'copy';};
 const dragEnter=event=>{if(!hasFiles(event))return;event.preventDefault();event.stopPropagation();dragDepth.current++;if(!disabled&&!busy)setDragging(true);};
 const dragLeave=event=>{event.stopPropagation();dragDepth.current=Math.max(0,dragDepth.current-1);if(!dragDepth.current)setDragging(false);};
 const drop=event=>dropAssetFiles(event,{dragDepth,onDragging:setDragging,onFiles:upload,disabled:disabled||uploading.current});
 return <section className={'asset-explorer'+(dragging?' dragging':'')} onDragEnter={dragEnter} onDragOver={dragOver} onDragLeave={dragLeave} onDrop={drop} onContextMenu={OpenCreationMenu} onKeyDown={FileKeyDown} tabIndex={0} aria-label={tr("Проводник проекта")} data-help-title={tr("Проводник")} data-help={tr("Файлы проекта: модели, изображения, звук и другие ресурсы. Импортируйте файлы или папки, ищите по имени, создавайте папки и добавляйте модели в сцену.")} aria-busy={busy}>
  <header className="asset-explorer-toolbar"><div className="asset-breadcrumb"><button onClick={()=>navigate('')}><Icon name="FolderOpen" size={15}/>Assets</button>{path.split('/').filter(Boolean).map((part,index)=><React.Fragment key={index}><Icon name="ChevronRight" size={12}/><button onClick={()=>navigate(path.split('/').slice(0,index+1).join('/'))}>{part}</button></React.Fragment>)}</div>
   <label className="asset-search"><Icon name="Search" size={14}/><input aria-label={tr("Поиск файлов в папке")} placeholder={tr("Поиск файлов…")} value={search} onChange={e=>setSearch(e.target.value)}/></label>
   <Button icon="FolderPlus" disabled={disabled||busy||path.startsWith('Engine')} onClick={()=>{if(adapter){BeginFileOperation('folder',null,path);}else{setNewFolder('');}}}>{tr("Папка")}</Button><Button icon="Upload" disabled={disabled||busy||!!adapter&&path.startsWith("Engine")} onClick={()=>window.sacuraDesktop?.isDesktop===true?pickDesktop():input.current.click()}>{busy?tr("Импорт…"):tr("Импорт файлов")}</Button><Button icon="FolderInput" title={adapter?"Импорт папок не подключён":undefined} disabled={!!adapter||disabled||busy} onClick={()=>directory.current.click()}>{tr("Импорт папки")}</Button>
   <input ref={input} type="file" multiple hidden onChange={e=>{upload(e.target.files);e.target.value='';}}/><input ref={directory} type="file" multiple webkitdirectory="" hidden onChange={e=>{upload(e.target.files);e.target.value='';}}/>
  </header>
  {newFolder!==null&&<form className="asset-folder-form" onSubmit={createFolder}><label>{tr("Имя папки")}<input autoFocus aria-label={tr("Имя новой папки")} value={newFolder} onChange={e=>setNewFolder(e.target.value)}/></label><Button type="submit" disabled={disabled||!newFolder.trim()}>{tr("Создать")}</Button><Button type="button" onClick={()=>setNewFolder(null)}>{tr("Отмена")}</Button></form>}
  {error&&<p role="alert" className="asset-explorer-error">{message(error)}</p>}
  <div className="asset-explorer-body"><nav className="asset-folder-tree" aria-label={tr("Папки проекта")}><button className={'asset-root '+(!path?'selected':'')} onClick={()=>navigate('')}><Icon name="FolderOpen" size={16}/>Assets</button>{tree()}</nav>
   <div className="asset-file-area">
    <div className="asset-file-grid" aria-label={tr("Файлы и папки")}>{contents.folders.map(item=><button key={item} title={item} data-asset-folder={item} className={'asset-file-tile folder'+(SelectedFolder===item?' selected':'')} onClick={()=>{SetSelectedFolder(item);setSelected(null);}} onDoubleClick={()=>navigate(item)}><Icon name="Folder" size={34}/><strong>{item.split('/').at(-1)}</strong><small>{tr("Папка")}</small></button>)}
     {contents.files.map(item=><button key={item.id} title={item.path||item.name} data-asset-id={item.id} draggable={!!adapter && !disabled && !busy && !!GetNativeAssetDrop(item)} onDragStart={Event=>StartNativeAssetDrag(Event,item)} className={'asset-file-tile '+(selected===item.id?'selected':'')} aria-pressed={selected===item.id} onClick={()=>{SetSelectedFolder(null);setSelected(item.id);if(item.logicResourceId)onLogic?.(item.logicResourceId);else onSelectFile?.(item.id);}} onDoubleClick={()=>!disabled&&open(item)}>
      {adapter?<AssetThumbnail assetId={item.id} type={item.typeId}/>:item.kind==='image'&&!/\.svg$/i.test(item.name)?<img src={item.src} alt=""/>:<Icon name={icons[item.kind]||'File'} size={32}/>}<strong>{item.name}</strong><small>{search?item.path:item.builtin?tr("Встроенный файл"):item.bytes!=null?tr("{0} КБ", [(item.bytes/1024).toFixed(1)]):tr("Файл проекта")}</small>
     </button>)}
    </div>
    {!contents.files.length&&!contents.folders.length&&<div className="asset-explorer-empty"><Icon name="FolderOpen" size={35}/><strong>{search?tr("Файлы не найдены"):tr("Папка пуста")}</strong><p>{search?tr("Попробуйте другое имя или откройте Assets."):(adapter?"Используйте «Импорт файлов».":tr("Перетащите файлы сюда или используйте импорт. Структура импортированной папки сохранится."))}</p></div>}
   </div>
  </div>
  {CreationMenu && <EditorContextMenu x={CreationMenu.x} y={CreationMenu.y} title={tr("Проводник")} onClose={CloseCreationMenu} items={BuildFileMenu(CreationMenu)}/>}
  {OperationDialog && <NativeAssetOperationDialog operation={OperationDialog} submit={ApplyFileOperation} close={CloseOperationDialog}/>}
  {CreationDialog && <NativeAssetCreateDialog type={CreationDialog.type} folder={CreationDialog.folder} selectedAsset={CreationDialog.selectedAsset} create={CreateAsset} close={CloseCreationDialog}/>}
  {dragging&&<div className="asset-drop-overlay" role="status"><Icon name="Upload" size={32}/><strong>{adapter?"Перетаскивание пока не подключено":tr("Отпустите файлы для загрузки")}</strong><span>{tr("В папку ")}{path||'Assets'} {tr("· до 100 МБ на файл")}</span></div>}
  {file&&<footer className="asset-explorer-details">{file?<><Icon name={icons[file.kind]||'File'} size={16}/><span><strong>{file.name}</strong><small>{file.path}{file.users.length?tr(" · Используется: {0}", [file.users.map(user=>user.name).join(', ')]):''}</small></span>{file.logicResourceId&&<Button icon="Code2" onClick={()=>onLogic?.(file.logicResourceId)}>{tr("Открыть компонент")}</Button>}{file.kind==='model'&&<Button icon="Plus" disabled={disabled||busy} onClick={()=>onPlaceModel(file)}>{tr("Добавить в сцену")}</Button>}{file.users.map(user=><Button key={user.id} icon="Focus" onClick={()=>onSelectObject(user.id)}>{user.name}</Button>)}{file.audioId&&<Button icon="Music2" onClick={onAudio}>{tr("Открыть звук")}</Button>}</>:<span>{files.length} {tr("файлов в проекте · выберите файл для подробностей · импорт до 100 МБ на файл")}</span>}</footer>}
 </section>;
}
