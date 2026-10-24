const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.Run = async ({Application, Window, Viewport}) => {
  const Output = process.env.SACURA_PLAYER_TEST_OUTPUT;
  const Report = {checks: []};
  const Wait = async Predicate => {
    const Deadline = Date.now() + 60000;
    while (!Predicate()) {
      if (Viewport.state.error) { throw new Error(Viewport.state.error); }
      if (Viewport.state.presentation?.error) { throw new Error(Viewport.state.presentation.error); }
      if (Date.now() > Deadline) { throw new Error('Player check timed out: ' + JSON.stringify(Viewport.state)); }
      await Delay(100);
    }
  };
  let Sequence = 0;
  const ReactCommand = async (Method, Parameters = {}) => {
    const Identifier = ++Sequence;
    Window.webContents.send('native-test', {id: Identifier, method: Method, params: Parameters});
    await Wait(() => Viewport.testResult?.id === Identifier);
    Assert.equal(Viewport.testResult.ok, true, JSON.stringify(Viewport.testResult));
    return Viewport.testResult.value;
  };
  try {
    Assert.ok(Output, 'SACURA_PLAYER_TEST_OUTPUT must name a report directory');
    await FileSystem.mkdir(Output, {recursive: true});
    await Wait(() => Viewport.state.presentation?.frames > 5);
    Report.checks.push('React presents native GPU frames');
    let State = await ReactCommand('game.state');
    Assert.equal(State.gameRunning, true);
    State = await ReactCommand('game.pause', {paused: true});
    Assert.equal(State.gamePaused, true);
    await Delay(200);
    await FileSystem.writeFile(Path.join(Output, 'paused.png'), (await Window.webContents.capturePage()).toPNG());
    await ReactCommand('game.pause', {paused: false});
    Report.checks.push('React pause and resume use game state');
    if(process.env.SACURA_PLAYER_TEST_INTERACTIVE==='1') {
      State=await ReactCommand('game.state');
      if(State.dialogue.canAdvance){await ReactCommand('game.advance');}
      if(State.dialogue.canChoose){await ReactCommand('game.choose',{index:0});}
    }
    if(process.env.SACURA_PLAYER_TEST_ANIMATION==='1') {
      await Delay(600);
      const FirstFrame = (await Window.webContents.capturePage()).toPNG();
      await Delay(800);
      const SecondFrame = (await Window.webContents.capturePage()).toPNG();
      Assert.equal(FirstFrame.equals(SecondFrame),false,'Animated character presentation did not change');
      await FileSystem.writeFile(Path.join(Output,'animation-first.png'),FirstFrame);
      await FileSystem.writeFile(Path.join(Output,'animation-second.png'),SecondFrame);
      Report.checks.push('Animated character changes rendered frames');
    }
    for (const Size of [[1280, 800], [640, 720]]) {
      Window.setSize(...Size);
      await Delay(1000);
      await Wait(() => Viewport.state.stats?.width === Viewport.state.bounds?.width && Viewport.state.presentation?.frames > 10);
      await FileSystem.writeFile(Path.join(Output, 'player-' + Size.join('x') + '.png'), (await Window.webContents.capturePage()).toPNG());
      Report.checks.push('Player presentation at ' + Size.join('x'));
    }
    for (let Step = 0; Step < 200&&process.env.SACURA_PLAYER_TEST_INTERACTIVE!=='1'; ++Step) {
      State = await ReactCommand('game.state');
      if (State.dialogue.canAdvance) { await ReactCommand('game.advance'); }
      else if (State.dialogue.canChoose) { await ReactCommand('game.choose', {index: 0}); }
      else if (State.dialogue.phase === 'action') {
        const Identifier = process.env.SACURA_PLAYER_TEST_ACTION;
        const View = State.gameViews?.find(Entry => Entry.actions?.some(Action => Action.id === Identifier && Action.enabled !== false));
        if (View) {
          await ReactCommand('game.action', {objectId:View.objectId,componentId:View.componentId,action:Identifier});
        }
        await Delay(100);
      }
      else { break; }
    }
    Report.dialogue = State.dialogue.phase;
    if(process.env.SACURA_PLAYER_TEST_INTERACTIVE==='1') {
      State=await ReactCommand('game.state');
      Assert.notEqual(State.dialogue.phase,'failed',JSON.stringify(State.dialogue));
      Assert.ok(State.gameViews?.[0]?.widget);
      Report.checks.push('Authored gameplay widget is supplied by the native project');
    } else { Assert.equal(State.dialogue.phase, 'end', JSON.stringify(State.dialogue)); }
    const Forbidden = await Viewport.bridge.request('object.create', {name: 'Forbidden'});
    Assert.equal(Forbidden.error.code, 'PLAYER_READ_ONLY');
    Report.checks.push('Editor mutation rejected');
    Report.ok = true;
    await FileSystem.writeFile(Path.join(Output, 'report.json'), JSON.stringify(Report, null, 2));
    await Viewport.stop();
    Application.exit(0);
  } catch (Failure) {
    Report.error = Failure.stack;
    Report.state = Viewport.state;
    if (Output) { await FileSystem.writeFile(Path.join(Output, 'report.json'), JSON.stringify(Report, null, 2)); }
    console.error(Failure);
    await Viewport.stop();
    Application.exit(1);
  }
};
