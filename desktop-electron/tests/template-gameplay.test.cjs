const {test:Test} = require('node:test');
const Assert = require('node:assert/strict');
const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const {spawn:Spawn} = require('node:child_process');
const {setTimeout:Delay} = require('node:timers/promises');
const {BridgeTransport} = require('../bridge/transport.cjs');
const Repository = Path.resolve(__dirname, '../..');
const Host = process.env.SACURA_NATIVE_HOST || Path.join(Repository, 'build/ElectronNative/Debug/SakuraElectronHost.exe');

async function RunProcess(Arguments) {
  const Process = Spawn(Host, Arguments, {windowsHide:true});
  let Output = '';
  Process.stdout.on('data', Chunk => {Output += Chunk;});
  Process.stderr.on('data', Chunk => {Output += Chunk;});
  const Code = await new Promise((Resolve, Reject) => {Process.once('error',Reject);Process.once('exit',Resolve);});
  Assert.equal(Code,0,Output);
}

async function WithTemplate(Scenario, Verify) {
  const Directory = await FileSystem.mkdtemp(Path.join(Repository, 'build/gameplay-' + Scenario + '-'));
  let Process;
  let Transport;
  let Output = '';
  try {
    await RunProcess(['--project-create', Directory, 'Game', 'builtin-' + Scenario]);
    const Project = Path.join(Directory,'Game/Game.project');
    Transport = await BridgeTransport.listen();
    Process = Spawn(Host, ['--bridge-headless',Project,Transport.path], {windowsHide:true,stdio:['pipe','pipe','pipe']});
    Process.stdout.on('data', Chunk => {Output += Chunk;});
    Process.stderr.on('data', Chunk => {Output += Chunk;});
    const Deadline = Date.now() + 20000;
    while (!Transport.connected && Date.now() < Deadline) {
      await Delay(20);
    }
    Assert.ok(Transport.connected,Output);
    const Command = async (Method, Parameters={}) => {
      const Result = await Transport.request(Method,Parameters);
      Assert.equal(Result.ok,true,Method + ': ' + JSON.stringify(Result) + '\n' + Output.slice(-5000));
      return Result.value;
    };
    await Command('play.start');
    const State = await Command('bridge.describe');
    Assert.equal(State.gameViews.length,1,Output);
    const Action = async Identifier => {
      const View = (await Command('bridge.describe')).gameViews[0];
      return Command('game.action',{objectId:View.objectId,componentId:View.componentId,action:Identifier});
    };
    await Verify(Command,Action,Project,Event => Process.stdin.write(JSON.stringify({type:'input',event:Event}) + '\n'));
    await Command('play.stop');
    Assert.equal((await Command('bridge.describe')).gameViews.length,0);
    await Command('play.start');
    Assert.equal((await Command('bridge.describe')).gameViews.length,1);
    Assert.ok(!Output.includes('Script lifecycle failed'),Output);
  } finally {
    if (Process) {
      Process.stdin.end('{"type":"shutdown"}\n');
      const Timeout = setTimeout(() => Process.kill(),5000);
      await new Promise(Resolve => Process.once('exit',Resolve));
      clearTimeout(Timeout);
    }
    Transport?.close();
  }
}

async function Tap(Input,Code) {
  Input({kind:'key',code:Code,down:true});
  await Delay(60);
  Input({kind:'key',code:Code,down:false});
  await Delay(60);
}

Test('Quest: original HUD, persistent key, independent unlock and rotation', {timeout:40000},async()=>{
  await WithTemplate('quest',async(Command,Action)=>{
    await Action('door');
    Assert.equal((await Command('bridge.describe')).gameViews[0].text,'Заперто');
    await Action('pickup');
    await Action('door');
    await Delay(600);
    await Command('play.advanceDialogue');
    await Delay(120);
    Assert.match((await Command('bridge.describe')).dialogue.text,/разблокировал/);
    await Action('warehouse');
    await Delay(600);
    const Objects=await Command('scene.getHierarchy');
    const Door=Objects.find(Entry=>Entry.transform.position[0]===2&&Entry.transform.position[1]===1);
    Assert.ok(Door);
    Assert.ok(Math.abs(Door.transform.rotation[1])>0.5,JSON.stringify(Door));
    const View=(await Command('bridge.describe')).gameViews[0];
    Assert.equal(View.values[0].value,1);
    Assert.equal(View.widget.elements.filter(Entry=>Entry.type==='button').length,3);
  });
});

Test('Quest: pointer coordinates survive pause and select visible geometry', {timeout:40000},async()=>{
  await WithTemplate('quest',async(Command,Action,Project,Input)=>{
    const Oracle=JSON.parse(await FileSystem.readFile(Path.join(Repository,'Root/Verification/WebTemplateOracle/quest.json'),'utf8'));
    const Camera=Oracle.scenes[0].cameras[0];
    const Difference=Camera.target.map((Value,Axis)=>Value-Camera.position[Axis]);
    const Pitch=Math.atan2(Difference[1],Math.hypot(Difference[0],Difference[2]));
    const Point=Identifier=>{
      const Target=Oracle.objects.find(Entry=>Entry.id===Identifier).transforms[Oracle.scenes[0].id].position;
      const Offset=Target.map((Value,Axis)=>Value-Camera.position[Axis]);
      const Depth=Offset[1]*Math.sin(Pitch)-Offset[2]*Math.cos(Pitch);
      return [Offset[0]/Depth/Math.tan(Camera.fov*Math.PI/360),(Offset[1]*Math.cos(Pitch)+Offset[2]*Math.sin(Pitch))/Depth/Math.tan(Camera.fov*Math.PI/360),0];
    };
    const FeedPoint=Identifier=>Input({kind:'gameControl',device:'Mouse',control:'Position',value:Point(Identifier),source:'Test'});
    const Click=async()=>{
      Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[1,0,0],source:'Test'});
      await Delay(60);
      Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[0,0,0],source:'Test'});
      await Delay(120);
    };
    FeedPoint('quest-door');
    await Delay(80);
    await Command('play.pause');
    await Command('play.start');
    await Click();
    let View=(await Command('bridge.describe')).gameViews[0];
    Assert.equal(View.text,'Заперто');
    Assert.equal(View.values[0].value,0);
    FeedPoint('quest-key');
    await Click();
    View=(await Command('bridge.describe')).gameViews[0];
    Assert.equal(View.values[0].value,1);
  });
});

Test('Novel: source choices, independent snapshot and save restoration', {timeout:40000},async()=>{
  await WithTemplate('novel',async(Command,Action,Project)=>{
    Assert.equal((await Command('bridge.describe')).dialogue.speaker,'Рассказчик');
    await Action('load');
    Assert.equal((await Command('bridge.describe')).gameViews[0].text,'Сохранение не найдено.');
    await Command('play.selectChoice',{index:0});
    await Delay(120);
    await Action('save');
    const SavePath=Path.join(Path.dirname(Project),'Saved/Gameplay.json');
    const Saved=await FileSystem.readFile(SavePath,'utf8');
    await FileSystem.writeFile(SavePath,'{broken');
    await Action('load');
    Assert.equal((await Command('bridge.describe')).gameViews[0].storageStatus,'');
    Assert.deepEqual((await Command('bridge.describe')).gameViews[0].values.map(Entry=>Entry.value),[1,1]);
    await FileSystem.writeFile(SavePath,Saved);
    await Command('play.advanceDialogue');
    await Delay(120);
    await Command('play.selectChoice',{index:0});
    await Delay(120);
    Assert.deepEqual((await Command('bridge.describe')).gameViews[0].values.map(Entry=>Entry.value),[2,1]);
    await Action('load');
    await Delay(120);
    Assert.deepEqual((await Command('bridge.describe')).gameViews[0].values.map(Entry=>Entry.value),[1,1]);
    Assert.match((await Command('bridge.describe')).dialogue.text,/Спасибо за поддержку/);
    await Action('restart');
    await Delay(120);
    await Command('play.selectChoice',{index:1});
    await Delay(120);
    const ColdState=await Command('bridge.describe');
    Assert.ok(ColdState.gameViews[0].dialogueChoices,JSON.stringify(ColdState));
    Assert.equal(ColdState.gameViews[0].dialogueChoices[0].enabled,false);
    await Command('play.selectChoice',{index:0});
    Assert.equal((await Command('bridge.describe')).dialogue.text,'Разговор окончен.');
    await Command('play.advanceDialogue');
    Assert.equal((await Command('bridge.describe')).dialogue.phase,'end');
    Assert.equal((await Command('bridge.describe')).gameViews[0].ending,'Конец');
    await Action('restart');
    await Delay(120);
    await Command('play.selectChoice',{index:0});
    await Delay(120);
    Assert.equal((await Command('bridge.describe')).dialogue.phase,'line');
  });
});

Test('Shooter: source timing, weapons outside battle, input, reload and pause', {timeout:40000},async()=>{
  await WithTemplate('shooter',async(Command,Action,Project,Input)=>{
    let State=await Command('bridge.describe');
    Assert.equal(State.gameViews[0].values.find(Entry=>Entry.label==='Аптечки').value,1);
    await Tap(Input,'Digit2');
    Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[1,0,0],source:'Test'});
    await Delay(80);
    Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[0,0,0],source:'Test'});
    Assert.equal((await Command('bridge.describe')).gameViews[0].values[1].value,'1/8');
    await Command('play.advanceDialogue');
    await Delay(120);
    Assert.equal((await Command('bridge.describe')).dialogue.text,'Бой начался. Можно продолжить реплику: бой продолжится.');
    await Action('stop');
    await Tap(Input,'KeyR');
    await Delay(1550);
    Assert.equal((await Command('bridge.describe')).gameViews[0].values[1].value,'2/7');
    const Before=(await Command('scene.getHierarchy')).find(Entry=>Entry.name==='camera').transform.position;
    Input({kind:'key',code:'KeyD',down:true});
    await Delay(250);
    Input({kind:'key',code:'KeyD',down:false});
    await Delay(80);
    const After=(await Command('scene.getHierarchy')).find(Entry=>Entry.name==='camera').transform.position;
    Assert.ok(After[0]>Before[0]+0.3,JSON.stringify({Before,After}));
    Input({kind:'gameControl',device:'Touch',control:'move',value:[1,0,0],source:'TestTouch'});
    await Delay(250);
    Input({kind:'gameControl',device:'Touch',control:'move',value:[0,0,0],source:'TestTouch'});
    await Delay(60);
    const TouchPosition=(await Command('scene.getHierarchy')).find(Entry=>Entry.name==='camera').transform.position;
    Assert.ok(TouchPosition[0]>After[0]+0.3);
    await Command('play.pause');
    await Assert.rejects(Action('medkit'));
    await Action('save');
  });
});

Test('Horror: exact props, range, puzzle progression, save and single ending', {timeout:45000},async()=>{
  await WithTemplate('horror',async(Command,Action,Project,Input)=>{
    const Oracle=JSON.parse(await FileSystem.readFile(Path.join(Repository,'Root/Verification/WebTemplateOracle/horror.json'),'utf8'));
    const Names=Object.fromEntries(Oracle.objects.map(Entry=>[Entry.id,Entry.name]));
    const Position=async Identifier=>(await Command('scene.getHierarchy')).find(Entry=>Entry.name===Names[Identifier]).transform.position;
    const MoveTo=async(Axis,Target)=>{
      for(let Attempt=0;Attempt<30;++Attempt){
        const Current=(await Position('horror-player'))[Axis];
        if(Math.abs(Target-Current)<0.08){return;}
        let Code='KeyD';
        if(Axis===0&&Target<Current){Code='KeyA';}
        if(Axis===2){Code='KeyS';if(Target<Current){Code='KeyW';}}
        Input({kind:'key',code:Code,down:true});
        await Delay(Math.min(350,Math.abs(Target-Current)/2.8*1000));
        Input({kind:'key',code:Code,down:false});
        await Delay(60);
      }
      Assert.fail('Movement did not reach '+JSON.stringify({Axis,Target,Position:await Position('horror-player')}));
    };
    const Inspect=async Identifier=>{
      const Objects=await Command('scene.getHierarchy');
      const Camera=Objects.find(Entry=>Entry.name==='camera').transform.position;
      const Target=await Position(Identifier);
      const Difference=Target.map((Value,Axis)=>Value-Camera[Axis]);
      const Pitch=-0.16;
      const Depth=Difference[1]*Math.sin(Pitch)-Difference[2]*Math.cos(Pitch);
      const Vertical=Difference[1]*Math.cos(Pitch)+Difference[2]*Math.sin(Pitch);
      Input({kind:'gameControl',device:'Mouse',control:'Position',value:[Difference[0]/Depth/Math.tan(32*Math.PI/180),Vertical/Depth/Math.tan(32*Math.PI/180),0],source:'Test'});
      await Delay(60);
      Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[1,0,0],source:'Test'});
      await Delay(60);
      Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[0,0,0],source:'Test'});
      await Delay(80);
    };
    await Command('play.advanceDialogue');
    await Delay(120);
    await Inspect('horror-fuse');
    Assert.match((await Command('bridge.describe')).gameViews[0].text,/Подойдите ближе/);
    await MoveTo(2,4.6);
    await Inspect('horror-fuse');
    Assert.equal((await Command('bridge.describe')).gameViews[0].values[0].value,'да');
    await Action('save');
    await MoveTo(2,-4.2);
    await MoveTo(0,-1.8);
    await Inspect('horror-power');
    Assert.equal((await Command('bridge.describe')).gameViews[0].values[1].value,'да');
    await MoveTo(0,0);
    await MoveTo(2,-1.5);
    await MoveTo(0,0.5);
    await Inspect('horror-tape');
    Assert.equal((await Command('bridge.describe')).gameViews[0].values[2].value,'да');
    await MoveTo(0,0);
    await MoveTo(2,-7.3);
    await Inspect('horror-exit');
    await Delay(1200);
    Assert.equal((await Command('bridge.describe')).dialogue.text,Oracle.chapters[0].beats.find(Entry=>Entry.id==='horror-ending').text);
    await Action('load');
    await Delay(120);
    Assert.deepEqual((await Command('bridge.describe')).gameViews[0].values.map(Entry=>Entry.value),['да','нет','нет']);
    Assert.ok(!(await Command('bridge.describe')).gameViews[0].actions.some(Entry=>Entry.id==='stay'));
  });
});

Test('Shooter: native raycast kills all five targets and interrupts combat dialogue', {timeout:45000},async()=>{
  await WithTemplate('shooter',async(Command,Action,Project,Input)=>{
    const Oracle=JSON.parse(await FileSystem.readFile(Path.join(Repository,'Root/Verification/WebTemplateOracle/shooter.json'),'utf8'));
    await Command('play.advanceDialogue');
    await Delay(100);
    await Action('stop');
    let Yaw=0;
    let Pitch=0;
    for(const Source of Oracle.objects.filter(Entry=>/^enemy-[1-5]$/.test(Entry.id))){
      for(let Attempt=0;Attempt<6;++Attempt){
        const Objects=await Command('scene.getHierarchy');
        const Enemy=Objects.find(Entry=>Entry.name===Source.name);
        if(!Enemy.active){break;}
        const Origin=Objects.find(Entry=>Entry.name==='camera').transform.position;
        const Target=[...Enemy.transform.position];
        Target[1]+=1.25;
        const Distance=Math.hypot(...Target.map((Value,Axis)=>Value-Origin[Axis]));
        const NextYaw=Math.atan2(Origin[0]-Target[0],Origin[2]-Target[2]);
        const NextPitch=Math.asin((Target[1]-Origin[1])/Distance);
        Input({kind:'gameControl',device:'Mouse',control:'Delta',value:[(Yaw-NextYaw)/0.003,(Pitch-NextPitch)/0.003,0],transient:true,source:'Test'});
        Yaw=NextYaw;Pitch=NextPitch;
        await Delay(60);
        if((await Command('bridge.describe')).gameViews[0].values[1].value.startsWith('0/')){
          await Tap(Input,'KeyR');await Delay(1050);
        }
        Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[1,0,0],source:'Test'});
        await Delay(60);
        Input({kind:'gameControl',device:'Mouse',control:'Primary',value:[0,0,0],source:'Test'});
        Pitch+=0.025;
        await Delay(300);
      }
    }
    const State=await Command('bridge.describe');
    Assert.equal(State.gameViews[0].values[2].value,500);
    Assert.equal(State.dialogue.text,Oracle.chapters[0].beats.find(Entry=>Entry.id==='shooter-win').text);
    await Action('restart');
    await Delay(100);
    await Command('play.advanceDialogue');
    const Deadline=Date.now()+20000;
    let Defeat=await Command('bridge.describe');
    while(Date.now()<Deadline&&Defeat.gameViews[0].values[0].value>0){await Delay(200);Defeat=await Command('bridge.describe');}
    Assert.equal(Defeat.gameViews[0].values[0].value,0);
    Assert.equal(Defeat.dialogue.text,Oracle.chapters[0].beats.find(Entry=>Entry.id==='shooter-game-over').text);
  });
});
