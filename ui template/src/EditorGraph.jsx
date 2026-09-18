import React, {
  memo,
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  ReactFlow,
  Background,
  BackgroundVariant,
  Handle,
  Position,
  MiniMap,
  MarkerType,
  applyNodeChanges,
  useReactFlow,
  ReactFlowProvider,
  Panel,
} from "@xyflow/react";
import dagre from "@dagrejs/dagre";
import {
  allBeats,
  sceneFor,
  PHASES,
  batchesFor,
  bindingActions,
  TYPES,
  JOIN_LABELS,
} from "./studioModel.js";
import { Icon, Button } from "./StudioParts.jsx";
import "@xyflow/react/dist/style.css";
import ActionFields from "./ActionFields.jsx";

const color = {
  dialogue: "#bc899f",
  choice: "#c2a06c",
  gate: "#86adb5",
  merge: "#a398bc",
  end: "#b98e9e",
  portal: "#839dbd",
  event: "#b69bba",
  action: "#97aba1",
  marker: "#8d939d",
  fork: "#b4a4bd",
  join: "#a5b7a8",
};
export function storyGraph(project, selectedId, scope = "chapter") {
  const selected =
    allBeats(project).find((b) => b.id === selectedId) || allBeats(project)[0];
  const chapter = project.chapters.find((c) =>
      c.beats.some((b) => b.id === selected.id),
    ),
    scene = sceneFor(project, selected.id);
  let beats =
    scope === "all"
      ? allBeats(project)
      : scope === "scene"
        ? project.chapters
            .filter((c) => c.subsceneId === scene.id)
            .flatMap((c) => c.beats)
        : chapter.beats;
  if (scope === "nearby") {
    const all = allBeats(project),
      visible = new Set([selected.id]);
    let frontier = [selected];
    for (let depth = 0; depth < 2; depth++) {
      frontier = frontier.flatMap((b) =>
        (b.kind === "choice" ? b.choices.map((c) => c.next) : [b.next])
          .map((id) => all.find((b) => b.id === id))
          .filter(Boolean),
      );
      frontier.forEach((b) => visible.add(b.id));
    }
    beats = all.filter((b) => visible.has(b.id));
  }
  const ids = new Set(beats.map((b) => b.id)),
    nodes = [],
    edges = [];
  for (const b of beats) {
    nodes.push({
      id: b.id,
      type: "editor",
      position: { x: 0, y: 0 },
      data: {
        kind: b.kind || "dialogue",
        beat: b,
        title: b.speaker,
        text: b.text,
        subtitle: b.id,
        phases: PHASES.map((p) => ({
          id: p.id,
          label: p.label,
          count: b.bindings.filter((x) => x.hook === p.id).length,
        })),
        ports:
          b.kind === "choice"
            ? b.choices.map((c) => ({
                id: "choice:" + c.id,
                label: c.label,
                condition: c.condition,
                threshold: c.threshold,
              }))
            : b.kind !== "end"
              ? [{ id: "next", label: "Дальше" }]
              : [],
      },
      width: 252,
      height: b.kind === "choice" ? 135 + b.choices.length * 40 : 190,
    });
    const outs =
      b.kind === "choice"
        ? b.choices.map((c) => ({
            to: c.next,
            port: "choice:" + c.id,
            label:
              c.condition === "trust"
                ? `Доверие ≥ ${c.threshold ?? 3}`
                : c.condition === "letter"
                  ? "Письмо найдено"
                  : "",
          }))
        : b.next
          ? [{ to: b.next, port: "next", label: "" }]
          : [];
    for (const out of outs) {
      if (!out.to) continue;
      const target = allBeats(project).find((x) => x.id === out.to);
      if (!target) continue;
      const targetId = ids.has(out.to) ? out.to : "portal:" + out.to;
      if (!ids.has(out.to) && !nodes.some((n) => n.id === targetId)) {
        nodes.push({
          id: targetId,
          type: "editor",
          position: { x: 0, y: 0 },
          width: 220,
          height: 136,
          data: {
            kind: "portal",
            title: sceneFor(project, out.to).location,
            text: target.text,
            subtitle: "Перейти · " + out.to,
            beat: target,
            ports: [],
          },
        });
      }
      edges.push({
        id: `${b.id}/${out.port}`,
        source: b.id,
        target: targetId,
        sourceHandle: out.port,
        targetHandle: "in",
        label: out.label,
        data: { sourceBeat: b.id, port: out.port, targetBeat: out.to },
        reconnectable: true,
      });
    }
  }
  const g = new dagre.graphlib.Graph();
  g.setGraph({
    rankdir: "LR",
    nodesep: 55,
    ranksep: 95,
    marginx: 38,
    marginy: 45,
  });
  g.setDefaultEdgeLabel(() => ({}));
  nodes.forEach((n) => g.setNode(n.id, { width: n.width, height: n.height }));
  edges.forEach((e) => g.setEdge(e.source, e.target));
  dagre.layout(g);
  nodes.forEach((n) => {
    const xy = g.node(n.id);
    n.position = { x: xy.x - n.width / 2, y: xy.y - n.height / 2 };
  });
  return { nodes, edges };
}

export function stagingGraph(project, beatId, phaseFilter = "ALL") {
  const beat = allBeats(project).find((b) => b.id === beatId),
    nodes = [],
    edges = [];
  let x = 30,
    last = null,
    counter = 0;
  const add = (id, kind, title, text, extra = {}, y = 150, width = 235) => {
    const n = {
      id,
      type: "editor",
      position: { x, y },
      width,
      height: kind === "event" ? 176 : 110,
      data: { kind, title, text, ports: [{ id: "next", label: "" }], ...extra },
    };
    nodes.push(n);
    return id;
  };
  const link = (a, b, label = "") => {
    if (a)
      edges.push({
        id: "flow-" + counter++,
        source: a,
        target: b,
        sourceHandle: "next",
        targetHandle: "in",
        label,
        reconnectable: false,
      });
  };
  for (const phase of PHASES.filter(
    (p) => phaseFilter === "ALL" || p.id === phaseFilter,
  )) {
    if(phase.id==='AFTER'&&phaseFilter==='ALL'){
      const wait=add('player-input','gate','Ждать игрока','Дальше, выбранный ответ или клик по объекту',{},150,180);
      link(last,wait);last=wait;x+=220;
    }
    const marker = add(
      "phase-" + phase.id,
      "marker",
      phase.label,
      phase.id === "BEFORE"
        ? "Сначала подготовить сцену"
        : phase.id === "ON_START"
          ? "Показать текст и начать постановку"
          : "Игрок нажал «Дальше»",
      { phase: phase.id },
      150,
      180,
    );
    link(last, marker);
    last = marker;
    x += 220;
    for (const batch of batchesFor(beat, phase.id)) {
      if (batch.mode === "PARALLEL" && batch.bindings.length > 1) {
        const fork = add(
          "fork-" + batch.id,
          "fork",
          "Запустить вместе",
          batch.bindings.length + " события",
          { batchId: batch.id, batch, phase: phase.id },
          172,
          160,
        );
        link(last, fork);
        x += 195;
        const eventX = x;
        const outs = [];
        batch.bindings.forEach((b, i) => {
          x = eventX;
          const ev = project.events.find((e) => e.id === b.eventId),
            id = add(
              b.id,
              "event",
              ev?.name || "Событие удалено",
              bindingActions(project, b)
                .slice(0, 3)
                .map(
                  (a) =>
                    `${project.objects.find((o) => o.id === a.target)?.name || TYPES[a.type]?.label} → ${a.value}`,
                )
                .join("\n"),
              {
                binding: b,
                event: ev,
                phase: phase.id,
                batchId: batch.id,
                batch,
              },
              i * 240,
            );
          link(fork, id);
          outs.push(id);
        });
        x = eventX + 275;
        const join = add(
          "join-" + batch.id,
          "join",
          "Соединить результаты",
          batch.bindings.every((b) => b.join === "EVENT_END")
            ? "Дождаться всех событий"
            : "Дождаться указанных результатов",
          { batchId: batch.id, batch, phase: phase.id },
          172,
          175,
        );
        outs.forEach((id) =>
          link(
            id,
            join,
            JOIN_LABELS[beat.bindings.find((b) => b.id === id).join]
              .replace("Ждём ", "")
              .replace(" · фон продолжится", ""),
          ),
        );
        last = join;
        x += 215;
      } else
        for (const b of batch.bindings) {
          const ev = project.events.find((e) => e.id === b.eventId);
          const id = add(
            b.id,
            "event",
            ev?.name || "Событие удалено",
            bindingActions(project, b)
              .slice(0, 3)
              .map(
                (a) =>
                  `${project.objects.find((o) => o.id === a.target)?.name || TYPES[a.type]?.label} → ${a.value}`,
              )
              .join("\n"),
            {
              binding: b,
              event: ev,
              phase: phase.id,
              batchId: batch.id,
              batch,
            },
          );
          link(last, id);
          last = id;
          x += 280;
        }
    }
  }
  const end = add(
    "flow-end",
    "marker",
    phaseFilter === "BEFORE"
      ? "Показать реплику"
      : phaseFilter === "ON_START"
        ? "Ждать игрока"
        : "Продолжить историю",
    phaseFilter === "BEFORE" ? `${beat.speaker} · ${beat.id}` : phaseFilter === "ON_START"
      ? "Текст остаётся на экране"
      : beat.next
        ? `→ ${beat.next}`
        : "Следующий выбранный путь",
  );
  link(last, end);
  return { nodes, edges };
}

export function eventGraph(project, eventId, binding) {
  const ev = project.events.find((e) => e.id === eventId),
    nodes = [],
    edges = [];
  if (!ev) return { nodes, edges };
  let x = 30,
    last = null,
    index = 0;
  const node = (id, kind, title, text, y = 125, width = 245, data = {}) => {
    nodes.push({
      id,
      type: "editor",
      position: { x, y },
      width,
      height: kind === "action" ? 236 : 120,
      data: { kind, title, text, ports: [{ id: "next", label: "" }], ...data },
    });
    return id;
  };
  const edge = (a, b, label = "") => {
    if (a)
      edges.push({
        id: "event-wire-" + index++,
        source: a,
        target: b,
        sourceHandle: "next",
        targetHandle: "in",
        label,
        reconnectable: false,
      });
  };
  last = node("start", "marker", "Начать событие", ev.name);
  x += 310;
  for (const [i, g] of ev.groups.entries()) {
    const fork = node(
      g.id,
      "fork",
      `${i + 1}. ${g.name}`,
      g.actions.length > 1 ? "Действия запускаются вместе" : "Один шаг",
      125,
      200,
      { groupId: g.id, event: ev },
    );
    edge(last, fork);
    x += 260;
    const ids = [];
    g.actions.forEach((a, j) => {
      const resolved = binding
        ? bindingActions(project, binding).find((x) => x.id === a.id)
        : a;
      const id = node(
        a.id,
        "action",
        TYPES[resolved.type]?.label,
        resolved.value,
        j * 280,
        270,
        { action: resolved, groupId: g.id, event: ev, binding },
      );
      edge(fork, id);
      ids.push(id);
    });
    x += 350;
    const join = node(
      "done-" + g.id,
      "join",
      "Дождаться действий",
      "Конечные — завершены\nДлительные — запущены",
      125,
      200,
      { groupId: g.id, event: ev },
    );
    ids.forEach((id) => edge(id, join));
    last = join;
    x += 275;
  }
  const end = node(
    "finish",
    "marker",
    ev.retention === "AUTO_CLOSE_ON_FLOW_END"
      ? "Событие завершено"
      : "Продолжать в фоне",
    ev.retention === "AUTO_CLOSE_ON_FLOW_END"
      ? "Следующий шаг"
      : ev.owner === "Scene"
        ? "До остановки · вся сцена"
        : "До остановки · эта сабсцена",
  );
  edge(last, end);
  return { nodes, edges };
}

const EditorNode = memo(function EditorNode({ id, data, selected }) {
  const {
    kind,
    beat,
    title,
    text,
    ports = [],
    onOpen,
    onEdit,
    onPhase,
    action,
    project,
  } = data;
  const icon =
    kind === "choice"
      ? "GitFork"
      : kind === "gate"
        ? "MousePointerClick"
        : kind === "end"
          ? "Flag"
          : kind === "portal"
            ? "ExternalLink"
            : kind === "event"
              ? "Layers"
              : kind === "action"
                ? TYPES[action.type]?.icon
                : kind === "fork"
                  ? "GitFork"
                  : kind === "join"
                    ? "GitMerge"
                    : kind === "marker"
                      ? "CirclePlay"
                      : "MessageSquare";
  return (
    <article
      className={`enode ${kind} ${selected ? "is-selected" : ""} ${data.active ? "is-playing" : ""} ${data.error ? "has-error" : ""}`}
      style={{ "--node-color": color[kind] || color.dialogue }}
    >
      <Handle type="target" position={Position.Left} id="in" />
      <header className="node-grab">
        <Icon name={icon} size={15} />
        <strong>{title}</strong>
        {data.active ? (
          <Icon name="Play" size={12} />
        ) : (
          <span>{data.subtitle || ""}</span>
        )}
      </header>
      {action ? (
        <div className="node-action-form nodrag nowheel">
          <ActionFields
            compact
            action={action}
            project={project}
            onChange={(v) => data.onPatchAction(action.id, v)}
          />
          <div className="node-action-end">
            <span>
              <Icon name="Timer" size={12} />
              {TYPES[action.type]?.completion === "CONTINUOUS"
                ? "В фоне"
                : TYPES[action.type]?.completion === "INSTANT"
                  ? "Сразу"
                  : "До завершения"}
            </span>
            <button className="nodrag" onClick={() => onEdit?.(data)}>
              Все параметры <Icon name="ArrowUpRight" size={12} />
            </button>
          </div>
        </div>
      ) : (
        <div className="node-copy">{text}</div>
      )}
      {data.phases && (
        <div className="node-phase-slots nodrag">
          {data.phases.map((p) => (
            <button
              key={p.id}
              className={p.count ? "filled" : ""}
              title={`Открыть события: ${p.label}`}
              onClick={() => onPhase?.(beat.id, p.id)}
            >
              <span>
                {p.id === "BEFORE"
                  ? "До"
                  : p.id === "ON_START"
                    ? "Во время"
                    : "После"}
              </span>
              <b>{p.count || "—"}</b>
            </button>
          ))}
        </div>
      )}
      {kind === "event" && (
        <>
          <div className="node-event-life">
            <Icon
              name={
                data.event?.retention === "AUTO_CLOSE_ON_FLOW_END"
                  ? "Check"
                  : "Infinity"
              }
              size={12}
            />
            {data.binding?.join === "EVENT_END"
              ? "Ждать завершения"
              : data.binding?.join === "FLOW_END"
                ? "Дождаться шагов · фон остаётся"
                : "Запустить и идти дальше"}
          </div>
          <button className="node-open nodrag" onClick={() => onOpen?.(data)}>
            Открыть действия <Icon name="ChevronRight" size={13} />
          </button>
        </>
      )}
      {kind === "portal" && (
        <button className="node-open nodrag" onClick={() => onOpen?.(data)}>
          Открыть продолжение <Icon name="ArrowUpRight" size={13} />
        </button>
      )}
      {kind === "choice" ? (
        <div className="node-answers">
          {ports.map((p, i) => (
            <div key={p.id}>
              <span>
                {i + 1}. {p.label}
              </span>
              {p.condition !== "always" && (
                <Icon name="LockKeyhole" size={11} />
              )}
              <Handle
                type="source"
                position={Position.Right}
                id={p.id}
                style={{ top: "50%" }}
              />
            </div>
          ))}
        </div>
      ) : (
        ports.length > 0 && (
          <Handle type="source" position={Position.Right} id={ports[0].id} />
        )
      )}
      {data.batch && kind !== "join" && (
        <div className="node-batch-controls nodrag">
          <select
            aria-label="Порядок группы"
            value={data.batch.mode}
            onChange={(e) => data.onBatch(data.batchId, "mode", e.target.value)}
          >
            <option value="SEQUENTIAL">По очереди →</option>
            <option value="PARALLEL">Вместе ⇉</option>
          </select>
          <button
            title="Добавить событие в эту группу"
            onClick={() => data.onBatch(data.batchId, "add")}
          >
            <Icon name="Plus" size={14} />
          </button>
          <button
            title="Настроить группу событий"
            onClick={() => data.onBatch(data.batchId, "select")}
          >
            <Icon name="Settings2" size={14} />
          </button>
        </div>
      )}
      {data.status && (
        <div className={"node-runtime-status " + data.status}>
          <Icon
            name={
              data.status === "running"
                ? "Play"
                : data.status === "paused"
                  ? "Pause"
                  : data.status === "held"
                    ? "Infinity"
                    : data.status === "error"
                      ? "TriangleAlert"
                      : "Check"
            }
            size={12}
          />
          {{
            running: "Выполняется",
            paused: "На паузе",
            held: "Работает в фоне",
            done: "Готово",
            stopped: "Остановлено",
            skipped: "Пропущено",
            error: "Ошибка",
          }[data.status] || data.status}
        </div>
      )}
      {data.error && (
        <div className="node-error">
          <Icon name="TriangleAlert" size={12} /> {data.error}
        </div>
      )}
    </article>
  );
});
const nodeTypes = { editor: EditorNode };

function GraphCanvas({
  project,
  mode = "story",
  selectedId,
  selectionId,
  eventId,
  binding,
  phase = "ALL",
  scope = "chapter",
  onSelect,
  onOpen,
  onEdit,
  onPhase,
  onPatchAction,
  onBatch,
  onConnect,
  issues = [],
  preview,
  positions = {},
  onPositions,
  onContext,
  onReady,
}) {
  const api = useReactFlow(),
    [nodes, setNodes] = useState([]),
    [zoom, setZoom] = useState(1),
    [selectedEdge, setSelectedEdge] = useState(null),
    initKey = useRef(null),
    container = useRef();
  const graph = useMemo(
    () =>
      mode === "story"
        ? storyGraph(project, selectedId, scope)
        : mode === "staging"
          ? stagingGraph(project, selectedId, phase)
          : eventGraph(project, eventId, binding),
    [project, mode, selectedId, scope, phase, eventId, binding],
  );
  const graphKey =
    mode === "story"
      ? `${mode}:${scope}:${scope === "chapter" ? project.chapters.find((c) => c.beats.some((b) => b.id === selectedId))?.id : scope === "scene" ? sceneFor(project, selectedId).id : scope === "nearby" ? selectedId : "all"}`
      : `${mode}:${mode === "staging" ? selectedId + phase : eventId}`;
  useEffect(() => {
    setNodes((previous) =>
      graph.nodes.map((n) => ({
        ...n,
        position: previous.find((p) => p.id === n.id)?.dragging
          ? previous.find((p) => p.id === n.id).position
          : positions[graphKey]?.[n.id] || n.position,
        dragging: previous.find((p) => p.id === n.id)?.dragging,
        style: { width: n.width },
        selected:
          mode === "story"
            ? n.id === selectedId
            : n.id === selectionId || n.data.batchId === selectionId,
        dragHandle: ".node-grab",
        data: {
          ...n.data,
          project,
          onOpen,
          onEdit,
          onPhase,
          onPatchAction,
          onBatch,
          error: issues.find((i) =>
            mode === "story"
              ? i.beatId === n.id
              : i.eventId === n.data.event?.id,
          )?.title,
          status:
            preview?.phase === "EDIT"
              ? null
              : preview?.phase === "ERROR" &&
                  preview?.states?.[n.id] === "error"
                ? "error"
                : preview?.paused && preview?.states?.[n.id] === "running"
                  ? "paused"
                  : preview?.states?.[n.id],
          active:
            preview?.phase !== "EDIT" &&
            !preview?.paused &&
            preview?.phase !== "ERROR" &&
            (mode === "story"
              ? preview?.beatId === n.id
              : preview?.states?.[n.id] === "running"),
        },
      })),
    );
  }, [
    graph,
    positions,
    graphKey,
    selectedId,
    selectionId,
    issues,
    preview,
    onOpen,
    onEdit,
    onPhase,
    onPatchAction,
    onBatch,
    project,
  ]);
  useEffect(() => {
    if (!container.current) return;
    let previous = container.current.getBoundingClientRect().height;
    const observer = new ResizeObserver(() => {
      const height = container.current.getBoundingClientRect().height;
      if (height > 0 && previous > 0 && Math.abs(height - previous) > 1) {
        const v = api.getViewport();
        api.setViewport({ ...v, y: v.y + (height - previous) / 2 });
      }
      previous = height;
    });
    observer.observe(container.current);
    return () => observer.disconnect();
  }, [api]);
  const focus = useCallback(
    (id = selectedId) => {
      const node = api.getNode(id) || api.getNodes()[0];
      if (node) {
        const rect = container.current?.getBoundingClientRect();
        const targetY =
          node.position.y + (node.measured?.height || node.height || 180) / 2;
        api.setViewport(
          {
            x: 35 - node.position.x * 0.9,
            y: Math.max(15, (rect?.height || 350) / 2) - targetY * 0.9,
            zoom: 0.9,
          },
          { duration: 240 },
        );
      }
    },
    [api, selectedId],
  );
  useEffect(() => {
    if (!nodes.length || initKey.current === graphKey) return;
    const t = setTimeout(() => {
      focus(mode === "story" ? selectedId : graph.nodes[0]?.id);
      initKey.current = graphKey;
    }, 90);
    return () => clearTimeout(t);
  }, [nodes.length, graphKey, focus, mode, selectedId, graph.nodes]);
  useEffect(() => {
    if (mode !== "story") return;
    const timer = setTimeout(() => {
      const n = api.getNode(selectedId),
        r = container.current?.getBoundingClientRect(),
        v = api.getViewport();
      if (!n || !r) return;
      const x = n.position.x * v.zoom + v.x,
        y = n.position.y * v.zoom + v.y,
        w = (n.measured?.width || 252) * v.zoom,
        h = (n.measured?.height || 190) * v.zoom;
      if (x < 10 || y < 10 || x + w > r.width - 15 || y + h > r.height - 50)
        focus(selectedId);
    }, 110);
    return () => clearTimeout(timer);
  }, [selectedId, mode, graphKey, focus, graph]);
  useEffect(() => {
    onReady?.({
      focus,
      fit: () => api.fitView({ padding: 0.12, maxZoom: 1, duration: 240 }),
      zoomIn: () => api.zoomIn(),
      zoomOut: () => api.zoomOut(),
      arrange: () => {
        onPositions?.(graphKey, {});
        setNodes(graph.nodes);
        api.fitView({ padding: 0.15, maxZoom: 1 });
      },
    });
  }, [focus, api, graph, onReady, graphKey, onPositions]);
  const edges = graph.edges.map((e) => ({
    ...e,
    type: "smoothstep",
    pathOptions: { borderRadius: 6, offset: 10 },
    markerEnd: {
      type: MarkerType.ArrowClosed,
      width: 15,
      height: 15,
      color: "#97919f",
    },
    style: {
      stroke: e.id === selectedEdge ? "#d9a8bc" : "#807c8b",
      strokeWidth: e.id === selectedEdge ? 2 : 1.5,
    },
    labelStyle: { fill: "#b8b1bf", fontSize: 11 },
    labelBgStyle: { fill: "#202127" },
    labelBgPadding: [6, 3],
    labelBgBorderRadius: 3,
  }));
  return (
    <div className="graph-canvas" ref={container}>
      <ReactFlow
        nodes={nodes}
        edges={edges}
        nodeTypes={nodeTypes}
        onNodesChange={(changes) =>
          setNodes((ns) => applyNodeChanges(changes, ns))
        }
        onNodeDragStop={(_, node) =>
          onPositions?.(graphKey, {
            ...positions[graphKey],
            [node.id]: node.position,
          })
        }
        onNodeClick={(_, n) => {
          if (n.data.kind === "portal") onSelect?.(n.data.beat.id);
          else onSelect?.(mode === "story" ? n.id : n.data);
        }}
        onNodeDoubleClick={(_, n) => onOpen?.(n.data)}
        onEdgeClick={(_, e) => setSelectedEdge(e.id)}
        onPaneClick={() => setSelectedEdge(null)}
        onPaneContextMenu={(e) => {
          e.preventDefault();
          onContext?.();
        }}
        onConnect={(c) => onConnect?.(c)}
        onReconnect={(e, c) =>
          onConnect?.({ ...c, source: e.source, sourceHandle: e.sourceHandle })
        }
        onMove={(_, v) => setZoom(v.zoom)}
        nodesConnectable={mode === "story"}
        edgesReconnectable={mode === "story"}
        deleteKeyCode={null}
        minZoom={0.12}
        maxZoom={1.75}
        panOnScroll
        panOnDrag={[0, 1, 2]}
        zoomOnScroll={false}
        zoomActivationKeyCode="Control"
        selectionOnDrag={false}
        connectionRadius={25}
        colorMode="dark"
      >
        <Background
          variant={BackgroundVariant.Lines}
          gap={24}
          size={0.5}
          color="#303139"
        />
        <MiniMap
          pannable
          zoomable
          nodeColor={(n) => color[n.data.kind] || "#917b98"}
          maskColor="#15161dc4"
          position="bottom-right"
        />
        <Panel position="bottom-left">
          <div className="graph-navigation">
            <Button
              icon="Minus"
              title="Уменьшить масштаб"
              onClick={() => api.zoomOut()}
            />
            <button onClick={() => api.zoomTo(1)}>
              {Math.round(zoom * 100)}%
            </button>
            <Button
              icon="Plus"
              title="Увеличить масштаб"
              onClick={() => api.zoomIn()}
            />
            <span />
            <Button
              icon="Focus"
              title="Показать выделенный блок"
              onClick={() => focus()}
            />
            <Button
              icon="Scan"
              title="Показать граф целиком"
              onClick={() => api.fitView({ padding: 0.15, maxZoom: 1 })}
            />
          </div>
        </Panel>
        {selectedEdge && mode === "story" && (
          <Panel position="top-center">
            <Button
              icon="Unplug"
              onClick={() => {
                const edge = graph.edges.find((e) => e.id === selectedEdge);
                onConnect?.({
                  source: edge.source,
                  sourceHandle: edge.sourceHandle,
                  target: null,
                });
                setSelectedEdge(null);
              }}
            >
              Удалить выбранную связь
            </Button>
          </Panel>
        )}
      </ReactFlow>
    </div>
  );
}
export default function EditorGraph(props) {
  return (
    <ReactFlowProvider>
      <GraphCanvas {...props} />
    </ReactFlowProvider>
  );
}
