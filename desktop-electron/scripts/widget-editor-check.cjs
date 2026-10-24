const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, windows: Windows, editors: Editors, getViewport: GetViewport, output: Output}) => {
  const Destination = Path.resolve(Output);
  const WidgetName = "WidgetAcceptance" + Date.now();
  let EditorWindow;
  let Editor;
  async function Wait(Predicate) {
    const Deadline = Date.now() + 45000;
    while (!await Predicate()) {
      if (Date.now() > Deadline) {
        throw Error('Widget editor check timed out');
      }
      await Delay(75);
    }
  }
  try {
    await Wait(() => !Window.webContents.isLoading());
    await Window.webContents.executeJavaScript('window.sacuraDesktop.editorLocalization({language:"ru",overrides:{}})');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector("button[title=\\\"Редактор интерфейса игры\\\"]")'));
    await Window.webContents.executeJavaScript('document.querySelector("button[title=\\\"Редактор интерфейса игры\\\"]").click()');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".native-widget-list")'));
    await Wait(() => Window.webContents.executeJavaScript('document.querySelector(".native-widget-list>button")?.disabled === false'));
    await Window.webContents.executeJavaScript('document.querySelector(".native-widget-list>button").click()');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".asset-create-dialog input")'));
    await Window.webContents.executeJavaScript(`(() => {
      const Input = document.querySelector('.asset-create-dialog input');
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(WidgetName)});
      Input.dispatchEvent(new Event('input',{bubbles:true}));
    })()`);
    await Delay(100);
    await Window.webContents.executeJavaScript('document.querySelector(".asset-create-dialog button[type=submit]").click()');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".native-widget-asset") && !document.querySelector(".asset-create-dialog")'));
    async function Search(Value) {
      await Window.webContents.executeJavaScript(`(() => {
        const Input = document.querySelector('.native-widget-list input[type=search]');
        Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(Value)});
        Input.dispatchEvent(new Event('input',{bubbles:true}));
      })()`);
      await Delay(100);
    }
    await Search('no-such-widget-acceptance');
    Assert.equal(await Window.webContents.executeJavaScript('document.querySelectorAll(".native-widget-asset").length'),0);
    Assert.ok(await Window.webContents.executeJavaScript('!!document.querySelector(".native-widget-list [role=status]")'));
    await Search(WidgetName);
    Assert.equal(await Window.webContents.executeJavaScript('document.querySelectorAll(".native-widget-asset").length'),1);
    await FileSystem.writeFile(Destination.replace('.png','-list.png'),(await Window.webContents.capturePage()).toPNG());
    await Window.webContents.executeJavaScript(`Array.from(document.querySelectorAll(".native-widget-asset")).find(Element=>Element.textContent.includes(${JSON.stringify(WidgetName)})).click()`);
    await Wait(() => Array.from(Editors.values()).some(Entry => Entry.label.includes(WidgetName) && Entry.ready));
    Editor = Array.from(Editors.values()).find(Entry => Entry.label.includes(WidgetName));
    EditorWindow = await require('./workspace-capture.cjs').DetachDocument(Window, Windows, Editor.identifier);
    const Evaluate = Source => EditorWindow.webContents.executeJavaScript(Source);
    async function ClickToolbar(Label) {
      await Wait(() => Evaluate(`Array.from(document.querySelectorAll(".native-document-toolbar button")).find(Button=>Button.textContent===${JSON.stringify(Label)})?.disabled===false`));
      await Evaluate(`Array.from(document.querySelectorAll(".native-document-toolbar button")).find(Button=>Button.textContent===${JSON.stringify(Label)}).click()`);
    }

    await Wait(() => Evaluate('!!document.querySelector(".widget-canvas .widget-hit")'));
    const Read = async () => {
      const Result = await Editor.viewport.bridge.request('assetDocument.execute',{...Editor.identity,operation:'get'});
      Assert.equal(Result.ok,true,JSON.stringify(Result));
      return Result.value;
    };
    Assert.equal((await Read()).document.format,'sakura.widget');
    Assert.equal(await Evaluate(`!!document.querySelector('select[aria-label="Виджет для редактирования"]')`),false);
    Assert.equal(await Evaluate(`document.querySelectorAll('select[aria-label^="Стандартный виджет"]').length`),0);
    const Before = (await Read()).document.widget.elements[1].layout;
    const Bounds = await Evaluate(`(() => {const Bounds=document.querySelectorAll('.widget-hit')[1].getBoundingClientRect();return {x:Math.round(Bounds.left+20),y:Math.round(Bounds.top+Bounds.height/2)};})()`);
    EditorWindow.webContents.sendInputEvent({type:'mouseMove',...Bounds});
    EditorWindow.webContents.sendInputEvent({type:'mouseDown',button:'left',clickCount:1,...Bounds});
    EditorWindow.webContents.sendInputEvent({type:'mouseMove',x:Bounds.x+32,y:Bounds.y-24});
    await Delay(100);
    EditorWindow.webContents.sendInputEvent({type:'mouseUp',button:'left',clickCount:1,x:Bounds.x+32,y:Bounds.y-24});
    await Wait(async () => (await Read()).dirty);
    const Changed = (await Read()).document.widget.elements[1].layout;
    Assert.notDeepEqual(Changed,Before);
    await ClickToolbar('Отменить');
    await Wait(async () => !(await Read()).dirty);
    Assert.deepEqual((await Read()).document.widget.elements[1].layout,Before);
    await ClickToolbar('Повторить');
    await Wait(async () => (await Read()).dirty);
    Assert.deepEqual((await Read()).document.widget.elements[1].layout,Changed);
    await ClickToolbar('Сохранить');
    await Wait(async () => !(await Read()).dirty);
    await Evaluate('document.querySelector("select[aria-label=\\\"Добавить элемент виджета\\\"]").value="text";document.querySelector("select[aria-label=\\\"Добавить элемент виджета\\\"]").dispatchEvent(new Event("change",{bubbles:true}))');
    await Wait(async () => (await Read()).document.widget.elements.length===7);
    await ClickToolbar('Сохранить');
    await Wait(async () => !(await Read()).dirty);
    await Editors.open(Editor.identity);
    Assert.equal(Array.from(Editors.values()).filter(Entry=>Entry.identity.assetId===Editor.identity.assetId).length,1);
    EditorWindow.setSize(1440,900);
    await Delay(350);
    await FileSystem.writeFile(Destination,(await EditorWindow.webContents.capturePage()).toPNG());
    await Evaluate('Array.from(document.querySelectorAll(".widget-workspace>header button")).find(Button=>Button.textContent.includes("Предпросмотр")).click()');
    await Wait(() => Evaluate('!!document.querySelector(".native-widget-preview")'));
    await Evaluate('document.querySelector(".native-widget-preview button").click()');
    await Wait(() => Evaluate('!!document.querySelector(".widget-canvas")'));
    EditorWindow.setSize(480,720);
    await Delay(350);
    Assert.equal(await Evaluate('document.documentElement.scrollWidth<=innerWidth'),true);
    await FileSystem.writeFile(Destination.replace('.png','-mobile.png'),(await EditorWindow.webContents.capturePage()).toPNG());
    const Saved = await Read();
    const Malformed = structuredClone(Saved.document);
    Malformed.widget.elements[0].parentId = Malformed.widget.elements[0].id;
    const Rejected = await Editor.viewport.bridge.request('assetDocument.execute',{...Editor.identity,operation:'edit',revision:Saved.revision,value:Malformed});
    Assert.equal(Rejected.ok,false);
    Assert.deepEqual((await Read()).document,Saved.document);
    Assert.equal(await Editors.closeAll(),true);
    await Editors.open(Editor.identity);
    await Wait(() => Array.from(Editors.values()).some(Entry=>Entry.identity.assetId===Editor.identity.assetId&&Entry.ready));
    Editor=Array.from(Editors.values()).find(Entry=>Entry.identity.assetId===Saved.assetId);
    Assert.deepEqual((await Read()).document,Saved.document);
    await Wait(async () => {
      for (const Candidate of [Window,...Windows.values()]) {
        if (await Candidate.webContents.executeJavaScript('!!document.querySelector(".native-widget-editor .widget-canvas")').catch(()=>false)) {
          return true;
        }
      }
      return false;
    });
    await Editors.closeAll();
    const MainEvaluate = Source => Window.webContents.executeJavaScript(Source);
    async function Command(Method, Parameters = {}) {
      const Result = await GetViewport().bridge.request(Method, Parameters);
      Assert.equal(Result.ok, true, JSON.stringify(Result));
      return Result.value;
    }
    await Command('play.start');
    await Wait(() => MainEvaluate('!!document.querySelector(".native-runtime-widget-dialogue")'));
    const BeforeDialogue = (await Command('bridge.describe')).dialogue.text;
    await MainEvaluate(`(() => {
      const Surface = document.querySelector('.native-runtime-widget-dialogue');
      const Choice = Surface.querySelector('.widget-choices button:not(:disabled)');
      if (Choice) { Choice.click(); }
      else { Surface.querySelector('button:not(:disabled)').click(); }
    })()`);
    await Wait(async () => (await Command('bridge.describe')).dialogue.text !== BeforeDialogue);
    const RuntimeState = await Command('bridge.describe');
    Assert.equal(RuntimeState.widgetInstances.find(Entry => Entry.kind === 'dialogue').fields.Text, RuntimeState.dialogue.text);
    await FileSystem.writeFile(Destination.replace('.png','-runtime.png'),(await Window.webContents.capturePage()).toPNG());
    await Command('play.pause');
    await Wait(() => MainEvaluate('!!document.querySelector(".native-runtime-widget-pauseMenu")'));
    await MainEvaluate('document.querySelector(".native-runtime-widget-pauseMenu button").click()');
    await Wait(async () => (await Command('bridge.describe')).play === 'playing');
    await Command('play.stop');
    await MainEvaluate('Array.from(document.querySelectorAll(".editor-menubar>button")).find(Button=>Button.textContent.trim()==="Правка").click()');
    await Wait(() => MainEvaluate('!!document.querySelector("[data-project-settings]")'));
    await MainEvaluate('document.querySelector("[data-project-settings]").click()');
    await Wait(() => MainEvaluate(`!!document.querySelector('select[aria-label="Продолжить диалог"]')`));
    Assert.equal(await MainEvaluate('document.querySelectorAll(".project-startup-input").length'),1);
    await MainEvaluate('document.querySelector("#editor-window-project-settings form").requestSubmit()');
    await Wait(() => MainEvaluate('!!document.querySelector("#editor-window-project-settings [role=status]")'));
    await FileSystem.writeFile(Destination.replace('.png','-settings.png'),(await Window.webContents.capturePage()).toPNG());
    console.log('Widget editor check passed: create, detached canvas drag, undo/redo, save, add element, preview, reuse, reopen, validation, desktop and mobile');
    await GetViewport().stop();
    Application.exit(0);
  } catch (Failure) {
    console.error('Widget editor check failed:',Failure);
    console.error(await Window.webContents.executeJavaScript('document.body.innerText').catch(()=>''));
    await FileSystem.writeFile(Destination.replace('.png','-main-failure.png'),(await Window.webContents.capturePage()).toPNG()).catch(()=>{});
    if (EditorWindow&&!EditorWindow.isDestroyed()) {
      await FileSystem.writeFile(Destination.replace('.png','-failure.png'),(await EditorWindow.webContents.capturePage()).toPNG()).catch(()=>{});
    }
    for (const Entry of Editors.values()) {
      await Entry.viewport?.stop();
    }
    await GetViewport()?.stop();
    Application.exit(1);
  }
};
