const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.Run = async ({Application, Window, GetViewport, CreateNativeProject, ReplaceProject}) => {
  const Output = Path.resolve(process.env.SACURA_PERFORMANCE_OUTPUT || Path.join(__dirname, '../../build/render-performance-' + Date.now()));
  const Report = {Host:process.env.SACURA_NATIVE_HOST, Electron:process.versions.electron, Measurements:[], Checks:[]};
  const Status = () => Window.webContents.executeJavaScript('window.sacuraDesktop.nativeStatus()');
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (GetViewport()?.state.error) {
        throw Error(GetViewport().state.error);
      }
      if (Date.now() > Deadline) {
        throw Error('Render performance check timed out');
      }
      await Delay(100);
    }
  }
  async function Measure(Name) {
    await Delay(4000);
    const Viewport = GetViewport();
    const First = (await Status()).presentation.frames;
    const Start = Date.now();
    const Samples = [];
    for (let SampleIndex = 0; SampleIndex < 10; ++SampleIndex) {
      await Delay(1000);
      Assert.ok(!Viewport.state.error && !Viewport.state.presentation.error, JSON.stringify(Viewport.state));
      Assert.ok(Viewport.imports <= 9);
      Samples.push({Statistics:{...Viewport.state.stats}, Native:{...Viewport.state.timing}, Presentation:(await Status()).presentation});
    }
    const Measurement = {Name, Seconds:(Date.now()-Start)/1000, Frames:(await Status()).presentation.frames-First, Samples};
    Measurement.FramesPerSecond = Measurement.Frames / Measurement.Seconds;
    Assert.ok(Measurement.FramesPerSecond > 10, 'Presentation stalled');
    Report.Measurements.push(Measurement);
    console.log('Render performance: ' + Name + ' ' + Measurement.FramesPerSecond.toFixed(2) + ' FPS');
    await FileSystem.writeFile(Path.join(Output, Name + '.png'), (await Window.webContents.capturePage()).toPNG());
  }
  try {
    Assert.ok(!/^C:/i.test(Output), 'Performance artifacts must stay outside C:');
    await FileSystem.mkdir(Output, {recursive:true});
    setTimeout(() => Application.exit(1), 180000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Window.webContents.executeJavaScript('!!window.sacuraDesktop?.bridge'));
    if (!process.env.SACURA_NATIVE_PROJECT) {
      await CreateNativeProject('RenderPerformance', Output, 'builtin-quest');
    }
    await Wait(() => GetViewport().state.presentation?.frames > 60 && GetViewport().state.stats?.visibleObjects > 0);
    Window.setSize(1440, 900);
    await Measure('Editor');
    const Play = await GetViewport().bridge.request('play.start', {});
    Assert.equal(Play.ok, true, JSON.stringify(Play));
    await Measure('Play');
    await GetViewport().bridge.request('play.stop', {});
    if (process.env.SACURA_PERFORMANCE_REGRESSIONS === '1') {
      async function CaptureMode(Mode) {
        GetViewport().tools({tool:'translate', space:'local', grid:true, visualization:Mode});
        await Delay(700);
        const Snapshot = await Status();
        Assert.ok(!Snapshot.presentation.error, JSON.stringify(Snapshot));
        await FileSystem.writeFile(Path.join(Output, Mode + '.png'), (await Window.webContents.capturePage()).toPNG());
      }
      for (const Mode of ['shadow_atlas', 'transparency_accumulation', 'transparency_revealage', 'lit']) {
        await CaptureMode(Mode);
      }
      Report.Checks.push('Empty shadow/transparency diagnostics remain available');
      const ValidationProject = Path.join(Output, 'ValidationProject');
      await FileSystem.cp(Path.resolve(__dirname, '../../Samples/RenderValidation'), ValidationProject, {recursive:true});
      await ReplaceProject(Path.join(ValidationProject, 'RenderValidation.project'));
      await Wait(() => GetViewport().state.stats?.visibleObjects > 10 && GetViewport().state.timing?.gpuAvailable);
      await Measure('TransparencyAndShadows');
      Assert.ok(GetViewport().state.timing.shadowMs > 0);
      Assert.ok(GetViewport().state.timing.transparencyMs > 0);
      for (const Mode of ['transparency_accumulation', 'transparency_revealage', 'shadow_atlas', 'lit']) {
        await CaptureMode(Mode);
      }
      Report.Checks.push('Validation scene renders lit geometry, shadows, cutouts and overlapping transparency');
      for (const Size of [[960,680], [1600,1000], [1440,900]]) {
        const PreviousFrame = GetViewport().state.stats.frame;
        Window.setSize(...Size);
        await Wait(() => GetViewport().lastBounds && GetViewport().state.stats.frame > PreviousFrame + 10
          && GetViewport().state.stats.width === GetViewport().lastBounds.width
          && GetViewport().state.stats.height === GetViewport().lastBounds.height);
      }
      Report.Checks.push('Resize reallocates attachments and continues shared GPU frames');
      Window.minimize();
      await Wait(() => Window.isMinimized());
      await Delay(500);
      Window.restore();
      const PreviousFrames = GetViewport().state.presentation.frames;
      await Wait(() => GetViewport().state.bounds?.visible && GetViewport().state.presentation.frames > PreviousFrames + 10);
      Report.Checks.push('Minimize/restore resumes presentation');
    }
    Report.Passed = true;
    await FileSystem.writeFile(Path.join(Output, 'report.json'), JSON.stringify(Report, null, 2));
    await GetViewport().stop();
    console.log('Render performance report: ' + Output);
    Application.exit(0);
  } catch (Failure) {
    console.error(Failure);
    Report.Error = Failure.stack;
    await FileSystem.writeFile(Path.join(Output, 'report.json'), JSON.stringify(Report, null, 2));
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
