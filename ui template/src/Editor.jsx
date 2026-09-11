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
  newEvent,
  makeAction,
  uid,
  AUDIO_ASSETS,
  conditionPass,
  validateStudio,
  fixStudio,
} from "./studioModel.js";
import { fixIssue } from "./model.js";
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
import ActionFields from "./ActionFields.jsx";
import FailureLab from "./FailureLab.jsx";

const PROJECT_KEY = "sacura-studio-v2",
  LAYOUT_KEY = "sacura-workspace-v3";
const defaultSceneHeight = () =>
  Math.max(320, Math.round(window.innerHeight * 0.52));
const initialLayout = {
  left: 224,
  right: 300,
  height: defaultSceneHeight(),
  scope: "chapter",
  positions: {},
};
function loadProject() {
  const raw =
    localStorage.getItem(PROJECT_KEY) ||
    localStorage.getItem("sacura-ui-project-v1");
  if (!raw) return upgradeProject();
  const p = JSON.parse(raw);
  if (
    ![1, 2].includes(p.version) ||
    !p.chapters?.length ||
    !p.events ||
    !p.objects
  )
    throw new Error("Неподдерживаемый формат сохранения");
  if (p.version === 1 && !localStorage.getItem("sacura-ui-project-v1-backup"))
    localStorage.setItem("sacura-ui-project-v1-backup", raw);
  return upgradeProject(p);
}
function Fold({ title, icon, children, open = true, extra }) {
  return (
    <details className="inspector-section" open={open}>
      <summary>
        <Icon name="ChevronRight" size={12} />
        <Icon name={icon} size={15} />
        <span>{title}</span>
        {extra}
      </summary>
      <div className="inspector-fields">{children}</div>
    </details>
  );
}
function ResizeBar({ axis, onMove, onReset, label }) {
  const drag = useRef();
  return (
    <div
      role="separator"
      tabIndex={0}
      aria-label={label}
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
  beat,
  preview,
  running,
  variables,
  onAdvance,
  onStart,
  show,
  objectLabel,
}) {
  if (!show || !beat || (running && !preview.textVisible)) return null;
  if (preview?.phase === "FINISHED" && running)
    return (
      <div className="game-ending">
        <Icon name="Flower2" size={29} />
        <span>Конец истории</span>
        <strong>{preview.ending}</strong>
        <Button onClick={onStart}>Сыграть ещё раз</Button>
      </div>
    );
  const ready = !running || (preview.ready && !preview.paused);
  return (
    <div
      className="game-ui"
      onPointerDown={(e) => e.stopPropagation()}
      onPointerUp={(e) => e.stopPropagation()}
    >
      <div className="game-name">
        <Icon name="Flower2" size={18} />
        {beat.speaker}
      </div>
      {beat.kind === "choice" && (
        <div className="game-choices">
          {beat.choices?.map((c) => (
            <button
              key={c.id}
              disabled={!ready || !conditionPass(c, variables)}
              onClick={() => running && onAdvance(c.id)}
            >
              {!conditionPass(c, variables) && (
                <Icon name="LockKeyhole" size={13} />
              )}
              <span>{c.label}</span>
              <Icon name="ChevronRight" size={14} />
            </button>
          ))}
        </div>
      )}
      <div className="game-dialogue">
        <p>{beat.text}</p>
        {beat.kind === "gate" ? (
          <div className="game-wait">
            <Icon name="MousePointer2" size={14} />
            <span>
              {preview?.hint ||
                `Нажмите на объект «${objectLabel || beat.signal}» в сцене`}
            </span>
          </div>
        ) : (
          <button
            className="game-next"
            title={running ? "Следующая реплика" : "Запустить сцену"}
            disabled={!ready || beat.kind === "choice"}
            onClick={() => (running ? onAdvance() : onStart())}
          >
            <Icon name={ready ? "ChevronDown" : "LoaderCircle"} size={20} />
          </button>
        )}
        <div className="game-dialogue-footer">
          <span>
            {running
              ? preview.paused
                ? "Пауза"
                : preview.phase === "AFTER"
                  ? "Завершение постановки"
                  : !preview.ready
                    ? "Выполняются действия"
                    : beat.kind === "gate"
                      ? "Сценарий ждёт взаимодействия"
                      : beat.kind === "choice"
                        ? "Выберите ответ"
                        : "Нажмите, чтобы продолжить"
              : "Предпросмотр выбранной реплики"}
          </span>
          <span>
            {running ? "PLAYTEST" : "UI PREVIEW"}{" "}
            <Icon name="Flower2" size={12} />
          </span>
        </div>
      </div>
    </div>
  );
}

export default function Editor() {
  const seed = useRef();
  if (!seed.current) {
    try {
      seed.current = { project: loadProject() };
    } catch (e) {
      seed.current = { project: upgradeProject(), error: e.message };
    }
  }
  const [project, setProject] = useState(seed.current.project),
    [saveError, setSaveError] = useState(seed.current.error),
    [layout, setLayout] = useState(() => {
      try {
        return {
          ...initialLayout,
          ...JSON.parse(localStorage.getItem(LAYOUT_KEY) || "{}"),
        };
      } catch {
        return initialLayout;
      }
    }),
    [selectedBeat, setSelectedBeat] = useState("a1"),
    [selection, setSelection] = useState({ kind: "beat", id: "a1" }),
    [dock, setDock] = useState("story"),
    [phase, setPhase] = useState("ALL"),
    [eventContext, setEventContext] = useState(null),
    [mode, setMode] = useState("game"),
    [showDialogue, setShowDialogue] = useState(true),
    [showGrid, setShowGrid] = useState(true),
    [maximized, setMaximized] = useState(null),
    [preview, setPreview] = useState(null),
    [follow, setFollow] = useState(true),
    [query, setQuery] = useState(""),
    [treeTab, setTreeTab] = useState("hierarchy"),
    [picker, setPicker] = useState(null),
    [menu, setMenu] = useState(null),
    [notice, setNotice] = useState(""),
    [assetCategory, setAssetCategory] = useState("events"),
    [audioTick, setAudioTick] = useState(0),
    [inspectorPinned, setInspectorPinned] = useState(false),
    [detailEditor, setDetailEditor] = useState(false),
    [history, setHistory] = useState([]);
  const root = useRef(),
    fileInput = useRef(),
    graphApi = useRef(),
    previewRef = useRef(),
    runtime = useRef();
  if (!runtime.current)
    runtime.current = new PreviewRuntime(soundDesk, setPreview);
  const rt = runtime.current;
  previewRef.current = preview;
  const nodes = useMemo(() => allBeats(project), [project]),
    beat = nodes.find((b) => b.id === selectedBeat) || nodes[0],
    scene = sceneFor(project, beat.id),
    chapter = project.chapters.find((c) =>
      c.beats.some((b) => b.id === beat.id),
    ),
    running = rt.running && preview?.phase !== "EDIT";
  const playProject = running ? rt.project : project,
    playBeat = running
      ? allBeats(playProject).find((b) => b.id === preview.beatId) || beat
      : beat,
    playScene = sceneFor(playProject, playBeat.id),
    displayScene = running ? playScene : scene;
  const world = useMemo(
    () =>
      running
        ? {
            ...preview.world,
            paused: preview.paused,
            weatherPaused: preview.effects.weather?.status === "paused",
            interactionTarget:
              preview.phase === "WAITING_OBJECT" ? playBeat.signal : null,
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
    [running, preview, scene, playBeat.signal],
  );
  const objects = useMemo(
    () =>
      playProject.objects.filter(
        (o) => !o.subsceneId || o.subsceneId === displayScene.id,
      ),
    [playProject, displayScene.id],
  );
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
    if (seed.current.error) return;
    try {
      localStorage.setItem(PROJECT_KEY, JSON.stringify(project));
      setSaveError(null);
    } catch {
      setSaveError("Не удалось сохранить. Экспортируйте проект.");
    }
  }, [project]);
  useEffect(() => {
    localStorage.setItem(LAYOUT_KEY, JSON.stringify(layout));
  }, [layout]);
  useEffect(() => soundDesk.subscribe(() => setAudioTick((n) => n + 1)), []);
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
        setHistory((h) => [...h.slice(-19), p]);
        return next;
      }),
    [],
  );
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
    setDock("staging");
    setPhase(ph);
    setDetailEditor(false);
    setSelection({ kind: "beat", id });
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
          beatId: beat.id,
        });
        setDock("event");
        setSelection({ kind: "event", id: data.event.id });
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
      if (typeof value === "string") {
        selectBeat(value);
        return;
      }
      if (value.batchId && !value.binding)
        setSelection({ kind: "batch", id: value.batchId });
      else if (value.action)
        setSelection({ kind: "action", id: value.action.id });
      else if (value.binding) {
        setSelection({ kind: "binding", id: value.binding.id });
        setEventContext({
          eventId: value.event.id,
          bindingId: value.binding.id,
          beatId: beat.id,
        });
      } else if (value.groupId)
        setSelection({ kind: "group", id: value.groupId });
    },
    [selectBeat, beat.id],
  );
  const changeBatch = useCallback(
    (batchId, command, value) => {
      const phaseId = PHASES.find((ph) =>
        beat.batches?.[ph.id]?.some((b) => b.id === batchId),
      )?.id;
      if (!phaseId) return;
      if (command === "select") {
        setSelection({ kind: "batch", id: batchId });
        return;
      }
      if (command === "add") {
        setPicker({ kind: "event", batchId, phase: phaseId });
        return;
      }
      mutate((p) => {
        const b = allBeats(p).find((b) => b.id === beat.id),
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
          b.bindings = b.bindings.filter((x) => x.id !== value);
          batch.bindingIds = batch.bindingIds.filter((id) => id !== value);
        }
      });
    },
    [beat, mutate],
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
      const target = c.target?.startsWith("portal:")
        ? c.target.slice(7)
        : c.target;
      if (c.source === target) {
        setNotice("Узел не может продолжаться сам в себя.");
        return;
      }
      mutate((p) => {
        const b = allBeats(p).find((b) => b.id === c.source);
        if (!b) return;
        if (c.sourceHandle?.startsWith("choice:")) {
          const choice = b.choices.find(
            (x) => x.id === c.sourceHandle.slice(7),
          );
          if (choice) choice.next = target || null;
        } else b.next = target || null;
      });
      setNotice(
        "Связь изменена. " +
          (running ? "Будет использована в следующем запуске." : ""),
      );
    },
    [mutate, running],
  );
  const patchBeat = (values) =>
    mutate((p) =>
      Object.assign(
        allBeats(p).find((b) => b.id === beat.id),
        values,
      ),
    );
  const start = () => {
    setFollow(true);
    setMode("game");
    setShowDialogue(true);
    rt.start(project, beat.id);
  };
  const selectObject = (id) => {
    if (!inspectorPinned) setSelection({ kind: "object", id });
  };
  const interact = (id) => {
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
  const exportProject = () => {
    const a = document.createElement("a"),
      url = URL.createObjectURL(
        new Blob([JSON.stringify(project, null, 2)], {
          type: "application/json",
        }),
      );
    a.href = url;
    a.download = "sacura-project.json";
    a.click();
    URL.revokeObjectURL(url);
    setMenu(null);
  };
  const importProject = async (e) => {
    try {
      const raw = JSON.parse(await e.target.files[0].text());
      if (
        ![1, 2].includes(raw.version) ||
        !raw.events ||
        !raw.objects ||
        !raw.chapters
      )
        throw new Error("Неизвестный формат");
      localStorage.setItem(PROJECT_KEY + "-backup-v3", JSON.stringify(project));
      seed.current.error = null;
      const next = upgradeProject(raw);
      setProject(next);
      setSelectedBeat(allBeats(next)[0].id);
      setNotice("Проект загружен.");
    } catch (err) {
      setNotice(err.message);
    }
    e.target.value = "";
  };
  const undo = () => {
    if (!history.length) return;
    setProject(history.at(-1));
    setHistory((h) => h.slice(0, -1));
  };
  const addBeat = (kind) => {
    const id = uid("line");
    mutate((p) => {
      const c = p.chapters.find((c) => c.id === chapter.id),
        index = c.beats.findIndex((b) => b.id === beat.id),
        b = {
          id,
          kind,
          speaker: kind === "dialogue" ? "Алиса" : "Рассказчик",
          text:
            kind === "choice"
              ? "Что вы решите?"
              : kind === "gate"
                ? "Осмотрите письмо."
                : kind === "end"
                  ? "Конец истории."
                  : "Новая реплика",
          next:
            beat.kind === "choice"
              ? beat.choices.find(
                  (c) => c.id === (picker?.choiceId || beat.choices[0]?.id),
                )?.next
              : beat.next,
          bindings: [],
          batches: { BEFORE: [], ON_START: [], AFTER: [] },
        };
      if (kind === "choice")
        b.choices = [
          {
            id: uid("choice"),
            label: "Остаться",
            condition: "always",
            next: b.next || null,
          },
          {
            id: uid("choice"),
            label: "Выйти в сад",
            condition: "always",
            next: "garden-entry",
          },
        ];
      if (kind === "gate") {
        b.signal = "letter";
        b.timeout = 30;
      }
      if (kind === "end") {
        b.next = null;
        b.ending = "Новая концовка";
      }
      if (beat.kind === "choice") {
        const answer = c.beats[index].choices.find(
          (c) => c.id === (picker?.choiceId || beat.choices[0]?.id),
        );
        if (answer) answer.next = id;
      } else if (beat.kind !== "end") c.beats[index].next = id;
      c.beats.splice(index + 1, 0, b);
    });
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
    setDock("staging");
    setNotice("Событие добавлено в постановку реплики.");
  };
  const createEvent = () => {
    const e = newEvent("move", "alice", "окно", "Новое событие");
    mutate((p) => p.events.push(e));
    setEventContext({ eventId: e.id });
    setDock("event");
    setSelection({ kind: "event", id: e.id });
    setPicker(null);
  };
  const addObject = (type) => {
    const id = uid("object");
    mutate((p) =>
      p.objects.push({
        id,
        type,
        name: type === "Персонаж" ? "Новый персонаж" : "Новый объект",
        color: "#bb99aa",
        position: "стол",
        active: true,
        subsceneId: scene.id,
        interaction: "Осмотреть",
      }),
    );
    setSelection({ kind: "object", id });
    setPicker(null);
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
    setDock("staging");
    setNotice("Звук назначен реплике " + beat.id);
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
    setLayout((l) => ({
      ...l,
      left: 224,
      right: 300,
      height: defaultSceneHeight(),
    }));
    setMaximized(null);
    setMenu(null);
  };
  useEffect(() => {
    const listener = (e) => {
      if (e.key === "Escape") {
        setPicker(null);
        setMenu(null);
        setDetailEditor(false);
        setMaximized(null);
      }
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
        e.preventDefault();
        setNotice("Проект сохранён локально.");
      }
      if (
        (e.ctrlKey || e.metaKey) &&
        e.key.toLowerCase() === "z" &&
        !["INPUT", "TEXTAREA"].includes(e.target.tagName)
      ) {
        e.preventDefault();
        if (history.length) {
          setProject(history.at(-1));
          setHistory((h) => h.slice(0, -1));
        }
      }
    };
    window.addEventListener("keydown", listener);
    return () => window.removeEventListener("keydown", listener);
  }, [history]);

  const renderInspector = () => {
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
                <strong>Группа событий {index + 1}</strong>
                <small>
                  {ph.label} · {beat.id}
                </small>
              </div>
            </div>
            <Fold title="Порядок запуска" icon="GitFork">
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
                  ? "Все события стартуют вместе. Выход — после результатов, указанных на связях."
                  : "Следующее событие начинается после результата предыдущего."}
              </p>
              <div className="button-row">
                <Button
                  icon="ArrowUp"
                  disabled={index === 0}
                  onClick={() => changeBatch(batch.id, "move", -1)}
                >
                  Раньше
                </Button>
                <Button
                  icon="ArrowDown"
                  disabled={index === list.length - 1}
                  onClick={() => changeBatch(batch.id, "move", 1)}
                >
                  Позже
                </Button>
              </div>
            </Fold>
            <Fold title="События в группе" icon="Layers">
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
                    title="Убрать событие из реплики"
                    onClick={() => changeBatch(batch.id, "removeBinding", b.id)}
                  />
                </div>
              ))}
              <Button icon="Plus" onClick={() => changeBatch(batch.id, "add")}>
                Добавить в эту группу
              </Button>
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
                Запустить вместе с предыдущей
              </Button>
            )}
          </>
        );
    }
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
                name={o.type === "Персонаж" ? "PersonStanding" : "Box"}
                size={28}
              />
            </span>
            <div>
              <input
                value={o.name}
                onChange={(e) => patch({ name: e.target.value })}
              />
              <small>{o.type}</small>
            </div>
            <input
              type="checkbox"
              checked={o.active}
              onChange={(e) => patch({ active: e.target.checked })}
              title="Объект активен"
            />
          </div>
          <Fold title="Положение" icon="Move">
            <Field label="Опорная точка">
              <Select
                value={o.position || "стол"}
                options={["стол", "камин", "окно", "диван"]}
                onChange={(position) => patch({ position })}
              />
            </Field>
            <Field label="Локация">
              <Select
                value={o.subsceneId || ""}
                options={[
                  ["", "Все сабсцены"],
                  ...project.subscenes.map((s) => [s.id, s.location]),
                ]}
                onChange={(subsceneId) => patch({ subsceneId })}
              />
            </Field>
            <Field label="Цвет в макете">
              <input
                type="color"
                value={o.color || "#b99cac"}
                onChange={(e) => patch({ color: e.target.value })}
              />
            </Field>
          </Fold>
          {o.type === "Активный меш" && (
            <Fold title="Взаимодействие" icon="MousePointer2">
              <Field label="Действие игрока">
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
                Ждать в этой реплике
              </Button>
            </Fold>
          )}
          <Button
            className="inspector-add"
            icon="Plus"
            onClick={() => {
              const e = newEvent(
                "move",
                o.id,
                "окно",
                o.name + " подходит к окну",
              );
              mutate((p) => {
                p.events.push(e);
                addToBatch(p, beat.id, "ON_START", null, e.id);
              });
              openStaging(beat.id, "ON_START");
            }}
          >
            Создать действие с объектом
          </Button>
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
                  ? TYPES[inspectedAction.type].label
                  : event.name}
              </strong>
              <small>
                {binding ? "Экземпляр · " + contextBeat.id : "Шаблон события"}
              </small>
            </div>
          </div>
          {binding && (
            <div className="instance-source">
              <Icon name="Link2" size={13} />
              <span>{event.name}</span>
              <Button
                title="Открыть исходный шаблон"
                icon="ExternalLink"
                onClick={() => {
                  setEventContext({ eventId: event.id });
                  setSelection({ kind: "event", id: event.id });
                }}
              />
            </div>
          )}
          {inspectedAction ? (
            <>
              <Fold title="Параметры действия" icon="SlidersHorizontal">
                <Field label="Тип">
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
                <ActionFields
                  action={inspectedAction}
                  project={project}
                  onChange={(v) => patchAction(inspectedAction.id, v)}
                />
              </Fold>
              <Fold title="Конфликты и завершение" icon="Shield">
                <Field label="Если объект занят">
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
                  / {TYPES[inspectedAction.type].domain || "Условие"}
                </div>
              </Fold>
            </>
          ) : (
            <Fold
              title={binding ? "Параметры размещения" : "Свойства события"}
              icon="Layers"
            >
              {binding ? (
                <>
                  <Field label="Момент запуска">
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
                  <Field label="Когда идти дальше">
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
                    Сбросить изменения здесь
                  </Button>
                </>
              ) : (
                <>
                  <Field label="Название">
                    <input
                      value={event.name}
                      onChange={(e) => patchEvent({ name: e.target.value })}
                    />
                  </Field>
                  <Field label="После действий">
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
                  <Field label="Живёт в пределах">
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
                    Добавить в реплику
                  </Button>
                </>
              )}
            </Fold>
          )}
          <Fold title="Группы действий" icon="ListOrdered">
            {event.groups.map((g, i) => (
              <button
                className="inspector-list-item"
                key={g.id}
                onClick={() => graphApi.current?.focus(g.id)}
              >
                <span>{i + 1}</span>
                <strong>{g.name}</strong>
                <small>{g.actions.length}</small>
              </button>
            ))}
            {!binding && (
              <Button
                icon="Plus"
                onClick={() =>
                  mutate((p) =>
                    p.events
                      .find((e) => e.id === event.id)
                      .groups.push({
                        id: uid("group"),
                        name: "Следующий шаг",
                        actions: [makeAction()],
                      }),
                  )
                }
              >
                Следующая группа
              </Button>
            )}
          </Fold>
          <Button
            className="inspector-add"
            icon="Maximize2"
            onClick={() => {
              setDetailEditor(true);
              setMaximized("graph");
            }}
          >
            Все параметры рядом
          </Button>
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
                ? "Выбор игрока"
                : beat.kind === "gate"
                  ? "Ожидание игрока"
                  : "Реплика " + beat.id}
            </strong>
            <small>{chapter.name}</small>
          </div>
          <span className="subtle">{beat.bindings.length} событий</span>
        </div>
        <Fold title="Диалог" icon="MessageSquare">
          <Field label="Говорит">
            <Select
              value={beat.speaker}
              options={[
                "Рассказчик",
                ...project.objects
                  .filter((o) => o.type === "Персонаж")
                  .map((o) => o.name),
              ]}
              onChange={(speaker) => patchBeat({ speaker })}
            />
          </Field>
          <textarea
            className="dialogue-text-field"
            aria-label="Текст выбранной реплики"
            value={beat.text}
            rows={4}
            onChange={(e) => patchBeat({ text: e.target.value })}
          />
          <Field label="Тип блока">
            <Select
              value={beat.kind}
              options={[
                ["dialogue", "Реплика"],
                ["choice", "Выбор"],
                ["gate", "Взаимодействие"],
                ["merge", "Схождение"],
                ["end", "Концовка"],
              ]}
              onChange={(kind) =>
                patchBeat({
                  kind,
                  ...(kind === "end"
                    ? { next: null, ending: beat.ending || "Конец истории" }
                    : {}),
                  ...(kind === "choice" && !beat.choices
                    ? {
                        choices: [
                          {
                            id: uid("choice"),
                            label: "Продолжить",
                            condition: "always",
                            next: beat.next || null,
                          },
                        ],
                      }
                    : {}),
                  ...(kind === "gate"
                    ? {
                        signal: beat.signal || "letter",
                        timeout: beat.timeout || 30,
                      }
                    : {}),
                })
              }
            />
          </Field>
        </Fold>
        <Fold title="Постановка реплики" icon="Clapperboard">
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
              <span>{p.label}</span>
              <b>{beat.bindings.filter((b) => b.hook === p.id).length}</b>
              <Icon name="ChevronRight" size={13} />
            </button>
          ))}
          <Button icon="Workflow" onClick={() => openStaging(beat.id)}>
            Открыть всю постановку
          </Button>
        </Fold>
        {beat.kind === "choice" ? (
          <Fold title="Ответы и условия" icon="GitFork">
            {beat.choices.map((c, i) => (
              <div className="answer-fields" key={c.id}>
                <label className="answer-number">
                  {i + 1}
                  <input
                    value={c.label}
                    onChange={(e) =>
                      patchBeat({
                        choices: beat.choices.map((x) =>
                          x.id === c.id ? { ...x, label: e.target.value } : x,
                        ),
                      })
                    }
                  />
                </label>
                <Select
                  value={c.condition}
                  options={[
                    ["always", "Всегда"],
                    ["trust", "Если достаточно доверия"],
                    ["letter", "Если найдено письмо"],
                  ]}
                  onChange={(condition) =>
                    patchBeat({
                      choices: beat.choices.map((x) =>
                        x.id === c.id ? { ...x, condition } : x,
                      ),
                    })
                  }
                />
                {c.condition === "trust" && (
                  <input
                    aria-label="Порог доверия"
                    type="number"
                    value={c.threshold ?? 3}
                    onChange={(e) =>
                      patchBeat({
                        choices: beat.choices.map((x) =>
                          x.id === c.id
                            ? { ...x, threshold: Number(e.target.value) }
                            : x,
                        ),
                      })
                    }
                  />
                )}
                <Select
                  value={c.next || ""}
                  options={[
                    ["", "Выберите продолжение"],
                    ...nodes.map((n) => [
                      n.id,
                      sceneFor(project, n.id).location +
                        " / " +
                        n.id +
                        " · " +
                        n.text.slice(0, 24),
                    ]),
                  ]}
                  onChange={(next) =>
                    patchBeat({
                      choices: beat.choices.map((x) =>
                        x.id === c.id ? { ...x, next } : x,
                      ),
                    })
                  }
                />
              </div>
            ))}
            <Button
              icon="Plus"
              onClick={() =>
                patchBeat({
                  choices: [
                    ...beat.choices,
                    {
                      id: uid("choice"),
                      label: "Новый ответ",
                      condition: "always",
                      next: "garden-entry",
                    },
                  ],
                })
              }
            >
              Добавить ответ
            </Button>
          </Fold>
        ) : beat.kind === "gate" ? (
          <Fold title="Ждать взаимодействие" icon="MousePointerClick">
            <Field label="Объект">
              <Select
                value={beat.signal}
                options={project.objects
                  .filter((o) => o.type === "Активный меш")
                  .map((o) => [o.id, o.name])}
                onChange={(signal) => patchBeat({ signal })}
              />
            </Field>
            <Field label="Подсказка через, сек">
              <input
                type="number"
                value={beat.timeout || 30}
                onChange={(e) => patchBeat({ timeout: Number(e.target.value) })}
              />
            </Field>
          </Fold>
        ) : null}
        {!["choice", "end"].includes(beat.kind) && (
          <Fold title="Продолжение" icon="Route">
            <Select
              value={beat.next || ""}
              options={[
                ["", "Конец / выход по выбору"],
                ...nodes.map((n) => [
                  n.id,
                  sceneFor(project, n.id).location +
                    " / " +
                    n.id +
                    " · " +
                    n.text.slice(0, 30),
                ]),
              ]}
              onChange={(next) => patchBeat({ next: next || null })}
            />
            <div className="resource-note">
              {beat.next
                ? sceneFor(project, beat.next).location === scene.location
                  ? "В этой же локации"
                  : "Переход в другую сабсцену"
                : "Без последовательного продолжения"}
            </div>
          </Fold>
        )}
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
            ? AUDIO_ASSETS
            : project.events;
    return (
      <div className="project-browser">
        <div className="asset-folders">
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
              {label}
            </button>
          ))}
        </div>
        <div className="asset-content">
          <div className="asset-path">
            Проект <Icon name="ChevronRight" size={12} />{" "}
            {assetCategory === "events"
              ? "События"
              : assetCategory === "audio"
                ? "Звук"
                : assetCategory === "scenes"
                  ? "Сабсцены"
                  : "Объекты"}
            <span />
            {assetCategory === "events" && (
              <Button icon="Plus" onClick={createEvent}>
                Событие
              </Button>
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
                      setEventContext({ eventId: a.id });
                      setSelection({ kind: "event", id: a.id });
                      setDock("event");
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
                        {a.groups.length === 1 ? "группа" : "группы"}
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
      <div className="active-table">
        <div className="table-header">
          <span>Событие / эффект</span>
          <span>Состояние</span>
          <span>Граница жизни</span>
          <span>Управление</span>
        </div>
        {Object.entries(effects).map(([key, e]) => (
          <div className="active-row" key={key}>
            <span>
              <Icon
                name={
                  key === "music"
                    ? "Music2"
                    : key === "weather"
                      ? "CloudRain"
                      : "Moon"
                }
                size={17}
              />
              <strong>{e.name}</strong>
              <small>{e.origin}</small>
            </span>
            <span className={"effect-status " + e.status}>
              {e.status === "paused"
                ? "На паузе"
                : e.status === "stopped"
                  ? "Завершено"
                  : "Удерживается"}
            </span>
            <span>
              {{
                Scene: "Вся сцена",
                SubScene: "Сабсцена",
                GameSession: "Вся игра",
              }[e.owner] || e.owner}
            </span>
            <div>
              {key !== "music" && (
                <Select
                  value={e.name}
                  options={
                    key === "weather"
                      ? ["Ясно", "Дождь", "Гроза", "Туман"]
                      : ["Рассвет", "День", "Закат", "Ночь"]
                  }
                  onChange={(value) =>
                    running
                      ? rt.controlEffect(key, "set", value)
                      : mutate(
                          (p) =>
                            (p.subscenes.find((s) => s.id === scene.id)[key] =
                              value),
                        )
                  }
                />
              )}
              <Button
                disabled={!running}
                title={e.status === "paused" ? "Продолжить" : "Пауза"}
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
                title="Остановить этот эффект"
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
                    ? "На паузе"
                    : i.status === "running"
                      ? "Выполняется"
                      : "Готово · удерживается"}
                </span>
                <span>
                  {{
                    Scene: "Вся сцена",
                    SubScene: "Сабсцена",
                    GameSession: "Вся игра",
                  }[i.owner] || i.owner}
                </span>
                <div>
                  <Button
                    title={
                      i.status === "paused"
                        ? "Продолжить событие"
                        : "Приостановить событие"
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
                    title="Завершить событие целиком"
                    onClick={() => rt.controlInstance(i.id, "stop")}
                  />
                </div>
              </div>
            ))}
        <div className="runtime-caption">
          {running
            ? "Событие может закончить свои шаги и продолжать жить в фоне."
            : "Начальное состояние. Запустите сцену, чтобы увидеть реальные экземпляры событий."}
        </div>
      </div>
    );
  };

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
        {["Файл", "Правка", "Создать", "Окно"].map((m) => (
          <button
            className={menu === m ? "active" : ""}
            key={m}
            onClick={() => setMenu(menu === m ? null : m)}
          >
            {m}
          </button>
        ))}
        <div className="flex-space" />
        <span className="project-title">
          <Icon name="FolderOpen" size={14} />
          {project.title}
        </span>
        <button
          title="Сохранено на этом компьютере"
          onClick={() =>
            setNotice("Сохранено локально. Экспорт доступен в меню «Файл».")
          }
        >
          <Icon name={saveError ? "TriangleAlert" : "CloudCheck"} size={15} />
        </button>
      </header>
      <div className="editor-toolbar">
        <div className="toolbar-project">
          <Icon name="Box" size={15} />
          <Select
            value={scene.id}
            onChange={(id) =>
              selectBeat(project.subscenes.find((s) => s.id === id).entry)
            }
            options={project.subscenes.map((s) => [s.id, s.name])}
          />
        </div>
        <div className="main-play-controls">
          <Button
            className={running ? "playing" : ""}
            icon={running ? "Square" : "Play"}
            title={
              running
                ? "Остановить предпросмотр"
                : "Запустить с выбранной реплики"
            }
            onClick={() => (running ? rt.stop() : start())}
          />
          <Button
            icon="Pause"
            title="Пауза / продолжить"
            className={preview?.paused ? "active" : ""}
            disabled={!running || preview.phase === "FINISHED"}
            onClick={() => rt.togglePause()}
          />
          <Button
            icon="StepForward"
            title="Следующая реплика"
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
          <span className={running ? "play-label" : "edit-label"}>
            {running
              ? preview.paused
                ? "На паузе"
                : "Предпросмотр"
              : "Редактирование"}
          </span>
          <Button
            icon="PanelsTopLeft"
            title="Восстановить раскладку"
            onClick={resetLayout}
          />
          <Select
            aria-label="Раскладка редактора"
            value={maximized || "balanced"}
            onChange={(v) => setMaximized(v === "balanced" ? null : v)}
            options={[
              ["balanced", "Постановка"],
              ["graph", "Сценарий"],
              ["scene", "Только сцена"],
            ]}
          />
        </div>
      </div>
      <div className="editor-workspace">
        <aside className="hierarchy-panel">
          <div className="panel-tabs">
            <button
              className={treeTab === "hierarchy" ? "active" : ""}
              onClick={() => setTreeTab("hierarchy")}
            >
              <Icon name="ListTree" size={14} />
              Иерархия
            </button>
            <button
              className={treeTab === "story" ? "active" : ""}
              onClick={() => setTreeTab("story")}
            >
              История
            </button>
            <div className="flex-space" />
            <Button
              icon="Plus"
              title="Создать объект"
              onClick={() => setPicker({ kind: "object" })}
            />
          </div>
          <div className="panel-search">
            <Icon name="Search" size={13} />
            <input
              placeholder={
                treeTab === "story" ? "Найти реплику…" : "Найти объект…"
              }
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
          </div>
          <div className="hierarchy-tree">
            {treeTab === "hierarchy" ? (
              <>
                <div className="tree-scene">
                  <Icon name="ChevronDown" size={13} />
                  <Icon name="Box" size={15} />
                  <strong>{displayScene.name}</strong>
                </div>
                <button
                  className="tree-row system"
                  onClick={() => setSelection({ kind: "scene", id: scene.id })}
                >
                  <Icon name="Video" />
                  Главная камера
                  <Icon name="Eye" />
                </button>
                <button
                  className="tree-row system"
                  onClick={() => setShowDialogue((v) => !v)}
                >
                  <Icon name="MessagesSquare" />
                  Система диалогов
                  <Icon name={showDialogue ? "Eye" : "EyeOff"} />
                </button>
                <button
                  className="tree-row system"
                  onClick={() => setDock("active")}
                >
                  <Icon name="AudioLines" />
                  Звук и окружение
                  <Icon name="Eye" />
                </button>
                <div className="tree-folder">
                  <Icon name="ChevronDown" size={13} />
                  <Icon name="Folder" size={14} />
                  Объекты сцены
                </div>
                {objects
                  .filter((o) =>
                    o.name.toLowerCase().includes(query.toLowerCase()),
                  )
                  .map((o) => (
                    <div
                      className={
                        "tree-object " +
                        (selection.id === o.id ? "selected" : "")
                      }
                      key={o.id}
                    >
                      <button
                        draggable
                        onDragStart={(e) =>
                          e.dataTransfer.setData(
                            "application/sacura-asset",
                            JSON.stringify({ id: o.id, kind: "objects" }),
                          )
                        }
                        onClick={() => selectObject(o.id)}
                      >
                        <Icon
                          name={
                            o.type === "Персонаж"
                              ? "PersonStanding"
                              : o.type === "Активный меш"
                                ? "MousePointer2"
                                : "Box"
                          }
                          size={15}
                          style={{ color: o.color }}
                        />
                        <span>{o.name}</span>
                      </button>
                      <button
                        title={o.active ? "Скрыть объект" : "Показать объект"}
                        onClick={() =>
                          mutate(
                            (p) =>
                              (p.objects.find((x) => x.id === o.id).active =
                                !o.active),
                          )
                        }
                      >
                        <Icon name={o.active ? "Eye" : "EyeOff"} size={13} />
                      </button>
                    </div>
                  ))}
                <div className="tree-folder">
                  <Icon name="ChevronDown" size={13} />
                  <Icon name="MapPin" size={14} />
                  Точки постановки
                </div>
                {["У камина", "У окна", "У стола", "У дивана"].map((t) => (
                  <div className="tree-anchor" key={t}>
                    <Icon name="LocateFixed" size={13} />
                    {t}
                  </div>
                ))}
              </>
            ) : (
              project.subscenes.map((s) => (
                <details key={s.id} open={!!query || s.id === scene.id}>
                  <summary>
                    <Icon name="ChevronRight" size={12} />
                    <Icon name="PanelsTopLeft" size={14} />
                    {s.name}
                  </summary>
                  {project.chapters
                    .filter((c) => c.subsceneId === s.id)
                    .map((c) => (
                      <details key={c.id} open={!!query || c.id === chapter.id}>
                        <summary>
                          <Icon name="ChevronRight" size={11} />
                          {c.name}
                          <small>{c.beats.length}</small>
                        </summary>
                        {c.beats
                          .filter((b) =>
                            (b.text + b.id)
                              .toLowerCase()
                              .includes(query.toLowerCase()),
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
              ))
            )}
          </div>
          <div className="hierarchy-footer">
            <Icon name="Layers" size={13} />
            {objects.length} объектов
            <span />
            {nodes.length} блоков
          </div>
          <div className="subscene-list">
            <div className="compact-heading">
              Сабсцены
              <Button
                icon="Plus"
                title="Создать сабсцену"
                onClick={() => {
                  const id = uid("scene"),
                    entry = uid("line");
                  mutate((p) => {
                    p.subscenes.push({
                      id,
                      entry,
                      name: "Новая сабсцена",
                      location: "Новая локация",
                      kind: "living",
                      sceneId: "chapter1",
                      weather: "Ясно",
                      time: "День",
                    });
                    p.chapters.push({
                      id: uid("chapter"),
                      subsceneId: id,
                      name: "Первый эпизод",
                      beats: [
                        {
                          id: entry,
                          kind: "dialogue",
                          speaker: "Рассказчик",
                          text: "Новая история.",
                          next: null,
                          bindings: [],
                        },
                      ],
                    });
                  });
                  selectBeat(entry);
                }}
              />
            </div>
            {project.subscenes.map((s) => (
              <button
                className={scene.id === s.id ? "active" : ""}
                key={s.id}
                onClick={() => selectBeat(s.entry)}
              >
                <Icon
                  name={
                    s.kind === "living"
                      ? "Armchair"
                      : s.kind === "garden"
                        ? "Trees"
                        : "TramFront"
                  }
                  size={16}
                />
                <span>{s.location}</span>
                <small>
                  {project.chapters
                    .filter((c) => c.subsceneId === s.id)
                    .reduce((n, c) => n + c.beats.length, 0)}
                </small>
              </button>
            ))}
          </div>
        </aside>
        <ResizeBar
          axis="x"
          label="Ширина иерархии"
          onMove={(d) => resize("left", d)}
          onReset={() => setLayout((l) => ({ ...l, left: 224 }))}
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
                className={mode === "scene" ? "active" : ""}
                onClick={() => setMode("scene")}
              >
                <Icon name="Box" size={14} />
                Сцена
              </button>
              <button
                className={mode === "game" ? "active" : ""}
                onClick={() => setMode("game")}
              >
                <Icon name="Gamepad2" size={15} />
                Игра
              </button>
              <span className="viewport-location">{displayScene.location}</span>
              <div className="flex-space" />
              <button
                title="Диалог в игровом кадре"
                className={showDialogue ? "toggled" : ""}
                onClick={() => setShowDialogue((v) => !v)}
              >
                <Icon name="PanelBottom" size={14} />
              </button>
              <button
                title="Развернуть сцену"
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
            <div
              className="scene-viewport"
              tabIndex={0}
              onKeyDown={(e) => {
                if (
                  mode === "game" &&
                  [" ", "Enter"].includes(e.key) &&
                  e.target === e.currentTarget
                ) {
                  e.preventDefault();
                  running ? rt.advance() : start();
                }
              }}
            >
              <LocationScene
                kind={displayScene.kind}
                objects={objects}
                state={world}
                selected={selection.kind === "object" ? selection.id : null}
                mode={mode}
                showGrid={showGrid}
                onSelect={selectObject}
                onInteract={interact}
              />
              <div className="viewport-tools">
                <Button
                  icon="Hand"
                  className={mode === "scene" ? "active" : ""}
                  title="Вращать сцену мышью"
                  onClick={() => setMode("scene")}
                />
                <Button
                  icon="Grid3X3"
                  className={showGrid ? "active" : ""}
                  title="Сетка сцены"
                  onClick={() => setShowGrid((v) => !v)}
                />
                <Button
                  icon="Crosshair"
                  title="Выделить Алису"
                  onClick={() => selectObject("alice")}
                />
              </div>
              <div className="viewport-state">
                <span>
                  <Icon
                    name={
                      world.weather === "Гроза" ? "CloudLightning" : "CloudRain"
                    }
                    size={13}
                  />
                  {world.weather}
                </span>
                <span>
                  <Icon name="Moon" size={13} />
                  {world.time}
                </span>
              </div>
              <GameDialogue
                beat={playBeat}
                preview={preview}
                running={running}
                variables={variables}
                onAdvance={(c) => rt.advance(c)}
                onStart={start}
                objectLabel={
                  playProject.objects.find((o) => o.id === playBeat.signal)
                    ?.name
                }
                show={showDialogue}
              />
              {running && !preview.textVisible && (
                <div className="scene-preparing">
                  <Icon name="LoaderCircle" size={15} />
                  {preview.error || "Подготовка сцены перед репликой"}
                </div>
              )}
              <div className="viewport-caption">
                <Icon name="Video" size={12} />
                {mode === "game"
                  ? "Главная камера"
                  : "Перспектива · вращение мышью"}
                <span />
                {running
                  ? `${playBeat.id} / ${preview.phase}`
                  : "3D · примитивы"}
              </div>
            </div>
            <div className="live-strip">
              <button onClick={() => setDock("active")}>
                <Icon name="Activity" size={13} />
                {running
                  ? Object.values(preview.effects).filter(
                      (e) => e.status !== "stopped",
                    ).length
                  : "2"}{" "}
                активных состояния
              </button>
              <span className="live-separator" />
              <span>
                <Icon name="Music2" size={13} />
                {soundDesk.get("background")?.status === "playing"
                  ? "Главная тема · " +
                    clockLabel(soundDesk.get("background").audio.currentTime)
                  : "Музыка не играет"}
              </span>
              <div className="flex-space" />
              {soundDesk.ducks.size > 0 && (
                <span className="duck-label">Голос → музыка −12 dB</span>
              )}
              <button onClick={() => setDock("active")}>
                Управление <Icon name="ChevronRight" size={12} />
              </button>
            </div>
          </section>
          <ResizeBar
            axis="y"
            label="Размер сцены и графа"
            onMove={(d) => resize("height", d)}
            onReset={() =>
              setLayout((l) => ({ ...l, height: defaultSceneHeight() }))
            }
          />
          <section className="graph-dock">
            <div className="panel-tabs dock-tabs">
              {[
                ["story", "Workflow", "Сценарий"],
                ["staging", "Clapperboard", "Постановка"],
                ["event", "Layers", "Событие"],
                ["assets", "FolderOpen", "Проект"],
                ["sound", "Music2", "Звук"],
                ["active", "Activity", "Активные"],
                ["issues", "TriangleAlert", "Ошибки"],
              ].map(([id, icon, label]) => (
                <button
                  key={id}
                  className={dock === id ? "active" : ""}
                  disabled={id === "event" && !event}
                  onClick={() => {
                    setDock(id);
                    setDetailEditor(false);
                  }}
                >
                  <Icon name={icon} size={13} />
                  {label}
                  {id === "issues" && issues.length > 0 && (
                    <b>{issues.length}</b>
                  )}
                </button>
              ))}
              <div className="flex-space" />
              <Button
                icon={maximized === "graph" ? "Minimize2" : "Maximize2"}
                title="Развернуть граф"
                onClick={() =>
                  setMaximized(maximized === "graph" ? null : "graph")
                }
              />
            </div>
            {["story", "staging", "event"].includes(dock) && !detailEditor && (
              <div className="graph-toolbar">
                <button
                  className="graph-back"
                  title="Вернуться в сценарий"
                  disabled={dock === "story"}
                  onClick={() =>
                    setDock(dock === "event" ? "staging" : "story")
                  }
                >
                  <Icon name="ArrowLeft" size={13} />
                </button>
                <span className="graph-breadcrumb">
                  {dock === "story"
                    ? chapter.name
                    : dock === "staging"
                      ? `${beat.id} · ${beat.speaker}`
                      : event?.name}
                </span>
                {dock === "story" ? (
                  <Select
                    aria-label="Область графа"
                    value={layout.scope}
                    options={[
                      ["chapter", "Эпизод"],
                      ["nearby", "Ближайшие связи"],
                      ["scene", "Сабсцена"],
                      ["all", "Вся история"],
                    ]}
                    onChange={(scope) => setLayout((l) => ({ ...l, scope }))}
                  />
                ) : dock === "staging" ? (
                  <Select
                    aria-label="Фаза постановки"
                    value={phase}
                    options={[
                      ["ALL", "Вся постановка"],
                      ...PHASES.map((p) => [p.id, p.label]),
                    ]}
                    onChange={setPhase}
                  />
                ) : (
                  <span className="context-label">
                    {binding ? "Размещение · " + contextBeat.id : "Шаблон"}
                  </span>
                )}
                <div className="flex-space" />
                {dock === "story" && beat.kind === "choice" && (
                  <Button
                    icon="GitFork"
                    onClick={() => {
                      setLayout((l) => ({ ...l, scope: "nearby" }));
                      setMaximized("graph");
                    }}
                  >
                    Обзор развилки
                  </Button>
                )}
                {running && (
                  <button
                    title="Следовать за проигрыванием"
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
                  title="Расположить узлы автоматически"
                  onClick={() => graphApi.current?.arrange()}
                />
                <Button
                  className="rose-button"
                  icon="Plus"
                  onClick={() =>
                    setPicker({
                      kind:
                        dock === "story"
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
                  {dock === "story"
                    ? "Блок"
                    : dock === "event"
                      ? "Действие"
                      : "Событие"}
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
              {detailEditor && event ? (
                <EventEditor
                  project={project}
                  event={event}
                  binding={binding}
                  beat={contextBeat}
                  update={mutate}
                  onBack={() => setDetailEditor(false)}
                  onTemplate={() => setEventContext({ eventId: event.id })}
                  onAdd={addEvent}
                  onPreview={start}
                />
              ) : ["story", "staging", "event"].includes(dock) ? (
                <EditorGraph
                  project={project}
                  mode={dock}
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
                  onReady={onGraphReady}
                  onContext={() =>
                    setPicker({
                      kind:
                        dock === "story"
                          ? "beat"
                          : dock === "event"
                            ? "action"
                            : "event",
                    })
                  }
                />
              ) : dock === "assets" ? (
                renderAssets()
              ) : dock === "sound" ? (
                <SoundWorkspace onAttach={attachSound} />
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
                        ? issues.length + " проблемы в проекте"
                        : "Проверки пройдены"}
                    </strong>
                    <span>Проверено: ресурсы, ожидания, переходы</span>
                  </div>
                  {issues.map((i) => (
                    <div className="issue-row" key={i.id}>
                      <Icon name="TriangleAlert" size={17} />
                      <button
                        onClick={() => {
                          selectBeat(i.beatId);
                          setDock("staging");
                        }}
                      >
                        <strong>{i.title}</strong>
                        <span>{i.detail}</span>
                        <small>
                          {i.beatId} · {sceneFor(project, i.beatId).location}
                        </small>
                      </button>
                      {i.fix && (
                        <Button onClick={() => fix(i)}>
                          {i.fix === "sequence-batch"
                            ? "Сделать по очереди"
                            : "Исправить"}
                        </Button>
                      )}
                    </div>
                  ))}
                  <FailureLab />
                </div>
              )}
            </div>
          </section>
        </section>
        <ResizeBar
          axis="x"
          label="Ширина инспектора"
          onMove={(d) => resize("right", -d)}
          onReset={() => setLayout((l) => ({ ...l, right: 300 }))}
        />
        <aside className="inspector-panel">
          <div className="panel-tabs">
            <button className="active">Инспектор</button>
            <button
              onClick={() => setSelection({ kind: "scene", id: scene.id })}
            >
              Сабсцена
            </button>
            <div className="flex-space" />
            <Button
              icon={inspectorPinned ? "LockKeyhole" : "Pin"}
              title="Закрепить инспектор"
              className={inspectorPinned ? "active" : ""}
              onClick={() => setInspectorPinned((v) => !v)}
            />
          </div>
          <div className="inspector-scroll">
            {selection.kind === "scene" ? (
              <>
                <div className="inspector-identity">
                  <Icon name="PanelsTopLeft" size={25} />
                  <strong>{scene.name}</strong>
                </div>
                <Fold title="Локация" icon="MapPin">
                  <Field label="Название">
                    <input
                      value={scene.name}
                      onChange={(e) =>
                        mutate(
                          (p) =>
                            (p.subscenes.find((s) => s.id === scene.id).name =
                              e.target.value),
                        )
                      }
                    />
                  </Field>
                  <Field label="Локация">
                    <input
                      value={scene.location}
                      onChange={(e) =>
                        mutate(
                          (p) =>
                            (p.subscenes.find(
                              (s) => s.id === scene.id,
                            ).location = e.target.value),
                        )
                      }
                    />
                  </Field>
                  <Field label="Сцена из примитивов">
                    <Select
                      value={scene.kind}
                      options={[
                        ["living", "Гостиная"],
                        ["garden", "Сад"],
                        ["station", "Станция"],
                      ]}
                      onChange={(kind) =>
                        mutate(
                          (p) =>
                            (p.subscenes.find((s) => s.id === scene.id).kind =
                              kind),
                        )
                      }
                    />
                  </Field>
                </Fold>
                <Fold title="Начальное окружение" icon="CloudSun">
                  <Field label="Погода">
                    <Select
                      value={scene.weather}
                      options={["Ясно", "Дождь", "Гроза", "Туман"]}
                      onChange={(weather) =>
                        mutate(
                          (p) =>
                            (p.subscenes.find(
                              (s) => s.id === scene.id,
                            ).weather = weather),
                        )
                      }
                    />
                  </Field>
                  <Field label="Время суток">
                    <Select
                      value={scene.time}
                      options={["Рассвет", "День", "Закат", "Ночь"]}
                      onChange={(time) =>
                        mutate(
                          (p) =>
                            (p.subscenes.find((s) => s.id === scene.id).time =
                              time),
                        )
                      }
                    />
                  </Field>
                </Fold>
              </>
            ) : (
              renderInspector()
            )}
            <Fold
              title="Переменные для проверки"
              icon="SlidersHorizontal"
              open={false}
            >
              <Field label="Доверие Алисы">
                <input
                  type="number"
                  value={variables.trust}
                  disabled={running}
                  onChange={(e) =>
                    mutate((p) => (p.variables.trust = Number(e.target.value)))
                  }
                />
              </Field>
              <label className="check">
                <input
                  type="checkbox"
                  disabled={running}
                  checked={!!variables.letter}
                  onChange={(e) =>
                    mutate((p) => (p.variables.letter = e.target.checked))
                  }
                />
                Письмо найдено
              </label>
            </Fold>
          </div>
          {running && (
            <div className="inspector-play-note">
              <Icon name="Info" size={13} />
              Изменения — для следующего запуска
            </div>
          )}
        </aside>
      </div>
      <footer className="editor-status">
        <button onClick={() => setDock("issues")}>
          <Icon
            name={issues.length ? "TriangleAlert" : "CheckCheck"}
            size={13}
          />
          {issues.length} проблемы
        </button>
        <span className="status-divider" />
        <span>{saveError || notice || "Сохранено на этом компьютере"}</span>
        <div className="flex-space" />
        <span>
          Выделено:{" "}
          {selection.kind === "object" ? inspectedObject?.name : beat.id}
        </span>
        <span className="status-divider" />
        <span>UI prototype 03</span>
        <Icon name="Flower2" size={13} />
      </footer>
      {menu && (
        <div
          className="editor-menu"
          style={{
            left:
              menu === "Файл"
                ? 247
                : menu === "Правка"
                  ? 293
                  : menu === "Создать"
                    ? 351
                    : 422,
          }}
        >
          {menu === "Файл" ? (
            <>
              <button
                onClick={() => {
                  setMenu(null);
                  fileInput.current.click();
                }}
              >
                <Icon name="FolderOpen" />
                Открыть проект…
              </button>
              <button onClick={exportProject}>
                <Icon name="Download" />
                Экспортировать JSON
              </button>
            </>
          ) : menu === "Правка" ? (
            <button
              disabled={!history.length}
              onClick={() => {
                undo();
                setMenu(null);
              }}
            >
              <Icon name="Undo2" />
              Отменить <kbd>Ctrl Z</kbd>
            </button>
          ) : menu === "Создать" ? (
            <>
              <button
                onClick={() => {
                  setMenu(null);
                  setPicker({ kind: "beat" });
                }}
              >
                <Icon name="MessageSquare" />
                Блок сценария
              </button>
              <button
                onClick={() => {
                  setMenu(null);
                  createEvent();
                }}
              >
                <Icon name="Layers" />
                Событие
              </button>
              <button
                onClick={() => {
                  setMenu(null);
                  setPicker({ kind: "object" });
                }}
              >
                <Icon name="Box" />
                Объект
              </button>
            </>
          ) : (
            <>
              <button onClick={resetLayout}>
                <Icon name="PanelsTopLeft" />
                Восстановить раскладку
              </button>
              <button
                onClick={() => {
                  setMaximized("scene");
                  setMenu(null);
                }}
              >
                <Icon name="Maximize2" />
                Развернуть сцену
              </button>
              <button
                onClick={() => {
                  setMaximized("graph");
                  setMenu(null);
                }}
              >
                <Icon name="Workflow" />
                Развернуть граф
              </button>
            </>
          )}
        </div>
      )}
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
                  ? "Добавить в сценарий"
                  : picker.kind === "event"
                    ? "Добавить событие"
                    : picker.kind === "object"
                      ? "Добавить объект"
                      : picker.kind === "object-action"
                        ? "Действие с объектом"
                        : "Добавить действие"}
              </strong>
              <Button
                icon="X"
                title="Закрыть"
                onClick={() => setPicker(null)}
              />
            </div>
            {picker.kind === "beat" ? (
              <>
                {beat.kind === "choice" && (
                  <div className="create-phase">
                    <Field label="Продолжить ответ">
                      <Select
                        value={picker.choiceId || beat.choices[0]?.id}
                        options={beat.choices.map((c) => [c.id, c.label])}
                        onChange={(choiceId) =>
                          setPicker((p) => ({ ...p, choiceId }))
                        }
                      />
                    </Field>
                  </div>
                )}
                <div className="create-options">
                  {[
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
                      "Разные ответы ведут к разным продолжениям",
                    ],
                    [
                      "gate",
                      "MousePointer2",
                      "Ждать взаимодействие",
                      "Продолжить после действия игрока",
                    ],
                    ["end", "Flag", "Концовка", "Завершить этот путь истории"],
                  ].map(([kind, icon, title, desc]) => (
                    <button key={kind} onClick={() => addBeat(kind)}>
                      <Icon name={icon} size={22} />
                      <span>
                        <strong>{title}</strong>
                        <small>{desc}</small>
                      </span>
                      <Icon name="ChevronRight" size={15} />
                    </button>
                  ))}
                </div>
              </>
            ) : picker.kind === "object" ? (
              <div className="create-options">
                {["Персонаж", "Активный меш", "Меш"].map((t, i) => (
                  <button key={t} onClick={() => addObject(t)}>
                    <Icon
                      name={["PersonStanding", "MousePointer2", "Box"][i]}
                      size={23}
                    />
                    <span>
                      <strong>{t}</strong>
                      <small>
                        {i === 0
                          ? "Участвует в диалогах и постановке"
                          : i === 1
                            ? "Игрок может взаимодействовать"
                            : "Декорация в сцене"}
                      </small>
                    </span>
                  </button>
                ))}
              </div>
            ) : picker.kind === "event" ? (
              <>
                <div className="create-phase">
                  <Field label="Когда запустить">
                    <Select
                      value={picker.phase || "ON_START"}
                      options={PHASES.map((p) => [p.id, p.label])}
                      onChange={(v) => setPicker((p) => ({ ...p, phase: v }))}
                    />
                  </Field>
                  <Button icon="Plus" onClick={createEvent}>
                    Создать шаблон
                  </Button>
                </div>
                <div className="event-selection-list">
                  {project.events
                    .filter((e) => !picker.assetId || e.id === picker.assetId)
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
                          setDock("staging");
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
                            {e.groups.length} группы ·{" "}
                            {e.groups.reduce((n, g) => n + g.actions.length, 0)}{" "}
                            действия
                          </small>
                        </span>
                        <Icon name="Plus" size={15} />
                      </button>
                    ))}
                </div>
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
                        <strong>{t.label}</strong>
                        <small>{t.domain || "Условие"}</small>
                      </span>
                    </button>
                  ))}
              </div>
            )}
          </div>
        </div>
      )}
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
