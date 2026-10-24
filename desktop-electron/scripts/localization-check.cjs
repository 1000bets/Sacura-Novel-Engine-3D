const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, getViewport: GetViewport, createNativeProject: CreateNativeProject}) => {
  const Output = Path.resolve(__dirname, '../../build/localization-check-' + Date.now());
  const Evaluate = async Source => {
    try {
      return await Window.webContents.executeJavaScript(Source);
    } catch (Failure) {
      throw new Error(Failure.message + '\nEvaluation: ' + Source);
    }
  };
  let Original;
  let Passed = false;
  async function Wait(Predicate) {
    const Deadline = Date.now() + 60000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw new Error('Localization check timed out');
      }
      await Delay(100);
    }
  }
  async function Capture(Name) {
    for (let Attempt = 0; Attempt < 5; Attempt++) {
      await Delay(500);
      try {
        await FileSystem.writeFile(Path.join(Output, Name + '.png'), (await Window.webContents.capturePage()).toPNG());
        return;
      } catch (Failure) {
        if (Attempt === 4) {
          throw Failure;
        }
      }
    }
  }
  async function Command(Method, Params) {
    const Result = await GetViewport().bridge.request(Method, Params);
    Assert.equal(Result.ok, true, JSON.stringify(Result));
    return Result.value;
  }
  async function OpenPreferences() {
    await Evaluate('document.querySelector("[data-editor-preferences]").click()');
    await Wait(() => Evaluate('!!document.querySelector("#editor-window-editor-settings .editor-preferences")'));
  }
  try {
    await FileSystem.mkdir(Output, {recursive:true});
    await Wait(async () => !Window.webContents.isLoading() && await Evaluate('!!window.sacuraDesktop?.editorLocalization'));
    Original = await Evaluate('window.sacuraDesktop.editorLocalization()');
    await Evaluate('window.sacuraDesktop.editorLocalization({language:"en",overrides:{}})');
    await CreateNativeProject('LocalizationCheck', Output, 'Empty');
    await Wait(() => Evaluate('!!document.querySelector("[data-editor-preferences]")'));
    const Created = await Command('object.create', {name:'Файл', componentType:'engine.CameraComponent'});
    await Command('selection.set', {objectId:Created.id});
    await Wait(() => Evaluate('document.querySelector(".component-instance-row strong")?.textContent === "Camera"'));
    const Before = await Command('object.getComponents', {objectId:Created.id});
    await Capture('English');
    const Light = await Command('object.create', {name:'Enum light',componentType:'engine.LightComponent'});
    await Command('selection.set', {objectId:Light.id});
    await Wait(() => Evaluate('Array.from(document.querySelectorAll(".game-field")).some(Field=>Field.querySelector("span")?.textContent === "Light Type" && Field.querySelector("select"))'));
    Assert.deepEqual(await Evaluate('Array.from(Array.from(document.querySelectorAll(".game-field")).find(Field=>Field.querySelector("span")?.textContent === "Light Type").querySelector("select").options).map(Option=>Option.value)'), ['Directional','Point','Spot']);
    await Evaluate('(() => {const Field=Array.from(document.querySelectorAll(".game-field")).find(Field=>Field.querySelector("span")?.textContent === "Light Type").querySelector("select");Field.value="Spot";Field.dispatchEvent(new Event("change",{bubbles:true}));})()');
    const LightComponent = (await Command('object.getComponents', {objectId:Light.id})).find(Component=>Component.typeId==='engine.LightComponent');
    await Wait(async () => (await Command('component.getProperties',{objectId:Light.id,componentId:LightComponent.id})).some(Property=>Property.id==='light_type'&&Property.value==='Spot'));
    await Capture('LightEnum');
    await Command('selection.set', {objectId:Created.id});
    await Wait(() => Evaluate('document.querySelector(".component-instance-row strong")?.textContent === "Camera"'));
    const PrimaryValue = () => Evaluate('Array.from(document.querySelectorAll(".component-instance-properties .game-field")).find(Field=>Field.querySelector("span")?.textContent === "Primary")?.querySelector("input[type=checkbox]")?.checked');
    const OriginalPrimary = await PrimaryValue();
    Assert.equal(typeof OriginalPrimary, 'boolean');
    await Evaluate('Array.from(document.querySelectorAll(".component-instance-properties .game-field")).find(Field=>Field.querySelector("span")?.textContent === "Primary").querySelector("input").click()');
    await Wait(async () => {
      const Camera = Before.find(Component=>Component.typeId === 'engine.CameraComponent');
      const Properties = await Command('component.getProperties', {objectId:Created.id,componentId:Camera.id});
      return Properties.some(Property=>Property.id === 'primary' && Property.value === !OriginalPrimary);
    });
    await Delay(300);
    await Evaluate('document.body.dispatchEvent(new KeyboardEvent("keydown",{key:"z",code:"KeyZ",ctrlKey:true,bubbles:true,cancelable:true}))');
    await Wait(async () => await PrimaryValue() === OriginalPrimary);
    await Delay(300);
    await Evaluate('document.body.dispatchEvent(new KeyboardEvent("keydown",{key:"z",code:"KeyZ",ctrlKey:true,shiftKey:true,bubbles:true,cancelable:true}))');
    await Wait(async () => await PrimaryValue() === !OriginalPrimary);
    await Delay(300);
    await Evaluate('document.body.dispatchEvent(new KeyboardEvent("keydown",{key:"z",code:"KeyZ",ctrlKey:true,bubbles:true,cancelable:true}))');
    await Wait(async () => await PrimaryValue() === OriginalPrimary);
    await OpenPreferences();
    await Evaluate('(() => {const Field=document.querySelector("#editor-window-editor-settings .editor-preferences .language-picker select");Field.value="ru";Field.dispatchEvent(new Event("change",{bubbles:true}));})()');
    await Wait(() => Evaluate('document.querySelector("#editor-window-editor-settings .editor-preferences h2")?.textContent === "Настройки редактора"'));
    await Wait(async () => (await Evaluate('window.sacuraDesktop.editorLocalization()')).language === 'ru');
    await Capture('RussianSettings');
    await Evaluate('document.querySelector("[data-dock-window=editor-settings] .workspace-tab-close").click()');
    await Wait(() => Evaluate('document.querySelector(".component-instance-row strong")?.textContent === "Камера"'));
    Assert.equal(await Evaluate('document.querySelector(".component-instances").textContent.includes("Угол обзора")'), true);
    Assert.deepEqual(await Command('object.getComponents', {objectId:Created.id}), Before);
    await Capture('RussianInspector');
    await OpenPreferences();
    await Evaluate('document.querySelector("#editor-window-editor-settings .editor-preferences details").open=true');
    await Evaluate('(() => {const Field=document.querySelector("#editor-window-editor-settings .editor-preferences textarea:not([readonly])");Object.getOwnPropertyDescriptor(HTMLTextAreaElement.prototype,"value").set.call(Field,"Параметры редактора");Field.dispatchEvent(new Event("input",{bubbles:true}));})()');
    await Evaluate('Array.from(document.querySelectorAll("#editor-window-editor-settings .editor-preferences button")).find(Button=>Button.textContent.trim()==="Сохранить перевод").click()');
    await Wait(() => Evaluate('document.querySelector("#editor-window-editor-settings .editor-preferences h2")?.textContent === "Параметры редактора"'));
    await Wait(async () => (await Evaluate('window.sacuraDesktop.editorLocalization()')).overrides['Editor.Settings.Title']?.text === 'Параметры редактора');
    Window.webContents.reload();
    await Wait(() => Evaluate('!!document.querySelector("[data-editor-preferences]")'));
    await OpenPreferences();
    Assert.equal(await Evaluate('document.querySelector("#editor-window-editor-settings .editor-preferences h2").textContent'), 'Параметры редактора');
    Window.setMinimumSize(360, 400);
    Window.setSize(480, 720);
    await Capture('CompactSettings');
    Assert.equal(await Evaluate('(() => {const Bounds=document.querySelector("#editor-window-editor-settings .editor-preferences").getBoundingClientRect();return Bounds.height>120&&Bounds.top<innerHeight&&Bounds.left>=0&&Bounds.right<=innerWidth;})()'), true);
    await Evaluate('document.querySelector("[data-dock-window=editor-settings] .workspace-tab-close").click()');
    await Evaluate('window.sacuraDesktop.editorLocalization({language:"en",overrides:{}})');
    Window.setSize(1280, 900);
    await Evaluate('Array.from(document.querySelectorAll(".editor-menubar>button")).find(Button=>Button.textContent.trim()==="Edit").click()');
    await Wait(() => Evaluate('!!document.querySelector("[data-project-settings]")'));
    await Evaluate('document.querySelector("[data-project-settings]").click()');
    await Wait(() => Evaluate('!!document.querySelector("input[name=startupScene]")'));
    const ProjectSettings = await Command('project.settings.get', {});
    Assert.equal(await Evaluate('document.querySelector("input[name=name]").value'), ProjectSettings.name);
    await Capture('ProjectSettings');
    await Evaluate('(() => {const Field=document.querySelector("input[name=name]");Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,"value").set.call(Field,"Renamed project");Field.dispatchEvent(new Event("input",{bubbles:true}));})()');
    await Evaluate('document.querySelector("#editor-window-project-settings .editor-preferences form").requestSubmit()');
    await Wait(async () => (await Command('project.settings.get', {})).name === 'Renamed project');
    await Wait(() => Evaluate('document.querySelector("#editor-window-project-settings .editor-preferences [role=status]")?.textContent === "Project settings saved." && !document.querySelector("#editor-window-project-settings .editor-preferences fieldset").disabled'));
    const ProjectFile = Path.join(Output, 'LocalizationCheck', 'LocalizationCheck.project');
    const SavedProject = JSON.parse(await FileSystem.readFile(ProjectFile, 'utf8'));
    Assert.equal(SavedProject.name, 'Renamed project');
    const Invalid = await GetViewport().bridge.request('project.settings.set', {settings:{name:'Rejected',startupScene:'../outside.scene',startupStory:''}});
    Assert.equal(Invalid.ok, false);
    Assert.equal(JSON.parse(await FileSystem.readFile(ProjectFile, 'utf8')).name, 'Renamed project');
    await Evaluate('(() => {const Field=document.querySelector("input[name=startupScene]");Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,"value").set.call(Field,"Content/Missing.scene");Field.dispatchEvent(new Event("input",{bubbles:true}));})()');
    await Evaluate('document.querySelector("#editor-window-project-settings .editor-preferences form").requestSubmit()');
    await Wait(() => Evaluate('!!document.querySelector("#editor-window-project-settings .editor-preferences [role=alert]")'));
    await Capture('ProjectSettingsError');
    await Evaluate('Array.from(document.querySelectorAll(".managed-workspace .workspace-tab-actions button")).at(-1).click()');
    Window.setSize(480, 720);
    await Capture('CompactProjectSettings');
    Assert.equal(await Evaluate('(() => {const Bounds=document.querySelector("#editor-window-project-settings .editor-preferences").getBoundingClientRect();return Bounds.height>120&&Bounds.top<innerHeight&&Bounds.left>=0&&Bounds.right<=innerWidth;})()'), true);
    Passed = true;
    console.log('Localization check passed: ' + Output);
  } catch (Failure) {
    console.error(Failure);
    await FileSystem.writeFile(Path.join(Output, 'failure.txt'), Failure.stack);
    await Capture('Failure').catch(() => {});
  } finally {
    if (Original) {
      await Evaluate('window.sacuraDesktop.editorLocalization(' + JSON.stringify({language:Original.language, overrides:Original.overrides}) + ')').catch(() => {});
    }
    await FileSystem.writeFile(Path.join(Output, 'result.json'), JSON.stringify({passed:Passed}, null, 2));
    await GetViewport()?.stop();
    let ExitCode = 1;
    if (Passed) {
      ExitCode = 0;
    }
    Application.exit(ExitCode);
  }
};
