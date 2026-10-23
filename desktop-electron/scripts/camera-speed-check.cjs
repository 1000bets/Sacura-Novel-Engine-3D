const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/camera-speed-check-' + Date.now());
  const Evaluate = Source => Window.webContents.executeJavaScript(Source);
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Camera speed check timed out');
      }
      await Delay(100);
    }
  }
  async function SetSpeed(Value, Selector = '#viewport-camera-speed') {
    await Evaluate(`(() => {const Field = document.querySelector(${JSON.stringify(Selector)}); Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set.call(Field, ${JSON.stringify(String(Value))}); Field.dispatchEvent(new Event('input', {bubbles:true}));})()`);
    await Wait(() => Math.abs(GetViewport().state.viewport?.data?.cameraSpeed - Value) < 0.001);
  }
  async function Fly(bAccelerated) {
    const Viewport = GetViewport();
    const Start = Viewport.state.viewport.data.cameraPosition;
    Viewport.input({kind:'pointer', phase:'down', x:400, y:200, button:2, buttons:2});
    Viewport.input({kind:'key', code:'KeyW', down:true, shift:bAccelerated});
    await Delay(650);
    Viewport.input({kind:'reset'});
    await Delay(300);
    const End = Viewport.state.viewport.data.cameraPosition;
    return Math.hypot(...End.map((Coordinate, Index) => Coordinate - Start[Index]));
  }
  async function Capture(Name) {
    await Delay(250);
    await FileSystem.writeFile(Path.join(Output, Name + '.png'), (await Window.webContents.capturePage()).toPNG());
  }
  try {
    await FileSystem.mkdir(Output, {recursive:true});
    setTimeout(() => Application.exit(1), 90000).unref();
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.bridge'));
    await CreateNativeProject('CameraSpeedCheck', Output, 'Empty');
    await Wait(() => GetViewport().state.viewport?.data?.cameraSpeed === 4);
    await Wait(() => Evaluate('!!document.querySelector(".viewport-camera-speed summary")'));
    await Evaluate('document.querySelector(".viewport-camera-speed summary").click()');
    await SetSpeed(2);
    const SlowDistance = await Fly(false);
    await SetSpeed(12, '.viewport-camera-speed input[type="range"]');
    const FastDistance = await Fly(false);
    Assert.ok(SlowDistance > 0.2 && FastDistance > SlowDistance * 3, JSON.stringify({SlowDistance, FastDistance}));
    await SetSpeed(2);
    const AcceleratedDistance = await Fly(true);
    Assert.ok(AcceleratedDistance > SlowDistance * 1.8, JSON.stringify({SlowDistance, AcceleratedDistance}));
    await SetSpeed(0.1);
    await SetSpeed(100);
    for (const InvalidSpeed of [0, -1, 101, null]) {
      Assert.throws(() => GetViewport().tools({tool:'translate', space:'local', cameraSpeed:InvalidSpeed}), /Camera speed/);
    }
    await Evaluate('document.querySelector(".viewport-camera-speed button").click()');
    await Wait(() => GetViewport().state.viewport.data.cameraSpeed === 4);
    Window.setSize(1440, 900);
    await Capture('Desktop');
    Window.setMinimumSize(480, 400);
    Window.setSize(640, 720);
    await Capture('Compact');
    Assert.equal(await Evaluate('document.documentElement.scrollWidth <= window.innerWidth'), true);
    Assert.equal(await Evaluate('(() => {const Bounds=document.querySelector(".viewport-camera-speed>div").getBoundingClientRect();return Bounds.left>=0&&Bounds.right<=innerWidth;})()'), true);
    const Result = {passed:true, SlowDistance, FastDistance, AcceleratedDistance};
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify(Result, null, 2));
    console.log('Camera speed check passed: ' + Output);
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error(Failure);
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:false,error:Failure.stack}, null, 2));
    await Capture('Failure').catch(() => {});
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
