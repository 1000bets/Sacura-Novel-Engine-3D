import React,{useMemo,useRef,useState} from 'react';
import {Icon,Button} from './StudioParts.jsx';
import {collectAssetFiles,assetFolders,folderContents,cleanAssetPath,importAssetFiles} from './assetFiles.js';
import './assetExplorer.css';

const icons={model:'FileBox',image:'Image',audio:'FileAudio',file:'File'};
export default function AssetExplorer({project,audioAssets,onImport,onCreateFolder,onSelectObject,onPlaceModel,onAudio,disabled}){
 const [folder,setFolder]=useState(''),[search,setSearch]=useState(''),[selected,setSelected]=useState(null),[busy,setBusy]=useState(false),[error,setError]=useState(''),[newFolder,setNewFolder]=useState(null),[expanded,setExpanded]=useState(new Set(['']));
 const input=useRef(),directory=useRef();
 const files=useMemo(()=>collectAssetFiles(project,audioAssets),[project,audioAssets]);
 const folders=useMemo(()=>assetFolders(files,project.assetFolders),[files,project.assetFolders]);
 const path=folders.includes(folder)?folder:'',contents=folderContents(files,folders,path,search),file=files.find(item=>item.id===selected);
 const navigate=next=>{setFolder(next);setSearch('');setSelected(null);setExpanded(previous=>new Set([...previous,next]));};
 const upload=async list=>{
  if(disabled||busy||!list?.length)return;setBusy(true);setError('');
  try{const imported=await importAssetFiles(Array.from(list),path);const stored=await onImport(imported);setSelected(stored?.[0]?.id||imported[0]?.id);}catch(cause){setError(cause.message);}finally{setBusy(false);}
 };
 const createFolder=async event=>{event.preventDefault();const name=cleanAssetPath(newFolder);if(!name)return;const next=[path,name].filter(Boolean).join('/');try{await onCreateFolder(next);navigate(next);setNewFolder(null);setError('');}catch(cause){setError(cause.message);}};
 const tree=(parent='',depth=0)=>folders.filter(item=>item&&item.split('/').slice(0,-1).join('/')===parent).map(item=>{const children=folders.some(child=>child.startsWith(item+'/'));return <React.Fragment key={item}><div className="asset-folder-row" style={{paddingLeft:depth*14}}>
  <button className="asset-folder-toggle" aria-label={`${expanded.has(item)?'Свернуть':'Развернуть'} ${item}`} disabled={!children} aria-expanded={children?expanded.has(item):undefined} onClick={()=>setExpanded(previous=>{const next=new Set(previous);next.has(item)?next.delete(item):next.add(item);return next;})}><Icon name={expanded.has(item)?'ChevronDown':'ChevronRight'} size={12}/></button>
  <button className={path===item?'selected':''} aria-current={path===item?'location':undefined} onClick={()=>navigate(item)}><Icon name="Folder" size={15}/><span>{item.split('/').at(-1)}</span></button>
 </div>{expanded.has(item)&&tree(item,depth+1)}</React.Fragment>;});
 const open=item=>{if(item.kind==='model')onPlaceModel(item);else if(item.audioId)onAudio();};
 return <section className="asset-explorer" aria-label="Проводник проекта" data-help-title="Проводник" data-help="Файлы проекта: модели, изображения, звук и другие ресурсы. Импортируйте файлы или папки, ищите по имени, создавайте папки и добавляйте модели в сцену." aria-busy={busy}>
  <header className="asset-explorer-toolbar"><div className="asset-breadcrumb"><button onClick={()=>navigate('')}><Icon name="FolderOpen" size={15}/>Assets</button>{path.split('/').filter(Boolean).map((part,index)=><React.Fragment key={index}><Icon name="ChevronRight" size={12}/><button onClick={()=>navigate(path.split('/').slice(0,index+1).join('/'))}>{part}</button></React.Fragment>)}</div>
   <label className="asset-search"><Icon name="Search" size={14}/><input aria-label="Поиск файлов в папке" placeholder="Поиск файлов…" value={search} onChange={e=>setSearch(e.target.value)}/></label>
   <Button icon="FolderPlus" disabled={disabled||busy} onClick={()=>setNewFolder('')}>Папка</Button><Button icon="Upload" disabled={disabled||busy} onClick={()=>input.current.click()}>{busy?'Импорт…':'Импорт файлов'}</Button><Button icon="FolderInput" disabled={disabled||busy} onClick={()=>directory.current.click()}>Импорт папки</Button>
   <input ref={input} type="file" multiple hidden onChange={e=>{upload(e.target.files);e.target.value='';}}/><input ref={directory} type="file" multiple webkitdirectory="" hidden onChange={e=>{upload(e.target.files);e.target.value='';}}/>
  </header>
  {newFolder!==null&&<form className="asset-folder-form" onSubmit={createFolder}><label>Имя папки<input autoFocus aria-label="Имя новой папки" value={newFolder} onChange={e=>setNewFolder(e.target.value)}/></label><Button type="submit" disabled={disabled||!newFolder.trim()}>Создать</Button><Button type="button" onClick={()=>setNewFolder(null)}>Отмена</Button></form>}
  {error&&<p role="alert" className="asset-explorer-error">{error}</p>}
  <div className="asset-explorer-body"><nav className="asset-folder-tree" aria-label="Папки проекта"><button className={'asset-root '+(!path?'selected':'')} onClick={()=>navigate('')}><Icon name="FolderOpen" size={16}/>Assets</button>{tree()}</nav>
   <div className="asset-file-area" onDragOver={e=>e.preventDefault()} onDrop={e=>{e.preventDefault();upload(e.dataTransfer.files);}}>
    <div className="asset-file-grid" aria-label="Файлы и папки">{contents.folders.map(item=><button key={item} className="asset-file-tile folder" onClick={()=>navigate(item)}><Icon name="Folder" size={34}/><strong>{item.split('/').at(-1)}</strong><small>Папка</small></button>)}
     {contents.files.map(item=><button key={item.id} className={'asset-file-tile '+(selected===item.id?'selected':'')} aria-pressed={selected===item.id} onClick={()=>setSelected(item.id)} onDoubleClick={()=>!disabled&&open(item)}>
      {item.kind==='image'&&!/\.svg$/i.test(item.name)?<img src={item.src} alt=""/>:<Icon name={icons[item.kind]||'File'} size={32}/>}<strong>{item.name}</strong><small>{item.builtin?'Встроенный файл':item.bytes!=null?`${(item.bytes/1024).toFixed(1)} КБ`:'Файл проекта'}</small>{search&&<small>{item.path}</small>}
     </button>)}
    </div>
    {!contents.files.length&&!contents.folders.length&&<div className="asset-explorer-empty"><Icon name="FolderOpen" size={35}/><strong>{search?'Файлы не найдены':'Папка пуста'}</strong><p>{search?'Попробуйте другое имя или откройте Assets.':'Перетащите файлы сюда или используйте импорт. Структура импортированной папки сохранится.'}</p></div>}
   </div>
  </div>
  <footer className="asset-explorer-details">{file?<><Icon name={icons[file.kind]||'File'} size={16}/><span><strong>{file.name}</strong><small>{file.path}{file.users.length?` · Используется: ${file.users.map(user=>user.name).join(', ')}`:''}</small></span>{file.kind==='model'&&<Button icon="Plus" disabled={disabled||busy} onClick={()=>onPlaceModel(file)}>Добавить в сцену</Button>}{file.users.map(user=><Button key={user.id} icon="Focus" onClick={()=>onSelectObject(user.id)}>{user.name}</Button>)}{file.audioId&&<Button icon="Music2" onClick={onAudio}>Открыть звук</Button>}</>:<span>{files.length} файлов в проекте · выберите файл для подробностей · импорт до 3 МБ на файл</span>}</footer>
 </section>;
}
