import {GameSession} from './GameSession.js';
import {gameSave,restoreGameSave} from './gameSave.js';
import {inputContextsFor} from './inputModel.js';
import {audioActionOptions} from './audioTransitions.js';
import {interactionTargets,playerControls} from './interactionModel.js';
import {navMeshSettings,collisionSettings,samplePath,pathSection} from './scenePhysics.js';
import {projectAudioAssets} from './audioAssets.js';
import {variableValue,runtimeVariableType} from './variableModel.js';
import {isPureNode,readLogic} from './logicModel.js';
import {choiceValue,ANSWER_VARIABLE,typedValue} from './choiceModel.js';
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
import {ANCHORS,isObjectInScene,objectTransform,resolvedPosition,sceneStagingPoints,findStagingPoint} from './sceneEditing.js';
import {normalizeSidechain} from './audioSettings.js';

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
  async closeEventAudio(instance){
    if(!instance)return;
    const commands=instance.endAudioCommands||[];instance.endAudioCommands=[];
    await Promise.all(commands.map(async request=>{
      if(!request.track||this.audio.get(request.key)!==request.track)return;
      await this.audio[request.command](request.key,{...request.options,endMode:'marker'});
      if(request.effect&&this.audio.get(request.key)===request.track&&this.snapshot.effects.music===request.effect){request.effect.status=request.command==='pause'?'paused':'stopped';this.emit();}
    }));
  }
  deferAudioEnd(event,command,options){
    const instance=this.snapshot.instances[event.runId];instance.endAudioCommands??=[];
    instance.endAudioCommands.push({key:'background',track:this.audio.get('background'),effect:this.snapshot.effects.music,command,options});
  }
  ownsAudio(key,runId){return this.audio.get?.(key)?.runId===runId;}
  setSidechain(settings){
    const sidechain=normalizeSidechain(settings);
    if(this.project)this.project.audioSettings={...this.project.audioSettings,sidechain};
    this.audio.setSidechain?.(sidechain);
  }
  invalidateStoryPreview(project) {
    if(!this.running || this.snapshot.audition)return false;
    // Playback owns a snapshot. Never show a newer route beside an older run.
    const routes=p=>JSON.stringify({
      input:p.input,gameplay:p.gameplay,functions:p.functions,
      scenes:p.subscenes.map(s=>[s.id,s.entry]),
      chapters:p.chapters.map(c=>[c.id,c.subsceneId,c.beats.map(b=>[
        b.id,b.kind,b.inputs,b.variable,b.value,b.operator,b.a,b.b,b.valueType,b.condition,b.next||null,b.signal||null,b.signals,b.controls,b.interactionEvents,b.inputContexts,b.trueNext,b.falseNext,b.test,b.resultVariable,
        (b.choices||[]).map(choice=>[choice.id,choice.next||null,choice.condition,choice.threshold,choice.availability,choice.enabledSource,choice.result]),
      ])]),
    });
    if(routes(project)===routes(this.project))return false;
    this.stop();
    return true;
  }
  async start(project, beatId) {
    this.audio.unlock?.().catch(()=>{});
    this.stop();
    this.project = structuredClone(project);
    this.audio.setAssets?.(projectAudioAssets(project));
    this.setSidechain(project.audioSettings?.sidechain);
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
      variables: structuredClone(project.variables),choiceResult:null,choiceResults:{},
      history: [],
      activity: [],audition:false,
      world: { positions: {}, poses: {}, visible: {} },
    };
    this.game=project.gameplay?.enabled?new GameSession(this):null;
    try {
      this.game?.initialize();
      await this.enter(beatId, token);
    } catch (e) {
      this.handle(e);
    }
  }
  async previewEvent(project,eventId,beatId){
    this.audio.unlock?.().catch(()=>{});
    if(!this.running||!this.snapshot.audition||this.snapshot.world.location!==sceneFor(project,beatId).id){
      this.stop();this.running=true;this.project=structuredClone(project);const scene=sceneFor(project,beatId);
      this.snapshot={...this.snapshot,beatId,phase:'EVENT_PREVIEW',audition:true,paused:false,ready:false,textVisible:false,error:null,activity:[],variables:{...project.variables},choiceResults:{},world:{location:scene.id,weather:scene.weather,time:scene.time,camera:'Общий план',cameraId:null,positions:{},poses:{},visible:{},motions:{},stagingPoints:sceneStagingPoints(scene,project.objects)}};
      for(const type of ['weather','time'])this.snapshot.effects[type]={key:type,type,name:scene[type],status:'held',owner:'SubScene',origin:scene.name};
    }else {this.project.events=structuredClone(project.events);}
    this.audio.setAssets?.(projectAudioAssets(project));
    this.setSidechain(project.audioSettings?.sidechain);
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
    this.resetInputValues();
    this.generation++;
    this.running = false;
    this.audio.stopAll();
    this.playerDestination=null;
    this.inputFlights=new Set();
    if(this.snapshot.world){this.snapshot.world.playerControl=null;for(const [id,motion]of Object.entries(this.snapshot.world.motions||{}))if(motion.player)delete this.snapshot.world.motions[id];}
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
  async enter(id, token, automaticDepth=0) {
    if(automaticDepth>100)throw new Error("Проверки образовали бесконечный цикл. Подключите выход к реплике или концовке.");
    this.assert(token);
    for(const instance of Object.values(this.snapshot.instances||{}))if(instance.bindingId?.startsWith('input:')&&instance.beatId!==id&&['running','paused'].includes(instance.status)){instance.status='stopped';this.closeEventAudio(instance);instance.audioKeys.forEach(key=>{if(this.ownsAudio(key,instance.id))this.audio.stop(key);});}
    const b = allBeats(this.project).find((b) => b.id === id);
    if (!b) throw new Error("Реплика назначения удалена");
    const scene = sceneFor(this.project, id),
      old = this.snapshot.world.location;
    const dialogueIndex=scene.id===old?(this.snapshot.world.dialogue?.index??-1)+1:0;
    if (scene.id !== old) {
      for (const i of Object.values(this.snapshot.instances || {}))
        if (i.owner === "SubScene" && i.sceneId !== scene.id) {
          i.audioKeys.forEach((k) => {if(this.ownsAudio(k,i.id))this.audio.stop(k);});
          this.closeEventAudio(i);i.status = "stopped";
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
        dialogue:null,cameraCueKey:null,
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
    this.snapshot.world.stagingPoints=sceneStagingPoints(scene,this.project.objects);
    this.playerDestination=null;
    this.snapshot.world.playerControl=null;
    this.snapshot.interacted=[];
    this.resetInputValues();this.snapshot.lastInput=null;this.snapshot.inputChoiceId=null;
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
      (i) => (i.beatId === id || (!i.beatId && ['input-','game-','function-','widget-'].some(prefix=>i.id?.startsWith(prefix)))) && i.level === "error",
    );
    if (errors.length) throw new Error(`Реплика «${id}»: `+errors.map((i) => i.title).join(" · "));
    await this.phase(b, "BEFORE", token);
    if(await this.game?.flushTransition())return;
    if(isPureNode(b))throw new Error('Эта нода вычисляет значение. Запустите реплику или If.');
    if(b.kind==='branch'||b.kind==='set-variable'){
      await this.phase(b,'ON_START',token);await this.phase(b,'AFTER',token);this.assert(token);
      if(b.kind==='set-variable')this.snapshot.variables[b.variable]=variableValue(this.project,b.variable,b.inputs?.value?readLogic(this.project,b.inputs.value,this.snapshot.variables,new Set(),this.snapshot.choiceResults):b.value);
      const next=chooseNext(this.project,b.id,null,this.snapshot.variables,this.snapshot.choiceResults);
      if(!next)throw new Error('У проверки не подключён выбранный выход. Соедините «Да» и «Нет» на графе.');
      await this.enter(next.id,token,automaticDepth+1);return;
    }
    this.snapshot.world.dialogue={
      key:token+':'+this.snapshot.history.length,beatId:id,index:dialogueIndex,kind:b.kind,
      speakerId:this.project.objects.find(o=>o.type==='Персонаж'&&o.name===b.speaker&&isObjectInScene(o,scene))?.id||null,
    };
    this.snapshot.textVisible = true;
    this.snapshot.phase = "ON_START";
    this.emit();
    await this.phase(b, "ON_START", token);
    this.assert(token);if(await this.game?.flushTransition())return;
    this.snapshot.phase =
      b.kind === "gameplay" ? "PLAYING" : b.kind === "gate"
        ? "WAITING_OBJECT"
        : b.kind === "choice"
          ? "WAITING_CHOICE"
          : b.kind === "end"
            ? "ENDING"
            : "WAITING_INPUT";
    this.snapshot.ready = true;
    this.snapshot.gateElapsed = 0;
    const controls=playerControls(b);
    if(b.kind==='gate'&&controls.characterId&&controls.mode!=='none')this.snapshot.world.playerControl=controls;
    if(this.game){this.game.state.active=b.kind==='gameplay';if(b.kind==='gameplay')this.game.enter();}
    if(b.kind==='choice')this.snapshot.inputChoiceId=b.choices.find(c=>conditionPass(c,this.snapshot.variables,this.project,this.snapshot.choiceResults))?.id||null;
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
              'Осталось осмотреть: '+interactionTargets(b).filter(id=>!this.snapshot.interacted.includes(id)).map(id=>this.project.objects.find(o=>o.id===id)?.name||id).join(', ')+'.';
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
            originShotKey: token+':'+this.snapshot.history.length,
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
      if(event.retention==='AUTO_CLOSE_ON_FLOW_END')await this.closeEventAudio(this.snapshot.instances[event.runId]);
      if(event.retention==='AUTO_CLOSE_ON_FLOW_END')await Promise.all(this.snapshot.instances[event.runId].audioKeys.map(key=>{const track=this.audio.get?.(key);if(this.ownsAudio(key,event.runId)&&track.endMode==='event')return this.audio.stop(key,{endMode:'marker',fadeOut:track.fadeOut});}));
      this.assert(token,event.runId);
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
    if(this.snapshot.ready)await this.game?.flushTransition();
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
        const scene=sceneFor(this.project,this.snapshot.beatId),previous=world.motions?.[a.target],anchors=ANCHORS[scene.kind||world.location]||ANCHORS.living;
        world.stagingPoints=sceneStagingPoints(scene,this.project.objects);
        if(!findStagingPoint(world.stagingPoints,a.value)&&!anchors[a.value]&&!(Array.isArray(a.value)&&a.value.length===3&&a.value.every(Number.isFinite)))throw new Error('Точка постановки отсутствует в этой сабсцене. Выберите доступную точку в параметрах движения.');
        const movingObject=this.project.objects.find(o=>o.id===a.target);
        let from=world.positions?.[a.target]||(movingObject?objectTransform(movingObject,world.location,sceneFor(this.project,this.snapshot.beatId).kind).position:'стол');
        if(previous&&movingObject)from=resolvedPosition(movingObject,world,sceneFor(this.project,this.snapshot.beatId).kind);
        const motion={from,to:a.value,progress:0,runId:event.runId};
        motion.animationId=a.animationId;
        const destination=resolvedPosition(movingObject,{...world,motions:{},positions:{...world.positions,[a.target]:a.value}},scene.kind);
        const physics=this.physicsApi?.current;
        const needsPhysics=(navMeshSettings(scene).enabled&&movingObject.type==='Персонаж')||collisionSettings(movingObject).enabled;
        if(needsPhysics){
          if(!physics||physics.sceneId()!==scene.id)throw new Error('Дождитесь загрузки 3D-сцены перед движением с nav mesh или коллизиями.');
          const start=resolvedPosition(movingObject,{...world,motions:{},positions:{...world.positions,[a.target]:from}},scene.kind);
          motion.path=physics.planMotion(movingObject,start,destination);motion.position=samplePath(motion.path,0);
        }
        world.motions={...world.motions,[a.target]:motion};
        const footstepKey=a.footstepAssetId?event.runId+':'+a.id+':footsteps':null;
        try{
        if(footstepKey){
          this.snapshot.instances[event.runId].audioKeys.push(footstepKey);
          const playback=this.audio.play(a.footstepAssetId,{key:footstepKey,volume:a.footstepVolume??1,loop:true,duck:false});
          if(this.audio.get(footstepKey))this.audio.get(footstepKey).runId=event.runId;
          Promise.resolve(playback).catch(()=>{});
        }
        for(let elapsed=0;elapsed<duration;elapsed+=50){await this.delay(Math.min(50,duration-elapsed),token,event.runId);const progress=Math.min(1,(elapsed+50)/duration);
          if(motion.path){
            const section=pathSection(motion.path,motion.progress,progress);
            try{for(let i=1;i<section.length;i++)physics.validateStep(movingObject,section[i-1],section[i]);}
            catch(error){motion.stopped=true;throw error;}
            motion.position=section.at(-1);
          }
          motion.progress=progress;activity.progress=motion.progress;this.emit();}
        if (
          event.owner === "SubScene" &&
          event.originScene !== this.snapshot.world.location
        )
          throw new Error("CANCELLED");
        world.positions = { ...world.positions, [a.target]: world.motions[a.target]?.position||a.value };
        }finally{if(footstepKey)this.audio.stop(footstepKey);}}
        break;
      case "pose":
        world.poses = { ...world.poses, [a.target]: a.value };
        break;
      case "camera":
        world.camera = a.value;
        world.cameraId = a.cameraId || null;
        world.cameraCueKey = event.originShotKey;
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
        if(this.audio.get('background')){await this.audio.stop('background',audioActionOptions(a));this.assert(token,event.runId);}
        this.snapshot.instances[event.runId].audioKeys.push("background");
        this.audio.play(a.assetId || "music-main", {
          key: "background",
          volume: a.volume ?? 0.42,
          loop: a.loop !== false,
          ...audioActionOptions(a),
        });
        if(this.audio.get('background'))this.audio.get('background').runId=event.runId;
        effect("music", a.value, "background");
        await this.audio.get('background')?.started;
        this.assert(token,event.runId);
        if(this.audio.get('background')?.error)throw new Error(this.audio.get('background').error);
        break;
      case "pause": {
        if(a.endMode==='event'){this.deferAudioEnd(event,'pause',audioActionOptions(a));break;}
        const track=this.audio.get('background');
        await this.audio.pause("background",audioActionOptions(a));
        this.assert(token,event.runId);
        if (this.audio.get('background')===track&&this.snapshot.effects.music)
          this.snapshot.effects.music.status = "paused";
        break;
      }
      case "resume":
        if (this.audio.get("background")) await this.audio.resume("background",audioActionOptions(a));
        else
          this.audio.play("music-main", {
            key: "background",
            volume: 0.42,
            loop: true,
            ...audioActionOptions(a),
          });
        if(this.audio.get('background'))this.audio.get('background').runId=event.runId;
        this.snapshot.instances[event.runId].audioKeys.push('background');
        effect("music", "Главная тема", "background");
        break;
      case "stop": {
        if(a.endMode==='event'){this.deferAudioEnd(event,'stop',audioActionOptions(a));break;}
        const track=this.audio.get('background');
        await this.audio.stop("background",audioActionOptions(a));
        this.assert(token,event.runId);
        if(this.audio.get('background')===track){
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
            ...audioActionOptions(a),
          });
          if(this.audio.get(key))this.audio.get(key).runId=event.runId;
          if(a.endMode==='event'){await this.audio.get(key)?.started;done.catch(e=>this.handle(e));}else await done;
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
        if(a.operation){const value=typedValue(a.value,(this.project.variableTypes?.[a.target]?runtimeVariableType(this.project,a.target):a.valueType)||typeof this.snapshot.variables[a.target]);this.snapshot.variables[a.target]=a.operation==='add'?Number(this.snapshot.variables[a.target]||0)+Number(value):value;if(this.project.variableTypes?.[a.target])this.snapshot.variables[a.target]=variableValue(this.project,a.target,this.snapshot.variables[a.target]);break;}
        const v = String(a.value);
        this.snapshot.variables[a.target] = v.startsWith("+")
          ? Number(this.snapshot.variables[a.target] || 0) + Number(v)
          : v === "true"
            ? true
            : v === "false"
              ? false
              : v;
        if(this.project.variableTypes?.[a.target])this.snapshot.variables[a.target]=variableValue(this.project,a.target,this.snapshot.variables[a.target]);
        break;
      }
      case 'gameplay':
        if(!this.game)throw new Error('Включите игровые механики проекта.');
        await this.game.command(a.command,{...a.value,...(a.target!=='world'?{target:a.target}:{})});break;
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
  saveGame(){const storage=this.storage||globalThis.localStorage;if(!storage)throw new Error('Хранилище сохранений недоступно.');storage.setItem('sacura-save:'+this.project.id,JSON.stringify(gameSave(this)));this.snapshot.hint='Прохождение сохранено';this.emit();}
  async loadGame(){const storage=this.storage||globalThis.localStorage,raw=storage?.getItem('sacura-save:'+this.project.id);if(!raw)throw new Error('Сохранение ещё не создано.');await restoreGameSave(this,JSON.parse(raw));this.snapshot.hint='Прохождение загружено';this.emit();}
  async uiAction(action,element={}){try{if(action==='event'&&element.eventId)await this.binding({id:'input:widget:'+element.id,eventId:element.eventId,join:'FLOW_END',overrides:{},actionOverrides:{}},this.generation);else if(action==='save')this.saveGame();else if(action==='load')await this.loadGame();else if(action==='gameplay'&&this.game)await this.game.command(element.command,element.value||{});await this.game?.flushTransition();this.emit();}catch(e){this.snapshot.hint=e.message;this.emit();}}
  writeInputValue(action,value){
    this.snapshot.inputValues??={};const changed=JSON.stringify(this.snapshot.inputValues[action.id])!==JSON.stringify(value);this.snapshot.inputValues[action.id]=value;
    const outputs=action.valueType==='axis2d'?[[action.variableX,value?.[0]||0],[action.variableY,value?.[1]||0]]:[[action.variable,action.valueType==='boolean'?Boolean(value):value||0]];
    for(const [id,v]of outputs)if(id&&Object.hasOwn(this.project.variables,id))this.snapshot.variables[id]=variableValue(this.project,id,v);
    return changed;
  }
  resetInputValues(){
    for(const id of Object.keys(this.snapshot.inputValues||{})){const action=this.project?.input?.actions.find(a=>a.id===id);if(action)this.writeInputValue(action,action.valueType==='axis2d'?[0,0]:0);}
    this.snapshot.inputValues={};
  }
  async inputAction({actionId,phase,value,payload,device}){
    if(!this.running||this.snapshot.audition)return;
    const action=this.project.input?.actions.find(a=>a.id===actionId);if(!action)return;
    if(['completed','canceled'].includes(phase)){this.writeInputValue(action,action.valueType==='axis2d'?[0,0]:0);this.snapshot.lastInput={actionId,name:action.name,phase,value,device};this.emit();return;}
    const beat=allBeats(this.project).find(b=>b.id===this.snapshot.beatId);
    const contexts=this.snapshot.paused||!this.snapshot.ready?['system']:inputContextsFor(this.project,beat);
    if(!this.project.input.contexts.some(c=>contexts.includes(c.id)&&c.bindings.some(b=>b.actionId===actionId)))return;
    const changed=this.writeInputValue(action,value);
    this.snapshot.lastInput={actionId,name:action.name,phase,value,device};
    if(changed||phase==='started')this.emit();
    if(phase!=='triggered')return;
    if(action.behavior==='pause'){this.togglePause();return;}
    if(this.snapshot.paused||!this.snapshot.ready)return;
    if(this.game&&['shoot','reload','jump','look','saveGame','loadGame'].includes(action.behavior)){try{if(action.behavior==='look')this.game.look(payload?.delta||value,device);else await this.game.command(({shoot:'shoot',saveGame:'save',loadGame:'load'})[action.behavior]||action.behavior,payload||{});await this.game.flushTransition();this.emit();}catch(e){this.game.error(e);}return;}
    if(action.behavior==='move')return; // The scene adapter converts 2D input to camera-relative movement.
    if(action.behavior==='interact'){if(this.snapshot.world.playerControl)this.playerInteract();else await this.advance(null,interactionTargets(beat).find(id=>!this.snapshot.interacted?.includes(id)));return;}
    if(action.behavior==='point'){if(this.snapshot.world.playerControl)this.playerClick(payload?.point,payload?.id);else await this.advance(null,payload?.id);return;}
    if(action.behavior==='advance'){
      const choices=beat?.choices?.filter(c=>conditionPass(c,this.snapshot.variables,this.project,this.snapshot.choiceResults));
      await this.advance(beat?.kind==='choice'?(this.snapshot.inputChoiceId||choices?.[0]?.id):undefined);return;
    }
    if(['choiceNext','choicePrevious'].includes(action.behavior)&&beat?.kind==='choice'){
      const choices=beat.choices.filter(c=>conditionPass(c,this.snapshot.variables,this.project,this.snapshot.choiceResults));if(!choices.length)return;
      const index=Math.max(0,choices.findIndex(c=>c.id===this.snapshot.inputChoiceId));
      this.snapshot.inputChoiceId=choices[(index+(action.behavior==='choiceNext'?1:choices.length-1))%choices.length].id;this.emit();return;
    }
    if(action.behavior==='event'&&action.eventId){
      const token=this.generation,key=token+':'+this.snapshot.history.length+':'+actionId;
      this.inputFlights??=new Set();if(this.inputFlights.has(key))return;this.inputFlights.add(key);
      try{await this.binding({id:'input:'+key,eventId:action.eventId,join:'FLOW_END',overrides:{},actionOverrides:{}},token);}catch(e){this.handle(e);}finally{this.inputFlights.delete(key);}
    }
  }
  playerContext(){
    if(!this.running||this.snapshot.paused||!this.snapshot.ready||this.snapshot.phase!=='WAITING_OBJECT')return null;
    const control=this.snapshot.world.playerControl;if(!control)return null;
    const scene=sceneFor(this.project,this.snapshot.beatId),object=this.project.objects.find(o=>o.id===control.characterId);
    if(!object||object.active===false||!isObjectInScene(object,scene)||this.snapshot.world.visible?.[object.id]===false)return null;
    return {control,scene,object,world:this.snapshot.world,beat:allBeats(this.project).find(b=>b.id===this.snapshot.beatId)};
  }
  playerClick(point,id){
    if(this.snapshot.phase==='PLAYING'&&this.game){if(id&&this.game.interact(id))return;if(point&&['point-click','both'].includes(this.game.config.controller.mode))this.game.setDestination(point);return;}
    const context=this.playerContext();if(!context)return;
    const {control,world,scene,object,beat}=context;
    const target=interactionTargets(beat).includes(id)&&!this.snapshot.interacted.includes(id)?this.project.objects.find(o=>o.id===id):null;
    if(target){
      const to=resolvedPosition(target,world,scene.kind),from=resolvedPosition(object,world,scene.kind);
      if(!control.radius||Math.hypot(to[0]-from[0],to[2]-from[2])<=control.radius){this.advance(null,id);return;}
      if(!['both','point-click'].includes(control.mode)){this.snapshot.hint='Подойдите ближе и выполните действие «Осмотреть» или нажмите на предмет.';this.emit();return;}
      point=[to[0],from[1],to[2]];
      // Stop within reach rather than trying to enter the prop collider.
      const distance=Math.hypot(point[0]-from[0],point[2]-from[2]),ratio=Math.max(0,(distance-control.radius*.85)/distance);
      point=from.map((v,i)=>i===1?v:v+(point[i]-v)*ratio);
    }
    if(!point||!['both','point-click'].includes(control.mode))return;
    const from=resolvedPosition(object,world,scene.kind),physics=this.physicsApi?.current;
    try{const path=physics?.sceneId()===scene.id?physics.planMotion(object,from,[point[0],from[1],point[2]]):navMeshSettings(scene).enabled?null:[from,[point[0],from[1],point[2]]];
      if(!path)throw new Error('Дождитесь загрузки области ходьбы.');this.playerDestination={path,distance:path.slice(1).reduce((sum,p,i)=>sum+Math.hypot(...p.map((v,j)=>v-path[i][j])),0),progress:0,targetId:target?.id};this.snapshot.hint=null;
    }catch(e){this.snapshot.hint=e.message;this.emit();}
  }
  playerInteract(){
    if(this.snapshot.phase==='PLAYING'&&this.game){const p=this.game.position(this.game.config.playerId),nearest=this.game.config.items.map(i=>i.worldObjectId).filter(id=>this.game.position(id)&&this.game.world.visible[id]!==false).sort((a,b)=>Math.hypot(...this.game.position(a).map((v,i)=>v-p[i]))-Math.hypot(...this.game.position(b).map((v,i)=>v-p[i])));if(nearest[0])this.game.interact(nearest[0]);return;}
    const c=this.playerContext();if(!c)return;const from=resolvedPosition(c.object,c.world,c.scene.kind);
    const nearest=interactionTargets(c.beat).filter(id=>!this.snapshot.interacted.includes(id)).map(id=>this.project.objects.find(o=>o.id===id)).filter(o=>o&&o.active!==false&&c.world.visible?.[o.id]!==false).map(o=>({o,p:resolvedPosition(o,c.world,c.scene.kind)})).sort((a,b)=>Math.hypot(a.p[0]-from[0],a.p[2]-from[2])-Math.hypot(b.p[0]-from[0],b.p[2]-from[2]));
    if(nearest[0]){
      if(c.control.radius&&Math.hypot(nearest[0].p[0]-from[0],nearest[0].p[2]-from[2])>c.control.radius){this.snapshot.hint='Подойдите ближе к предмету с помощью управления движением или щелчком по сцене.';this.emit();return;}
      this.playerClick(null,nearest[0].o.id);
    }
  }
  playerStep(direction,dt){
    if(this.game&&this.snapshot.phase==='PLAYING'){this.game.step(direction,dt);return;}
    const c=this.playerContext();if(!c)return;const {object,world,scene,control}=c,from=resolvedPosition(object,world,scene.kind),manual=direction.some(v=>Math.abs(v)>.001);
    if(manual)this.playerDestination=null;
    const route=this.playerDestination;let to=from;
    const step=Math.max(0,Math.min(.05,dt))*control.speed;
    if(manual)to=[from[0]+direction[0]*step,from[1],from[2]+direction[2]*step];
    else if(route){route.progress=Math.min(1,route.progress+step/Math.max(.001,route.distance));to=samplePath(route.path,route.progress);}
    else {if(world.motions?.[object.id]?.player){delete world.motions[object.id];this.emit();}return;}
    try{
      const nav=navMeshSettings(scene);if(nav.enabled&&(Math.abs(to[0]-nav.center[0])>nav.size[0]/2||Math.abs(to[2]-nav.center[2])>nav.size[1]/2))throw new Error('Край области ходьбы');
      const physics=this.physicsApi?.current;if((nav.enabled||collisionSettings(object).enabled)&&physics?.sceneId()!==scene.id)return;
      physics?.validateStep(object,from,to);
      world.positions[object.id]=to;world.motions||={};world.motions[object.id]={player:true,position:to,progress:0};this.snapshot.hint=null;
      if(route?.progress===1){this.playerDestination=null;delete world.motions[object.id];if(route.targetId)this.advance(null,route.targetId);}
      this.emit();
    }catch(e){this.playerDestination=null;delete world.motions?.[object.id];this.snapshot.hint=e.message;this.emit();}
  }
  async advance(choiceId, objectId) {
    this.audio.unlock?.().catch(()=>{});
    if (!this.running || !this.snapshot.ready || this.snapshot.paused) return;
    const b = allBeats(this.project).find((b) => b.id === this.snapshot.beatId),
      token = this.generation;
    if(b.kind==='gameplay')return;
    if (objectId && b.kind !== "gate") return;
    if (b.kind === "gate" && (!interactionTargets(b).includes(objectId)||this.snapshot.interacted.includes(objectId))) return;
    if (b.kind === "gate") {
      const item=this.project.objects.find(o=>o.id===objectId),scene=sceneFor(this.project,b.id);
      if(!item||item.active===false||!isObjectInScene(item,scene)||this.snapshot.world.visible?.[item.id]===false)return;
      const control=this.snapshot.world.playerControl,character=control&&this.project.objects.find(o=>o.id===control.characterId);
      if(control?.radius&&character){const from=resolvedPosition(character,this.snapshot.world,scene.kind),to=resolvedPosition(item,this.snapshot.world,scene.kind);if(Math.hypot(to[0]-from[0],to[2]-from[2])>control.radius)return;}
      this.snapshot.ready=false;
      this.playerDestination=null;
      if(this.snapshot.world.playerControl)delete this.snapshot.world.motions?.[this.snapshot.world.playerControl.characterId];
      try {
        const eventId=b.interactionEvents?.[objectId];
        if(eventId)await this.binding({id:'interact:'+b.id+':'+objectId,eventId,join:'FLOW_END',overrides:{},actionOverrides:{}},token);
        this.assert(token);
      }catch(e){this.handle(e);return;}
      this.snapshot.interacted.push(objectId);
      this.snapshot.variables[objectId]=true;
      if ((item.builtin||item.id) === "letter") this.snapshot.variables.letter = true;
      if(!interactionTargets(b).every(id=>this.snapshot.interacted.includes(id))){this.snapshot.ready=true;this.snapshot.hint=null;this.emit();return;}
      this.snapshot.world.playerControl=null;
    }
    let selectedChoice;
    if(b.kind==='choice'){
      selectedChoice=b.choices?.find(c=>c.id===choiceId);
      if(!selectedChoice||!conditionPass(selectedChoice,this.snapshot.variables,this.project,this.snapshot.choiceResults))return;
    }
    const next = chooseNext(
      this.project,
      b.id,
      choiceId,
      this.snapshot.variables,
      this.snapshot.choiceResults,
    );
    if (b.kind === "choice" && !next) return;
    if (!next && b.kind !== 'end') {
      this.handle(new Error(b.next
        ? `Переход из реплики «${b.id}» ведёт в недоступную реплику «${b.next}». Переподключите выход «Дальше».`
        : `У реплики «${b.id}» не подключён выход «Дальше». Соедините его со следующей репликой или выберите тип «Концовка».`));
      return;
    }
    if(selectedChoice){
      this.snapshot.choiceResults[b.id]={choiceId:selectedChoice.id,type:selectedChoice.result?.type||"string",value:choiceValue(selectedChoice)};
      if(selectedChoice.result?.type==='none'){this.snapshot.choiceResult=null;delete this.snapshot.variables[ANSWER_VARIABLE];}
      else {
      const value=choiceValue(selectedChoice);
      this.snapshot.choiceResult={beatId:b.id,choiceId:selectedChoice.id,type:selectedChoice.result?.type||'string',value};
      this.snapshot.variables[ANSWER_VARIABLE]=value;
      if(b.resultVariable?.trim())this.snapshot.variables[b.resultVariable.trim()]=this.project.variableTypes?.[b.resultVariable.trim()]?variableValue(this.project,b.resultVariable.trim(),value):value;
      }
    }
    this.snapshot.ready = false;
    this.emit();
    try {
      await this.phase(b, "AFTER", token);
      if(await this.game?.flushTransition())return;
      if (next) await this.enter(next.id, token);
      else {
        this.generation++;
        this.audio.stopAll();
        this.snapshot.effects = {};
        for (const i of Object.values(this.snapshot.instances)) {
          this.closeEventAudio(i);
          i.status = "stopped";
        }
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
      this.resetInputValues();
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
    else {i.status = "stopped";this.closeEventAudio(i);}
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
      if (e.audioKey){
        const track=this.audio.get(e.audioKey),result=this.audio[command](e.audioKey);
        if(command==='stop'&&track?.pending){
          e.status='stopping';Promise.resolve(result).then(()=>{if(this.snapshot.effects[key]===e&&this.audio.get(e.audioKey)===track){e.status='stopped';this.emit();}});
        }
      }
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
