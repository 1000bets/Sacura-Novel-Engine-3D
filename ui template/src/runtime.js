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
import {ANCHORS,objectTransform,resolvedPosition} from './sceneEditing.js';

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
  ownsAudio(key,runId){return this.audio.get?.(key)?.runId===runId;}
  async start(project, beatId) {
    this.audio.unlock?.().catch(()=>{});
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
      activity: [],audition:false,
      world: { positions: {}, poses: {}, visible: {} },
    };
    try {
      await this.enter(beatId, token);
    } catch (e) {
      this.handle(e);
    }
  }
  async previewEvent(project,eventId,beatId){
    this.audio.unlock?.().catch(()=>{});
    if(!this.running||!this.snapshot.audition||this.snapshot.world.location!==sceneFor(project,beatId).id){
      this.stop();this.running=true;this.project=structuredClone(project);const scene=sceneFor(project,beatId);
      this.snapshot={...this.snapshot,beatId,phase:'EVENT_PREVIEW',audition:true,paused:false,ready:false,textVisible:false,error:null,activity:[],variables:{...project.variables},world:{location:scene.id,weather:scene.weather,time:scene.time,camera:'Общий план',cameraId:null,positions:{},poses:{},visible:{},motions:{}}};
      for(const type of ['weather','time'])this.snapshot.effects[type]={key:type,type,name:scene[type],status:'held',owner:'SubScene',origin:scene.name};
    }else {this.project.events=structuredClone(project.events);}
    this.snapshot.auditionName=this.project.events.find(e=>e.id===eventId)?.name;this.snapshot.error=null;this.emit();
    const token=this.generation,b={id:'audition-'+eventId,eventId,hook:'ON_START',join:'FLOW_END',overrides:{},actionOverrides:{}};
    const candidate=structuredClone(this.project),beat=allBeats(candidate).find(x=>x.id===beatId);beat.bindings=[b];beat.batches={ON_START:[{id:'audition',mode:'SEQUENTIAL',bindingIds:[b.id]}]};
    try{const issue=validateStudio(candidate).find(i=>i.level==='error'&&(i.beatId===beatId||i.eventId===eventId));if(issue)throw new Error(issue.title);
      const claims=bindingActions(candidate,b).filter(a=>TYPES[a.type]?.domain).map(a=>a.target+'/'+TYPES[a.type].domain);
      const conflict=Object.values(this.snapshot.instances).find(i=>['running','paused'].includes(i.status)&&this.project.events.find(e=>e.id===i.eventId)?.groups.some(g=>g.actions.some(a=>claims.includes(a.target+'/'+TYPES[a.type]?.domain))));
      if(conflict){this.snapshot.error='«'+conflict.name+'» ещё выполняется. Дождитесь завершения или остановите его во вкладке «Активные».';this.emit();return;}
      await this.binding(b,token);}catch(e){this.handle(e);}
  }
  stop() {
    this.generation++;
    this.running = false;
    this.audio.stopAll();
    this.snapshot = {
      ...this.snapshot,
      phase: "EDIT",
      paused: false,
      audition:false,activity:[],
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
          i.audioKeys.forEach((k) => {if(this.ownsAudio(k,i.id))this.audio.stop(k);});
          i.status = "stopped";
        }
      for (const [key, e] of Object.entries(this.snapshot.effects))
        if (e.owner === "SubScene") {
          if(e.audioKey&&this.ownsAudio(e.audioKey,e.runId))this.audio.stop(e.audioKey);
          delete this.snapshot.effects[key];
        }
      this.snapshot.world = {
        ...this.snapshot.world,
        location: scene.id,
        weather: scene.weather,
        time: scene.time,
        camera: "Общий план",
        cameraId:null,
        positions: {},
        poses: {},
        visible: {},
        motions:{},lighting:null,particles:null,doors:{},highlights:{},pausedDoors:{},door:null,highlight:null,
      };
      for(const e of Object.values(this.snapshot.effects))if(['Scene','GameSession'].includes(e.owner)&&e.status!=='stopped'){
        if(e.type==='door')this.snapshot.world.doors[e.target]=e.name;
        else if(e.type==='highlight')this.snapshot.world.highlights[e.target]=e.name!=='Выключить';
        else if(['weather','time','lighting','particles'].includes(e.type))this.snapshot.world[e.type]=e.name;
      }
      for (const key of ["weather", "time"])
        if(!this.snapshot.effects[key]||this.snapshot.effects[key].status==='stopped')
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
    this.snapshot.hint=null;
    if(id==='a1'&&!this.snapshot.history.length&&!this.project.objects.find(o=>o.id==='alice')?.transforms?.[scene.id])this.snapshot.world.positions.alice='вход';
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
    const activity={id:event.runId+':'+a.id,runId:event.runId,actionId:a.id,type:a.type,target:a.target,value:a.value,name:event.name,status:'running',progress:0};
    this.snapshot.activity=[...(this.snapshot.activity||[]).slice(-11),activity];
    this.emit();
    const world = this.snapshot.world;
    const effect = (type, name, audioKey, target) => {
      const key=target?type+':'+target:type;
      this.snapshot.effects[key] = {
        key,
        runId: event.runId,
        type,
        target,
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
        {const duration=Math.max(.1,Number(a.duration||2))*1000;
        const previous=world.motions?.[a.target],anchors=ANCHORS[sceneFor(this.project,this.snapshot.beatId).kind||world.location]||ANCHORS.living;
        const movingObject=this.project.objects.find(o=>o.id===a.target);
        let from=world.positions?.[a.target]||(movingObject?objectTransform(movingObject,world.location,sceneFor(this.project,this.snapshot.beatId).kind).position:'стол');
        if(previous&&movingObject)from=resolvedPosition(movingObject,world,sceneFor(this.project,this.snapshot.beatId).kind);
        const motion={from,to:a.value,progress:0,runId:event.runId};
        world.motions={...world.motions,[a.target]:motion};
        for(let elapsed=0;elapsed<duration;elapsed+=50){await this.delay(Math.min(50,duration-elapsed),token,event.runId);motion.progress=Math.min(1,(elapsed+50)/duration);activity.progress=motion.progress;this.emit();}}
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
        world.cameraId = a.cameraId || null;
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
        if(this.audio.get('background'))this.audio.get('background').runId=event.runId;
        effect("music", a.value, "background");
        await this.audio.get('background')?.started;
        this.assert(token,event.runId);
        if(this.audio.get('background')?.error)throw new Error(this.audio.get('background').error);
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
        if(this.audio.get('background'))this.audio.get('background').runId=event.runId;
        this.snapshot.instances[event.runId].audioKeys.push('background');
        effect("music", "Главная тема", "background");
        break;
      case "stop": {
        const track=this.audio.get('background');
        await this.audio.fade("background", 0, Number(a.duration || 2));
        await this.delay(1, token, event.runId);
        if(this.audio.get('background')===track){this.audio.stop("background");
          if (this.snapshot.effects.music)this.snapshot.effects.music.status = "stopped";}
        break;
      }
      case "sound":
        if (a.assetId) {
          const key=event.runId+':'+a.id;
          this.snapshot.instances[event.runId].audioKeys.push(key);
          const done=this.audio.play(a.assetId, {
            key,
            volume: a.volume ?? 1,
            duck: a.duck !== false,
          });
          if(this.audio.get(key))this.audio.get(key).runId=event.runId;
          await done;
          this.assert(token, event.runId);
          if (this.audio.get(key)?.error)
            throw new Error(this.audio.get(key).error);
        } else
          await this.delay(Number(a.duration || 1) * 1000, token, event.runId);
        break;
      case "duck":
        this.audio.duck(event.runId+':'+a.id, true);
        try {
          await this.delay(Number(a.duration || 2) * 1000, token, event.runId);
        } finally {
          this.audio.duck(event.runId+':'+a.id, false);
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
      case 'lighting':world.lighting=a.value;effect('lighting',a.value);break;
      case 'particles':world.particles=a.value;effect('particles',a.value);break;
      case 'door':world.doors={...world.doors,[a.target]:a.value};world.pausedDoors={...world.pausedDoors,[a.target]:false};effect('door',a.value,null,a.target);break;
      case 'highlight':world.highlights={...world.highlights,[a.target]:a.value!=='Выключить'};effect('highlight',a.value,null,a.target);break;
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
    activity.status=this.snapshot.states[a.id];activity.progress=1;
    this.emit();
  }
  async advance(choiceId, objectId) {
    this.audio.unlock?.().catch(()=>{});
    if (!this.running || !this.snapshot.ready || this.snapshot.paused) return;
    const b = allBeats(this.project).find((b) => b.id === this.snapshot.beatId),
      token = this.generation;
    if (objectId && b.kind !== "gate") return;
    if (b.kind === "gate" && objectId !== b.signal) return;
    if (b.kind === "gate") {
      this.snapshot.variables[b.signal] = true;
      const item=this.project.objects.find(o=>o.id===b.signal);
      if ((item?.builtin||item?.id) === "letter") this.snapshot.variables.letter = true;
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
        .filter((t) => ['playing','loading'].includes(t.status));
      this.pausedAudio.forEach((t) => this.audio.pause(t.key));
    } else this.pausedAudio?.forEach((t) => {if(this.audio.get(t.key)===t&&t.status==='paused')this.audio.resume(t.key);});
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
    for(const m of Object.values(this.snapshot.world.motions||{}))if(m.runId===id){m.paused=command==='pause';if(command==='stop')m.stopped=true;}
    for(const a of this.snapshot.activity||[])if(a.runId===id&&a.status==='running'&&command==='stop')a.status='stopped';
    // A shared music output may already belong to a newer instance.
    i.audioKeys.forEach((k) => {
      if(this.ownsAudio(k,id))this.audio[command](k);
    });
    for (const [key, e] of Object.entries(this.snapshot.effects))
      if (e.runId === id) this.controlEffect(key, command);
    this.snapshot.states[i.bindingId] = i.status;
    this.emit();
  }
  controlEffect(key, command, value) {
    const e = this.snapshot.effects[key];
    if (!e) return;
    if(['door','highlight'].includes(e.type)){
      e.status=command==='pause'?'paused':command==='stop'?'stopped':'held';if(command==='set')e.name=value;
      if(e.type==='door')this.snapshot.world.pausedDoors={...this.snapshot.world.pausedDoors,[e.target]:command==='pause'};
      const field=e.type==='door'?'doors':'highlights';this.snapshot.world[field]||={};
      if(command!=='pause')this.snapshot.world[field][e.target]=command==='stop'?(e.type==='door'?null:false):(e.type==='door'?e.name:e.name!=='Выключить');
      this.emit();return;
    }
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
      if(command==='stop'&&['lighting','particles','door','highlight'].includes(key))this.snapshot.world[key]=null;
      if(command==='resume')this.snapshot.world[key]=e.name;
    }
    this.emit();
  }
}
