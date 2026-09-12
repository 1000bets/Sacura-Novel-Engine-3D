// The story has no authored duration. This is an ordered flow of subscene
// fragments, not a seconds-based ruler. Contract only unambiguous linear runs:
// merging an entire scene would invent paths between unrelated entrances/exits.
export const timelineVariableName = id => ({trust:'Доверие',letter:'Письмо найдено'}[id] || id);
export const timelineVariableValue = value => value === undefined ? 'не задано' : typeof value === 'boolean' ? (value ? 'да' : 'нет') : String(value);
export function timelineCondition(choice) {
  if (!choice.condition || choice.condition === 'always') return 'Без условия';
  if (choice.condition === 'trust') return `Доверие ≥ ${choice.threshold ?? 3}`;
  return `${timelineVariableName(choice.condition)} = да`;
}

export function buildTimelineModel(project) {
  const scenes = project.subscenes || [], beats = [], sceneByBeat = new Map();
  for (const chapter of project.chapters || []) for (const beat of chapter.beats || []) {
    beats.push(beat);
    sceneByBeat.set(beat.id, scenes.find(scene => scene.id === chapter.subsceneId) || scenes[0]);
  }
  const beatById = new Map(beats.map(beat => [beat.id, beat]));
  const outgoing = new Map(), incoming = new Map(), rawEdges = [];
  for (const beat of beats) {
    // An explicit ending stays terminal, even in a malformed imported project.
    const choices = beat.kind === 'end' ? [] : beat.kind === 'choice' ? (beat.choices || []) : beat.next ? [{next:beat.next}] : [];
    const edges = choices.map((choice, index) => ({
      id:`route:${JSON.stringify([beat.id, beat.kind === 'choice' ? choice.id ?? index : null])}`,
      from:beat.id, to:choice.next || null,
      choiceId:beat.kind === 'choice' ? choice.id : undefined,
      label:beat.kind === 'choice' ? choice.label || 'Ответ без названия' : 'Продолжить',
      condition:choice.condition || 'always', threshold:choice.threshold,
      conditionLabel:timelineCondition(choice),
    }));
    outgoing.set(beat.id, edges);
    rawEdges.push(...edges);
    for (const edge of edges) if (beatById.has(edge.to)) incoming.set(edge.to, [...(incoming.get(edge.to) || []), edge]);
  }
  const regular = beat => beat && !['choice', 'end'].includes(beat.kind);
  const canJoin = edge => {
    if (!edge) return false;
    const from = beatById.get(edge.from), to = beatById.get(edge.to), scene = sceneByBeat.get(edge.from);
    return regular(from) && regular(to) && from.id !== to.id && scene?.id === sceneByBeat.get(to.id)?.id
      && to.id !== scene?.entry && incoming.get(to.id)?.length === 1 && outgoing.get(from.id)?.length === 1;
  };
  const nodes = [], beatToNode = {}, internalEdges = new Set();
  const addNode = first => {
    if (beatToNode[first.id]) return;
    const scene = sceneByBeat.get(first.id), members = [first];
    const id = `${first.kind === 'choice' ? 'choice' : first.kind === 'end' ? 'ending' : 'scene'}:${first.id}`;
    beatToNode[first.id] = id;
    let last = first;
    while (regular(last)) {
      const edge = outgoing.get(last.id)?.[0];
      if (!canJoin(edge) || beatToNode[edge.to]) break;
      last = beatById.get(edge.to);
      internalEdges.add(edge.id); members.push(last); beatToNode[last.id] = id;
    }
    nodes.push({
      id, kind:first.kind === 'choice' ? 'choice' : first.kind === 'end' ? 'ending' : 'subscene',
      sceneId:scene?.id, sceneName:scene?.name || 'Без сабсцены', color:scene?.color || '#bd94a9',
      location:scene?.location || '', entryBeatId:first.id, beatIds:members.map(beat => beat.id),
      text:first.text || '', title:first.kind === 'end' ? first.ending || 'Концовка без названия' : scene?.name || 'Без сабсцены',
      choices:first.kind === 'choice' ? (first.choices || []).map(choice => ({...choice, conditionLabel:timelineCondition(choice)})) : [],
      isSceneEntry:first.id === scene?.entry,
      openEnd:regular(last) && !(outgoing.get(last.id)?.length),
      emptyChoice:first.kind === 'choice' && !first.choices?.length,
      gates:members.filter(beat => beat.kind === 'gate').length,
    });
  };
  // Start at real chain boundaries regardless of how chapters/beats are stored.
  for (const beat of beats) if (!regular(beat) || !canJoin(incoming.get(beat.id)?.length === 1 ? incoming.get(beat.id)[0] : null)) addNode(beat);
  // Disconnected cycles have no boundary; keep one representative and its loop.
  for (const beat of beats) addNode(beat);
  for (const scene of scenes) if (!beats.some(beat => sceneByBeat.get(beat.id)?.id === scene.id)) nodes.push({
    id:`empty:${scene.id}`, kind:'subscene', sceneId:scene.id, sceneName:scene.name, title:scene.name,
    color:scene.color || '#bd94a9', location:scene.location || '', entryBeatId:null, beatIds:[], choices:[], empty:true, openEnd:true,
  });
  const edges = rawEdges.filter(edge => !internalEdges.has(edge.id)).map(edge => {
    let target = beatToNode[edge.to];
    if (!target) {
      target = `missing:${edge.id}`;
      const source = nodes.find(node => node.id === beatToNode[edge.from]);
      nodes.push({id:target,kind:'missing',sceneId:source?.sceneId,sceneName:source?.sceneName,
        title:edge.to ? 'Реплика не найдена' : 'Нет продолжения',text:edge.to || 'Назначьте продолжение ответа',
        entryBeatId:edge.from,beatIds:[],choices:[]});
    }
    return {...edge,source:beatToNode[edge.from],target,crossScene:!!edge.to && sceneByBeat.get(edge.from)?.id !== sceneByBeat.get(edge.to)?.id,isReturn:false};
  });
  const firstBeat = beats.find(beat => beat.id === scenes[0]?.entry) || beats[0];
  const entryNodeId = firstBeat ? beatToNode[firstBeat.id] : nodes[0]?.id;
  const graph = new Map(nodes.map(node => [node.id, []]));
  edges.forEach(edge => graph.get(edge.source)?.push(edge));
  const reachable = new Set(), visitReachable = id => {
    if (!id || reachable.has(id)) return;
    reachable.add(id); graph.get(id)?.forEach(edge => visitReachable(edge.target));
  };
  visitReachable(entryNodeId);
  const visited = new Set(), stack = new Set(), nodeById = new Map(nodes.map(node => [node.id,node]));
  const markReturns = id => {
    if (visited.has(id)) return;
    visited.add(id); stack.add(id);
    for (const edge of graph.get(id) || []) {
      if (stack.has(edge.target)) edge.isReturn = true;
      else {
        // A return may re-enter a *different* fragment in an earlier subscene;
        // do not require its exact destination beat to be an ancestor.
        const targetScene = nodeById.get(edge.target)?.sceneId;
        if (edge.crossScene && targetScene && [...stack].some(nodeId => nodeById.get(nodeId)?.sceneId === targetScene)) edge.isReturn = true;
        markReturns(edge.target);
      }
    }
    stack.delete(id);
  };
  if (entryNodeId) markReturns(entryNodeId);
  nodes.forEach(node => markReturns(node.id));
  const parts = new Map();
  for (const node of nodes) if (node.kind === 'subscene') parts.set(node.sceneId, [...(parts.get(node.sceneId) || []), node]);
  for (const node of nodes) {
    node.reachable = reachable.has(node.id);
    if (node.kind === 'subscene') { node.part = parts.get(node.sceneId).indexOf(node) + 1; node.parts = parts.get(node.sceneId).length; }
  }
  const variableIds = new Set(Object.keys(project.variables || {})), dependencies = [];
  for (const node of nodes) if (node.kind === 'choice') {
    for (const variableId of new Set(node.choices.map(choice => choice.condition).filter(condition => condition && condition !== 'always'))) {
      variableIds.add(variableId);
      dependencies.push({id:`variable:${JSON.stringify([variableId,node.id])}`,variableId,target:node.id,beatId:node.entryBeatId});
    }
  }
  return {
    nodes, edges, beatToNode, entryNodeId, dependencies,
    variables:[...variableIds].map(id => ({id,name:timelineVariableName(id),value:project.variables?.[id],
      missing:!Object.hasOwn(project.variables || {}, id),usedBy:dependencies.filter(edge => edge.variableId === id).map(edge => edge.target)})),
    counts:{scenes:scenes.length,beats:beats.length,choices:nodes.filter(node => node.kind === 'choice').length,
      endings:nodes.filter(node => node.kind === 'ending').length,returns:edges.filter(edge => edge.isReturn).length},
  };
}

export function timelineNodeForBeat(model, beatId) { return model.beatToNode[beatId] || null; }

// Variable cards are shown one at a time, but each keeps its own placement.
export const timelineVariableNodeId = variableId => `global-variable:${variableId}`;

export function cleanTimelinePositions(project, positions = project.editor?.timelinePositions, model = buildTimelineModel(project)) {
  const validIds = new Set([...model.nodes.map(node => node.id), ...model.variables.map(variable => timelineVariableNodeId(variable.id))]);
  // Imported layout data cannot introduce ghost nodes or invalid graph bounds.
  // Keep hidden variables; visibility is an editor filter, not deletion.
  return Object.fromEntries(Object.entries(positions && typeof positions === 'object' && !Array.isArray(positions) ? positions : {})
    .filter(([id,position]) => validIds.has(id) && position && Number.isFinite(position.x) && Number.isFinite(position.y))
    .map(([id,position]) => [id,{x:position.x,y:position.y}]));
}
