const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {dialog: Dialog} = require('electron');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, windows: Windows, getViewport: GetViewport, output: Output}) => {
  const Destination = Path.resolve(Output);
  let Panel = Window;
  let Localization;
  const OriginalDialog = Dialog.showOpenDialog;
  const Evaluate = Source => Panel.webContents.executeJavaScript(Source, true);
  async function Wait(Predicate, Label) {
    const Deadline = Date.now() + 30000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Audio check timed out: ' + Label);
      }
      await Delay(75);
    }
  }
  async function Click(Label) {
    await Wait(() => Evaluate(`Array.from(document.querySelectorAll('.native-sound-workspace button')).find(Button=>Button.textContent.trim()===${JSON.stringify(Label)})?.disabled===false`), Label);
    await Evaluate(`Array.from(document.querySelectorAll('.native-sound-workspace button')).find(Button=>Button.textContent.trim()===${JSON.stringify(Label)}).click()`);
  }
  async function Call(Method, Parameters = {}) {
    const Result = await GetViewport().bridge.request(Method, Parameters);
    Assert.equal(Result.ok,true,JSON.stringify(Result));
    return Result.value;
  }
  try {
    await FileSystem.mkdir(Path.dirname(Destination),{recursive:true});
    await Wait(() => GetViewport()?.bridge.connected,'Bridge');
    await Wait(() => !Window.webContents.isLoading(),'Renderer');
    Localization = await Evaluate('window.sacuraDesktop.editorLocalization()');
    await Evaluate('window.sacuraDesktop.editorLocalization({language:"ru",overrides:{}})');
    await Wait(() => Evaluate('!!document.querySelector("[data-windows-menu]")'),'Menu');
    await Evaluate('document.querySelector("[data-windows-menu]").click()');
    await Wait(() => Evaluate('Array.from(document.querySelectorAll(".windows-menu-container button")).some(Button=>Button.textContent.includes("Звук"))'),'Windows menu');
    await Evaluate('Array.from(document.querySelectorAll(".windows-menu-container button")).find(Button=>Button.textContent.trim()==="Звук").click()');
    await Wait(() => Evaluate('!!document.querySelector(".native-sound-workspace")'),'Sound panel');
    Assert.equal(await Evaluate('document.querySelectorAll(".native-sound-workspace .sound-card").length'),0);
    const Source = process.env.SACURA_AUDIO_FIXTURE;
    Dialog.showOpenDialog = async () => ({canceled:false,filePaths:[Source]});
    await Click('Импортировать WAV');
    await Wait(() => Evaluate('document.querySelectorAll(".native-sound-workspace .sound-card").length===1'),'Import');
    await Wait(() => Evaluate('!!document.querySelector(".native-sound-workspace .wave svg")'),'Decoded waveform');
    await Click('Слушать');
    await Click('Пауза');
    await Click('Продолжить');
    await Click('Остановить всё');
    await Click('Сделать стартовым');
    await Wait(async () => (await Call('bridge.describe')).startupAudioPath === 'Audio/Preview.wav','Startup persisted');
    await Wait(() => Evaluate('Array.from(document.querySelectorAll(".sound-footer button")).some(Button=>Button.textContent.trim()==="Стартовый звук" && Button.disabled)'),'Startup UI');
    await Evaluate(`(() => {
      const Input = document.querySelector('.sound-controls input[type=range]');
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,'0.23');
      Input.dispatchEvent(new Event('input',{bubbles:true}));
    })()`);
    await Wait(() => Evaluate('Object.values(JSON.parse(localStorage.getItem("sacura.native.sound.preview.v1")).assets).some(Asset=>Asset.volume===0.23)'),'Preview settings');
    await Click('Музыка');
    Assert.equal(await Evaluate('document.querySelectorAll(".sound-card").length'),0);
    await Click('Все');
    Dialog.showOpenDialog = async () => ({canceled:false,filePaths:[Path.join(Path.dirname(Source),'Invalid.wav')]});
    await Click('Импортировать WAV');
    await Wait(() => Evaluate('!!document.querySelector(".native-sound-workspace [role=alert]")'),'Import error');
    await Click('Убрать стартовый звук');
    await Wait(async () => !(await Call('bridge.describe')).startupAudio,'Clear startup');
    await Click('Сделать стартовым');
    await Evaluate(`(() => {
      const Selector = document.querySelector('.sound-card select');
      Selector.value = 'music';
      Selector.dispatchEvent(new Event('change',{bubbles:true}));
      const Input = document.querySelector('.audio-marker-add input');
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,'2');
      Input.dispatchEvent(new Event('input',{bubbles:true}));
    })()`);
    await Wait(() => Evaluate('document.querySelector(".audio-marker-add button")?.disabled===false'),'Marker input');
    await Evaluate('document.querySelector(".audio-marker-add button").click()');
    await Wait(() => Evaluate('document.querySelectorAll(".audio-marker-list input").length===1 && !!document.querySelector(".audio-music-grid")'),'Marker and music grid');
    Panel = await require('./workspace-capture.cjs').DetachDocument(Window,Windows,'sound');
    await Wait(() => Evaluate('!!document.querySelector(".native-sound-workspace .wave svg")'),'Detached sound');
    Panel.setSize(1200,900);
    await Delay(400);
    await FileSystem.writeFile(Destination,(await Panel.webContents.capturePage()).toPNG());
    await Evaluate('document.querySelector(".sound-footer").scrollIntoView({block:"end"})');
    await Delay(150);
    await FileSystem.writeFile(Destination.replace('.png','-controls.png'),(await Panel.webContents.capturePage()).toPNG());
    await Click('Слушать');
    await Call('play.start');
    await Wait(() => Evaluate('Array.from(document.querySelectorAll(".native-sound-workspace button")).find(Button=>Button.textContent.trim()==="Слушать")?.disabled===true'),'Stop preview during Play');
    await Call('play.stop');
    await Wait(() => Evaluate('Array.from(document.querySelectorAll(".native-sound-workspace button")).find(Button=>Button.textContent.trim()==="Слушать")?.disabled===false'),'Play exit');
    Panel.setMinimumSize(320,400);
    Panel.setSize(480,720);
    await Delay(400);
    Assert.equal(await Evaluate('document.documentElement.scrollWidth<=innerWidth'),true);
    Assert.equal(await Evaluate('document.querySelector(".native-sound-workspace").scrollWidth<=document.querySelector(".native-sound-workspace").clientWidth'),true);
    await FileSystem.writeFile(Destination.replace('.png','-mobile.png'),(await Panel.webContents.capturePage()).toPNG());
    await Evaluate('document.querySelector(".sound-footer").scrollIntoView({block:"end"})');
    await Delay(150);
    await FileSystem.writeFile(Destination.replace('.png','-mobile-controls.png'),(await Panel.webContents.capturePage()).toPNG());
    console.log('Audio editor check passed: empty, import, decode, play/pause/resume/stop, local settings, filter, error, startup set/clear, detached, Play protection, desktop/mobile');
    Dialog.showOpenDialog = OriginalDialog;
    if (Localization) {
      await Window.webContents.executeJavaScript(`window.sacuraDesktop.editorLocalization(${JSON.stringify(Localization)})`);
    }
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    Dialog.showOpenDialog = OriginalDialog;
    console.error('Audio editor check failed:',Failure);
    console.error(await Evaluate('document.body.innerText').catch(() => ''));
    await FileSystem.writeFile(Destination.replace('.png','-failure.png'),(await Panel.webContents.capturePage()).toPNG()).catch(() => {});
    if (Localization) {
      await Window.webContents.executeJavaScript(`window.sacuraDesktop.editorLocalization(${JSON.stringify(Localization)})`).catch(() => {});
    }
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
