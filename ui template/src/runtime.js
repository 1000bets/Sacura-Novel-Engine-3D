import {
  allBeats,
  batchesFor,
  bindingActions,
  PHASES,
  sceneFor,
  chooseNext,
  validateStudio,
  TYPES,
  conditionPass,
} from "./studioModel.js";

export class PreviewRuntime {
  constructor(audio, onChange = () => {}) {
    this.audio = audio;
    this.onChange = onChange;
    this.generation = 0;
    this.running = false;
    this.snapshot = {
      beatId: null,
      phase: "EDIT",
      textVisible: true,
      ready: false,
      states: {},
      effects: {},
      instances: {},
      world: {
        weather: "Дождь",
        time: "Закат",
        location: "living",
        positions: {},
        poses: {},
        visible: {},
      },
      variables: {},
      history: [],
    };
  }
  emit() {
    this.onChange({
      ...this.snapshot,
      states: { ...this.snapshot.states },
      effects: { ...this.snapshot.effects },
      world: { ...this.snapshot.world },
    });
  }
  async delay(ms, token, runId) {
    let remaining = ms;
    while (
      remaining > 0 ||
      this.snapshot.paused ||
      (runId && this.snapshot.instances[runId]?.status === "paused")
    ) {
      this.assert(token, runId);
      await new Promise((r) => setTimeout(r, Math.min(50, remaining)));
      if (
        !this.snapshot.paused &&
        (!runId || this.snapshot.instances[runId]?.status !== "paused")
      )
        remaining -= 50;
    }
    this.assert(token, runId);
  }
  assert(token, runId) {
    if (token !== this.generation || !this.running)
      throw new Error("CANCELLED");
    if (runId && this.snapshot.instances[runId]?.status === "stopped")
      throw new Error("INSTANCE_STOPPED");
  }
  async start(project, beatId) {
    this.stop();
    this.project = structuredClone(project);
    this.running = true;
    const token = ++this.generation;
    this.snapshot = {
      ...this.snapshot,
      beatId,
      phase: "BEFORE",
      paused: false,
      ready: false,
      error: null,
      states: {},
      effects: {},
      instances: {},
      variables: { ...project.variables },
      history: [],
      world: { positions: {}, poses: {}, visible: {} },
    };
    try {
      await this.enter(beatId, token);
    } catch (e) {
      this.handle(e);
    }
  }
  stop() {
    this.generation++;
    this.running = false;
    this.audio.stopAll();
    this.snapshot = {
      ...this.snapshot,
      phase: "EDIT",
      paused: false,
      ready: false,
      textVisible: true,
      effects: {},
      states: {},
      instances: {},
    };
    this.emit();
  }
  handle(e) {
    if (["CANCELLED", "INSTANCE_STOPPED"].includes(e.message)) return;
    this.generation++;
    this.audio.stopAll();
    for (const id of Object.keys(this.snapshot.states))
      if (this.snapshot.states[id] === "running")
        this.snapshot.states[id] = "error";
    for (const i of Object.values(this.snapshot.instances))
      if (i.status === "running") i.status = "error";
    this.snapshot.error = e.message;
    this.snapshot.phase = "ERROR";
    this.snapshot.ready = false;
    this.emit();
  }
  async enter(id, token) {
    this.assert(token);
    const b = allBeats(this.project).find((b) => b.id === id);
    if (!b) throw new Error("Реплика назначения удалена");
    const scene = sceneFor(this.project, id),
      old = this.snapshot.world.location;
    if (scene.id !== old) {
      for (const i of Object.values(this.snapshot.instances || {}))
        if (i.owner === "SubScene" && i.sceneId !== scene.id) {
          i.audioKeys.forEach((k) => this.audio.stop(k));
          i.status = "stopped";
        }
      for (const [key, e] of Object.entries(this.snapshot.effects))
        if (e.owner === "SubScene") {
          this.audio.stop(e.audioKey);
          delete this.snapshot.effects[key];
        }
      this.snapshot.world = {
        ...this.snapshot.world,
        location: scene.id,
        weather: scene.weather,
        time: scene.time,
        camera: "Общий план",
        positions: {},
        poses: {},
      };
      for (const key of ["weather", "time"])
        this.snapshot.effects[key] = {
          key,
          name: scene[key],
          status: "held",
          owner: "SubScene",
          origin: scene.name,
          beatId: id,
          type: key,
        };
    }
    this.snapshot.beatId = id;
    this.snapshot.ready = false;
    this.snapshot.error = null;
    this.snapshot.textVisible = false;
    this.snapshot.states = {};
    this.snapshot.history.push(id);
    this.snapshot.phase = "BEFORE";
    this.emit();
    const errors = validateStudio(this.project).filter(
      (i) => i.beatId === id && i.level === "error",
    );
    if (errors.length) throw new Error(errors.map((i) => i.title).join(" · "));
    await this.phase(b, "BEFORE", token);
    this.snapshot.textVisible = true;
    this.snapshot.phase = "ON_START";
    this.emit();
    await this.phase(b, "ON_START", token);
    this.assert(token);
    this.snapshot.phase =
      b.kind === "gate"
        ? "WAITING_OBJECT"
        : b.kind === "choice"
          ? "WAITING_CHOICE"
          : b.kind === "end"
            ? "ENDING"
            : "WAITING_INPUT";
    this.snapshot.ready = true;
    this.snapshot.gateElapsed = 0;
    this.emit();
    if (b.kind === "gate") {
      const timeout = Number(b.timeout || 30);
      this.delay(timeout * 1000, token)
        .then(() => {
          if (
            this.snapshot.beatId === id &&
            this.snapshot.phase === "WAITING_OBJECT"
          ) {
            this.snapshot.gateElapsed = timeout;
            this.snapshot.hint =
              "Подсказка: нажмите на выделенный объект в превью. Ожидание можно отменить кнопкой «Стоп».";
            this.emit();
          }
        })
        .catch(() => {});
    }
  }
  async phase(beat, phase, token) {
    this.snapshot.phase = phase;
    this.emit();
    for (const batch of batchesFor(beat, phase)) {
      this.assert(token);
      if (batch.mode === "PARALLEL")
        await Promise.all(batch.bindings.map((b) => this.binding(b, token)));
      else for (const b of batch.bindings) await this.binding(b, token);
    }
  }
  async binding(binding, token) {
    const definition = this.project.events.find(
        (e) => e.id === binding.eventId,
      ),
      event = definition
        ? {
            ...definition,
            originScene: this.snapshot.world.location,
            originBeat: this.snapshot.beatId,
          }
        : null;
    if (!event) throw new Error("Событие удалено");
    if (
      binding.condition &&
      !conditionPass(
        { condition: binding.condition, threshold: binding.threshold },
        this.snapshot.variables,
      )
    ) {
      this.snapshot.states[binding.id] = "skipped";
      this.emit();
      return;
    }
    event.runId = `${token}:${binding.id}:${++this.runCounter || (this.runCounter = 1)}`;
    this.snapshot.instances[event.runId] = {
      id: event.runId,
      name: event.name,
      eventId: event.id,
      bindingId: binding.id,
      beatId: event.originBeat,
      sceneId: event.originScene,
      owner: event.owner,
      status: "running",
      audioKeys: [],
    };
    this.snapshot.states[binding.id] = "running";
    this.emit();
    const work = (async () => {
      for (const group of event.groups) {
        await this.delay(0, token, event.runId);
        await Promise.all(
          bindingActions(this.project, binding)
            .filter((a) => a.groupId === group.id)
            .map((a) => this.action(a, event, binding, token)),
        );
      }
      await this.delay(0, token, event.runId);
      this.snapshot.states[binding.id] =
        event.retention === "AUTO_CLOSE_ON_FLOW_END" ? "done" : "held";
      this.snapshot.instances[event.runId].status =
        this.snapshot.states[binding.id];
      this.emit();
    })().catch((e) => {
      if (e.message === "INSTANCE_STOPPED") {
        this.snapshot.states[binding.id] = "stopped";
        for (const a of bindingActions(this.project, binding))
          if (this.snapshot.states[a.id] === "running")
            this.snapshot.states[a.id] = "stopped";
        this.emit();
        return;
      }
      throw e;
    });
    if (["NONE", "STARTED"].includes(binding.join)) {
      work.catch((e) => this.handle(e));
      return;
    }
    await work;
    if (
      binding.join === "EVENT_END" &&
      event.retention !== "AUTO_CLOSE_ON_FLOW_END"
    )
      throw new Error(
        `«${event.name}» удерживается до остановки. Для продолжения выберите «Ждём готовности · фон продолжится».`,
      );
  }
  async action(a, event, binding, token) {
    this.assert(token, event.runId);
    await this.delay(1, token, event.runId);
    if (
      event.owner === "SubScene" &&
      event.originScene !== this.snapshot.world.location
    )
      throw new Error("CANCELLED");
    this.snapshot.states[a.id] = "running";
    this.emit();
    const world = this.snapshot.world;
    const effect = (type, name, audioKey) => {
      this.snapshot.effects[type] = {
        key: type,
        runId: event.runId,
        type,
        name,
        status: "held",
        owner: event.owner || "SubScene",
        origin: event.name,
        beatId: event.originBeat,
        eventId: event.id,
        bindingId: binding.id,
        audioKey,
      };
    };
    switch (a.type) {
      case "move":
        await this.delay(
          Math.max(0.1, Number(a.duration || 2)) * 1000,
          token,
          event.runId,
        );
        if (
          event.owner === "SubScene" &&
          event.originScene !== this.snapshot.world.location
        )
          throw new Error("CANCELLED");
        world.positions = { ...world.positions, [a.target]: a.value };
        break;
      case "pose":
        world.poses = { ...world.poses, [a.target]: a.value };
        break;
      case "camera":
        world.camera = a.value;
        break;
      case "weather":
        world.weather = a.value;
        effect("weather", a.value);
        break;
      case "time":
        world.time = a.value;
        effect("time", a.value);
        break;
      case "music":
        this.snapshot.instances[event.runId].audioKeys.push("background");
        this.audio.play(a.assetId || "music-main", {
          key: "background",
          volume: a.volume ?? 0.42,
          loop: a.loop !== false,
          fade: a.fade ?? 1,
        });
        effect("music", a.value, "background");
        break;
      case "pause":
        this.audio.pause("background");
        if (this.snapshot.effects.music)
          this.snapshot.effects.music.status = "paused";
        break;
      case "resume":
        if (this.audio.get("background")) this.audio.resume("background");
        else
          this.audio.play("music-main", {
            key: "background",
            volume: 0.42,
            loop: true,
          });
        effect("music", "Главная тема", "background");
        break;
      case "stop":
        await this.audio.fade("background", 0, Number(a.duration || 2));
        await this.delay(1, token, event.runId);
        this.audio.stop("background");
        if (this.snapshot.effects.music)
          this.snapshot.effects.music.status = "stopped";
        break;
      case "sound":
        if (a.assetId) {
          this.snapshot.instances[event.runId].audioKeys.push(a.id);
          await this.audio.play(a.assetId, {
            key: a.id,
            volume: a.volume ?? 1,
            duck: a.duck !== false,
          });
          this.assert(token, event.runId);
          if (this.audio.get(a.id)?.error)
            throw new Error(this.audio.get(a.id).error);
        } else
          await this.delay(Number(a.duration || 1) * 1000, token, event.runId);
        break;
      case "duck":
        this.audio.duck(a.id, true);
        try {
          await this.delay(Number(a.duration || 2) * 1000, token, event.runId);
        } finally {
          this.audio.duck(a.id, false);
        }
        break;
      case "variable": {
        const v = String(a.value);
        this.snapshot.variables[a.target] = v.startsWith("+")
          ? Number(this.snapshot.variables[a.target] || 0) + Number(v)
          : v === "true"
            ? true
            : v === "false"
              ? false
              : v;
        break;
      }
      case "visibility":
        world.visible = { ...world.visible, [a.target]: a.value !== "Скрыть" };
        break;
      case "wait":
        if (a.waitFor)
          throw new Error(
            "Зависимость ожидает другое событие. Откройте диагностику связей.",
          );
        await this.delay(
          Math.min(Number(a.duration || 2), Number(a.executionTimeout || 30)) *
            1000,
          token,
          event.runId,
        );
        break;
    }
    await this.delay(0, token, event.runId);
    this.snapshot.states[a.id] =
      TYPES[a.type]?.completion === "CONTINUOUS" ? "held" : "done";
    this.emit();
  }
  async advance(choiceId, objectId) {
    if (!this.running || !this.snapshot.ready || this.snapshot.paused) return;
    const b = allBeats(this.project).find((b) => b.id === this.snapshot.beatId),
      token = this.generation;
    if (objectId && b.kind !== "gate") return;
    if (b.kind === "gate" && objectId !== b.signal) return;
    if (b.kind === "gate") {
      this.snapshot.variables[b.signal] = true;
      if (b.signal === "letter") this.snapshot.variables.letter = true;
    }
    const next = chooseNext(
      this.project,
      b.id,
      choiceId,
      this.snapshot.variables,
    );
    if (b.kind === "choice" && !next) return;
    this.snapshot.ready = false;
    this.emit();
    try {
      await this.phase(b, "AFTER", token);
      if (next) await this.enter(next.id, token);
      else {
        this.generation++;
        this.audio.stopAll();
        this.snapshot.effects = {};
        for (const i of Object.values(this.snapshot.instances))
          i.status = "stopped";
        this.snapshot.phase = "FINISHED";
        this.snapshot.ending = b.ending || "Конец истории";
        this.emit();
      }
    } catch (e) {
      this.handle(e);
    }
  }
  togglePause() {
    if (!this.running) return;
    this.snapshot.paused = !this.snapshot.paused;
    if (this.snapshot.paused) {
      this.pausedAudio = [...this.audio.tracks.values()]
        .filter((t) => t.status === "playing")
        .map((t) => t.key);
      this.pausedAudio.forEach((k) => this.audio.pause(k));
    } else this.pausedAudio?.forEach((k) => this.audio.resume(k));
    this.emit();
  }
  controlInstance(id, command) {
    const i = this.snapshot.instances[id];
    if (!i || i.status === "stopped") return;
    if (command === "pause") {
      i.resumeStatus = i.status;
      i.status = "paused";
    } else if (command === "resume") i.status = i.resumeStatus || "running";
    else i.status = "stopped";
    // A shared music output may already belong to a newer instance.
    i.audioKeys.forEach((k) => {
      const owner = Object.values(this.snapshot.effects).find(
        (e) => e.audioKey === k,
      );
      if (!owner || owner.runId === id) this.audio[command](k);
    });
    for (const [key, e] of Object.entries(this.snapshot.effects))
      if (e.runId === id) this.controlEffect(key, command);
    this.snapshot.states[i.bindingId] = i.status;
    this.emit();
  }
  controlEffect(key, command, value) {
    const e = this.snapshot.effects[key];
    if (!e) return;
    if (command === "set") {
      e.name = value;
      this.snapshot.world[key] = value;
      e.status = "held";
    } else {
      e.status =
        command === "pause"
          ? "paused"
          : command === "resume"
            ? "held"
            : "stopped";
      if (e.audioKey) this.audio[command](e.audioKey);
      if (key === "weather" && command === "stop")
        this.snapshot.world.weather = "Ясно";
      if (key === "time" && command === "stop")
        this.snapshot.world.time = "День";
    }
    this.emit();
  }
}
