const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');

exports.run = async ({app: Application, window: Window, windows: Windows, editors: Editors, getViewport: GetViewport, output: Output}) => {
  const Destination = Path.resolve(Output);
  const LibraryName = "WidgetAcceptance" + Date.now();
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
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".native-widget-libraries")'));
    await Wait(() => Window.webContents.executeJavaScript('document.querySelector(".native-widget-libraries>button")?.disabled === false'));
    await Window.webContents.executeJavaScript('document.querySelector(".native-widget-libraries>button").click()');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".asset-create-dialog input")'));
    await Window.webContents.executeJavaScript(`(() => {
      const Input = document.querySelector('.asset-create-dialog input');
      Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(Input,${JSON.stringify(LibraryName)});
      Input.dispatchEvent(new Event('input',{bubbles:true}));
    })()`);
    await Delay(100);
    await Window.webContents.executeJavaScript('document.querySelector(".asset-create-dialog button[type=submit]").click()');
    await Wait(() => Window.webContents.executeJavaScript('!!document.querySelector(".native-widget-library") && !document.querySelector(".asset-create-dialog")'));
    await Window.webContents.executeJavaScript(`Array.from(document.querySelectorAll(".native-widget-library")).find(Element=>Element.textContent.includes(${JSON.stringify(LibraryName)})).click()`);
    await Wait(() => Array.from(Editors.values()).some(Entry => Entry.label.includes(LibraryName) && Entry.ready));
    Editor = Array.from(Editors.values()).find(Entry => Entry.label.includes(LibraryName));
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
    Assert.equal((await Read()).document.widgets.length,4);
    const Before = (await Read()).document.widgets[0].elements[1].layout;
    const Bounds = await Evaluate(`(() => {const Bounds=document.querySelectorAll('.widget-hit')[1].getBoundingClientRect();return {x:Math.round(Bounds.left+20),y:Math.round(Bounds.top+Bounds.height/2)};})()`);
    EditorWindow.webContents.sendInputEvent({type:'mouseMove',...Bounds});
    EditorWindow.webContents.sendInputEvent({type:'mouseDown',button:'left',clickCount:1,...Bounds});
    EditorWindow.webContents.sendInputEvent({type:'mouseMove',x:Bounds.x+32,y:Bounds.y-24});
    await Delay(100);
    EditorWindow.webContents.sendInputEvent({type:'mouseUp',button:'left',clickCount:1,x:Bounds.x+32,y:Bounds.y-24});
    await Wait(async () => (await Read()).dirty);
    const Changed = (await Read()).document.widgets[0].elements[1].layout;
    Assert.notDeepEqual(Changed,Before);
    await ClickToolbar('Отменить');
    await Wait(async () => !(await Read()).dirty);
    Assert.deepEqual((await Read()).document.widgets[0].elements[1].layout,Before);
    await ClickToolbar('Повторить');
    await Wait(async () => (await Read()).dirty);
    Assert.deepEqual((await Read()).document.widgets[0].elements[1].layout,Changed);
    await ClickToolbar('Сохранить');
    await Wait(async () => !(await Read()).dirty);
    await Evaluate('document.querySelector("select[aria-label=\\\"Добавить элемент виджета\\\"]").value="text";document.querySelector("select[aria-label=\\\"Добавить элемент виджета\\\"]").dispatchEvent(new Event("change",{bubbles:true}))');
    await Wait(async () => (await Read()).document.widgets[0].elements.length===7);
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
    Malformed.widgets[0].elements[0].parentId = Malformed.widgets[0].elements[0].id;
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
