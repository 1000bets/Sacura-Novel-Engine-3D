import InputWorkspace,{InputContextSettings} from './InputWorkspace.jsx';
import TouchInput from './TouchInput.jsx';
import GameplayWorkspace from './GameplayWorkspace.jsx';
import GameHUD from './GameHUD.jsx';
import {exportPlayableGame} from './gameExport.js';
import {builtinTemplates,loadBuiltinTemplate} from './builtinTemplates.js';
import {inputContextsFor,inputHelp} from './inputModel.js';
import LanguagePicker from './LanguagePicker.jsx';
import WidgetWorkspace from './WidgetWorkspace.jsx';
import WidgetRenderer,{WidgetMenu} from './WidgetRenderer.jsx';
import WidgetAssignment from './WidgetAssignment.jsx';
import {resolveWidget,createDialogueVariant,isUnmodifiedDefaultWidget} from './widgetModel.js';
import {t as tr, useLocale, literalLabel, localeTag, message} from './i18n.jsx';
import InteractionInspector from './InteractionInspector.jsx';
import {interactionTargets} from './interactionModel.js';
import FileInspector from './FileInspector.jsx';
import {collectAssetFiles,updateAssetFile} from './assetFiles.js';
import {moveEventBinding} from './eventPlacement.js';
import {createObjectGroup,addObjectsToGroup,removeObjectsFromGroups,sceneObjectGroups} from './objectGroups.js';
import {validateImportSize} from './fileLimits.js';
import {projectAudioAssets,validateImportedAudio} from './audioAssets.js';
import {accountStorage} from './accountStorage.js';
import {serverStorageEnabled,saveServerProject,listServerProjects,openServerProject,portableProject} from './serverStorage.js';
import AssetExplorer from './AssetExplorer.jsx';
import {storeAssetFile,retainObjectModel} from './assetFiles.js';
import {beatPreview} from './storyLabels.js';
import {editVariable} from './variableModel.js';
import VariableInspector from './VariableInspector.jsx';
import {isPureNode,migrateLogicGraph,compareSymbols,variablePalette,initializeVariableNode,MATH_OPERATIONS,logicType,dataConnectionError} from './logicModel.js';
import {newChoice,ANSWER_VARIABLE,typedValue} from './choiceModel.js';
import {copySceneObjects,pasteSceneObjects,cloneStoryNodes,clipboardCommand} from './editorClipboard.js';
import {copyAction,copyGroup} from './authoringModel.js';
import {playbackStartId,deleteStoryNode,insertStoryNode,changeStoryNodeKind,STORY_NODE_KINDS} from './storyEditing.js';
import {applyConnections,connectionError} from './storyConnections.js';
import {InfoView, FloatingWindow} from './EditorHelp.jsx';
import './editorHelp.css';
import ThemePicker from './ThemePicker.jsx';
import {normalizeSidechain} from './audioSettings.js';
import React, {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  allBeats,
  upgradeProject,
  sceneFor,
  PHASES,
  TYPES,
  JOIN_LABELS,
  batchesFor,
  bindingActions,
  normalizeBatches,
  addToBatch,
  removeEventBinding,
  newEvent,
  makeAction,
  uid,
  AUDIO_ASSETS,
  conditionPass,
  validateStudio,
  fixStudio,
} from "./studioModel.js";
import { fixIssue } from "./model.js";
import {readProject} from './projectFiles.js';
import {rememberLocalProject,BACKUP_KEY,PROJECT_FILE_TYPES,backupProject,copyProjectAs,createEmptyProject,createProjectTemplate,parseProjectFile,projectFilename,projectFromTemplate,readTemplates,storeTemplate,writeProjectFile} from './projectLifecycle.js';
import ProjectDialog from './ProjectDialog.jsx';
import { PreviewRuntime } from "./runtime.js";
import { soundDesk, clockLabel } from "./audio.js";
import {
  Icon,
  Button,
  Field,
  Select,
  SoundWorkspace,
  Waveform,
  EventEditor,
} from "./StudioParts.jsx";
import LocationScene from "./LocationScene.jsx";
import EditorGraph from "./EditorGraph.jsx";
import EditorContextMenu from "./EditorContextMenu.jsx";
import ActionFields from "./ActionFields.jsx";
import EventInspector from "./EventInspector.jsx";
import {exportLocation,importLocation} from "./locationFiles.js";
import FailureLab from "./FailureLab.jsx";
import SubsceneWorkspace from './SubsceneWorkspace.jsx';
import CharacterWorkspace from './CharacterWorkspace.jsx';
import {createCharacter,setCharacterInScene} from './characterModel.js';
import {createSubscene,cloneSubscene,setSceneEntry,connectSubscene,changeSceneLocation,newSceneDraft,LOCATION_PRIMITIVES} from './subsceneModel.js';
import EventPlayground from './EventPlayground.jsx';
import AuthoringWorkspace from './AuthoringWorkspace.jsx';
import TransformInspector from './TransformInspector.jsx';
import CollisionInspector from './CollisionInspector.jsx';
import MeshInspector from './MeshInspector.jsx';
import MultiTransformInspector from './MultiTransformInspector.jsx';
import {selectedObjectIds,selectSceneObject,transformSelection} from './sceneSelection.js';
import CameraWorkspace from './CameraWorkspace.jsx';
import CameraInspector from './CameraInspector.jsx';
import {newCamera,cleanCamera,cameraFromView,cameraPose,resolveCamera} from './cameraModel.js';
import {objectTransform,setObjectTransform,isObjectInScene,sceneStagingPoints} from './sceneEditing.js';
import HierarchyTree from './HierarchyTree.jsx';
import EditorSelect from './EditorSelect.jsx';
import StoryFlow from './StoryFlow.jsx';

const PROJECT_KEY = "sacura-studio-v2",
  LAYOUT_KEY = "sacura-workspace-v3";
const defaultSceneHeight = () =>
  Math.max(260, Math.round(window.innerHeight * 0.54));
const initialLayout = {
  left: 210,
  right: 280,
  height: defaultSceneHeight(),
  scope: "chapter",
  positions: {},
  treeOpen: {},
  hiddenHierarchy:false,
  hiddenInspector:false,
};
function loadProject() {
  const raw =
    accountStorage.getItem(PROJECT_KEY) ||
    accountStorage.getItem("sacura-ui-project-v1");
  if (!raw) return upgradeProject();
  const p = JSON.parse(raw);
  if (
    ![1, 2].includes(p.version) ||
    !p.chapters?.length ||
    !p.events ||
    !p.objects
  )
    throw new Error("Неподдерживаемый формат сохранения");
  if (p.version === 1 && !accountStorage.getItem("sacura-ui-project-v1-backup"))
    accountStorage.setItem("sacura-ui-project-v1-backup", raw);
  return readProject(p);
}
function Fold({ title, icon, children, open = true, extra }) {
 useLocale();
  return (
    <details className="inspector-section" open={open}>
      <summary>
        <Icon name="ChevronRight" size={12} />
        <span>{tr(title)}</span>
        {extra}
      </summary>
      <div className="inspector-fields">{children}</div>
    </details>
  );
}
function ResizeBar({ axis, onMove, onReset, label }) {
 useLocale();
  const drag = useRef();
  return (
    <div
      role="separator"
      tabIndex={0}
      aria-label={tr(label)}
      aria-orientation={axis === "x" ? "vertical" : "horizontal"}
      className={"splitter " + axis}
      onDoubleClick={onReset}
      onKeyDown={(e) => {
        const d = ["ArrowLeft", "ArrowUp"].includes(e.key)
          ? -16
          : ["ArrowRight", "ArrowDown"].includes(e.key)
            ? 16
            : 0;
        if (d) {
          e.preventDefault();
          onMove(d);
        }
      }}
      onPointerDown={(e) => {
        e.currentTarget.setPointerCapture(e.pointerId);
        drag.current = axis === "x" ? e.clientX : e.clientY;
        document.body.classList.add("resizing-" + axis);
      }}
      onPointerMove={(e) => {
        if (drag.current == null) return;
        const pos = axis === "x" ? e.clientX : e.clientY,
          delta = pos - drag.current;
        drag.current = pos;
        onMove(delta);
      }}
      onPointerUp={(e) => {
        drag.current = null;
        document.body.classList.remove("resizing-" + axis);
        e.currentTarget.releasePointerCapture(e.pointerId);
      }}
      onPointerCancel={() => {
        drag.current = null;
        document.body.classList.remove("resizing-" + axis);
      }}
    >
      <span />
    </div>
  );
}

function GameDialogue({
  project,
  beat,
  preview,
  running,
  variables,
  onAdvance,
  onStart,
  show,
  objectLabel,
  widgetOverride,
  onUiAction,
}) {
 useLocale();
  if(beat?.kind==='gameplay')return null;
  if (!show || !beat || (running && !preview.textVisible)) return null;
  if (preview?.phase === "FINISHED" && running)
    return (
      <div className="game-ending">
        <Icon name="Flower2" size={29} />
        <span>{tr("Конец истории")}</span>
        <strong>{preview.ending}</strong>
        <Button onClick={onStart}>{tr("Сыграть ещё раз")}</Button>
      </div>
    );
  const ready = !running || (preview.ready && !preview.paused);
  const canAdvance=ready&&!['choice','gate','gameplay'].includes(beat.kind);
  const dialogueWidget=widgetOverride||resolveWidget(project,'dialogue',beat);
  if (beat.kind==='gate' && beat.controls?.mode && beat.controls.mode!=='none' && isUnmodifiedDefaultWidget(dialogueWidget)) {
    const help=inputHelp(project.input,inputContextsFor(project,beat),beat.controls.mode==='point-click'?['interact','point']:['move','interact','point']);
    return <div className="game-task">
      <strong>{beat.text}</strong>
      <div><small>{tr(help)}</small><span className="interaction-checklist" role="status" aria-label={tr('Осмотренные предметы')}>
        {interactionTargets(beat).map(id=><span key={id} className={preview?.interacted?.includes(id)?'done':''}>{project.objects.find(o=>o.id===id)?.name||id}</span>)}
      </span></div>
      {preview?.hint&&<small className="interaction-hint">{message(preview.hint)}</small>}
    </div>;
  }
  const status=beat.kind==='gate'?(message(preview?.hint)||(beat.controls?.mode&&beat.controls.mode!=='none'?inputHelp(project.input,inputContextsFor(project,beat),['move','interact','point']):`${tr('Нажмите на объект')} «${objectLabel||beat.signal}»`)):
    !running?tr('Предпросмотр выбранной реплики'):!preview.ready?tr('Выполняются действия'):beat.kind==='choice'?tr('Выберите ответ'):tr('Нажмите, чтобы продолжить');
  return <WidgetRenderer widget={dialogueWidget} context={{project,variables,game:preview?.world.game,speaker:beat.speaker,text:beat.text,status,title:project.title,choices:beat.kind==='choice'?beat.choices:[],selectedChoiceId:preview?.inputChoiceId,ready,canAdvance}}
    onAction={(action,element)=>{if(action==='advance'&&canAdvance)running?onAdvance():onStart();else if(action!=='advance')onUiAction?.(action,element);}}
    onChoice={id=>running&&onAdvance(id)} choiceEnabled={choice=>conditionPass(choice,variables,project,preview?.choiceResults)}/>;
}

// Keep everyday workspaces visible; secondary tools remain one click away.
const dockTools = [
  ["story", "Поток истории"], ["subscenes", "Сабсцены"], ["characters", "Персонажи"],
  ["widgets", "Виджеты UI"], ["gameplay", "Игровые механики"], ["cameras", "Камеры"], ["input", "Ввод"], ["files", "Проводник"], ["active", "Звук и окружение"], ["event", "Событие"],
  ["create", "Создание"], ["assets", "Проект"], ["sound", "Звук"],
  ["samples", "Эффекты"], ["issues", "Проблемы"],
];
function DockTools({ dock, event, issueCount, onSelect }) {
 useLocale();
  const [open, setOpen] = useState(false);
  const [opensUp, setOpensUp] = useState(false);
  const popup = useRef(null), trigger = useRef(null);
  const secondary = dockTools.slice(6);
  const current = secondary.find(([id]) => id === dock);
  useEffect(() => {
    if (!open) return;
    const outside = e => { if (!popup.current?.contains(e.target)) setOpen(false); };
    const escape = e => {
      if (e.key === "Escape" && !e.defaultPrevented) { e.stopPropagation(); setOpen(false); trigger.current?.focus(); }
    };
    document.addEventListener("pointerdown", outside);
    document.addEventListener("keydown", escape, true);
    return () => {
      document.removeEventListener("pointerdown", outside);
      document.removeEventListener("keydown", escape, true);
    };
  }, [open]);
  return <>
    {dockTools.slice(0, 6).map(([id, label]) => <button key={id}
      aria-pressed={dock === id} className={dock === id ? "active" : ""}
      onClick={() => onSelect(id)}>{tr(label)}</button>)}
    <div className={"dock-more" + (opensUp ? " opens-up" : "")} ref={popup} onBlur={e => {
      if (!e.currentTarget.contains(e.relatedTarget)) setOpen(false);
    }}>
      <button ref={trigger} className={current ? "active" : ""}
        aria-expanded={open} aria-controls={open ? "dock-more-tools" : undefined}
        onClick={e => {setOpensUp(window.innerHeight - e.currentTarget.getBoundingClientRect().bottom < 270); setOpen(v => !v);}}>{tr("Ещё")}<Icon name="ChevronDown" size={12}/></button>
      {open && <div className="dock-more-popup" id="dock-more-tools" aria-label={tr("Другие инструменты")}>
        {secondary.map(([id, label]) => <button key={id} disabled={id === "event" && !event}
          aria-pressed={dock === id} onClick={() => {onSelect(id); setOpen(false); trigger.current?.focus();}}>
          <span>{tr(label)}</span>{id === "issues" && issueCount > 0 && <span className="tool-issue-count">{issueCount}</span>}
          {dock === id && <Icon name="Check" size={13}/>}
        </button>)}
      </div>}
    </div>
  </>;
}

export default function Editor({initialProject=null,user=null,onLogout,onHome}) {
 useLocale();
  const clipboard=useRef(null);
  const seed = useRef();
  if (!seed.current) {
    try {
      seed.current = { project: migrateLogicGraph(initialProject?readProject(initialProject):loadProject()) };
    } catch (e) {
      seed.current = { project: migrateLogicGraph(upgradeProject()), error: e.message };
    }
  }
  seed.current.project.id ||= uid('project');
  const initialBeat=seed.current.project.subscenes[0].entry||allBeats(seed.current.project)[0].id;
  const [project, setProject] = useState(seed.current.project),
    [saveError, setSaveError] = useState(seed.current.error),
    [layout, setLayout] = useState(() => {
      try {
        return {
          ...initialLayout,
          ...JSON.parse(accountStorage.getItem(LAYOUT_KEY) || "{}"),
        };
      } catch {
        return initialLayout;
      }
    }),
    [selectedBeat, setSelectedBeat] = useState(initialBeat),
    [selection, setSelection] = useState({ kind: "beat", id: initialBeat }),
    [dock, setDock] = useState(seed.current.project.gameplay?.enabled ? "gameplay" : "story"),
    [phase, setPhase] = useState("ALL"),
    [eventContext, setEventContext] = useState(null),
    [mode, setMode] = useState("scene"),
    [showDialogue, setShowDialogue] = useState(true),
    [showGrid, setShowGrid] = useState(true),
    [maximized, setMaximized] = useState(seed.current.project.gameplay?.enabled ? "graph" : null),
    [preview, setPreview] = useState(null),
    [follow, setFollow] = useState(true),
    [query, setQuery] = useState(""),
    [historyQuery, setHistoryQuery] = useState(""),
    [floatingDock, setFloatingDock] = useState(false),
    [historyOpen, setHistoryOpen] = useState(false),
    [compactPanel,setCompactPanel]=useState('workspace'),
    [picker, setPicker] = useState(null),
    [characterDraft,setCharacterDraft]=useState(null),
    [contextMenu,setContextMenu]=useState(null),
    [contextNodeId,setContextNodeId]=useState(null),
    [menu, setMenu] = useState(null),
    [menuAnchor, setMenuAnchor] = useState({left: 100, top: 40}),
    [projectDialog,setProjectDialog]=useState(null),
    [projectFileError,setProjectFileError]=useState(''),
    [projectFileBusy,setProjectFileBusy]=useState(false),
    [projectTemplates,setProjectTemplates]=useState([]),
    [notice, setNotice] = useState(""),
    [assetCategory, setAssetCategory] = useState("events"),
    [audioTick, setAudioTick] = useState(0),
    [inspectorPinned, setInspectorPinned] = useState(false),
    [detailEditor, setDetailEditor] = useState(false),
    [history, setHistory] = useState([]),
    [authorRequest,setAuthorRequest]=useState(null),
    [widgetRequest,setWidgetRequest]=useState(null),
    [gameMenu,setGameMenu]=useState(null),
    [previewWidgetId,setPreviewWidgetId]=useState(null),
    [subsceneRequest,setSubsceneRequest]=useState(null),
    [subsceneDraft,setSubsceneDraft]=useState(null),
    [editTool,setEditTool]=useState("translate"),
    [editSpace,setEditSpace]=useState("world"),
    [snap,setSnap]=useState(false),
    [focusRequest,setFocusRequest]=useState(0),
    [cameraPilotId,setCameraPilotId]=useState(null),
    [cameraPreviewId,setCameraPreviewId]=useState(null),
    [showCameras,setShowCameras]=useState(true);
  const currentProject=useRef(project);currentProject.current=project;
  const root = useRef(),
    fileInput = useRef(),
    projectFile = useRef(null),
    fileOperation = useRef(false),
    graphApi = useRef(),
    cameraApi = useRef(),
    physicsApi = useRef(),
    previewRef = useRef(),
    runtime = useRef();
  if (!runtime.current)
    runtime.current = new PreviewRuntime(soundDesk, setPreview);
  const rt = runtime.current;
  rt.physicsApi=physicsApi;
  previewRef.current = preview;
  const nodes = useMemo(() => allBeats(project), [project]),
    beat = nodes.find((b) => b.id === selectedBeat) || nodes[0],
    scene = sceneFor(project, beat.id),
    chapter = project.chapters.find((c) =>
      c.beats.some((b) => b.id === beat.id),
    ),
    running = rt.running && preview?.phase !== "EDIT";
  const hierarchyVisible=maximized==='hierarchy'||(!['scene','graph','inspector'].includes(maximized)&&!layout.hiddenHierarchy&&(window.innerWidth>1000||compactPanel==='hierarchy'));
  const inspectorVisible=maximized==='inspector'||(!['scene','graph','hierarchy'].includes(maximized)&&!layout.hiddenInspector&&(window.innerWidth>1000||compactPanel==='inspector'));
  const playProject = running ? rt.project : project,
    playBeat = running
      ? allBeats(playProject).find((b) => b.id === preview.beatId) || beat
      : beat,
    playScene = sceneFor(playProject, playBeat.id),
    displayScene = running ? playScene : scene;
  useEffect(()=>setCompactPanel('workspace'),[dock,displayScene.id]);
  useEffect(()=>{
    if(rt.invalidateStoryPreview(project))setNotice('Маршрут истории изменён. Предпросмотр остановлен — нажмите «Запуск» или «С реплики», чтобы проверить новые связи.');
  },[project,rt]);
  const [inputDevices,setInputDevices]=useState([]);
  const world = useMemo(
    () =>
      running
        ? {
            ...preview.world,
            paused: preview.paused,
            inputActive:!preview.audition&&!['FINISHED','ERROR'].includes(preview.phase)&&gameMenu!=='mainMenu',
            inputReady:preview.ready,
            inputContexts:inputContextsFor(playProject,playBeat),
            weatherPaused: preview.effects.weather?.status === "paused",
            particlesPaused:preview.effects.particles?.status==='paused',
            interactionTarget:
              preview.phase === "WAITING_OBJECT" && !preview.world.playerControl ? playBeat.signal : null,
            interactionTargets:preview.phase==='WAITING_OBJECT'?interactionTargets(playBeat).filter(id=>!preview.interacted?.includes(id)):[],
          }
        : {
            weather: scene.weather,
            time: scene.time,
            location: scene.id,
            camera: "Общий план",
            positions: {},
            poses: {},
            visible: {},
          },
    [running, preview, scene, playBeat, playProject, gameMenu],
  );
  const objects = useMemo(
    () =>
      playProject.objects.filter(
        (o) => isObjectInScene(o,displayScene),
      ),
    [playProject, displayScene],
  );
  const objectSelectionIds=selectedObjectIds(selection,objects),selectedObjects=objectSelectionIds.map(id=>objects.find(object=>object.id===id));
  useEffect(()=>setSelection(current=>{
    if(current.kind!=='object')return current;
    const ids=selectedObjectIds(current,objects),previous=selectedObjectIds(current);
    if(ids.length===previous.length&&ids.includes(current.id))return current;
    return ids.length?{kind:'object',id:ids.includes(current.id)?current.id:ids.at(-1),ids}:{kind:'scene',id:displayScene.id};
  }),[objects,displayScene.id]);
  const [positionPick,setPositionPick]=useState(null);
  const [eventSearch,setEventSearch]=useState('');
  useEffect(()=>{if(picker?.kind==='event')setEventSearch('');},[picker?.kind,picker?.phase,picker?.batchId]);
  useEffect(()=>{const cancel=e=>{if(e.key==='Escape')setPositionPick(null);};window.addEventListener('keydown',cancel);return ()=>window.removeEventListener('keydown',cancel);},[]);
  const issues = useMemo(() => validateStudio(project), [project]),
    variables = running ? preview.variables : project.variables,
    event = project.events.find((e) => e.id === eventContext?.eventId),
    contextBeat = nodes.find((b) => b.id === eventContext?.beatId) || beat,
    binding = contextBeat.bindings.find(
      (b) => b.id === eventContext?.bindingId,
    ),
    inspectedObject = project.objects.find((o) => o.id === selection.id),
    inspectedAction =
      selection.kind === "action"
        ? (binding
            ? bindingActions(project, binding)
            : event?.groups.flatMap((g) => g.actions)
          )?.find((a) => a.id === selection.id)
        : null;
  useEffect(() => {
    if (!project.actionTemplates || !project.groupTemplates || (project.sceneEditingVersion||0)<2 || project.subscenes.some(s=>!Array.isArray(s.cameras)||!Array.isArray(s.stagingPoints)))
      setProject(p => upgradeProject(p));
  }, [project]);
  useEffect(() => {
    if (seed.current.error) return;
    try {
      if(serverStorageEnabled)accountStorage.setItem(PROJECT_KEY, JSON.stringify(project));
      else rememberLocalProject(accountStorage,project);
      if(!serverStorageEnabled)setSaveError(null);
    } catch {
      setSaveError("Не удалось сохранить. Экспортируйте проект.");
    }
  }, [project]);
  useEffect(() => {
    if(!serverStorageEnabled||seed.current.error)return;
    setSaveError('Ожидает сохранения на сервере…');
    let active=true;
    const timer=setTimeout(()=>{
      saveServerProject(project).then(saved=>{
        if(active){setSaveError(null);if(JSON.stringify(saved)!==JSON.stringify(project))setProject(current=>current===project?saved:current);}
      }).catch(error=>{if(active)setSaveError(error.message);});
    },1200);
    return()=>{active=false;clearTimeout(timer);};
  },[project]);
  useEffect(() => {
    accountStorage.setItem(LAYOUT_KEY, JSON.stringify(layout));
  }, [layout]);
  useEffect(() => soundDesk.subscribe(() => setAudioTick((n) => n + 1)), []);
  useEffect(()=>soundDesk.setAssets(projectAudioAssets(project)),[project]);
  useEffect(() => rt.setSidechain(project.audioSettings?.sidechain), [project.audioSettings?.sidechain]);
  useEffect(() => {
    if (running && follow && preview.beatId) {
      setSelectedBeat(preview.beatId);
      if (!inspectorPinned) setSelection({ kind: "beat", id: preview.beatId });
    }
  }, [running, preview?.beatId, follow, inspectorPinned]);
  useEffect(() => {
    if (!notice) return;
    const t = setTimeout(() => setNotice(""), 4200);
    return () => clearTimeout(t);
  }, [notice]);
  useEffect(() => () => rt.stop(), []);
  const mutate = useCallback(
    (fn) =>
      setProject((p) => {
        const next = structuredClone(p);
        fn(next);
        allBeats(next).forEach(normalizeBatches);
        if (JSON.stringify(next) === JSON.stringify(p)) return p;
        setHistory((h) => [...h.slice(-19), p]);
        return next;
      }),
    [],
  );
  const importProjectAudioFiles=async files=>{
    await validateImportedAudio(files,soundDesk.output);
    return changeAssetLibrary(p=>files.map(file=>storeAssetFile(p,file)));
  };
  const changeAssetLibrary=operation=>{
    if(rt.running)throw new Error('Остановите предпросмотр перед импортом.');
    const next=structuredClone(currentProject.current),result=operation(next);
    try{if(!serverStorageEnabled)accountStorage.setItem(PROJECT_KEY,JSON.stringify(next));}catch{throw new Error('Не хватает места для автосохранения. Уменьшите файлы или экспортируйте проект.');}
    mutate(operation);return result;
  };
  const placeLibraryModel=file=>{
    if(rt.running||!file.model)return;
    const id=uid('object'),model={...file.model,src:file.src};
    try{changeAssetLibrary(p=>p.objects.push({id,type:'Меш',name:file.name.replace(/\.[^.]+$/,''),color:'#bb99aa',active:true,subsceneId:scene.id,primitive:'box',model,transforms:{[scene.id]:{position:[0,0,1],rotation:[0,0,0],scale:[1,1,1]}}}));
      editScene();setSelection({kind:'object',id});setFocusRequest({nonce:uid('focus')});setNotice('Модель добавлена в сцену.');
    }catch(error){setNotice(error.message);}
  };
  const selectBeat = useCallback(
    (id) => {
      setSelectedBeat(id);
      if (!inspectorPinned) setSelection({ kind: "beat", id });
      if (rt.running) setFollow(false);
    },
    [inspectorPinned],
  );
  const setPositions = useCallback(
    (key, value) =>
      setLayout((l) => ({ ...l, positions: { ...l.positions, [key]: value } })),
    [],
  );
  const onGraphReady = useCallback((api) => {
    graphApi.current = api;
  }, []);
  const openStaging = useCallback((id, ph = "ALL") => {
    setSelectedBeat(id);
    setDock("story");
    setPhase(ph);
    setDetailEditor(false);
    setSelection({ kind: "beat", id });
    graphApi.current?.focus(id);
  }, []);
  const openGraph = useCallback(
    (data) => {
      if (data.kind === "portal") {
        selectBeat(data.beat.id);
        setDock("story");
        return;
      }
      if (data.beat) {
        openStaging(data.beat.id);
        return;
      }
      if (data.kind === "event") {
        setEventContext({
          eventId: data.event.id,
          bindingId: data.binding.id,
          beatId: data.flowBeatId || beat.id,
        });
        if(data.flowBeatId)setSelectedBeat(data.flowBeatId);
        setSelection({ kind: "binding", id: data.binding.id });
        setCompactPanel("inspector");
        return;
      }
      if (data.action) {
        setSelection({ kind: "action", id: data.action.id });
      }
    },
    [beat.id, openStaging, selectBeat],
  );
  const graphSelect = useCallback(
    (value) => {
      if (value?.flowBeatId) setSelectedBeat(value.flowBeatId);
      if (typeof value === "string") {
        selectBeat(value);
        return;
      }
      if (value.batchId && !value.binding)
        setSelection({ kind: "batch", id: value.batchId });
      else if (value.action)
        setSelection({ kind: "action", id: value.action.id });
      else if (value.binding) {
        setCompactPanel("inspector");
        setSelection({ kind: "binding", id: value.binding.id });
        setEventContext({
          eventId: value.event.id,
          bindingId: value.binding.id,
          beatId: value.flowBeatId || beat.id,
        });
      } else if (value.groupId)
        setSelection({ kind: "group", id: value.groupId });
    },
    [selectBeat, beat.id],
  );
  const changeBatch = useCallback(
    (batchId, command, value, ownerId = beat.id) => {
      const owner = nodes.find(b=>b.id===ownerId) || beat;
      const phaseId = PHASES.find((ph) =>
        owner.batches?.[ph.id]?.some((b) => b.id === batchId),
      )?.id;
      if (!phaseId) return;
      if (command === "select") {
        setSelectedBeat(ownerId);
        setSelection({ kind: "batch", id: batchId });
        return;
      }
      if (command === "add") {
        setSelectedBeat(ownerId);
        setPicker({ kind: "event", batchId, phase: phaseId });
        return;
      }
      if(command==='removeBinding'&&selection.id===value){setEventContext(null);setSelection({kind:'beat',id:ownerId});}
      mutate((p) => {
        const b = allBeats(p).find((b) => b.id === ownerId),
          list = b.batches[phaseId],
          index = list.findIndex((g) => g.id === batchId),
          batch = list[index];
        if (command === "mode") batch.mode = value;
        if (command === "joinPrevious" && index > 0) {
          list[index - 1].bindingIds.push(...batch.bindingIds);
          list[index - 1].mode = "PARALLEL";
          list.splice(index, 1);
        }
        if (command === "move" && list[index + value]) {
          [list[index], list[index + value]] = [
            list[index + value],
            list[index],
          ];
        }
        if (command === "removeBinding") {
          removeEventBinding(p,ownerId,value);
        }
      });
    },
    [beat, nodes, mutate, selection.id],
  );
  const patchAction = useCallback(
    (id, values) =>
      mutate((p) => {
        if (eventContext?.bindingId) {
          const b = allBeats(p)
            .find((b) => b.id === eventContext.beatId)
            ?.bindings.find((b) => b.id === eventContext.bindingId);
          b.actionOverrides ??= {};
          b.actionOverrides[id] = { ...b.actionOverrides[id], ...values };
        } else {
          const a = p.events
            .find((e) => e.id === eventContext?.eventId)
            ?.groups.flatMap((g) => g.actions)
            .find((a) => a.id === id);
          if (a) Object.assign(a, values);
        }
      }),
    [eventContext, mutate],
  );
  const connect = useCallback(
    (c) => {
      const changes=(c.changes || [c]).map(change=>({...change,sourceHandle:change.sourceHandle||'next',target:change.target?.replace(/^(portal:|missing:)/,'')||null}));
      const error=connectionError(project,changes);
      if(error){setNotice(error);return;}
      mutate(p=>applyConnections(p,changes));
      setNotice("Связь изменена.");
    },
    [mutate, running, project],
  );
  const patchBeat = (values) =>
    mutate((p) =>
      Object.assign(
        allBeats(p).find((b) => b.id === beat.id),
        values,
      ),
    );
  const deleteFlowNode = ids => {
    if(rt.running){setNotice('Остановите предпросмотр, чтобы удалить ноды.');return;}
    const next=structuredClone(project);let result;
    for(const id of Array.isArray(ids)?ids:[ids]){
      result=deleteStoryNode(next,id);
      if(result.error){setNotice(result.error);return;}
    }
    if(!result)return;
    mutate(p=>Object.assign(p,next));
    setSelectedBeat(result.nextId);setSelection({kind:'beat',id:result.nextId});
    setNotice('Выделенные ноды и их связи удалены. Ctrl + Z — отменить.');
  };
  const nodeKindField=()=> <Field label={tr("Тип блока")}><Select value={beat.kind} options={STORY_NODE_KINDS} onChange={kind=>mutate(p=>changeStoryNodeKind(allBeats(p).find(b=>b.id===beat.id),kind))}/></Field>;
  const createSpeaker=()=>{
    const name=characterDraft?.trim();if(!name||running)return;
    mutate(p=>{const character=createCharacter(p,sceneFor(p,beat.id).id,name);allBeats(p).find(b=>b.id===beat.id).speaker=character.name;});
    setCharacterDraft(null);setNotice(tr("Персонаж «{0}» создан и выбран для реплики.", [name]));
  };
  const openWidgets=id=>{setWidgetRequest({id,token:uid("widget-request")});setDock("widgets");setDetailEditor(false);setFloatingDock(true);setMaximized(null);setCompactPanel("workspace");};
  const customizeWidget=owner=>{const id=uid('widget');mutate(p=>createDialogueVariant(p,owner,id));openWidgets(id);};
  const start = (fromSelection=false,entryId=null) => {
    setGameMenu(null);setPreviewWidgetId(null);
    const startId=entryId||playbackStartId(project,beat.id,fromSelection);
    if(!startId){setNotice('Укажите начальную реплику во вкладке «Сабсцены».');return;}
    setCameraPreviewId(null);setCameraPilotId(null);
    soundDesk.unlock().catch(()=>{});
    setFollow(true);
    setMode("game");
    setMaximized(project.gameplay?.enabled ? "scene" : null);setFloatingDock(false);
    setShowDialogue(true);
    rt.start(project, startId);
  };
  rt.storage=accountStorage;
  const widgetAction=async(action,element)=>{
    if(action==='load'&&!running){setMode('game');setGameMenu(null);await rt.start(project,project.subscenes[0].entry);await rt.uiAction(action,element);return;}
    if(['event','gameplay','save','load'].includes(action)&&running){rt.storage=accountStorage;rt.uiAction(action,element);}
    if(action==='start'||action==='restart')start(false,project.subscenes[0]?.entry);
    if(action==='resume'&&preview?.paused){setPreviewWidgetId(null);rt.togglePause();}
    if(action==='mainMenu'){rt.stop();setMode('game');setGameMenu('mainMenu');setPreviewWidgetId(null);}
  };
  const audition=(eventId)=>{setCameraPreviewId(null);setCameraPilotId(null);soundDesk.unlock().catch(()=>{});setMode('game');setFollow(false);rt.previewEvent(project,eventId,beat.id);};
  const editSample=(eventId)=>{setEventContext({eventId});setSelection({kind:'event',id:eventId});setCompactPanel('inspector');setDetailEditor(false);};
  const selectObject = (id,options={}) => {
    if (!inspectorPinned) setSelection(current=>selectSceneObject(current,id,!!options.additive,objects));
  };
  const openObjectTransform = () => {
    setLayout(current=>({...current,hiddenInspector:false}));
    setCompactPanel('inspector');
    setMaximized(null);
  };
  useEffect(()=>{
    if(mode==='scene'&&!running&&selection.kind==='object'&&dock!=='characters'){
      setLayout(current=>current.hiddenInspector?{...current,hiddenInspector:false}:current);
      setCompactPanel('inspector');
    }
  },[selection.kind,selection.id,mode,running,dock]);
  const interact = (id) => {
    if(mode==='game'&&running&&preview.phase==='WAITING_INPUT'){rt.advance();return;}
    const o = objects.find((o) => o.id === id);
    if (
      mode === "game" &&
      running &&
      o?.active &&
      o.type === "Активный меш" &&
      world.visible?.[id] !== false
    )
      rt.advance(null, id);
  };
  const downloadFile = (text,filename) => {
    const a = document.createElement("a"),
      url = URL.createObjectURL(
        new Blob([text], {
          type: "application/json",
        }),
      );
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(()=>URL.revokeObjectURL(url),1000);
  };
  const exportProject = () => runFileOperation(async()=>{
    downloadFile(JSON.stringify(await portableProject(project),null,2),projectFilename(project.title));
    setMenu(null);
  });
  const replaceProject = (next,handle=null) => {
    next.id ||= uid('project');
    const entry=next.subscenes[0].entry||allBeats(next)[0].id;
    backupProject(accountStorage,project);
    rt.stop();setHistory([]);setEventContext(null);setAuthorRequest(null);setSubsceneRequest(null);setSubsceneDraft(null);
    setCameraPilotId(null);setCameraPreviewId(null);setPicker(null);setDetailEditor(false);setDock('story');setMaximized(null);setMode('scene');
    setQuery('');setFocusRequest(0);setMenu(null);setPreview(null);setFollow(true);
    setLayout(l=>({...l,positions:{},treeOpen:{}}));
    projectFile.current=handle;seed.current.error=null;
    setProject(migrateLogicGraph(next));setSelectedBeat(entry);setSelection({kind:'beat',id:entry});
  };
  const runFileOperation = async action => {
    if(fileOperation.current)return;
    fileOperation.current=true;setProjectFileBusy(true);setProjectFileError('');
    try {await action();}
    catch(err){if(err.name!=='AbortError'){const message=err.message||'Не удалось выполнить операцию с проектом.';setProjectFileError(message);setNotice(message);}}
    finally{fileOperation.current=false;setProjectFileBusy(false);}
  };
  const saveProject = () => {
    setMenu(null);
    return runFileOperation(async()=>{
      if(serverStorageEnabled){const saved=await saveServerProject(project);setProject(current=>current===project?saved:current);setSaveError(null);setNotice('Проект сохранён на сервере.');return;}
      const result=await writeProjectFile(await portableProject(project),{handle:projectFile.current,chooseFile:window.showSaveFilePicker?.bind(window),download:downloadFile,filename:projectFilename(project.title)});
      projectFile.current=result.handle;
      setNotice(result.downloaded?tr("Файл проекта передан в загрузки: {0}", [result.filename]):tr("Проект сохранён: {0}", [result.filename]));
    });
  };
  const openProjectDialog = mode => {
    if(fileOperation.current)return;
    setMenu(null);setProjectFileError('');
    try{if(mode==='new')setProjectTemplates([...builtinTemplates,...readTemplates(accountStorage)]);setProjectDialog(mode);}
    catch(err){setNotice(err.message);}
  };
  const submitProjectDialog = ({name,templateId}) => runFileOperation(async()=>{
    if(projectDialog==='new'){
      const template=projectTemplates.find(t=>t.id===templateId);if(template?.builtin)template.project=(await loadBuiltinTemplate(template.id)).project;
      const next=template?projectFromTemplate(template,name):createEmptyProject(name);
      replaceProject(next);setNotice(template?tr("Создан проект из шаблона «{0}».", [template.name]):'Создан пустой проект.');
    } else if(projectDialog==='template'){
      storeTemplate(accountStorage,createProjectTemplate(project,name));
      setNotice(tr("Шаблон «{0}» записан. Он доступен при создании нового проекта.", [name.trim()]));
    } else {
      const next=copyProjectAs(project,name);
      if(serverStorageEnabled){const saved=await saveServerProject(next);replaceProject(saved);setProjectDialog(null);setNotice('Копия проекта сохранена на сервере.');return;}
      const result=await writeProjectFile(await portableProject(next),{chooseFile:window.showSaveFilePicker?.bind(window),download:downloadFile,filename:projectFilename(next.title)});
      backupProject(accountStorage,project);projectFile.current=result.handle;seed.current.error=null;
      setProject(next);setHistory([]);
      setNotice(result.downloaded?tr("Копия проекта передана в загрузки: {0}", [result.filename]):tr("Копия проекта сохранена: {0}", [result.filename]));
    }
    setProjectDialog(null);
  });
  const [serverProjects,setServerProjects]=useState(null);
  const showServerProjects=()=>runFileOperation(async()=>{setMenu(null);setServerProjects(await listServerProjects());});
  const selectServerProject=id=>runFileOperation(async()=>{replaceProject(readProject(await openServerProject(id)));setServerProjects(null);setNotice('Проект открыт с сервера.');});
  const openProjectFile = () => {
    setMenu(null);
    if(!window.showOpenFilePicker){fileInput.current.click();return;}
    return runFileOperation(async()=>{
      const [handle]=await window.showOpenFilePicker({multiple:false,types:PROJECT_FILE_TYPES});
      const {project:next,template}=parseProjectFile(await (await handle.getFile()).text());
      replaceProject(next,template?null:handle);setNotice(template?'Создан проект из файла шаблона.':'Проект загружен.');
    });
  };
  const importProject = async (e) => {
    if(!e.target.files?.length)return;
    const file=e.target.files[0];e.target.value='';
    await runFileOperation(async()=>{
      validateImportSize(file);
      const {project:next,template}=parseProjectFile(await file.text());
      replaceProject(next);setNotice(template?'Создан проект из файла шаблона.':'Проект загружен.');
    });
  };
  const restorePreviousProject = () => runFileOperation(async()=>{
    const raw=accountStorage.getItem(BACKUP_KEY);if(!raw)throw new Error('Резервной копии пока нет.');
    replaceProject(readProject(raw));setNotice('Предыдущий проект восстановлен.');
  });
  const undo = () => {
    if (!history.length) return;
    rt.stop();setCameraPilotId(null);setCameraPreviewId(null);
    setProject(history.at(-1));
    setHistory((h) => h.slice(0, -1));
  };
  const addBeat = (kind,placement) => {
    if(rt.running)return;
    const id = uid("line");
    mutate((p) => {
      const b = {
          id,
          kind,
          speaker: "Рассказчик",
          text:
            kind === "choice"
              ? "Что вы решите?"
              : kind === "gate"
                ? "Осмотрите письмо."
                : kind === "end"
                  ? "Конец истории."
                  : "Новая реплика",
          next: null,
          bindings: [],
          batches: { BEFORE: [], ON_START: [], AFTER: [] },
        };
      if(kind==='choice'){b.choiceMode='value';b.choices=[newChoice(uid('choice'),'Ответ 1'),newChoice(uid('choice'),'Ответ 2')];}
      if(kind==='branch'){b.text='If · Если';b.next=null;b.condition=false;b.trueNext=null;b.falseNext=null;}
      if(kind==='variable'){let index=1;while(Object.hasOwn(p.variables,'Переменная '+index))index++;initializeVariableNode(p,b,placement?.variable||'Переменная '+index);}
      if(kind==='literal'){b.valueType=placement?.valueType||'number';b.value=typedValue('',b.valueType);}
      if(kind==='math'){b.operator=placement?.operator||'add';b.valueType='number';b.a=0;b.b=placement?.operator==='mul'||placement?.operator==='div'?1:0;}
      if(kind==='convert'){b.inputType=placement?.inputType||'string';b.valueType=placement?.valueType||'number';b.value=typedValue('',b.inputType);}
      if(kind==='not'){b.a=false;}
      if(kind==='compare'){b.operator=placement?.operator||'eq';b.valueType=placement?.valueType||'number';b.a=typedValue('',b.valueType);b.b=typedValue('',b.valueType);}
      if(kind==='set-variable')initializeVariableNode(p,b,placement?.variable||Object.keys(p.variables).find(id=>id!==ANSWER_VARIABLE)||'Переменная 1');
      if(isPureNode(b)||['branch','set-variable'].includes(kind))b.next=null;
      if (kind === "gate") {
        b.signal = objects.find(o=>o.type==='Активный меш'&&o.active!==false)?.id || 'letter';
        b.timeout = 30;
      }
      if (kind === "end") {
        b.next = null;
        b.ending = "Новая концовка";
      }
      insertStoryNode(p,chapter.id,beat.id,b);
      if(placement?.pin){const pin=placement.pin;
        const connection=pin.handleType==='source'?{source:pin.nodeId,sourceHandle:pin.handleId,target:id,targetHandle:placement.wireType==='flow'?'in':kind==='branch'?'condition':['set-variable','convert'].includes(kind)?'value':'a'}:{source:id,sourceHandle:placement.wireType==='flow'?'next':'value',target:pin.nodeId,targetHandle:pin.handleId};
        const error=applyConnections(p,[connection]);if(error)setNotice(error);
      }
    });
    if(placement?.position){placeContextNode(id,placement);setContextNodeId(id);}
    setSelectedBeat(id);
    setSelection({ kind: "beat", id });
    setPicker(null);
    setDock("story");
  };
  const addEvent = (id) => {
    mutate((p) =>
      addToBatch(
        p,
        beat.id,
        picker?.phase || (phase === "ALL" ? "ON_START" : phase),
        picker?.batchId || null,
        id,
      ),
    );
    setPicker(null);
    setDock("story");
    setNotice("Событие добавлено в постановку реплики.");
  };
  const openAuthoring=(kind='event',options={})=>{setAuthorRequest({kind,...options,token:uid('request')});setDock('create');setMaximized('graph');setDetailEditor(false);setEventContext(null);setPicker(null);setMenu(null);};
  const createEvent = () => openAuthoring('event');
  const editScene=()=>{setGameMenu(null);setPreviewWidgetId(null);if(rt.running){if(rt.snapshot.beatId)setSelectedBeat(rt.snapshot.beatId);rt.stop();}setCameraPilotId(null);setCameraPreviewId(null);setMode('scene');setMaximized(null);setCompactPanel('workspace');};
  const openSubscenes=(mode='edit',id=displayScene.id)=>{
    if(rt.running)rt.stop();const target=project.subscenes.find(s=>s.id===id);
    if(target&&mode!=='create'){setSelectedBeat(target.entry);setSelection({kind:'scene',id});}
    if(mode==='create')setSubsceneDraft({...newSceneDraft(project),choiceId:beat.choices?.[0]?.id||'',fromBeatId:beat.id});
    setCameraPilotId(null);setCameraPreviewId(null);setDock('subscenes');setDetailEditor(false);setMenu(null);setPicker(null);
    setSubsceneRequest({mode,token:uid('request')});setMaximized(value=>mode==='create'?'graph':value==='scene'?null:value);
  };
  const saveNewSubscene=(draft,copyId)=>{
    if(rt.running)return 'Остановите воспроизведение перед созданием сабсцены.';
    try{
      const next=structuredClone(project),created=copyId?cloneSubscene(next,copyId):createSubscene(next,draft,draft.fromBeatId||beat.id);
      mutate(p=>Object.assign(p,next));setSelectedBeat(created.entry);setSelection({kind:'scene',id:created.id});setMode('scene');setCompactPanel('workspace');
      setCameraPilotId(null);setCameraPreviewId(null);setMaximized(null);setSubsceneRequest({mode:'edit',token:uid('request')});
      setNotice(copyId?'Копия создана: сценарий, объекты и камеры независимы. Шаблоны событий общие.':'Локация создана. Добавляйте примитивы и расставляйте объекты в 3D-редакторе.');return null;
    }catch(e){setNotice(e.message);return e.message;}
  };
  const openSubsceneBeat=id=>{if(rt.running)rt.stop();setCameraPilotId(null);setCameraPreviewId(null);selectBeat(id);setDock('story');setMaximized(null);};
  const editSubscene=fn=>{if(!rt.running)mutate(fn);};
  const selectCamera=(id,{preview=true}={})=>{
    if(!id)return;
    if(preview){
      setCameraPilotId(null);setCameraPreviewId(id);setFocusRequest(0);
      if(!rt.running)setMode('scene');
      cameraApi.current?.preview(id);
    }
    setSelection({kind:'camera',id});setDetailEditor(false);
    setLayout(current=>({...current,hiddenInspector:false}));setCompactPanel('inspector');setMaximized(null);
  };
  const openCameras=()=>{setDock('cameras');selectCamera(selection.kind==='camera'?selection.id:displayScene.defaultCameraId||displayScene.cameras?.[0]?.id);};
  const changeCamera=(id,patch)=>{if(rt.running)return;mutate(p=>{const sc=p.subscenes.find(s=>s.id===displayScene.id),i=sc?.cameras?.findIndex(c=>c.id===id);if(i>=0)sc.cameras[i]=cleanCamera({...sc.cameras[i],...patch});});};
  const createCamera=(followTarget)=>{
    if(rt.running)return;const view=cameraApi.current?.capture();if(!view){setNotice('Дождитесь загрузки 3D-сцены.');return;}
    const c=newCamera(view,followTarget?'Слежение · '+followTarget.name:'Камера '+((scene.cameras?.length||0)+1));
    if(followTarget)Object.assign(c,{mode:'follow',followTargetId:followTarget.id,distance:3,height:1.7,yaw:-15,targetHeight:1.08});
    mutate(p=>{const sc=p.subscenes.find(s=>s.id===scene.id);sc.cameras||=[];sc.cameras.push(c);if(followTarget||!sc.defaultCameraId)sc.defaultCameraId=c.id;});
    selectCamera(c.id);setShowCameras(true);setNotice(followTarget?'Следящая камера назначена основной.':'Камера создана из текущего вида.');
    if(followTarget){setMode('game');setCameraPreviewId(c.id);}else{setCameraPilotId(null);setMode('scene');}
  };
  const followCharacter=()=>{const target=objects.find(o=>o.id===selection.id&&o.type==='Персонаж')||objects.find(o=>o.type==='Персонаж'&&o.active);if(target)createCamera(target);};
  const captureCamera=id=>{const c=scene.cameras?.find(c=>c.id===id),view=cameraApi.current?.capture();if(c&&view){changeCamera(id,cameraFromView(c,view,world,objects,scene.kind));setCameraPilotId(null);setNotice('Ракурс сохранён.');}};
  const pilotCamera=id=>{setFloatingDock(false);editScene();setCameraPilotId(id);setCameraPreviewId(null);selectCamera(id,{preview:false});};
  const viewCamera=id=>{setCameraPilotId(null);setCameraPreviewId(id);setMode('game');};
  const deleteCamera=id=>{if(rt.running)return;mutate(p=>{const sc=p.subscenes.find(s=>s.id===scene.id);sc.cameras=sc.cameras.filter(c=>c.id!==id);if(sc.defaultCameraId===id)sc.defaultCameraId=sc.cameras[0]?.id||null;});if(cameraPilotId===id)setCameraPilotId(null);if(cameraPreviewId===id)setCameraPreviewId(null);setSelection({kind:'scene',id:scene.id});setNotice('Камера удалена. Ctrl+Z — отменить.');};
  useEffect(()=>{setCameraPilotId(null);setCameraPreviewId(null);setFocusRequest(0);},[displayScene.id]);
  const transformObject=(id,value)=>{if(rt.running)return;mutate(p=>setObjectTransform(p,id,scene.id,value));};
  const transformObjects=changes=>{if(rt.running)return;mutate(p=>{for(const {id,value}of changes)setObjectTransform(p,id,scene.id,value);});};
  const objectClipboard=command=>{
    if(rt.running)return;
    if(command==='copy'){if(!selectedObjects.length)return;clipboard.current={kind:'objects',items:copySceneObjects(selectedObjects,scene),step:0};setNotice('Объекты скопированы. Ctrl+V — вставить.');}
    else if(clipboard.current?.kind==='objects'){const copies=pasteSceneObjects(clipboard.current.items,scene,++clipboard.current.step);mutate(p=>p.objects.push(...copies));setSelection({kind:'object',id:copies.at(-1).id,ids:copies.map(o=>o.id)});setFocusRequest({nonce:uid('focus')});setNotice('Объекты вставлены. Ctrl+Z — отменить.');}
  };
  const graphClipboard=(command,graphNodes,graphKey)=>{
    if(rt.running)return;
    if(command==='copy'){
      const chosen=graphNodes.filter(n=>n.selected),ids=chosen.map(n=>n.id);
      if(graphKey.startsWith('flow:')||graphKey.startsWith('story:')){const items=nodes.filter(b=>ids.includes(b.id));if(!items.length)return;clipboard.current={kind:'beats',items:structuredClone(items),positions:Object.fromEntries(chosen.map(n=>[n.id,n.position])),step:0};}
      else if(graphKey.startsWith('event:')&&event){const groups=event.groups.filter(g=>ids.includes(g.id)),actions=event.groups.flatMap(g=>g.actions).filter(a=>ids.includes(a.id)&&!groups.some(g=>g.actions.some(x=>x.id===a.id)));if(!groups.length&&!actions.length)return;clipboard.current={kind:'actions',groups:structuredClone(groups),actions:structuredClone(actions),positions:Object.fromEntries(chosen.map(n=>[n.id,n.position])),step:0};}
      else return;
      setNotice('Ноды скопированы. Ctrl+V — вставить.');return;
    }
    const data=clipboard.current;if(!data)return;
    if(data.kind==='beats'&&(graphKey.startsWith('flow:')||graphKey.startsWith('story:'))){
      const copies=cloneStoryNodes(data.items),step=++data.step,positions={...layout.positions[graphKey]};
      copies.forEach((copy,i)=>{const origin=data.positions[data.items[i].id]||{x:0,y:0};positions[copy.id]={x:origin.x+60*step,y:origin.y+60*step};});
      mutate(p=>{const chapter=p.chapters.find(c=>c.subsceneId===scene.id)||p.chapters.find(c=>c.beats.some(b=>b.id===beat.id));chapter.beats.push(...copies);});setPositions(graphKey,positions);setContextNodeId(copies.at(-1).id);selectBeat(copies.at(-1).id);setNotice('Ноды вставлены. Связи между копиями сохранены. Ctrl+Z — отменить.');
    }else if(data.kind==='actions'&&graphKey.startsWith('event:')&&event){
      if(binding){setNotice('Откройте исходный шаблон события, чтобы вставить ноды в его состав.');return;}
      const groups=data.groups.map(copyGroup),actions=data.actions.map(copyAction);
      mutate(p=>{const ev=p.events.find(e=>e.id===event.id);ev.groups.push(...groups);if(actions.length){if(!ev.groups.length)ev.groups.push({id:uid('group'),name:'Основное действие',actions:[]});ev.groups.at(-1).actions.push(...actions);}});
      const copied=[...groups,...actions],positions={...layout.positions[graphKey]},step=++data.step;copied.forEach((n,i)=>{const source=[...data.groups,...data.actions][i],origin=data.positions?.[source.id]||{x:100,y:100+80*i};positions[n.id]={x:origin.x+60*step,y:origin.y+60*step};});setPositions(graphKey,positions);setSelection({kind:actions.length?'action':'group',id:copied.at(-1).id});setNotice('Ноды вставлены. Ctrl+Z — отменить.');
    }
  };
  const duplicateObjects=()=>{
    if(rt.running)return;const copies=selectedObjects.map(object=>{const copy=structuredClone(object);copy.id=uid('object');copy.name=object.name+' · копия';copy.subsceneId=scene.id;copy.builtin=object.builtin||(['letter','door','fireplace','garden-note','ticket'].includes(object.id)?object.id:undefined);const value=objectTransform(object,scene.id,scene.kind);value.position[0]+=.65;copy.transforms={[scene.id]:value};return copy;});
    mutate(p=>p.objects.push(...copies));setSelection({kind:'object',id:copies.at(-1)?.id,ids:copies.map(object=>object.id)});setFocusRequest({nonce:uid('focus')});
  };
  const deleteObjects=()=>{if(rt.running)return;mutate(p=>{const removed=p.objects.filter(object=>objectSelectionIds.includes(object.id));p.objects=p.objects.filter(object=>!objectSelectionIds.includes(object.id));removed.forEach(object=>retainObjectModel(p,object));});setSelection({kind:'scene',id:scene.id});setNotice('Выделенные объекты удалены. Ctrl+Z — вернуть всю группу.');};
  const duplicateObject=(object)=>{
    const copy=structuredClone(object);copy.id=uid('object');copy.name=object.name+' · копия';copy.subsceneId=scene.id;copy.builtin=object.builtin||(['letter','door','fireplace','garden-note','ticket'].includes(object.id)?object.id:undefined);const t=objectTransform(object,scene.id,scene.kind);t.position[0]+=.65;copy.transforms={[scene.id]:t};
    mutate(p=>p.objects.push(copy));setSelection({kind:'object',id:copy.id});setFocusRequest({nonce:uid('focus')});
  };
  const deleteObject=id=>{mutate(p=>{const removed=p.objects.find(o=>o.id===id);p.objects=p.objects.filter(o=>o.id!==id);retainObjectModel(p,removed);});setSelection({kind:'scene',id:scene.id});setNotice('Объект удалён. Ctrl+Z — вернуть; ссылки в событиях видны во вкладке «Ошибки».');};
  const addStagingPoint=()=>{
    if(rt.running)return;const id=uid('point'),selectedObject=objects.find(o=>o.id===selection.id),position=contextMenu?.worldPosition||(selectedObject?objectTransform(selectedObject,scene.id,scene.kind).position:[0,0,0]);
    mutate(p=>{const target=p.subscenes.find(s=>s.id===scene.id);target.stagingPoints||=[];target.stagingPoints.push({id,label:'Новая точка постановки',position:[position[0],0,position[2]]});});
    setSelection({kind:'point',id});editScene();setPicker(null);setContextMenu(null);openObjectTransform();
    setLayout(l=>({...l,treeOpen:{...l.treeOpen,[scene.id+':points']:true}}));
  };
  const addSoundEvent=()=>{if(rt.running)return;const e=newEvent('sound','world','Звук','Новое звуковое событие');mutate(p=>p.events.push(e));setPicker(null);setContextMenu(null);editSample(e.id);setNotice('Выберите аудиофайл в инспекторе; затем добавьте событие к реплике.');};
  const addEmptyGroup=()=>{if(rt.running)return;const next=structuredClone(project),group=createObjectGroup(next,scene.id,[],'Новая группа',{allowEmpty:true});mutate(p=>Object.assign(p,next));setSelection({kind:'group',id:group.id});setLayout(l=>({...l,treeOpen:{...l.treeOpen,[scene.id+':objects']:true,[scene.id+':'+group.id]:true}}));openObjectTransform();};
  const addObject = (type,primitive="box") => {
    if(rt.running)return;
    editScene();
    const id = uid("object"),groupId=picker?.groupId||(selection.kind==='group'?selection.id:selection.groupId),position=contextMenu?.worldPosition;
    mutate((p) => {
      p.objects.push({
        id,
        type,
        name: type === "Источник света" ? "Источник освещения" : type === "Персонаж" ? "Новый персонаж" : type === "Меш" ? (LOCATION_PRIMITIVES.find(([id])=>id===primitive)?.[1]||"Новый объект") : "Интерактивный предмет",
        color: "#bb99aa",
        position: "стол",
        active: true,
        subsceneId: scene.id,
        interaction: "Осмотреть",
        primitive,
        ...(type==='Источник света'?{light:{intensity:35,distance:10,decay:2},color:'#fff0d0'}:{}),
        transforms:{[scene.id]:{position:position?[position[0],type==="Персонаж"?0:position[1],position[2]]:[0,type==="Персонаж"?0:type==='Источник света'?2:.3,1],rotation:[0,0,0],scale:[1,1,1]}},
      });
      if(groupId)addObjectsToGroup(p,groupId,[id]);
    });
    setContextMenu(null);
    setSelection({ kind: "object", id });
    setFocusRequest({nonce:uid('focus')});
    setPicker(null);
    if(type==='Персонаж'){setDock('characters');setCompactPanel('workspace');}
  };
  const patchCharacter=(id,patch)=>{
    if(rt.running)return;
    if('model' in patch||'extraAnimations' in patch){
      const next=structuredClone(project),character=next.objects.find(object=>object.id===id&&object.type==='Персонаж');
      if(!character)throw new Error('Персонаж удалён.');if('model' in patch&&character.model?.src!==patch.model?.src)retainObjectModel(next,character);Object.assign(character,patch);
      try{accountStorage.setItem(PROJECT_KEY,JSON.stringify(next));}catch{throw new Error('Не хватает места для модели в автосохранении. Уменьшите GLB или удалите неиспользуемые модели.');}
    }
    mutate(p=>{const character=p.objects.find(object=>object.id===id&&object.type==='Персонаж');if(character){if('model' in patch&&character.model?.src!==patch.model?.src)retainObjectModel(p,character);Object.assign(character,patch);}});
  };
  const duplicateCharacter=id=>{
    if(rt.running)return;const original=project.objects.find(object=>object.id===id&&object.type==='Персонаж');if(!original)return;
    const copy={...structuredClone(original),id:uid('character'),name:original.name+' · копия'};
    const next=structuredClone(project);next.objects.push(copy);for(const sc of next.subscenes)if(sc.excludedObjectIds?.includes(id))sc.excludedObjectIds.push(copy.id);
    try{accountStorage.setItem(PROJECT_KEY,JSON.stringify(next));}catch{setNotice('Не хватает места для копии модели в автосохранении.');return;}
    mutate(p=>Object.assign(p,next));setSelection({kind:'object',id:copy.id});setCompactPanel('workspace');
  };
  const attachSound = (asset, cfg) => {
    const e = newEvent(
      asset.kind === "music" ? "music" : "sound",
      asset.kind === "music" ? "audio" : "world",
      asset.caption,
      asset.name,
    );
    Object.assign(e.groups[0].actions[0], { assetId: asset.id, ...cfg });
    if (asset.kind === "music") {
      e.retention = "HOLD_UNTIL_REPLACED";
      e.owner = "Scene";
      e.channel = "Audio.BGM";
    }
    mutate((p) => {
      p.events.push(e);
      addToBatch(
        p,
        beat.id,
        asset.kind === "music" ? "BEFORE" : "ON_START",
        null,
        e.id,
      );
    });
    setDock("story");
    setNotice(tr("Звук назначен реплике {0}", [beat.id]));
  };
  const fix = (i) => {
    let next = fixStudio(project, i) || fixIssue(project, i);
    if (i.fix === "break-cycle") {
      const b = allBeats(next).find((b) => b.id === i.beatId);
      b.bindings = b.bindings.filter(
        (x) => !["wait-a", "wait-b"].includes(x.eventId),
      );
    }
    allBeats(next).forEach(normalizeBatches);
    setHistory((h) => [...h, project]);
    setProject(next);
  };
  const resize = (key, delta) =>
    setLayout((l) => ({
      ...l,
      [key]: Math.max(
        key === "height" ? 180 : key === "left" ? 180 : 260,
        Math.min(
          key === "height"
            ? Math.max(250, (root.current?.clientHeight || 900) - 365)
            : key === "left"
              ? 360
              : 480,
          l[key] + delta,
        ),
      ),
    }));
  const resetLayout = () => {
    setFloatingDock(false);
    setHistoryOpen(false);
    setLayout((l) => ({
      ...l,
      left: 210,
      right: 280,
      height: defaultSceneHeight(),
      hiddenHierarchy:false,hiddenInspector:false,
    }));
    setMaximized(null);
    setMenu(null);
  };
  useEffect(() => {
    const listener = (e) => {
      const command=clipboardCommand(e);
      if(command&&!e.target.closest?.('.react-flow, .widget-workspace')&&!contextMenu&&!picker&&!projectDialog&&!detailEditor&&!menu&&!running&&mode==='scene'&&(selection.kind==='object'||e.target.closest?.('.scene-viewport,.hierarchy'))){e.preventDefault();objectClipboard(command);return;}
      const tool = ({KeyQ:'select',KeyW:'translate',KeyE:'rotate',KeyR:'scale'})[e.code];
      if (
        tool && !e.defaultPrevented && !e.repeat &&
        !e.ctrlKey && !e.metaKey && !e.altKey && !e.shiftKey &&
        mode === 'scene' && !running && !cameraPilotId &&
        !contextMenu && !picker && !projectDialog && !detailEditor && !menu &&
        !e.target.isContentEditable &&
        !e.target.closest?.('input, textarea, select, [role="textbox"], [role="dialog"], .react-flow, .widget-workspace, .scene-viewport[data-navigating="true"]')
      ) {
        if(selection.kind==='camera'&&(tool==='scale'||(tool==='rotate'&&displayScene.cameras?.find(c=>c.id===selection.id)?.mode==='follow')))return;
        e.preventDefault();
        setEditTool(tool);
        return;
      }
      if (
        e.key === 'Delete' && !e.defaultPrevented && !e.repeat &&
        !e.ctrlKey && !e.metaKey && !e.altKey && !e.shiftKey &&
        mode === 'scene' && !running && selection.kind === 'object' && objectSelectionIds.length &&
        !contextMenu && !picker && !projectDialog && !detailEditor &&
        !e.target.isContentEditable &&
        !e.target.closest?.('input, textarea, select, [role="textbox"], [role="dialog"], .react-flow, .widget-workspace')
      ) {
        e.preventDefault();
        deleteObjects();
        return;
      }
      if (e.key === "Escape" && !e.defaultPrevented) {
        if(contextMenu){e.preventDefault();setContextMenu(null);return;}
        if(dock==='widgets'&&floatingDock){e.preventDefault();setFloatingDock(false);return;}
        setCompactPanel('workspace');
        setPicker(null);
        setMenu(null);
        setDetailEditor(false);
        setMaximized(null);
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
        e.preventDefault();
        if(!projectDialog)e.shiftKey?openProjectDialog('saveAs'):saveProject();
      }
      if (
        (e.ctrlKey || e.metaKey) &&
        e.key.toLowerCase() === "z" &&
        !["INPUT", "TEXTAREA"].includes(e.target.tagName)
      ) {
        e.preventDefault();
        if (history.length) {
          rt.stop();setCameraPilotId(null);setCameraPreviewId(null);
          setProject(history.at(-1));
          setHistory((h) => h.slice(0, -1));
        }
      }
    };
    const nativeClipboard=e=>{
      if(e.defaultPrevented||running||mode!=='scene'||contextMenu||picker||projectDialog||detailEditor||menu||e.target.isContentEditable||e.target.closest?.('input,textarea,select,[role="textbox"],[role="dialog"],.react-flow,.widget-workspace'))return;
      if(selection.kind!=='object'&&!e.target.closest?.('.scene-viewport,.hierarchy'))return;
      e.preventDefault();objectClipboard(e.type==='copy'?'copy':'paste');
      if(e.type==='copy')e.clipboardData?.setData('text/plain','Sacura · объекты сцены');
    };
    window.addEventListener("keydown", listener);window.addEventListener('copy',nativeClipboard);window.addEventListener('paste',nativeClipboard);
    return () => {window.removeEventListener("keydown", listener);window.removeEventListener('copy',nativeClipboard);window.removeEventListener('paste',nativeClipboard);};
  }, [history,project,projectDialog,mode,running,selection,picker,detailEditor,cameraPilotId,menu,displayScene,contextMenu,dock,floatingDock]);

  const updateVariable = (id,patch) => {
    if(rt.running)return 'Остановите игру, чтобы редактировать переменные.';
    const issue=editVariable(structuredClone(project),id,patch);
    if(!issue){
      mutate(p=>editVariable(p,id,patch));
      if(selection.kind==='variable'&&selection.id===id){
        if(patch.remove)setSelection({kind:'beat',id:beat.id});
        else if(patch.name)setSelection({kind:'variable',id:patch.name.trim()});
      }
    }
    return issue;
  };
  const inspectVariable = id => {
    if(!id)return;
    setSelection({kind:'variable',id});
    setLayout(current=>({...current,hiddenInspector:false}));
    setCompactPanel('inspector');
    setMaximized(current=>current==='graph'||current==='hierarchy'||current==='scene'?null:current);
  };
  const inspectedVariable=selection.kind==='variable'?selection.id:selection.kind==='beat'&&['variable','set-variable'].includes(beat.kind)?beat.variable:null;
  const createSelectionGroup=()=>{if(rt.running)return;const next=structuredClone(project),group=createObjectGroup(next,displayScene.id,objectSelectionIds);if(!group)return;mutate(p=>Object.assign(p,next));setSelection({kind:'object',id:group.objectIds[0],ids:group.objectIds,groupId:group.id});};
  const moveBinding=(sourceId,id,targetId,phase,batchId,beforeId)=>{if(rt.running)return;mutate(p=>moveEventBinding(p,sourceId,id,targetId,phase,batchId,beforeId));if(eventContext?.bindingId===id){setEventContext(c=>({...c,beatId:targetId}));setSelectedBeat(targetId);}};
  const renderInspector = () => {
    if(selection.kind==='point'){
      const point=sceneStagingPoints(scene,project.objects).find(p=>p.id===selection.id);
      if(point){const patch=value=>{if(!rt.running)mutate(p=>Object.assign(p.subscenes.find(s=>s.id===scene.id).stagingPoints.find(x=>x.id===point.id),value));};return <fieldset disabled={running} className="staging-point-editor"><legend>{tr("Точка постановки")}</legend><label>{tr("Название")}<input value={point.label} onChange={e=>patch({label:e.target.value})}/></label>{['X','Y','Z'].map((axis,i)=><label key={axis}>{tr("Положение ")}{axis}{tr(", м")}<input type="number" step="0.1" value={point.position[i]} onChange={e=>patch({position:point.position.map((v,j)=>i===j?Number(e.target.value):v),objectId:undefined,objectOrigin:undefined})}/></label>)}<small>{tr("Выберите эту точку в действии «Переместить → К точке».")}</small><Button icon="Focus" onClick={()=>setFocusRequest({nonce:uid('focus'),position:point.position})}>{tr("Показать в сцене")}</Button><Button icon="Trash2" onClick={()=>{mutate(p=>{const s=p.subscenes.find(s=>s.id===scene.id);s.stagingPoints=s.stagingPoints.filter(x=>x.id!==point.id);});setSelection({kind:'scene',id:scene.id});}}>{tr("Удалить точку")}</Button></fieldset>;}
    }
    if(selection.kind==='group'){
      const group=project.objectGroups?.find(g=>g.id===selection.id);
      if(group)return <><Field label={tr("Название группы")}><input disabled={running} value={group.name} onChange={e=>{const name=e.target.value;mutate(p=>p.objectGroups.find(g=>g.id===group.id).name=name);}}/></Field><Button icon="Plus" disabled={running} onClick={()=>setPicker({kind:'object',groupId:group.id})}>{tr("Создать объект в группе")}</Button><p>{tr("Перетащите сюда объекты из иерархии.")}</p></>;
    }
    if(selection.kind==='file'){
      const file=collectAssetFiles(project,projectAudioAssets(project)).find(f=>f.id===selection.id);
      if(file)return <FileInspector key={file.id+file.name} file={file} disabled={running} onChange={patch=>{if(rt.running)return;const next=structuredClone(project);updateAssetFile(next,file,patch);mutate(p=>Object.assign(p,next));}} onPlaceModel={placeLibraryModel} onPreviewAudio={file=>{const key='file-preview';if(soundDesk.get(key)?.status==='playing'){soundDesk.stop(key);return;}soundDesk.play(file.audioId||file.id,{key,duck:false});}}/>;
      return <p>{tr("Файл больше не существует.")}</p>;
    }
    if(selection.kind==='object'&&selection.groupId){
      const group=project.objectGroups?.find(g=>g.id===selection.groupId);
      if(group)return <><Field label={tr("Имя группы")}><input disabled={running} value={group.name} onChange={e=>{const name=e.target.value;if(!rt.running)mutate(p=>p.objectGroups.find(g=>g.id===group.id).name=name);}}/></Field><p>{selectedObjects.length} {tr("объектов · перемещаются вместе")}</p><MultiTransformInspector key={objectSelectionIds.join('|')} objects={selectedObjects} disabled={running} onChange={change=>{if(!rt.running)mutate(p=>{for(const item of transformSelection(selectedObjects,displayScene,change))setObjectTransform(p,item.id,displayScene.id,item.value);});}} onFocus={()=>setFocusRequest({nonce:uid('focus')})} onDuplicate={duplicateObjects} onDelete={deleteObjects} onReset={()=>{if(!rt.running)mutate(p=>{for(const id of objectSelectionIds){const object=p.objects.find(o=>o.id===id);if(object?.transforms)delete object.transforms[displayScene.id];}});}}/><Button icon="Ungroup" disabled={running} onClick={()=>{mutate(p=>p.objectGroups=p.objectGroups.filter(g=>g.id!==group.id));setSelection({kind:'object',id:objectSelectionIds[0],ids:objectSelectionIds});}}>{tr("Разгруппировать")}</Button></>;
    }

    if(inspectedVariable&&(inspectedVariable===ANSWER_VARIABLE||Object.hasOwn(project.variables,inspectedVariable)))return <><VariableInspector key={inspectedVariable} variable={inspectedVariable} project={project} preview={preview} disabled={running} onEdit={updateVariable} onAddNode={(variable,kind)=>graphApi.current?.addVariableNode?.(variable,kind)}/>{selection.kind==='beat'&&<Button icon="Focus" onClick={()=>graphApi.current?.focus(beat.id)}>{tr("Показать ноду")}</Button>}</>;
    if(selection.kind==='beat'&&(isPureNode(beat)||['branch','set-variable'].includes(beat.kind)))return <><div className="inspector-identity"><strong>{beat.kind==='branch'?tr("If · Если"):beat.kind==='variable'?tr("Переменная"):beat.kind==='set-variable'?tr("Задать переменную"):tr("Логика · ")+(compareSymbols[beat.operator]||beat.kind)}</strong></div>{beat.kind==='branch'&&nodeKindField()}<p className="choice-note">{tr("Настройки операции находятся на ноде. Круглые пины передают значения, стрелки задают ход истории. Подключите результат операции ко входу Set, чтобы изменить переменную.")}</p><Button icon="Focus" onClick={()=>graphApi.current?.focus(beat.id)}>{tr("Показать ноду")}</Button></>;

    if(selection.kind==='camera'){
      const c=displayScene.cameras?.find(c=>c.id===selection.id);
      if(c)return <CameraInspector camera={c} scene={displayScene} objects={objects} state={world}
        onChange={changeCamera} onDefault={id=>{if(!rt.running)mutate(p=>p.subscenes.find(s=>s.id===displayScene.id).defaultCameraId=id);}}
        onDelete={deleteCamera} onPilot={pilotCamera} onView={viewCamera} onCapture={captureCamera}
        piloting={cameraPilotId} running={running} onEdit={editScene}/>;
    }
    if (selection.kind === "batch") {
      const ph = PHASES.find((ph) =>
          beat.batches?.[ph.id]?.some((g) => g.id === selection.id),
        ),
        list = ph ? batchesFor(beat, ph.id) : [],
        batch = list.find((g) => g.id === selection.id),
        index = list.indexOf(batch);
      if (batch)
        return (
          <>
            <div className="inspector-identity">
              <Icon name="Workflow" size={24} />
              <div>
                <strong>{tr("Группа событий ")}{index + 1}</strong>
                <small>
                  {tr(ph.label)} · {beat.id}
                </small>
              </div>
            </div>
            <Fold title={tr("Порядок запуска")} icon="GitFork">
              <Select
                value={batch.mode}
                options={[
                  ["SEQUENTIAL", "По очереди →"],
                  ["PARALLEL", "Одновременно ⇉"],
                ]}
                onChange={(v) => changeBatch(batch.id, "mode", v)}
              />
              <p className="resource-note">
                {batch.mode === "PARALLEL"
                  ? tr("Все события стартуют вместе. Выход — после результатов, указанных на связях.")
                  : tr("Следующее событие начинается после результата предыдущего.")}
              </p>
              <div className="button-row">
                <Button
                  icon="ArrowUp"
                  disabled={index === 0}
                  onClick={() => changeBatch(batch.id, "move", -1)}
                >
                  {tr("Раньше")}</Button>
                <Button
                  icon="ArrowDown"
                  disabled={index === list.length - 1}
                  onClick={() => changeBatch(batch.id, "move", 1)}
                >
                  {tr("Позже")}</Button>
              </div>
            </Fold>
            <Fold title={tr("События в группе")} icon="Layers">
              {batch.bindings.map((b) => (
                <div className="batch-member" key={b.id}>
                  <button
                    onClick={() =>
                      graphSelect({
                        binding: b,
                        event: project.events.find((e) => e.id === b.eventId),
                      })
                    }
                  >
                    {project.events.find((e) => e.id === b.eventId)?.name}
                  </button>
                  <Button
                    icon="X"
                    title={tr("Убрать событие из реплики")}
                    onClick={() => changeBatch(batch.id, "removeBinding", b.id)}
                  />
                </div>
              ))}
              <Button icon="Plus" onClick={() => changeBatch(batch.id, "add")}>
                {tr("Добавить в эту группу")}</Button>
            </Fold>
            {index > 0 && (
              <Button
                className="inspector-add"
                icon="Combine"
                onClick={() => {
                  setSelection({ kind: "batch", id: list[index - 1].id });
                  changeBatch(batch.id, "joinPrevious");
                }}
              >
                {tr("Запустить вместе с предыдущей")}</Button>
            )}
          </>
        );
    }
    if(selection.kind==='object'&&selectedObjects.length>1)return <MultiTransformInspector key={objectSelectionIds.join('|')} objects={selectedObjects} disabled={running}
      onChange={value=>transformObjects(transformSelection(selectedObjects,scene,value))}
      onFocus={()=>{editScene();setFocusRequest({nonce:uid('focus')});}}
      onReset={()=>mutate(p=>{for(const id of objectSelectionIds){const object=p.objects.find(o=>o.id===id);if(object?.transforms)delete object.transforms[scene.id];}})}
      onDuplicate={duplicateObjects} onDelete={deleteObjects}/>;
    if (selection.kind === "object" && inspectedObject) {
      const o = inspectedObject,
        patch = (v) =>
          mutate((p) =>
            Object.assign(
              p.objects.find((x) => x.id === o.id),
              v,
            ),
          );
      return (
        <>
          <div className="inspector-identity">
            <span className="identity-icon" style={{ color: o.color }}>
              <Icon
                name={o.type === "Персонаж" ? "PersonStanding" : o.type==='Источник света'?'Lightbulb':"Box"}
                size={28}
              />
            </span>
            <div>
              <input
                value={o.name}
                onChange={(e) => patch({ name: e.target.value })}
              />
              <small>{tr(o.type)}</small>
            </div>
            <input
              type="checkbox"
              checked={o.active}
              onChange={(e) => patch({ active: e.target.checked })}
              title={tr("Объект активен")}
            />
          </div>
          <Field label={tr("Группа в иерархии")}><Select disabled={running} value={project.objectGroups?.find(g=>g.sceneId===scene.id&&g.objectIds.includes(o.id))?.id||''} options={[["","Объекты сцены · корень"],...(project.objectGroups||[]).filter(g=>g.sceneId===scene.id).map(g=>[g.id,literalLabel(g.name)])]} onChange={groupId=>mutate(p=>groupId?addObjectsToGroup(p,groupId,[o.id]):removeObjectsFromGroups(p,scene.id,[o.id]))}/></Field>
          {o.type==='Персонаж'&&<Fold title={tr("Виджет диалога")} icon="PanelsTopLeft"><WidgetAssignment project={project} character={o} onCustomize={()=>customizeWidget({characterId:o.id})} onChange={dialogueWidgetId=>patch({dialogueWidgetId})} onEdit={openWidgets} disabled={running}/></Fold>}
          {o.type==='Персонаж'&&<Button icon="Users" onClick={()=>{setDock('characters');setCompactPanel('workspace');}}>{tr("Модель и анимации персонажа")}</Button>}
          <TransformInspector object={o} scene={scene} disabled={running} onChange={value=>transformObject(o.id,value)}
            onReset={()=>mutate(p=>{const x=p.objects.find(x=>x.id===o.id);if(x.transforms)delete x.transforms[scene.id];})}
            onFocus={()=>{editScene();setFocusRequest({nonce:uid('focus')});}} onDuplicate={()=>duplicateObject(o)} onDelete={()=>deleteObject(o.id)}/>
          <CollisionInspector object={o} disabled={running} onChange={collision=>patch({collision})}/>
          {o.type!=='Персонаж'&&o.type!=='Источник света'&&<MeshInspector key={o.id} object={o} disabled={running}
            allowUpload={!o.builtin&&!['letter','door','fireplace','garden-note','ticket'].includes(o.id)}
            onChange={async values=>{
              if(rt.running)return;
              if('model' in values){
                const next=structuredClone(project),target=next.objects.find(x=>x.id===o.id);
                if(!target)throw new Error('Объект удалён.');if(target.model?.src!==values.model?.src)retainObjectModel(next,target);Object.assign(target,values);
                try{accountStorage.setItem(PROJECT_KEY,JSON.stringify(next));}catch{throw new Error('Не хватает места для автосохранения. Уменьшите модель или удалите неиспользуемые модели.');}
              }
              mutate(p=>{const target=p.objects.find(x=>x.id===o.id);if(!target)return;if('model' in values&&target.model?.src!==values.model?.src)retainObjectModel(p,target);Object.assign(target,values);});
            }}/>}
          {running&&<Button icon="Square" onClick={editScene}>{tr("Остановить и редактировать сцену")}</Button>}
          <Fold title={tr("Объект сцены")} icon="Box">
            <Field label={tr("Опорная точка")}>
              <Select
                value=""
                disabled={running}
                options={[["","Переместить к точке…"],...sceneStagingPoints(scene,objects).filter(point=>point.objectId!==o.id).map(point=>[point.id,literalLabel(point.label)])]}
                onChange={id=>{const point=sceneStagingPoints(scene,objects).find(point=>point.id===id);if(!point||running)return;const transform=objectTransform(o,scene.id,scene.kind),height=transform.position[1];transform.position=[...point.position];if(o.type!=='Персонаж')transform.position[1]=height;transformObject(o.id,transform);}}
              />
            </Field>
            <Field label={tr("Локация")}>
              <Select
                value={o.subsceneId || ""}
                options={[
                  ["", "Все сабсцены"],
                  ...project.subscenes.map((s) => [s.id, literalLabel(s.location)]),
                ]}
                onChange={(subsceneId) => patch({ subsceneId })}
              />
            </Field>
            {!o.model&&!o.builtin&&!['letter','door','fireplace','garden-note','ticket'].includes(o.id)&&o.type!=='Персонаж'&&o.type!=='Источник света'&&<Field label={tr("Форма")}><Select value={o.primitive||'box'} options={[["box","Куб"],["sphere","Сфера"],["cylinder","Цилиндр"]]} onChange={primitive=>patch({primitive})}/></Field>}
            <Field label={o.type==='Источник света'?tr("Цвет света"):tr("Цвет в макете")}>
              <input
                type="color"
                value={o.color || "#b99cac"}
                onChange={(e) => patch({ color: e.target.value })}
              />
            </Field>
          </Fold>
          {o.type==='Источник света'&&<Fold title={tr("Освещение")} icon="Lightbulb"><Field label={tr("Интенсивность")}><input type="number" min="0" step="1" disabled={running} value={o.light?.intensity??35} onChange={e=>patch({light:{...o.light,intensity:Math.max(0,Number(e.target.value))}})}/></Field><Field label={tr("Дальность, м")}><input type="number" min="0" step="0.5" disabled={running} value={o.light?.distance??10} onChange={e=>patch({light:{...o.light,distance:Math.max(0,Number(e.target.value))}})}/></Field><p className="resource-note">{tr("Точечный источник светит во все стороны. Дальность 0 — без ограничения.")}</p></Fold>}
          {o.type === "Активный меш" && (
            <Fold title={tr("Взаимодействие")} icon="MousePointer2">
              <Field label={tr("Действие игрока")}>
                <input
                  value={o.interaction || ""}
                  onChange={(e) => patch({ interaction: e.target.value })}
                />
              </Field>
              <Button
                icon="Hourglass"
                onClick={() => {
                  patchBeat({ kind: "gate", signal: o.id, timeout: 30 });
                  setDock("story");
                }}
              >
                {tr("Ждать в этой реплике")}</Button>
            </Fold>
          )}
          <Button
            className="inspector-add"
            icon="Plus"
            onClick={() => openAuthoring('action',{target:o.id})}
          >
            {tr("Создать действие с объектом")}</Button>
        </>
      );
    }
    if (
      (selection.kind === "event" ||
        selection.kind === "binding" ||
        selection.kind === "action" ||
        selection.kind === "group") &&
      event
    ) {
      const patchEvent = (v) =>
        mutate((p) =>
          Object.assign(
            p.events.find((e) => e.id === event.id),
            v,
          ),
        );
      return (
        <>
          <div className="inspector-identity">
            <span className="identity-icon">
              <Icon
                name={
                  inspectedAction ? TYPES[inspectedAction.type].icon : "Layers"
                }
                size={25}
              />
            </span>
            <div>
              <strong>
                {inspectedAction
                  ? tr(TYPES[inspectedAction.type].label)
                  : event.name}
              </strong>
              <small>
                {binding ? tr("Экземпляр · ") + contextBeat.id : tr("Шаблон события")}
              </small>
            </div>
          </div>
          {binding && (
            <div className="instance-source">
              <Icon name="Link2" size={13} />
              <span>{event.name}</span>
              <Button
                title={tr("Открыть исходный шаблон")}
                icon="ExternalLink"
                onClick={() => {
                  setEventContext({ eventId: binding.sourceEventId||event.id });
                  setSelection({ kind: "event", id: binding.sourceEventId||event.id });
                }}
              />
            </div>
          )}
          {inspectedAction ? (
            <>
              <Fold title={tr("Параметры действия")} icon="SlidersHorizontal">
                <Field label={tr("Тип")}>
                  <Select
                    value={inspectedAction.type}
                    options={Object.entries(TYPES).map(([id, t]) => [
                      id,
                      t.label,
                    ])}
                    onChange={(type) =>
                      patchAction(inspectedAction.id, {
                        ...makeAction(type),
                        id: inspectedAction.id,
                      })
                    }
                  />
                </Field>
                <ActionFields onPickPosition={id=>{editScene();setPositionPick({id,context:{...eventContext}});setNotice("Нажмите на место в 3D-сцене. Escape — отмена.");}} sceneId={sceneFor(project,contextBeat.id).id}
                  action={inspectedAction}
                  project={project}
                  onChange={(v) => patchAction(inspectedAction.id, v)}
                />
              </Fold>
              <Fold title={tr("Конфликты и завершение")} icon="Shield">
                <Field label={tr("Если объект занят")}>
                  <Select
                    value={inspectedAction.conflict}
                    options={[
                      ["FAIL_NEW", "Показать ошибку"],
                      ["REPLACE_CURRENT", "Заменить текущее"],
                      ["QUEUE", "Ожидать · модель поведения"],
                    ]}
                    onChange={(conflict) =>
                      patchAction(inspectedAction.id, { conflict })
                    }
                  />
                </Field>
                <div className="resource-note">
                  {project.objects.find((o) => o.id === inspectedAction.target)
                    ?.name || inspectedAction.target}{" "}
                  / {tr(TYPES[inspectedAction.type].domain) || tr("Условие")}
                </div>
              </Fold>
            </>
          ) : (
            <Fold
              title={binding ? tr("Параметры размещения") : tr("Свойства события")}
              icon="Layers"
            >
              {binding ? (
                <>
                  <Field label={tr("Момент запуска")}>
                    <Select
                      value={binding.hook}
                      options={PHASES.map((p) => [p.id, p.label])}
                      onChange={(hook) =>
                        mutate((p) => {
                          const b = allBeats(p).find(
                              (b) => b.id === contextBeat.id,
                            ),
                            item = b.bindings.find((x) => x.id === binding.id);
                          item.hook = hook;
                        })
                      }
                    />
                  </Field>
                  <Field label={tr("Когда идти дальше")}>
                    <Select
                      value={binding.join}
                      options={Object.entries(JOIN_LABELS)}
                      onChange={(join) =>
                        mutate(
                          (p) =>
                            (allBeats(p)
                              .find((b) => b.id === contextBeat.id)
                              .bindings.find((x) => x.id === binding.id).join =
                              join),
                        )
                      }
                    />
                  </Field>
                  <Button
                    icon="RotateCcw"
                    onClick={() =>
                      mutate((p) => {
                        const b = allBeats(p)
                          .find((b) => b.id === contextBeat.id)
                          .bindings.find((x) => x.id === binding.id);
                        b.actionOverrides = {};
                        b.overrides = {};
                      })
                    }
                  >
                    {tr("Сбросить изменения здесь")}</Button>
                </>
              ) : (
                <>
                  <Field label={tr("Название")}>
                    <input
                      value={event.name}
                      onChange={(e) => patchEvent({ name: e.target.value })}
                    />
                  </Field>
                  <Field label={tr("После действий")}>
                    <Select
                      value={event.retention}
                      options={[
                        ["AUTO_CLOSE_ON_FLOW_END", "Завершить событие"],
                        ["HOLD_UNTIL_STOPPED", "Оставить до остановки"],
                        ["HOLD_UNTIL_REPLACED", "До замены или выхода"],
                      ]}
                      onChange={(retention) =>
                        patchEvent({
                          retention,
                          channel: event.channel || "Effect." + event.id,
                        })
                      }
                    />
                  </Field>
                  <Field label={tr("Живёт в пределах")}>
                    <Select
                      value={event.owner}
                      options={[
                        ["SubScene", "Сабсцены"],
                        ["Scene", "Всей сцены"],
                        ["GameSession", "Игровой сессии"],
                      ]}
                      onChange={(owner) => patchEvent({ owner })}
                    />
                  </Field>
                  <Button
                    icon="Plus"
                    onClick={() => {
                      addEvent(event.id);
                      openStaging(beat.id);
                    }}
                  >
                    {tr("Добавить в реплику")}</Button>
                </>
              )}
            </Fold>
          )}
          <Fold title={tr("Полное редактирование события")} icon="ListOrdered">
            <EventInspector event={event} project={project} binding={binding} sceneId={sceneFor(project,contextBeat.id).id}
              onAction={patchAction} onPickPosition={id=>{editScene();setPositionPick({id,context:{...eventContext}});setNotice('Нажмите на место в 3D-сцене. Escape — отмена.');}}
              onEdit={fn=>{
                const detach=binding&&(!binding.localEventId||nodes.filter(b=>b.bindings.some(x=>x.eventId===event.id)).length>1||nodes.some(b=>b.bindings.filter(x=>x.eventId===event.id).length>1));
                const nextId=detach?uid('event'):event.id;
                mutate(p=>{
                  let target=p.events.find(e=>e.id===event.id);
                  if(binding){const item=allBeats(p).find(b=>b.id===contextBeat.id).bindings.find(b=>b.id===binding.id);
                    if(detach){target={...structuredClone(target),id:nextId,standardPreset:false};for(const a of target.groups.flatMap(g=>g.actions))Object.assign(a,item.overrides&&a.id===target.groups[0]?.actions[0]?.id?item.overrides:{},item.actionOverrides?.[a.id]||{});p.events.push(target);item.sourceEventId=item.sourceEventId||event.id;item.eventId=target.id;item.localEventId=target.id;item.overrides={};item.actionOverrides={};}
                  }
                  fn(target);
                });
                if(nextId!==event.id)setEventContext(c=>({...c,eventId:nextId}));
              }}/>
          </Fold>
          {binding&&<Button icon="Trash2" onClick={()=>{mutate(p=>{removeEventBinding(p,contextBeat.id,binding.id);});setEventContext(null);setSelection({kind:'beat',id:contextBeat.id});}}>{tr("Удалить событие из реплики")}</Button>}
          <Button
            className="inspector-add"
            icon="Maximize2"
            onClick={() => {
              setDetailEditor(true);
              setFloatingDock(true);
              setMaximized(null);
            }}
          >
            {tr("Открыть событие в отдельном окне")}</Button>
        </>
      );
    }
    return (
      <>
        <div className="inspector-identity">
          <span className="identity-icon">
            <Icon
              name={
                beat.kind === "choice"
                  ? "GitFork"
                  : beat.kind === "gate"
                    ? "MousePointer2"
                    : "MessageSquare"
              }
              size={26}
            />
          </span>
          <div>
            <strong>
              {beat.kind === "choice"
                ? tr("Выбор игрока")
                : beat.kind === "gate"
                  ? tr("Ожидание игрока")
                  : beat.kind==='branch'?tr("Проверка") : (beat.text?.trim() ? tr("Реплика · ") + beat.text.trim().split(/\s+/).slice(0,3).join(" ") + (beat.text.trim().split(/\s+/).length>3 ? "…" : "") : tr("Пустая реплика"))}
            </strong>
            <small>{chapter.name}</small>
          </div>
          <span className="subtle">{beat.bindings.length} {tr("событий")}</span>
        </div>
        {beat.kind!=='branch'&&<Fold title={tr("Диалог")} icon="MessageSquare">
          <Field label={tr("Говорит")}>
            <EditorSelect label={tr("Говорит")} value={beat.speaker}
              options={[["Рассказчик","Рассказчик"],...project.objects.filter(o=>o.type==='Персонаж').map(o=>[o.name,literalLabel(o.name)]),['__create_character__','＋ Создать персонажа…']]}
              onChange={speaker=>speaker==='__create_character__'?setCharacterDraft(''):patchBeat({speaker})}/>
          </Field>
          {characterDraft!==null&&<div className="speaker-create">
            <Field label={tr("Имя нового персонажа")}><input autoFocus aria-label={tr("Имя нового персонажа")} value={characterDraft} onChange={e=>setCharacterDraft(e.target.value)} onKeyDown={e=>{if(e.key==='Enter'){e.preventDefault();createSpeaker();}if(e.key==='Escape'){e.stopPropagation();setCharacterDraft(null);}}}/></Field>
            <Button icon="UserPlus" disabled={!characterDraft.trim()||running} onClick={createSpeaker}>{tr("Создать и выбрать")}</Button>
            <Button onClick={()=>setCharacterDraft(null)}>{tr("Отмена")}</Button>
          </div>}
          <textarea
            className="dialogue-text-field"
            aria-label={tr("Текст выбранной реплики")}
            value={beat.text}
            rows={4}
            onChange={(e) => patchBeat({ text: e.target.value })}
          />
          {nodeKindField()}
        </Fold>
        }<Fold title={tr("Постановка реплики")} icon="Clapperboard">
          {PHASES.map((p) => (
            <button
              className="phase-inspector-row"
              key={p.id}
              onClick={() => openStaging(beat.id, p.id)}
            >
              <Icon
                name={
                  p.id === "BEFORE"
                    ? "CornerDownRight"
                    : p.id === "ON_START"
                      ? "MessageSquare"
                      : "CornerRightDown"
                }
                size={14}
              />
              <span>{tr(p.label)}</span>
              <b>{beat.bindings.filter((b) => b.hook === p.id).length}</b>
              <Icon name="ChevronRight" size={13} />
            </button>
          ))}
          <Button icon="Workflow" onClick={() => openStaging(beat.id)}>
            {tr("Показать реплику в потоке")}</Button>
        </Fold>
        <Fold title={tr("Контексты ввода")} icon="Gamepad2"><InputContextSettings project={project} beat={beat} onChange={patchBeat} disabled={running} onOpen={()=>{setDock('input');setCompactPanel('workspace');}}/></Fold>
        {beat.kind === "choice" ? (
          <Fold title={tr("Ответы игрока")} icon="GitFork"><p>{tr("Текст ответа, результат и вход доступности редактируются прямо в ноде выбора. Подключите результат сравнения к круглому входу ответа.")}</p><Button icon="Focus" onClick={()=>graphApi.current?.focus(beat.id)}>{tr("К ноде выбора")}</Button></Fold>
        ) : beat.kind==='branch' ? null : beat.kind === "gate" ? (
          <Fold title={tr("Ждать взаимодействие")} icon="MousePointerClick">
            <InteractionInspector beat={beat} project={project} scene={scene} onChange={patchBeat}/>
            <Field label={tr("Подсказка через, сек")}>
              <input
                type="number"
                value={beat.timeout || 30}
                onChange={(e) => patchBeat({ timeout: Number(e.target.value) })}
              />
            </Field>
          </Fold>
        ) : null}
        {!["choice", "branch", "end"].includes(beat.kind) && (
          <Fold title={tr("Продолжение")} icon="Route">
            <Select
              value={beat.next || ""}
              options={[
                ["", "Выход не подключён"],
                ...nodes.filter(n=>!isPureNode(n)).map((n) => [
                  n.id,
                  sceneFor(project, n.id).name + ' · ' + beatPreview(n,6),
                ]),
              ]}
              onChange={(next) => patchBeat({ next: next || null })}
            />
            <div className="resource-note">
              {beat.next
                ? sceneFor(project, beat.next).location === scene.location
                  ? tr("В этой же локации")
                  : tr("Переход в другую сабсцену")
                : tr("Без последовательного продолжения")}
            </div>
          </Fold>
        )}
        {['dialogue','choice','gate','end'].includes(beat.kind)&&<Fold title={tr("Виджет диалогового окна")} icon="PanelsTopLeft"><WidgetAssignment project={project} beat={beat} onCustomize={()=>customizeWidget({beatId:beat.id})} onChange={widgetId=>patchBeat({widgetId})} onEdit={openWidgets} disabled={running}/></Fold>}
      </>
    );
  };

  const renderAssets = () => {
    const assets =
      assetCategory === "scenes"
        ? project.subscenes
        : assetCategory === "objects"
          ? project.objects
          : assetCategory === "audio"
            ? projectAudioAssets(project)
            : project.events;
    return (
      <div className="project-browser">
        <div className="asset-folders">
            <button onClick={()=>{setDock('create');setMaximized('graph');}}><Icon name="MousePointer2" size={15}/>{tr("Действия и группы")}</button>
          {[
            ["scenes", "PanelsTopLeft", "Сабсцены"],
            ["events", "Layers", "События"],
            ["objects", "Users", "Персонажи и объекты"],
            ["audio", "Music2", "Аудио"],
          ].map(([id, icon, label]) => (
            <button
              className={assetCategory === id ? "active" : ""}
              key={id}
              onClick={() => setAssetCategory(id)}
            >
              <Icon name={icon} size={15} />
              {tr(label)}
            </button>
          ))}
        </div>
        <div className="asset-content">
          <div className="asset-path">
            {tr("Проект ")}<Icon name="ChevronRight" size={12} />{" "}
            {assetCategory === "events"
              ? tr("События")
              : assetCategory === "audio"
                ? tr("Звук")
                : assetCategory === "scenes"
                  ? tr("Сабсцены")
                  : tr("Объекты")}
            <span />
            {assetCategory === "events" && (
              <Button icon="Plus" onClick={createEvent}>
                {tr("Событие")}</Button>
            )}
          </div>
          <div className="asset-grid">
            {assets
              .filter((a) =>
                (a.name || a.caption || "")
                  .toLowerCase()
                  .includes(query.toLowerCase()),
              )
              .map((a) => (
                <button
                  className="asset-tile"
                  key={a.id}
                  draggable
                  onDragStart={(e) =>
                    e.dataTransfer.setData(
                      "application/sacura-asset",
                      JSON.stringify({ id: a.id, kind: assetCategory }),
                    )
                  }
                  onDoubleClick={() => {
                    if (assetCategory === "scenes") {
                      selectBeat(a.entry);
                      setDock("story");
                    } else if (assetCategory === "events") {
                      openAuthoring('event',{id:a.id});
                    } else if (assetCategory === "audio") setDock("sound");
                    else selectObject(a.id);
                  }}
                  onClick={() => {
                    if (assetCategory === "objects") selectObject(a.id);
                    else if (assetCategory === "events") {
                      setEventContext({ eventId: a.id });
                      setSelection({ kind: "event", id: a.id });
                    }
                  }}
                >
                  <span className={"asset-preview " + assetCategory}>
                    <Icon
                      name={
                        assetCategory === "scenes"
                          ? a.kind === "garden"
                            ? "Trees"
                            : a.kind === "station"
                              ? "TramFront"
                              : "Armchair"
                          : assetCategory === "objects"
                            ? a.type === "Персонаж"
                              ? "PersonStanding"
                              : "Box"
                            : assetCategory === "audio"
                              ? a.kind === "music"
                                ? "Music2"
                                : "Mic"
                              : TYPES[a.groups[0]?.actions[0]?.type]?.icon ||
                                "Layers"
                      }
                      size={35}
                    />
                    {assetCategory === "events" && (
                      <small>
                        {a.groups.length}{" "}
                        {a.groups.length === 1 ? tr("группа") : tr("группы")}
                      </small>
                    )}
                  </span>
                  <strong>{a.name}</strong>
                </button>
              ))}
          </div>
        </div>
      </div>
    );
  };
  const renderActive = () => {
    const effects = running
      ? preview.effects
      : {
          weather: {
            name: scene.weather,
            status: "held",
            type: "weather",
            owner: "SubScene",
            origin: "Начальное состояние",
          },
          time: {
            name: scene.time,
            status: "held",
            type: "time",
            owner: "SubScene",
            origin: "Начальное состояние",
          },
        };
    return (
      <div className="active-table" data-help-title={tr("Звук и окружение")} data-help={tr("Здесь показано, что звучит и какие эффекты действуют сейчас. Начальные погоду и освещение задайте в сабсцене, изменения по ходу истории — в постановке.")}>
        <div className="workspace-guide"><h2>{tr("Звук и окружение")}</h2><p>{tr("Музыка, озвучка и состояние мира в выбранной сабсцене. Во время запуска здесь можно приостановить или завершить эффект.")}</p><div><Button icon="Music2" onClick={()=>setDock('sound')}>{tr("Звуковые файлы и озвучка")}</Button><Button icon="CloudSun" onClick={()=>openSubscenes()}>{tr("Начальное окружение")}</Button><Button icon="Clapperboard" onClick={()=>openStaging(beat.id)}>{tr("Изменения в сценарии")}</Button></div><p>{running?tr("Управление ниже действует на текущий предпросмотр."):tr("Предпросмотр остановлен. Ниже — начальные состояния сабсцены; для проверки нажмите «Запуск».")}</p></div>
        <div className="table-header">
          <span>{tr("Событие / эффект")}</span>
          <span>{tr("Состояние")}</span>
          <span>{tr("Где действует")}</span>
          <span>{tr("Управление")}</span>
        </div>
        {Object.entries(effects).map(([key, e]) => (
          <div className="active-row" key={key}>
            <span>
              <Icon
                name={
                  TYPES[e.type]?.icon || (key === "music"
                    ? "Music2"
                    : key === "weather"
                      ? "CloudRain"
                      : "Moon")
                }
                size={17}
              />
              <strong>{e.target ? (project.objects.find(o=>o.id===e.target)?.name||e.target)+' · ' : ''}{e.name}</strong>
              <small>{message(e.origin)}</small>
            </span>
            <span className={"effect-status " + e.status}>
              {!running ? tr("Задано в сабсцене") : e.status === "paused"
                ? tr("На паузе")
                : e.status === "stopped"
                  ? tr("Завершено")
                  : tr("Действует")}
            </span>
            <span>
              {{
                Scene: tr("Вся сцена"),
                SubScene: tr("Сабсцена"),
                GameSession: tr("Вся игра"),
              }[e.owner] || e.owner}
            </span>
            <div>
              <Button
                disabled={!running}
                title={e.status === "paused" ? tr("Продолжить") : tr("Пауза")}
                icon={e.status === "paused" ? "Play" : "Pause"}
                onClick={() =>
                  rt.controlEffect(
                    key,
                    e.status === "paused" ? "resume" : "pause",
                  )
                }
              />
              <Button
                disabled={!running}
                title={tr("Остановить этот эффект")}
                icon="Square"
                onClick={() => rt.controlEffect(key, "stop")}
              />
            </div>
          </div>
        ))}
        {running &&
          Object.values(preview.instances || {})
            .filter((i) => ["running", "held", "paused"].includes(i.status))
            .map((i) => (
              <div className="active-row" key={i.id}>
                <span>
                  <Icon name="Layers" />
                  <strong>{i.name}</strong>
                  <small>{i.beatId}</small>
                </span>
                <span>
                  {i.status === "paused"
                    ? tr("На паузе")
                    : i.status === "running"
                      ? tr("Выполняется")
                      : tr("Готово · удерживается")}
                </span>
                <span>
                  {{
                    Scene: tr("Вся сцена"),
                    SubScene: tr("Сабсцена"),
                    GameSession: tr("Вся игра"),
                  }[i.owner] || i.owner}
                </span>
                <div>
                  <Button
                    title={
                      i.status === "paused"
                        ? tr("Продолжить событие")
                        : tr("Приостановить событие")
                    }
                    icon={i.status === "paused" ? "Play" : "Pause"}
                    onClick={() =>
                      rt.controlInstance(
                        i.id,
                        i.status === "paused" ? "resume" : "pause",
                      )
                    }
                  />
                  <Button
                    icon="Square"
                    title={tr("Завершить событие целиком")}
                    onClick={() => rt.controlInstance(i.id, "stop")}
                  />
                </div>
              </div>
            ))}
        <div className="runtime-caption">
          {running
            ? tr("Событие может закончить свои шаги и продолжать жить в фоне.")
            : tr("Начальное состояние. Запустите сцену, чтобы увидеть реальные экземпляры событий.")}
        </div>
      </div>
    );
  };

  const editClipboard=command=>{if(selection.kind==='object')objectClipboard(command);else {const api=graphApi.current?.clipboard?.();if(api)graphClipboard(command,api.nodes,api.graphKey);}setMenu(null);};
  const closeContextMenu=useCallback(()=>setContextMenu(null),[]);
  useEffect(()=>{setContextMenu(null);},[dock,mode,displayScene.id,running]);
  const placeContextNode=(id,placement)=>setLayout(l=>({...l,positions:{...l.positions,[placement.graphKey]:{...l.positions[placement.graphKey],[id]:placement.position}}}));
  const addContextAction=type=>{
    if(rt.running||!event||binding)return;
    const action=makeAction(type);
    mutate(p=>{const ev=p.events.find(e=>e.id===event.id);if(!ev.groups.length)ev.groups.push({id:uid('group'),name:'Основное действие',actions:[]});ev.groups.at(-1).actions.push(action);});
    placeContextNode(action.id,contextMenu);setSelection({kind:'action',id:action.id});
  };
  const contextItems=()=>{
    if(contextMenu?.kind==='wire-drop'){
      const type=contextMenu.valueType,fromSource=contextMenu.pin.handleType==='source';
      const add=(kind,patch={})=>addBeat(kind,{...contextMenu,...patch,wireType:type});
      if(type==='flow')return [['dialogue','Реплика'],['choice','Выбор игрока'],['branch','If · Если'],['set-variable','Задать переменную'],...(fromSource?[['end','Концовка']]:[])].map(([kind,label])=>({label,action:()=>add(kind)}));
      const items=[];
      if(type==='number')for(const [operator,op] of Object.entries(MATH_OPERATIONS))items.push({label:op.label,icon:'Calculator',action:()=>add('math',{operator})});
      if(!fromSource&&type==='boolean')for(const operator of ['eq','gt','gte','lt','lte'])items.push({label:tr('Сравнение · ')+compareSymbols[operator],action:()=>add('compare',{operator,valueType:'number'})});
      if(type==='boolean')for(const [kind,label]of [['and','И'],['or','ИЛИ'],['not','НЕ']])items.push({label,icon:'Binary',action:()=>add(kind)});
      if(fromSource){
        if(type!=='any')for(const operator of type==='number'?Object.keys(compareSymbols):['eq','ne'])items.push({label:tr('Сравнить · ')+compareSymbols[operator],icon:'Binary',action:()=>add('compare',{operator})});
        for(const [valueType,label]of [['number','число'],['boolean','Да / нет'],['string','текст']])items.push({label:tr('Преобразовать в ')+tr(label),icon:'RefreshCw',action:()=>add('convert',{valueType,inputType:type==='any'?'string':type})});
        for(const item of variablePalette(project).filter(i=>i.kind==='set-variable'&&logicType(project,{kind:'variable',variable:i.variable})===type))items.push({label:item.label,icon:'Database',action:()=>add('set-variable',{variable:item.variable})});
        if(type==='boolean')items.push({label:'If · Если',icon:'GitFork',action:()=>add('branch')});
      }else{
        if(type==='any')for(const valueType of ['string','number','boolean'])items.push({label:tr('Значение · ')+tr(valueType),action:()=>add('literal',{valueType})});
        else items.unshift({label:tr('Значение · ')+tr(type),action:()=>add('literal')});
        for(const item of variablePalette(project).filter(i=>i.kind==='variable'&&(type==='any'||logicType(project,{kind:'variable',variable:i.variable})===type)))items.push({label:item.label,action:()=>add('variable',{variable:item.variable})});
        if(type!=='any')items.push({label:tr('Конвертация в ')+tr(type),icon:'RefreshCw',action:()=>add('convert')});
      }
      return items;
    }

    if(contextMenu?.kind==='variable-drop')return [['variable','Получить · Get'],...(contextMenu.variable===ANSWER_VARIABLE?[]:[['set-variable','Задать · Set']])].map(([kind,label])=>({label,icon:'Database',action:()=>addBeat(kind,contextMenu)}));
    if(contextMenu?.kind==='scene')return [
      {label:'Персонаж',icon:'PersonStanding',group:'Добавить объект',action:()=>addObject('Персонаж')},
      {label:'Интерактивный предмет',icon:'MousePointer2',group:'Добавить объект',action:()=>addObject('Активный меш')},
      {label:'Объект',icon:'Box',group:'Добавить объект',children:LOCATION_PRIMITIVES.map(([shape,label,icon])=>({label,icon,action:()=>addObject('Меш',shape)}))},
      {label:'Источник освещения',icon:'Lightbulb',group:'Добавить объект',action:()=>addObject('Источник света')},
      {label:'Точка постановки здесь',icon:'MapPin',group:'Добавить объект',action:addStagingPoint},
      {label:'Звуковое событие',icon:'Volume2',group:'Добавить объект',action:addSoundEvent},
      {label:'Камера из текущего вида',icon:'Video',group:'Добавить объект',action:()=>createCamera()},
      {label:'Приблизить выделение',icon:'Focus',group:'Выделение',shortcut:'F',disabled:!objectSelectionIds.length,action:()=>setFocusRequest({nonce:uid('focus')})},
      {label:'Сгруппировать выделение',icon:'FolderPlus',group:'Выделение',disabled:objectSelectionIds.length<2,action:createSelectionGroup},
      {label:'Дублировать выделение',icon:'Copy',group:'Выделение',disabled:!objectSelectionIds.length,action:duplicateObjects},
      {label:'Удалить выделение',icon:'Trash2',group:'Выделение',shortcut:'Del',disabled:!objectSelectionIds.length,action:deleteObjects},
      {label:showGrid?'Скрыть сетку':'Показать сетку',icon:'Grid2X2',group:'Вид',action:()=>setShowGrid(v=>!v)},
      {label:showCameras?'Скрыть камеры':'Показать камеры',icon:'Video',group:'Вид',action:()=>setShowCameras(v=>!v)},
    ];
    if(contextMenu?.kind==='event')return binding?[
      {label:'Редактировать исходный шаблон',icon:'Layers',group:'Состав нод задаётся в шаблоне',action:()=>setEventContext({eventId:event.id})}
    ]:Object.entries(TYPES).map(([type,t])=>({label:t.label,icon:t.icon,group:'Добавить ноду действия',action:()=>addContextAction(type)}));
    return [
      {label:'Переменные проекта',icon:'Database',children:variablePalette(project).map(item=>({...item,icon:item.kind==='variable'?'Database':'Pencil',action:()=>addBeat(item.kind,{...contextMenu,variable:item.variable})}))},
      {label:'Математические операции',icon:'Calculator',children:Object.entries(MATH_OPERATIONS).map(([operator,op])=>({label:op.label,action:()=>addBeat('math',{...contextMenu,operator})}))},
      {label:'Конвертации',icon:'RefreshCw',children:[['number','В число'],['boolean','В Да / нет'],['string','В текст']].map(([valueType,label])=>({label,action:()=>addBeat('convert',{...contextMenu,valueType})}))},
      {label:'Сравнения',icon:'Binary',children:['eq','ne','gte','lte','gt','lt'].map(operator=>({label:compareSymbols[operator]+' · '+tr(({eq:'Равно',ne:'Не равно',gte:'Больше или равно',lte:'Меньше или равно',gt:'Больше',lt:'Меньше'})[operator]),icon:'Binary',action:()=>addBeat('compare',{...contextMenu,operator})}))},
      ...[['variable','Database','Переменная · получить'],['set-variable','Database','Переменная · задать'],['literal','Binary','Значение'],['and','Binary','И · оба условия'],['or','Binary','ИЛИ · любое условие'],['not','Binary','НЕ · инверсия условия']].map(([kind,icon,label])=>({label,icon,group:'Значения и переменные',action:()=>addBeat(kind,contextMenu)})),
      ...[['dialogue','MessageSquare','Реплика'],['gameplay','Gamepad2','Игровая сцена'],['choice','GitFork','Выбор игрока'],['branch','GitFork','If · Если'],['gate','MousePointerClick','Ждать взаимодействие'],['merge','Merge','Схождение веток'],['end','Flag','Концовка']].map(([kind,icon,label])=>({label,icon,group:'Добавить ноду',action:()=>addBeat(kind,contextMenu)})),
      {label:'Добавить событие к выбранной реплике…',icon:'Layers',group:'События',action:()=>setPicker({kind:'event',phase:phase==='ALL'?'ON_START':phase})},
    ];
  };

  const renderGraph = graphMode => (<EditorGraph
                  project={project}
                  mode={graphMode}
                  selectedId={beat.id}
                  eventId={eventContext?.eventId}
                  binding={binding}
                  phase={phase}
                  scope={layout.scope}
                  selectionId={selection.id}
                  onBatch={changeBatch}
                  onSelect={graphSelect}
                  onOpen={openGraph}
                  onEdit={(data) => {
                    setSelection({ kind: "action", id: data.action.id });
                    setDetailEditor(true);
                    setMaximized("graph");
                  }}
                  onPhase={openStaging}
                  onPatchAction={patchAction}
                  onConnect={connect}
                  issues={issues}
                  preview={preview}
                  positions={layout.positions}
                  onPositions={setPositions}
                  onReady={graphMode === dock ? onGraphReady : undefined}
                  onClipboard={graphClipboard}
                  onContext={point=>{if(!rt.running)setContextMenu({...point,kind:graphMode});}}

                />);
  const renderHistory = () => (<div className="history-window-content"><input aria-label={tr("Поиск реплик")} placeholder={tr("Найти реплику…")} value={historyQuery} onChange={e=>setHistoryQuery(e.target.value)}/>{project.subscenes.map((s) => (
                <details key={s.id} open={!!historyQuery || (layout.treeOpen?.['history:'+s.id]??s.id===scene.id)}>
                  <summary onClick={e=>{e.preventDefault();setLayout(l=>({...l,treeOpen:{...l.treeOpen,['history:'+s.id]:!(l.treeOpen?.['history:'+s.id]??s.id===scene.id)}}));}}>
                    <Icon name="ChevronRight" size={12} />
                    <Icon name="PanelsTopLeft" size={14} />
                    {s.name}
                  </summary>
                  {project.chapters
                    .filter((c) => c.subsceneId === s.id)
                    .map((c) => (
                      <details key={c.id} open={!!historyQuery || (layout.treeOpen?.['chapter:'+c.id]??c.id===chapter.id)}>
                        <summary onClick={e=>{e.preventDefault();setLayout(l=>({...l,treeOpen:{...l.treeOpen,['chapter:'+c.id]:!(l.treeOpen?.['chapter:'+c.id]??c.id===chapter.id)}}));}}>
                          <Icon name="ChevronRight" size={11} />
                          {c.name}
                          <small>{c.beats.length}</small>
                        </summary>
                        {c.beats
                          .filter((b) =>
                            (b.text + b.id)
                              .toLowerCase()
                              .includes(historyQuery.toLowerCase()),
                          )
                          .map((b) => (
                            <button
                              className={
                                "story-tree-row " +
                                (beat.id === b.id ? "selected" : "")
                              }
                              key={b.id}
                              onClick={() => {
                                selectBeat(b.id);
                                setDock("story");
                              }}
                              aria-label={b.text}
                              title={b.id + " · " + b.text}
                            >
                              <Icon
                                name={
                                  b.kind === "choice"
                                    ? "GitFork"
                                    : b.kind === "gate"
                                      ? "MousePointer2"
                                      : "MessageSquare"
                                }
                                size={12}
                              />
                              <span>{b.text}</span>
                              {issues.some((i) => i.beatId === b.id) && (
                                <Icon name="TriangleAlert" size={11} />
                              )}
                            </button>
                          ))}
                      </details>
                    ))}
                </details>
              ))}</div>);
  return (
    <div
      className="editor-app"
      ref={root}
      style={{
        "--left": layout.left + "px",
        "--right": layout.right + "px",
        "--scene-height": layout.height + "px",
      }}
    >
      <header className="editor-menubar">
        <div className="editor-brand">
          <Icon name="Flower2" size={23} />
          <strong>
            Sacura<span>Novel Studio</span>
          </strong>
        </div>
        {["Файл", "Правка", "Создать", "Окна"].map((m) => (
          <button
            className={menu === m ? "active" : ""}
            key={m}
            aria-expanded={menu === m}
            onClick={(e) => {
              const rect = e.currentTarget.getBoundingClientRect();
              setMenuAnchor({left: Math.max(8, Math.min(rect.left, window.innerWidth - 250)), top: rect.bottom + 4});
              setMenu(menu === m ? null : m);
            }}
          >
            {tr(m)}
          </button>
        ))}
        <Button icon="Gamepad2" onClick={()=>{setDock('gameplay');setDetailEditor(false);setFloatingDock(false);setMaximized('graph');setCompactPanel('workspace');setMenu(null);}}>{tr('Игровые механики')}</Button>
        <div className="flex-space" />
        <span className="project-title" title={project.title}>
          <Icon name="FolderOpen" size={14} />
          {project.title}
        </span>
        <div className="main-play-controls">
          <Button
            className={running ? "playing" : ""}
            icon={running ? "Square" : "Play"}
            title={
              running
                ? tr("Остановить предпросмотр")
                : tr("Запустить с начала текущей сабсцены")
            }
            onClick={() => (running ? rt.stop() : start())}
          >{running ? tr("Стоп") : tr("Запуск")}</Button>
          <Button icon="Play" title={tr("Проверить историю с выделенной реплики")} disabled={running} onClick={()=>start(true)}>{tr("С реплики")}</Button>
          <Button
            icon="Pause"
            title={tr("Пауза / продолжить")}
            className={preview?.paused ? "active" : ""}
            disabled={!running || preview.phase === "FINISHED"}
            onClick={() => rt.togglePause()}
          />
          <Button
            icon="StepForward"
            title={tr("Следующая реплика")}
            disabled={
              !running ||
              !preview.ready ||
              preview.paused ||
              ["choice", "gate"].includes(playBeat.kind)
            }
            onClick={() => rt.advance()}
          />
        </div>
        <div className="toolbar-layout">
          <span className="play-label" hidden={!running}>
            {running
              ? preview.paused
                ? tr("На паузе")
                : tr("Предпросмотр")
              : tr("Редактирование")}
          </span>
          <Select
            aria-label={tr("Раскладка редактора")}
            value={maximized || "balanced"}
            onChange={(v) => setMaximized(v === "balanced" ? null : v)}
            options={[
              ["balanced", "Сцена и редактор"],
              ["graph", "Рабочая область"],
              ["scene", "Только сцена"],
              ["hierarchy", "Только иерархия"],
              ["inspector", "Только инспектор"],
            ]}
          />
        </div>
        {onHome&&<Button icon="FolderOpen" disabled={projectFileBusy} onClick={()=>runFileOperation(async()=>{rt.stop();await onHome(project);})}>{tr("Главное меню")}</Button>}
        {user&&<button onClick={()=>onLogout(project)} title={tr("Выйти из аккаунта {0}", [user.login])}>{user.login} {tr("· Выйти")}</button>}
        <LanguagePicker onOpen={() => setMenu(null)} />
        <ThemePicker onOpen={() => setMenu(null)} />
      </header>
      <div className={'editor-workspace compact-'+compactPanel+(layout.hiddenHierarchy?' hierarchy-hidden':'')+(layout.hiddenInspector?' inspector-hidden':'')+(['scene','graph'].includes(maximized)?' focus-center':maximized==='hierarchy'?' focus-hierarchy':maximized==='inspector'?' focus-inspector':'')}>
        <nav className="compact-panel-tabs" aria-label={tr("Панели редактора")}>
          {[['hierarchy','ListTree','Объекты сцены'],['workspace','PanelsTopLeft','Рабочая область'],['inspector','Settings2','Свойства']].map(([id,icon,label])=><button key={id} aria-pressed={compactPanel===id} onClick={()=>{if(['hierarchy','inspector'].includes(maximized))setMaximized(null);setCompactPanel(id);}}><Icon name={icon} size={13}/>{tr(label)}</button>)}
        </nav>
        <aside className="hierarchy-panel">
          <button className="panel-restore" aria-label={tr("Показать иерархию")} title={tr("Показать иерархию")} onClick={()=>setLayout(l=>({...l,hiddenHierarchy:false}))}><Icon name="PanelLeftOpen" size={18}/><span>{tr("Иерархия")}</span></button>
          <div className="panel-tabs">
            <button className="active">{tr("Иерархия")}</button>
            <div className="flex-space" />
            <Button
              icon="Plus"
              title={tr("Создать объект")}
              onClick={() => setPicker({ kind: "object" })}
            />
            <Button icon={maximized==='hierarchy'?'Minimize2':'Maximize2'} title={maximized==='hierarchy'?tr("Восстановить иерархию"):tr("Развернуть иерархию")} onClick={()=>setMaximized(v=>v==='hierarchy'?null:'hierarchy')}/>
            <Button icon="PanelLeftClose" title={tr("Свернуть иерархию")} onClick={()=>{setMaximized(null);setLayout(l=>({...l,hiddenHierarchy:true}));setCompactPanel('workspace');}}/>
          </div>
          <div className="panel-search">
            <Icon name="Search" size={13} />
            <input
              aria-label={tr("Поиск объектов")}
              placeholder={tr("Найти объект…")}
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
          </div>
          <div className="hierarchy-tree">
              <HierarchyTree onAddObject={groupId=>setPicker({kind:'object',groupId})} onAddPoint={addStagingPoint} onAddSound={addSoundEvent} onCreateEmptyGroup={addEmptyGroup} onRootDrop={ids=>{if(!rt.running)mutate(p=>removeObjectsFromGroups(p,displayScene.id,ids));}} groups={sceneObjectGroups(project,displayScene.id,objects)} disabled={running} onCreateGroup={createSelectionGroup} onSelectGroup={group=>{if(rt.running)return;setMode('scene');setSelection(group.objectIds.length?{kind:'object',id:group.objectIds[0],ids:group.objectIds,groupId:group.id}:{kind:'group',id:group.id});openObjectTransform();}} onGroupDrop={(groupId,ids)=>{if(!rt.running)mutate(p=>addObjectsToGroup(p,groupId,ids));}} scene={displayScene} objects={objects} points={sceneStagingPoints(displayScene,objects)} query={query} selected={selection.id} selectedIds={objectSelectionIds}
                expanded={layout.treeOpen||{}} onExpanded={treeOpen=>setLayout(l=>({...l,treeOpen}))}
                onSelectObject={(id,options)=>{if(!running){setMode('scene');setCameraPilotId(null);setCameraPreviewId(null);}selectObject(id,options);}}
                onFrameObject={id=>{editScene();setSelection({kind:'object',id});setFocusRequest({nonce:uid('focus')});}}
                onVisibility={id=>mutate(p=>{const o=p.objects.find(o=>o.id===id);o.active=o.active===false;})}
                onPoint={point=>{editScene();setSelection({kind:'point',id:point.id});openObjectTransform();setFocusRequest({nonce:uid('focus'),position:point.position,objectId:point.objectId});}}
                onSelectCamera={selectCamera} onCameras={openCameras} showCameras={showCameras} onShowCameras={()=>setShowCameras(v=>!v)}
                showDialogue={showDialogue} onShowDialogue={()=>setShowDialogue(v=>!v)} onStory={()=>{setDock('story');setDetailEditor(false);setMaximized(null);}}
                onAudio={()=>{setDock('active');setDetailEditor(false);setMaximized(null);}}/>

          </div>
          <div className="hierarchy-footer">
            <Icon name="Layers" size={13} />
            {objects.length} {tr("объектов")}<span />
            {nodes.length} {tr("блоков")}</div>
          <InfoView />
        </aside>
        <ResizeBar
          axis="x"
          label={tr("Ширина иерархии")}
          onMove={(d) => resize("left", d)}
          onReset={() => setLayout((l) => ({ ...l, left: 210 }))}
        />
        <section
          className={
            "editor-center " +
            (maximized === "graph"
              ? "graph-max"
              : maximized === "scene"
                ? "scene-max"
                : "")
          }
        >
          <section className="viewport-panel">
            <div className="panel-tabs viewport-tabs">
              <button
                aria-label={tr("Редактор локации")}
                aria-pressed={mode === "scene"}
                className={mode === "scene" ? "active" : ""}
                onClick={editScene}
              >
                <Icon name="Box" size={14} />
                {tr("Локация")}</button>
              <button
                aria-pressed={mode === "game"}
                className={mode === "game" ? "active" : ""}
                onClick={() => setMode("game")}
              >
                <Icon name="Gamepad2" size={15} />
                {tr("Игра")}</button>
              <div className="toolbar-project">
          <span className="toolbar-context-label">{tr("Сабсцена")}</span>
          <EditorSelect
            label={tr("Текущая сабсцена")}
            value={displayScene.id}
            onChange={(id) =>
              openSubscenes('edit',id)
            }
            options={project.subscenes.map((s) => [s.id,literalLabel(s.name)])}
          />
        </div>
              <div className="flex-space" />
              <Button icon="PanelsTopLeft" title={tr("Редактор интерфейса игры")} onClick={()=>openWidgets(resolveWidget(project,'dialogue',beat).id)}/>
              {mode==='game'&&<Button icon="Menu" title={tr("Главное меню игры")} onClick={()=>{rt.stop();setGameMenu('mainMenu');setPreviewWidgetId(null);}}>{tr("Меню")}</Button>}
              <button
                title={tr("Диалог в игровом кадре")}
                hidden={mode !== "game"}
                aria-pressed={showDialogue}
                className={showDialogue ? "toggled" : ""}
                onClick={() => setShowDialogue((v) => !v)}
              >
                <Icon name="PanelBottom" size={14} />
              </button>
              <button
                title={maximized==='scene'?tr("Восстановить сцену"):tr("Развернуть сцену")}
                onClick={() =>
                  setMaximized(maximized === "scene" ? null : "scene")
                }
              >
                <Icon
                  name={maximized === "scene" ? "Minimize2" : "Maximize2"}
                  size={14}
                />
              </button>
            </div>
            <div className="viewport-commandbar" aria-label={tr("Инструменты сцены")}>
              {mode==='scene'&&!cameraPilotId&&<div className="scene-edit-bar">
                <button className="scene-edit-add" onClick={()=>setPicker({kind:'object'})}><Icon name="Plus" size={14}/>{tr("Добавить объект / примитив")}</button>
                {[['select','MousePointer2','Выбор','Q'],['translate','Move','Сдвиг','W'],['rotate','Rotate3D','Поворот','E'],['scale','Scaling','Масштаб','R']].map(([tool,icon,label,key])=><button key={tool} disabled={running||(selection.kind==='camera'&&(tool==='scale'||(tool==='rotate'&&displayScene.cameras?.find(c=>c.id===selection.id)?.mode==='follow')))} title={tr(label)+' · '+key} aria-pressed={editTool===tool} className={'transform-tool '+(editTool===tool?'active':'')} onClick={()=>setEditTool(tool)}><Icon name={icon} size={14}/><span className="tool-label">{tr(label)} · {key}</span></button>)}
                <Button icon="Settings2" disabled={selection.kind!=='object'} title={tr("Позиция, вращение и масштаб выбранного объекта")} onClick={openObjectTransform}>{tr("Положение, поворот, масштаб")}</Button>
                <button title={tr("Привязка: 0,25 м / 15° / 0,1×")} aria-pressed={snap} className={snap?'active':''} onClick={()=>setSnap(v=>!v)}><Icon name="Magnet" size={14}/></button>
                <select aria-label={tr("Оси трансформации")} value={editSpace} onChange={e=>setEditSpace(e.target.value)}><option value="world">{tr("Мир")}</option><option value="local">{tr("Объект")}</option></select>
                <Button icon="Video" title={tr("Показать камеры в 3D")} aria-pressed={showCameras} className={showCameras?"active":""} onClick={()=>setShowCameras(v=>!v)}/>
                <Button icon="Grid3X3" title={tr("Сетка сцены")} aria-pressed={showGrid} className={showGrid?"active":""} onClick={()=>setShowGrid(v=>!v)}/>
                <Button icon="Focus" title={tr("Приблизить выбранный объект · F")} disabled={selection.kind!=='object'} onClick={()=>setFocusRequest({nonce:uid('focus')})}/>
              </div>}
              {cameraPilotId&&mode==='scene'&&<div className="camera-pilot-bar"><Icon name="Video"/><span>{tr("Настройка: ")}{displayScene.cameras?.find(c=>c.id===cameraPilotId)?.name}<small>{tr("Обзор мышью · правая кнопка — сдвиг · колесо — приближение")}</small></span><Button icon="Check" onClick={()=>captureCamera(cameraPilotId)}>{tr("Сохранить ракурс")}</Button><Button icon="X" title={tr("Вернуться без сохранения")} onClick={()=>setCameraPilotId(null)}/></div>}
              {(mode==='game'||(cameraPreviewId&&!cameraPilotId))&&<div className="camera-view-badge"><Icon name={resolveCamera(displayScene,world,objects,cameraPreviewId).mode==='follow'?'UserRoundCheck':'Video'} size={14}/><span className="camera-view-name">{resolveCamera(displayScene,world,objects,cameraPreviewId).name}</span>{cameraPreviewId&&<button onClick={()=>setCameraPreviewId(null)}>{mode==='game'?tr("По сценарию"):tr("Вернуться к сцене")} <Icon name="X" size={12}/></button>}</div>}
              <details className="viewport-help"><summary title={tr("Управление сценой")} aria-label={tr("Управление сценой")}><Icon name="CircleHelp" size={15}/></summary><div>{mode==='game'?<>{tr("Кадр показан из активной камеры.")}<br/>{tr("Выберите камеру в иерархии, чтобы посмотреть её ракурс.")}<br/>{tr("«По сценарию» возвращает камеру, выбранную историей.")}<br/>{inputHelp(playProject.input,inputContextsFor(playProject,playBeat),['move','interact','point','advance','pause'])}<br/>{tr("Для настройки ракурса вернитесь в «Локацию».")}</>:<>{tr("W — перемещение · E — вращение")}<br/>{tr("R — масштаб · Q — выбор")}<br/>{tr("Работают и в русской раскладке")}<br/>{tr("Shift + щелчок — мультивыбор")}<br/>{tr("Alt + ЛКМ — орбита · СКМ — панорама")}<br/>{tr("ПКМ + WASD / QE — полёт")}<br/>{tr("F — выделение в кадр")}<br/>{tr("Delete — удалить выделенные объекты")}<br/>{tr("Ctrl+Z — вернуть удалённые объекты")}</>}</div></details>
            </div>
            <div
              className="scene-viewport"
              tabIndex={0}
              onKeyDown={(e) => {
                if(e.currentTarget.dataset.navigating==='true')return;
                if(mode==='scene'&&!running&&!e.ctrlKey&&!e.metaKey&&!e.altKey&&!e.shiftKey&&!['INPUT','SELECT','TEXTAREA'].includes(e.target.tagName)&&!e.target.isContentEditable&&e.code==='KeyF'){e.preventDefault();setFocusRequest({nonce:uid('focus')});}
                if (
                  mode === "game" &&
                  !running && !e.defaultPrevented &&
                  [" ", "Enter"].includes(e.key) &&
                  e.target === e.currentTarget
                ) {
                  e.preventDefault();
                  start();
                }
              }}
            >
              <LocationScene
                sceneId={displayScene.id}
                editTool={editTool} editSpace={editSpace} snap={snap} focusRequest={cameraPilotId?0:focusRequest} editing={!running&&!cameraPilotId&&!positionPick} onTransform={transformObject} onTransforms={transformObjects}
                cameraScene={displayScene} selectedCameraId={selection.kind==='camera'?selection.id:null} cameraPreviewId={cameraPreviewId} cameraPilotId={cameraPilotId} onCameraChange={changeCamera} onCameraSelect={selectCamera} cameraApi={cameraApi} physicsApi={physicsApi} showCameras={showCameras}
                kind={displayScene.kind}
                objects={objects}
                state={world}
                selected={selection.kind === "object" ? selection.id : null}
                selectedIds={objectSelectionIds}
                mode={mode}
                showGrid={showGrid}
                actionPosition={(()=>{if(selection.kind==='point')return sceneStagingPoints(displayScene,objects).find(p=>p.id===selection.id)?.position;const a=inspectedAction||(binding?bindingActions(project,binding):event?.groups.flatMap(g=>g.actions))?.find(a=>a.type==='move');return a?.type==='move'&&Array.isArray(a.value)?a.value:null;})()}
                onPickPosition={positionPick?value=>{
                  const request=positionPick;
                  mutate(p=>{if(request.context.bindingId){const b=allBeats(p).find(b=>b.id===request.context.beatId)?.bindings.find(b=>b.id===request.context.bindingId);if(b){b.actionOverrides||={};b.actionOverrides[request.id]={...b.actionOverrides[request.id],value};}}
                    else {const a=p.events.find(e=>e.id===request.context.eventId)?.groups.flatMap(g=>g.actions).find(a=>a.id===request.id);if(a)a.value=value;}});
                  setPositionPick(null);setNotice(tr("Координаты выбраны: {0}", [value.join(', ')]));
                }:undefined}
                onSelect={selectObject}
                onContext={point=>{if(!rt.running)setContextMenu({...point,kind:'scene'});}}
                inputSettings={playProject.input}
                onInputAction={event=>rt.inputAction(event)}
                onInputDevices={setInputDevices}
                onPlayerStep={(direction,dt)=>rt.playerStep(direction,dt)}
                onPlayerClick={(point,id)=>rt.playerClick(point,id)}
                onPlayerInteract={()=>rt.playerInteract()}
                onInteract={interact}
              />
              {mode==='game'&&<TouchInput settings={playProject.input} contexts={world.inputContexts||[]} active={world.inputActive} playerMode={world.playerControl?.mode} paused={world.paused||!world.inputReady}/>}
              {positionPick&&<div className="viewport-caption"><strong>{tr("Выберите точку на сцене")}</strong><Button icon="X" onClick={()=>setPositionPick(null)}>{tr("Отмена")}</Button></div>}
              <GameDialogue project={running?rt.project:project}
                beat={playBeat}
                onUiAction={widgetAction}
                widgetOverride={!running?project.widgets.find(w=>w.id===previewWidgetId&&w.kind==='dialogue'):null}
                preview={preview}
                running={running}
                variables={variables}
                onAdvance={(c) => rt.advance(c)}
                onStart={start}
                objectLabel={
                  playProject.objects.find((o) => o.id === playBeat.signal)
                    ?.name
                }
                show={mode==="game"&&showDialogue&&!preview?.audition&&!gameMenu&&!preview?.paused}
              />
              {running&&preview?.phase==='PLAYING'&&<GameHUD project={playProject} preview={preview} onAction={widgetAction} text={playBeat.text}/>}
              {mode==='game'&&(gameMenu==='mainMenu'||(running&&preview?.paused&&!preview?.audition))&&<WidgetMenu label={gameMenu==='mainMenu'?tr('Главное меню'):tr('Пауза')} widget={project.widgets.find(w=>w.id===previewWidgetId&&w.kind===(gameMenu==='mainMenu'?'mainMenu':'pauseMenu'))||resolveWidget(project,gameMenu==='mainMenu'?'mainMenu':'pauseMenu')}
                  context={{project:playProject,variables,game:preview?.world.game,title:project.title,paused:!!preview?.paused}} onAction={widgetAction}/>}
              {running && !preview.textVisible && !preview.audition && (
                <div className="scene-preparing">
                  <Icon name="LoaderCircle" size={15} />
                  {message(preview.error) || tr("Подготовка сцены перед репликой")}
                </div>
              )}
              {running&&!preview.world.playerControl&&<div className="scene-activity" aria-live="polite">
                {preview.audition&&<header><Icon name="Clapperboard" size={13}/><strong>{tr("Проба · ")}{preview.auditionName}</strong><button title={tr("Закончить пробу")} onClick={()=>rt.stop()}><Icon name="X" size={13}/></button></header>}
                {(preview.activity||[]).slice(-4).map(a=><div className={'activity-item '+a.status} key={a.id}><Icon name={a.status==='running'?'LoaderCircle':'Check'} size={12}/><span>{tr(TYPES[a.type]?.label)} <b>{project.objects.find(o=>o.id===a.target)?.name||''} {String(a.value).slice(0,42)}</b></span>{a.status==='running'&&<progress max="1" value={a.progress||0}/>}</div>)}
                {preview.error&&<span className="error-box">{message(preview.error)}</span>}
              </div>}
              {running && <div className="viewport-caption">
                <Icon name="Video" size={12} />
                {mode === "game"
                  ? tr("Главная камера")
                  : tr("Перспектива · вращение мышью")}
                <span />
                {running
                  ? `${playBeat.id} / ${preview.phase}`
                  : tr("3D · примитивы")}
              </div>}
            </div>
            {(running || [...soundDesk.tracks.values()].some(t=>["playing","error"].includes(t.status))) && <div className="live-strip">
              <output className="audio-output" title={tr("Измеренный уровень на аудиовыходе")} aria-label={tr("Уровень звукового выхода")}><Icon name="Volume2" size={13}/><meter min="0" max="1" value={[...soundDesk.tracks.values()].some(t=>t.status==='playing')?(soundDesk.output?.level()||0):0}/><span>{soundDesk.output?.context?.state==='running'?tr("Аудио включено"):tr("Аудио · нажмите Play")}</span></output>
              <button onClick={() => setDock("active")}>
                <Icon name="Activity" size={13} />
                {running
                  ? Object.values(preview.effects).filter(
                      (e) => e.status !== "stopped",
                    ).length
                  : "2"}{" "}
                {tr("активных состояния")}</button>
              <span className="live-separator" />
              <span>
                <Icon name="Music2" size={13} />
                {soundDesk.get("background")?.status === "playing"
                  ? tr("Главная тема · ") +
                    clockLabel(soundDesk.get("background").audio.currentTime)
                  : tr("Музыка не играет")}
              </span>
              <div className="flex-space" />
              {soundDesk.ducks.size > 0 && (
                <span className="duck-label">{tr("Голос → музыка −")}{normalizeSidechain(project.audioSettings?.sidechain).reductionDb} dB</span>
              )}
              {[...soundDesk.tracks.values()].some(t=>t.status==='error')&&<button className="audio-error" onClick={()=>setDock('sound')}>{tr("Ошибка аудио · открыть")}</button>}
              <button onClick={() => setDock("active")}>
                {tr("Управление ")}<Icon name="ChevronRight" size={12} />
              </button>
            </div>}
          </section>
          <ResizeBar
            axis="y"
            label={tr("Размер сцены и графа")}
            onMove={(d) => resize("height", d)}
            onReset={() =>
              setLayout((l) => ({ ...l, height: defaultSceneHeight() }))
            }
          />
          <FloatingWindow active={floatingDock} title={tr("Рабочая область")} onClose={()=>setFloatingDock(false)}><section className="graph-dock">
            <div className="panel-tabs dock-tabs">
              <DockTools dock={["story","timeline","staging"].includes(dock)?"story":dock} event={event} issueCount={issues.length} onSelect={id => {
                setDock(id);
                if (id === "create") setMaximized("graph");
                else setMaximized(null);
                if(id==='characters'){if(rt.running)rt.stop();setMode('scene');setCompactPanel('workspace');}
                setDetailEditor(false);
              }} />
              <div className="flex-space" />
              <Button
                icon={maximized === "graph" ? "Minimize2" : "Maximize2"}
                title={maximized==='graph'?tr("Восстановить рабочую область"):tr("Развернуть рабочую область")}
                onClick={() =>
                  setMaximized(maximized === "graph" ? null : "graph")
                }
              />
            </div>
            {["story", "timeline", "staging", "event"].includes(dock) && !detailEditor && (
              <div className="graph-toolbar">
                {dock==='event'?<><Button icon="ArrowLeft" title={tr("Вернуться в поток истории")} onClick={()=>setDock('story')}/><span className="graph-breadcrumb">{event?.name}</span></>:<><Icon name="Route" size={15}/><span className="flow-toolbar-context">{project.subscenes.length} {tr("сабсцены · ")}{nodes.length} {tr("реплик · единый поток")}</span><Button icon="Focus" onClick={()=>graphApi.current?.focus(beat.id)}>{tr("К реплике")}</Button><Button icon="Scan" onClick={()=>graphApi.current?.fit()}>{tr("Вся история")}</Button></>}
                <div className="flex-space" />
                <details className="viewport-help graph-help"><summary title={tr("Навигация по графу")} aria-label={tr("Навигация по графу")}><Icon name="CircleHelp" size={15}/></summary><div>{tr("ПКМ — панорама")}<br/>{tr("Колесо — масштаб")}<br/>{tr("Пин → пин — соединить")}<br/>{tr("Alt + щелчок по пину — разорвать связи")}<br/>{tr("Ctrl + перетаскивание пина — перенести связи")}<br/>{tr("Выбрать провод + Delete — разорвать")}<br/>{tr("ЛКМ — выбор и перемещение узла")}<br/>{tr("Двойной щелчок — открыть узел")}</div></details>
                {dock !== "event" && beat.kind === "choice" && (
                  <Button
                    icon="GitFork"
                    onClick={() => {
                      graphApi.current?.focus(beat.id);
                    }}
                  >
                    {tr("Обзор развилки")}</Button>
                )}
                {running && (
                  <button
                    title={tr("Следовать за проигрыванием")}
                    className={follow ? "active" : ""}
                    onClick={() => {
                      setFollow(!follow);
                      if (!follow) graphApi.current?.focus(preview.beatId);
                    }}
                  >
                    <Icon name="LocateFixed" size={13} />
                  </button>
                )}
                <Button
                  icon="AlignHorizontalDistributeCenter"
                  title={tr("Расположить узлы автоматически")}
                  onClick={() => graphApi.current?.arrange()}
                />
                <Button
                  className="rose-button"
                  icon="Plus"
                  onClick={e =>
                    dock === 'story' ? setContextMenu({kind:'story',x:e.currentTarget.getBoundingClientRect().left,y:e.currentTarget.getBoundingClientRect().bottom+4}) : setPicker({
                      kind:
                        dock !== "event"
                          ? "beat"
                          : dock === "event"
                            ? "action"
                            : "event",
                      phase: phase === "ALL" ? "ON_START" : phase,
                      groupId:
                        selection.kind === "group"
                          ? selection.id
                          : event?.groups.find((g) =>
                              g.actions.some((a) => a.id === selection.id),
                            )?.id,
                    })
                  }
                >
                  {dock !== "event"
                    ? tr("Нода")
                    : dock === "event"
                      ? tr("Действие")
                      : tr("Событие")}
                </Button>
              </div>
            )}
            <div
              className="dock-content"
              onDragOver={(e) => e.preventDefault()}
              onDrop={(e) => {
                e.preventDefault();
                try {
                  const a = JSON.parse(
                    e.dataTransfer.getData("application/sacura-asset"),
                  );
                  if (a.kind === "events") {
                    setPicker({
                      kind: "event",
                      phase: "ON_START",
                      assetId: a.id,
                    });
                  } else if (a.kind === "objects")
                    setPicker({ kind: "object-action", objectId: a.id });
                } catch {}
              }}
            >
              <AuthoringWorkspace hidden={dock!=='create'} project={project} request={authorRequest} mutate={mutate} beat={beat} onNotice={setNotice}
                onGraph={id=>{setEventContext({eventId:id});setSelection({kind:'event',id});setDock('event');}}
                onPreview={draft=>{setCameraPreviewId(null);setCameraPilotId(null);const test=structuredClone(project);const i=test.events.findIndex(e=>e.id===draft.id);if(i<0)test.events.push(draft);else test.events[i]=draft;soundDesk.unlock();rt.stop();setMode('game');setMaximized(null);setFollow(false);rt.previewEvent(test,draft.id,beat.id);}}
                onPlace={(draft,hook)=>{mutate(p=>addToBatch(p,beat.id,hook,null,draft.id));setPhase(hook);setDock('story');setMaximized(null);setNotice(tr("Добавлено: {0} · {1}", [draft.name, PHASES.find(x=>x.id===hook).label]));}}/>
              {dock==='create'?null:dock==='gameplay'?<GameplayWorkspace project={project} onChange={mutate} sceneId={displayScene.id} running={running} onStop={()=>rt.stop()} onWidgets={()=>{setWidgetRequest({id:project.gameplay.hudWidgetId||project.ui.hud});setDock('widgets');}} onInput={()=>setDock('input')} onEvent={id=>openAuthoring('event',id?{id}:{})} onStory={()=>{setDock('story');setDetailEditor(false);}}/>:dock==='input'?<InputWorkspace project={project} onChange={mutate} beat={playBeat} preview={preview} devices={inputDevices} running={running} onStop={()=>rt.stop()}/>:detailEditor && event ? (
                <EventEditor
                  project={project}
                  event={event}
                  binding={binding}
                  beat={contextBeat}
                  update={mutate}
                  onBack={() => setDetailEditor(false)}
                  onTemplate={() => setEventContext({ eventId: event.id })}
                  onAdd={addEvent}
                  onPreview={()=>audition(event.id)}
                />
              ) : ["story", "timeline", "staging"].includes(dock) ? (
                <StoryFlow running={running} onVariableEdit={updateVariable} onVariableAdd={(variable,kind,point)=>{if(!rt.running)addBeat(kind,{...point,variable});}} onInspectVariable={inspectVariable} selectedVariable={inspectedVariable} onVariableDrop={point=>{if(rt.running)return;if(point.get||point.set)addBeat(point.set&&point.variable!==ANSWER_VARIABLE?'set-variable':'variable',point);else setContextMenu({...point,kind:'variable-drop'});}} onLogicChange={(id,patch)=>{if(!rt.running)mutate(p=>Object.assign(allBeats(p).find(n=>n.id===id),patch));}} onVariable={(id,name,type,value)=>{if(!rt.running)mutate(p=>{const node=allBeats(p).find(n=>n.id===id);node.valueType=type;node.variable=name.trim();if(node.variable&&node.variable!==ANSWER_VARIABLE)p.variables[node.variable]=typedValue(value,type);});}} onChoice={(id,choiceId,patch)=>{if(!rt.running)mutate(p=>{const b=allBeats(p).find(n=>n.id===id);if(!choiceId)b.choices.push(newChoice(uid('choice'),patch.label));else if(!patch)b.choices=b.choices.filter(c=>c.id!==choiceId);else Object.assign(b.choices.find(c=>c.id===choiceId),patch);});}} onClipboard={graphClipboard} contextNodeId={contextNodeId} project={project} issues={issues} selectedId={beat.id} selectionId={selection.id} preview={preview}
                  onSelect={graphSelect} onOpen={openGraph} onMoveBinding={moveBinding} onBatch={changeBatch} onConnect={connect} onDeleteNode={deleteFlowNode}
                  onScene={id=>openSubscenes('edit',id)}
                  onContext={point=>{if(!rt.running)setContextMenu({...point,kind:point.kind||'story'});}}
                  onAdd={(id,hook)=>{setSelectedBeat(id);setSelection({kind:'beat',id});setPicker({kind:'event',phase:hook});}}
                  positions={layout.positions['flow:all']||{}} onPositions={positions=>setPositions('flow:all',positions)} onReady={onGraphReady}/>
              ) : dock === 'event' ? renderGraph('event') : dock === 'cameras' ? <CameraWorkspace scene={displayScene} objects={objects} selected={selection.kind==='camera'?selection.id:null} onSelect={selectCamera} onCreate={()=>createCamera()} onFollow={followCharacter} running={running} onEdit={editScene}/> : dock === 'subscenes' ? <SubsceneWorkspace project={project} scene={displayScene} beat={nodes.find(b=>b.id===subsceneDraft?.fromBeatId)||beat} request={subsceneRequest} draft={subsceneDraft} onDraftChange={setSubsceneDraft} running={running}
                onSaveLocation={async()=>{try{const data=await portableProject(exportLocation(project,displayScene.id));const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:'application/json'}));const link=document.createElement('a');link.href=url;link.download='location.sacura-location.json';link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);setNotice('Локация сохранена в файл.');}catch(e){setNotice(e.message);}}}
                onLoadLocation={async file=>{try{if(rt.running)return;validateImportSize(file);const raw=await file.text(),next=structuredClone(project),created=importLocation(next,raw);mutate(p=>Object.assign(p,next));setSelectedBeat(created.entry);setSelection({kind:'scene',id:created.id});setMode('scene');setMaximized(null);setNotice(tr("Локация загружена: {0}", [created.name]));}catch(e){setNotice(tr("Локация не загружена: {0}", [e.message]));}}}
                onStop={()=>rt.stop()} onSelect={id=>openSubscenes('edit',id)} onCreate={draft=>saveNewSubscene(draft)} onDuplicate={id=>saveNewSubscene(null,id)}
                onPatch={patch=>editSubscene(p=>Object.assign(p.subscenes.find(s=>s.id===scene.id),patch))}
                onKind={kind=>editSubscene(p=>changeSceneLocation(p,scene.id,kind))} onEntry={(id,reroute)=>editSubscene(p=>setSceneEntry(p,scene.id,id,reroute))}
                onConnect={(from,choice,to)=>{editSubscene(p=>connectSubscene(p,from,choice,to));setNotice('Переход сохранён. Он виден на карте и работает при запуске.');}}
                onDisconnect={(from,choice,to)=>{editSubscene(p=>{const b=allBeats(p).find(b=>b.id===from),edge=choice?b?.choices.find(c=>c.id===choice):b;if(edge?.next===to)edge.next=null;});setNotice('Переход убран. Ctrl+Z — вернуть.');}}
                onOpenBeat={openSubsceneBeat} onScene={editScene} onPrimitive={shape=>addObject("Меш",shape)} onCameras={openCameras} onCreateRequest={()=>openSubscenes('create')}
                onCancel={()=>setSubsceneRequest({mode:'edit',token:uid('request')})}/> : dock === 'samples' ? <EventPlayground project={project} preview={preview} scene={displayScene} onPreview={audition} onEdit={editSample} onAdd={id=>{mutate(p=>addToBatch(p,beat.id,'ON_START',null,id));setNotice(tr("Событие добавлено: во время реплики {0}", [beat.id]));}}/> : dock === "widgets" ? <WidgetWorkspace project={project} onChange={mutate} request={widgetRequest} beat={beat} running={running} onStop={()=>{rt.stop();setGameMenu(null);}}
                onPreview={widget=>{setMode('game');setFloatingDock(false);setPreviewWidgetId(widget.id);if(widget.kind==='mainMenu'){setGameMenu('mainMenu');}else if(widget.kind==='pauseMenu'){start();setPreviewWidgetId(widget.id);rt.togglePause();}else{setGameMenu(null);setShowDialogue(true);}}}/>: dock === "characters" ? <CharacterWorkspace project={project} scene={displayScene} selected={selection.kind==="object"?selection.id:null} running={running} onStop={()=>rt.stop()}
                onSelect={id=>{setSelection({kind:"object",id});setCompactPanel("workspace");}}
                onCreate={name=>{const next=structuredClone(project),character=createCharacter(next,scene.id,name);mutate(p=>Object.assign(p,next));setSelection({kind:"object",id:character.id});setMode("scene");setCompactPanel("workspace");}}
                onCustomizeWidget={id=>customizeWidget({characterId:id})} onEditWidget={openWidgets} onPatch={patchCharacter} onPresence={(id,sceneId,present)=>editSubscene(p=>setCharacterInScene(p,id,sceneId,present))}
                onTransform={(id,sceneId,value)=>editSubscene(p=>setObjectTransform(p,id,sceneId,value))}
                onReset={(id,sceneId)=>editSubscene(p=>{const character=p.objects.find(object=>object.id===id);if(character?.transforms)delete character.transforms[sceneId];})}
                onPlace={(id,sceneId)=>{openSubscenes("edit",sceneId);editScene();setSelection({kind:"object",id});setFocusRequest({nonce:uid("focus")});}}
                onDuplicate={duplicateCharacter} onDelete={id=>{if(!rt.running)deleteObject(id);}}/> : dock === "files" ? (
                <AssetExplorer onSelectFile={id=>{setSelection({kind:'file',id});setLayout(l=>({...l,hiddenInspector:false}));setCompactPanel('inspector');setMaximized(null);}} project={project} audioAssets={projectAudioAssets(project)} disabled={running}
                  onImport={importProjectAudioFiles}
                  onCreateFolder={path=>changeAssetLibrary(p=>{p.assetFolders=[...new Set([...(p.assetFolders||[]),path])];})}
                  onSelectObject={selectObject} onPlaceModel={placeLibraryModel} onAudio={()=>{setDock('sound');setDetailEditor(false);}}/>
              ) : dock === "assets" ? (
                renderAssets()
              ) : dock === "sound" ? (
                <SoundWorkspace project={project} onAssetTimingChange={(id,patch)=>mutate(p=>{p.audioSettings??={};p.audioSettings.assets??={};p.audioSettings.assets[id]={...p.audioSettings.assets[id],...patch};})} disabled={running} onImport={importProjectAudioFiles} onKindChange={(id,kind)=>mutate(p=>{const file=p.assetFiles?.find(file=>file.id===id);if(file)file.audioKind=kind;else{p.assetOverrides??={};p.assetOverrides['audio-file:'+id]={...p.assetOverrides['audio-file:'+id],audioKind:kind};}})} onAttach={attachSound} onSidechainChange={sidechain=>mutate(p=>{p.audioSettings={...p.audioSettings,sidechain};})} />
              ) : dock === "active" ? (
                renderActive()
              ) : (
                <div className="issues-workspace">
                  <div className="issues-toolbar">
                    <Icon
                      name={issues.length ? "ShieldAlert" : "ShieldCheck"}
                      size={18}
                    />
                    <strong>
                      {issues.length
                        ? issues.length + tr(" проблемы в проекте")
                        : tr("Проверки пройдены")}
                    </strong>
                    <span>{tr("Проверено: ресурсы, ожидания, переходы")}</span>
                  </div>
                  {issues.map((i) => (
                    <div className="issue-row" key={i.id}>
                      <Icon name="TriangleAlert" size={17} />
                      <button
                        onClick={() => {
                          selectBeat(i.beatId);
                          setDock("story");
                        }}
                      >
                        <strong>{tr(i.title)}</strong>
                        <span>{message(i.detail)}</span>
                        <small>
                          {i.beatId} · {sceneFor(project, i.beatId).location}
                        </small>
                      </button>
                      {i.fix && (
                        <Button onClick={() => fix(i)}>
                          {i.fix === "sequence-batch"
                            ? tr("Сделать по очереди")
                            : tr("Исправить")}
                        </Button>
                      )}
                    </div>
                  ))}
                  <details className="diagnostic-examples"><summary>{tr("Примеры диагностики")}</summary><FailureLab /></details>
                </div>
              )}
            </div>
          </section></FloatingWindow>
        </section>
        <ResizeBar
          axis="x"
          label={tr("Ширина инспектора")}
          onMove={(d) => resize("right", -d)}
          onReset={() => setLayout((l) => ({ ...l, right: 280 }))}
        />
        <aside className="inspector-panel">
          <button className="panel-restore" aria-label={tr("Показать инспектор")} title={tr("Показать инспектор")} onClick={()=>setLayout(l=>({...l,hiddenInspector:false}))}><Icon name="PanelRightOpen" size={18}/><span>{tr("Инспектор")}</span></button>
          <div className="panel-tabs">
            <button className="active">{tr("Инспектор")}</button>
            <button
              onClick={() => openSubscenes()}
            >
              {tr("Сабсцена")}</button>
            <div className="flex-space" />
            <Button
              icon={inspectorPinned ? "LockKeyhole" : "Pin"}
              title={tr("Закрепить инспектор")}
              className={inspectorPinned ? "active" : ""}
              onClick={() => setInspectorPinned((v) => !v)}
            />
            <Button icon={maximized==='inspector'?'Minimize2':'Maximize2'} title={maximized==='inspector'?tr("Восстановить инспектор"):tr("Развернуть инспектор")} onClick={()=>setMaximized(v=>v==='inspector'?null:'inspector')}/>
            <Button icon="PanelRightClose" title={tr("Свернуть инспектор")} onClick={()=>{setMaximized(null);setLayout(l=>({...l,hiddenInspector:true}));setCompactPanel('workspace');}}/>
          </div>
          <div className="inspector-scroll">
            {selection.kind === "scene" ? (
              <>
                <div className="inspector-identity">
                  <Icon name="PanelsTopLeft" size={25} />
                  <strong>{scene.name}</strong>
                </div>
                <Fold title={tr("Сабсцена")} icon="MapPin">
                  <p>{scene.location} · {tr(scene.weather)} · {tr(scene.time)}</p>
                  {scene.description && <p>{scene.description}</p>}
                  <Button icon="Settings2" onClick={() => openSubscenes()}>{tr("Открыть редактор сабсцены")}</Button>
                  <Button icon="Box" onClick={editScene}>{tr("Редактировать 3D-сцену")}</Button>
                  <Button icon="Workflow" onClick={() => openSubsceneBeat(scene.entry)}>{tr("Открыть сценарий")}</Button>
                </Fold>
              </>
            ) : (
              renderInspector()
            )}

          </div>
          {running && (
            <div className="inspector-play-note">
              <Icon name="Info" size={13} />
              {tr("Изменения — для следующего запуска")}</div>
          )}
        </aside>
      </div>
      <footer className="editor-status">
        <button className={"diagnostic-status "+(issues.length?"has-issues":"clear")} onClick={() => setDock("issues")}>
          <Icon
            name={issues.length ? "TriangleAlert" : "CheckCheck"}
            size={13}
          />
          {tr("Диагностика · ")}{issues.length}
        </button>
        <span className="status-divider" />
        <span data-help-title={tr("Хранение проекта")} data-help={serverStorageEnabled?tr("Автосохранение записывает проект в серверную БД, модели — в S3. Ctrl+S сохраняет сразу. Серверные проекты доступны через меню «Файл». Для переносимой копии используйте экспорт JSON."):tr("Автосохранение хранит последний проект в браузере. Ctrl+S сохраняет файл JSON.")}>{saveError || notice || (serverStorageEnabled?tr("Сохранено на сервере · Ctrl+S"):tr("Автосохранение в браузере · файл: Ctrl+S"))}</span>
        <div className="flex-space" />
        <span>
          {tr("Выделено:")}{" "}
          {selection.kind === "object" ? inspectedObject?.name : beatPreview(beat)}
        </span>

      </footer>
      {serverProjects&&<FloatingWindow active title={tr("Проекты на сервере")} onClose={()=>setServerProjects(null)}>
        {!serverProjects.length&&<p>{tr("Сохранённых проектов пока нет.")}</p>}
        {serverProjects.map(item=><button key={item.id} disabled={projectFileBusy} onClick={()=>selectServerProject(item.id)}>{item.title} · {new Date(item.updatedAt).toLocaleString(localeTag())}</button>)}
        {projectFileError&&<p role="alert">{message(projectFileError)}</p>}
      </FloatingWindow>}
      {historyOpen && <FloatingWindow active title={tr("История")} onClose={()=>setHistoryOpen(false)}>{renderHistory()}</FloatingWindow>}
      {menu && (
        <div
          className="editor-menu"
          style={menuAnchor}
        >
          {menu === "Файл" ? (
            <>
              <button disabled={projectFileBusy} onClick={()=>openProjectDialog('new')}><Icon name="FilePlus2"/>{tr("Новый пустой проект…")}</button>
              {serverStorageEnabled&&<button disabled={projectFileBusy} onClick={showServerProjects}><Icon name="Database"/>{tr("Открыть с сервера…")}</button>}
              <button disabled={projectFileBusy} onClick={openProjectFile}>
                <Icon name="FolderOpen" />
                {tr("Открыть проект…")}</button>
              <button disabled={projectFileBusy} onClick={saveProject}><Icon name="Save"/>{tr("Сохранить проект ")}<kbd>Ctrl S</kbd></button>
              <button disabled={projectFileBusy} onClick={()=>openProjectDialog('saveAs')}><Icon name="SaveAll"/>{tr("Сохранить проект как… ")}<kbd>Ctrl Shift S</kbd></button>
              <button disabled={projectFileBusy} onClick={()=>openProjectDialog('template')}><Icon name="BookCopy"/>{tr("Записать проект как шаблон…")}</button>
              <button disabled={projectFileBusy} onClick={()=>runFileOperation(async()=>{downloadFile(JSON.stringify(createProjectTemplate(await portableProject(project),project.title),null,2),projectFilename(project.title,true));setMenu(null);})}><Icon name="FileDown"/>{tr("Экспортировать шаблон…")}</button>
              <button disabled={projectFileBusy} onClick={()=>{setMenu(null);runFileOperation(async()=>{await exportPlayableGame(project);setNotice(tr('Игра экспортирована в HTML. Откройте скачанный файл в браузере.'));});}}>{tr('Экспортировать игру (HTML)…')}</button>
              <button onClick={exportProject}>
                <Icon name="Download" />
                {tr("Экспортировать JSON")}</button>
              <button disabled={projectFileBusy} onClick={restorePreviousProject}><Icon name="History"/>{tr("Восстановить предыдущий проект")}</button>
            </>
          ) : menu === "Правка" ? (
            <><button disabled={running} onClick={()=>editClipboard('copy')}><Icon name="Copy"/>{tr("Копировать ")}<kbd>Ctrl C</kbd></button><button disabled={running||!clipboard.current} onClick={()=>editClipboard('paste')}><Icon name="ClipboardPaste"/>{tr("Вставить ")}<kbd>Ctrl V</kbd></button><button onClick={resetLayout}><Icon name="PanelsTopLeft"/>{tr("Восстановить раскладку")}</button><button
              disabled={!history.length}
              onClick={() => {
                undo();
                setMenu(null);
              }}
            >
              <Icon name="Undo2" />
              {tr("Отменить ")}<kbd>Ctrl Z</kbd>
            </button></>
          ) : menu === "Создать" ? (
            <>
              <button onClick={()=>openSubscenes('create')}><Icon name="PanelsTopLeft"/>{tr("Сабсцена")}</button>
              <button onClick={()=>openAuthoring('action')}><Icon name="MousePointer2"/>{tr("Действие")}</button>
              <button onClick={()=>openAuthoring('group')}><Icon name="Columns2"/>{tr("Группа действий")}</button>
              <button
                onClick={() => {
                  setMenu(null);
                  setPicker({ kind: "beat" });
                }}
              >
                <Icon name="MessageSquare" />
                {tr("Блок сценария")}</button>
              <button
                onClick={() => {
                  setMenu(null);
                  createEvent();
                }}
              >
                <Icon name="Layers" />
                {tr("Событие")}</button>
              <button
                onClick={() => {
                  setMenu(null);
                  setPicker({ kind: "object" });
                }}
              >
                <Icon name="Box" />
                {tr("Объект")}</button>
            </>
          ) : (
            <>
              <button onClick={()=>{setMaximized(null);setLayout(l=>({...l,hiddenHierarchy:hierarchyVisible}));setCompactPanel(hierarchyVisible?'workspace':'hierarchy');setMenu(null);}}><Icon name="PanelLeft"/>{hierarchyVisible?tr("Свернуть"):tr("Показать")} {tr("иерархию")}</button>
              <button onClick={()=>{setMaximized(null);setLayout(l=>({...l,hiddenInspector:inspectorVisible}));setCompactPanel(inspectorVisible?'workspace':'inspector');setMenu(null);}}><Icon name="PanelRight"/>{inspectorVisible?tr("Свернуть"):tr("Показать")} {tr("инспектор")}</button>
              <button onClick={()=>{setHistoryOpen(true);setMenu(null);}}><Icon name="BookOpen"/>{tr("История · отдельное окно")}</button>
              {dockTools.map(([id,label])=><button key={id} disabled={id==='event'&&!event} onClick={()=>{setDock(id);setDetailEditor(false);setMaximized(null);setFloatingDock(true);setMenu(null);}}><Icon name="AppWindow"/>{tr(label)}</button>)}
              <button
                onClick={() => {
                  setMaximized("scene");
                  setMenu(null);
                }}
              >
                <Icon name="Maximize2" />
                {tr("Развернуть сцену")}</button>
              <button
                onClick={() => {
                  setMaximized("graph");
                  setMenu(null);
                }}
              >
                <Icon name="Workflow" />
                {tr("Развернуть граф")}</button>
            </>
          )}
        </div>
      )}
      {contextMenu&&<EditorContextMenu x={contextMenu.x} y={contextMenu.y} title={contextMenu.kind==='scene'?tr("Сцена"):tr("Добавить ноду")} items={contextItems()} onClose={closeContextMenu}/>}
      {picker && (
        <div
          className="create-overlay"
          onPointerDown={(e) => {
            if (e.target === e.currentTarget) setPicker(null);
          }}
        >
          <div className="create-window">
            <div className="create-title">
              <Icon name="Plus" size={16} />
              <strong>
                {picker.kind === "beat"
                  ? tr("Добавить ноду")
                  : picker.kind === "event"
                    ? tr("Добавить событие")
                    : picker.kind === "object"
                      ? tr("Добавить объект")
                      : picker.kind === "object-action"
                        ? tr("Действие с объектом")
                        : tr("Добавить действие")}
              </strong>
              <Button
                icon="X"
                title={tr("Закрыть")}
                onClick={() => setPicker(null)}
              />
            </div>
            {picker.kind === "beat" ? (
              <>
                {beat.kind === "choice" && (
                  <div className="create-phase">
                    <Field label={tr("Продолжить ответ")}>
                      <Select
                        value={picker.choiceId || beat.choices[0]?.id}
                        options={beat.choices.map((c) => [c.id, literalLabel(c.label)])}
                        onChange={(choiceId) =>
                          setPicker((p) => ({ ...p, choiceId }))
                        }
                      />
                    </Field>
                  </div>
                )}
                <div className="create-options">
                  {[
                    ["gameplay", "Gamepad2", "Игровая сцена", "Управление персонажем, оружие и игровые правила"],
                    [
                      "dialogue",
                      "MessageSquare",
                      "Реплика",
                      "Персонаж говорит, события сопровождают текст",
                    ],
                    [
                      "choice",
                      "GitFork",
                      "Выбор игрока",
                      "Ответы возвращают значения для дальнейшей логики",
                    ],
                    [
                      "gate",
                      "MousePointer2",
                      "Ждать взаимодействие",
                      "Продолжить после действия игрока",
                    ],
                    ["branch", "GitFork", "If · Если", "Подключите условие и пути «Да» / «Нет»"],
                    ["variable", "Database", "Переменная", "Создать или прочитать значение прямо в графе"],
                    ["set-variable", "Database", "Задать переменную", "Изменить значение по ходу истории"],
                    ...Object.entries(MATH_OPERATIONS).map(([operator,op])=>["math:"+operator,"Calculator",op.label,"Операция над числами"]),
                    ...['number','boolean','string'].map(valueType=>["convert:"+valueType,"RefreshCw","Конвертация в "+valueType,"Преобразовать тип значения"]),
                    ...Object.entries(compareSymbols).map(([operator,label])=>["compare:"+operator,"Binary",label,"Сравнить два значения"]),
                    ["end", "Flag", "Концовка", "Завершить этот путь истории"],
                  ].map(([kind, icon, title, desc]) => (
                    <button key={kind} onClick={() => kind.startsWith("math:")?addBeat("math",{operator:kind.split(":")[1]}):kind.startsWith("convert:")?addBeat("convert",{valueType:kind.split(":")[1]}):kind.startsWith("compare:")?addBeat("compare",{operator:kind.split(":")[1]}):addBeat(kind)}>
                      <Icon name={icon} size={22} />
                      <span>
                        <strong>{tr(title)}</strong>
                        <small>{desc}</small>
                      </span>
                      <Icon name="ChevronRight" size={15} />
                    </button>
                  ))}
                </div>
              </>
            ) : picker.kind === "object" ? (
              <><p>{tr("Создать в: ")}{project.objectGroups?.find(g=>g.id===(picker?.groupId||selection.groupId||(selection.kind==='group'?selection.id:null)))?.name||tr("Объекты сцены")}</p>
              <div className="create-options">
                <button onClick={addStagingPoint}><Icon name="MapPin" size={23}/><span><strong>{tr("Точка постановки")}</strong><small>{tr("Место назначения для движения персонажа")}</small></span></button>
                <button onClick={addSoundEvent}><Icon name="Volume2" size={23}/><span><strong>{tr("Звуковое событие")}</strong><small>{tr("Аудиофайл, громкость, fade in и fade out")}</small></span></button>
                {[['Персонаж','box','PersonStanding','Персонаж','Участвует в диалогах и постановке'],['Активный меш','box','MousePointer2','Интерактивный предмет','Клик игрока запускает продолжение'],['Меш','box','Box','Куб','Декорация · размеры меняются мышью'],['Меш','sphere','Circle','Сфера','Простой объёмный объект'],['Меш','cylinder','Cylinder','Цилиндр','Колонна, ваза или временный объект'],['Источник света','box','Lightbulb','Источник освещения','Точечный свет с настройкой цвета и интенсивности']].map(([type,shape,icon,name,hint])=><button key={name} onClick={()=>addObject(type,shape)}><Icon name={icon} size={23}/><span><strong>{tr(name)}</strong><small>{tr(hint)}</small></span></button>)}
              </div></>
            ) : picker.kind === "event" ? (
              <>
                <div className="create-phase">
                  <Field label={tr("Когда запустить")}>
                    <Select
                      value={picker.phase || "ON_START"}
                      options={PHASES.map((p) => [p.id, p.label])}
                      onChange={(v) => setPicker((p) => ({ ...p, phase: v }))}
                    />
                  </Field>

                </div>
                <input type="search" aria-label={tr("Поиск событий")} placeholder={tr("Найти событие…")} value={eventSearch} onChange={e=>setEventSearch(e.target.value)}/>
                <div className="event-selection-list">
                  {project.events
                    .filter((e) => (!picker.assetId || e.id === picker.assetId)&&e.name.toLowerCase().includes(eventSearch.toLowerCase()))
                    .map((e) => (
                      <button
                        key={e.id}
                        onClick={() => {
                          const chosenPhase = picker.phase || "ON_START";
                          mutate((p) =>
                            addToBatch(
                              p,
                              beat.id,
                              chosenPhase,
                              picker.batchId || null,
                              e.id,
                            ),
                          );
                          setPicker(null);
                          setDock("story");
                          setPhase(chosenPhase);
                        }}
                      >
                        <Icon
                          name={
                            TYPES[e.groups[0]?.actions[0]?.type]?.icon ||
                            "Layers"
                          }
                          size={17}
                        />
                        <span>
                          <strong>{e.name}</strong>
                          <small>
                            {e.groups.length} {tr("группы ·")}{" "}
                            {e.groups.reduce((n, g) => n + g.actions.length, 0)}{" "}
                            {tr("действия")}</small>
                        </span>
                        <Icon name="Plus" size={15} />
                      </button>
                    ))}
                </div>
                  <Button icon="Plus" onClick={createEvent}>
                    {tr("Добавить событие")}</Button>
              </>
            ) : (
              <div className="create-options action-type-picker">
                {Object.entries(TYPES)
                  .filter(
                    ([type]) =>
                      picker.kind !== "object-action" ||
                      ["move", "pose", "visibility", "sound"].includes(type),
                  )
                  .map(([type, t]) => (
                    <button
                      key={type}
                      onClick={() => {
                        if (picker.kind === "object-action") {
                          const o = project.objects.find(
                              (o) => o.id === picker.objectId,
                            ),
                            e = newEvent(
                              type,
                              o.id,
                              type === "move"
                                ? "окно"
                                : type === "pose"
                                  ? "улыбка"
                                  : "",
                              o.name + " · " + t.label,
                            );
                          mutate((p) => {
                            p.events.push(e);
                            addToBatch(p, beat.id, "ON_START", null, e.id);
                          });
                          openStaging(beat.id, "ON_START");
                        } else if (event) {
                          if (binding) {
                            setNotice(
                              "Сначала откройте исходный шаблон, чтобы изменить состав.",
                            );
                          } else
                            mutate((p) => {
                              const ev = p.events.find(
                                (e) => e.id === event.id,
                              );
                              if (!ev.groups.length)
                                ev.groups.push({
                                  id: uid("group"),
                                  name: "Основное действие",
                                  actions: [],
                                });
                              const group =
                                ev.groups.find(
                                  (g) => g.id === picker.groupId,
                                ) || ev.groups.at(-1);
                              group.actions.push(makeAction(type));
                            });
                        }
                        setPicker(null);
                      }}
                    >
                      <Icon name={t.icon} size={20} />
                      <span>
                        <strong>{tr(t.label)}</strong>
                        <small>{tr(t.domain) || tr("Условие")}</small>
                      </span>
                    </button>
                  ))}
              </div>
            )}
          </div>
        </div>
      )}
      {projectDialog&&<ProjectDialog key={projectDialog} mode={projectDialog} project={project} templates={projectTemplates} busy={projectFileBusy} error={message(projectFileError)} onSubmit={submitProjectDialog} onClose={()=>setProjectDialog(null)}/>}
      <input
        hidden
        type="file"
        accept=".json,application/json"
        ref={fileInput}
        onChange={importProject}
      />
    </div>
  );
}
