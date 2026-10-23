const {setTimeout:Delay} = require('node:timers/promises');
exports.DetachDocument = async (Window, Windows, Identifier) => {
  const Deadline = Date.now() + 30000;
  while (!await Window.webContents.executeJavaScript(`!!document.getElementById(${JSON.stringify('editor-tab-' + Identifier)})`)) {
    if (Date.now() > Deadline) {
      throw Error('Document tab did not open');
    }
    await Delay(100);
  }
  await Window.webContents.executeJavaScript(`(() => {
    const Tab = document.getElementById(${JSON.stringify('editor-tab-' + Identifier)});
    Tab.click();
    Tab.dispatchEvent(new MouseEvent('dblclick', {bubbles:true}));
  })()`);
  while (Date.now() < Deadline) {
    for (const Popup of Windows.values()) {
      if (await Popup.webContents.executeJavaScript(`!!document.getElementById(${JSON.stringify('editor-tab-' + Identifier)})`).catch(() => false)) {
        return Popup;
      }
    }
    await Delay(100);
  }
  throw Error('Detached document window did not open');
};
