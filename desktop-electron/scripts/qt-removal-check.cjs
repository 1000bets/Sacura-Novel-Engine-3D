const FileSystem = require('node:fs/promises');
const Path = require('node:path');
const Assert = require('node:assert/strict');
const {setTimeout: Delay} = require('node:timers/promises');
exports.Run = async ({Application, Window, Editors, GetViewport}) => {
  const Output = Path.resolve(__dirname, '../../build/qt-removal-ui');
  const Report = {checks: [], errors: []};
  Window.webContents.on('console-message', Details => {
    if (Details.level === 'error') { Report.errors.push(Details.message); }
  });
  const Wait = async Predicate => {
    const Deadline = Date.now() + 45000;
    while (!Predicate()) {
      if (Date.now() > Deadline) { throw Error('Migration UI check timed out'); }
      await Delay(100);
    }
  };
  try {
    await FileSystem.mkdir(Output, {recursive: true});
    await Wait(() => GetViewport().bridge.connected && GetViewport().heartbeat > 2);
    const Call = async (Method, Params = {}) => {
      const Result = await GetViewport().bridge.request(Method, Params);
      Assert.equal(Result.ok, true, JSON.stringify(Result));
      return Result.value;
    };
    await Call('story.addNarrative', {type: 'game.Multiply', x: 50, y: 50});
    const Story = await Call('story.addNarrative', {type: 'game.Branch', x: 500, y: 50});
    Window.webContents.send('native-test', {action: 'migration-story', nodeId: Story.document.nodes.at(-1).id});
    await Delay(1500);
    await FileSystem.writeFile(Path.join(Output, 'narrative.png'), (await Window.webContents.capturePage()).toPNG());
    Report.checks.push('Narrative canvas and inspector rendered from native snapshot');
    const Object = await Call('object.create', {name: 'PreviewRoot'});
    const Prefab = await Call('prefab.create', {objectId: Object.id, path: 'MigrationUi.prefab'});
    await Editors.open({assetId: Prefab.assetId});
    await Delay(2000);
    await FileSystem.writeFile(Path.join(Output, 'prefab.png'), (await Window.webContents.capturePage()).toPNG());
    const Owner = [...Editors.values()][0];
    await Wait(() => Owner.viewport.state.stats?.frame > 5);
    let Document = await Owner.viewport.bridge.request('assetDocument.execute', {assetId: Prefab.assetId, operation: 'get'});
    Assert.equal(Document.ok, true);
    Assert.equal(Document.value.document.length, 1);
    Document = await Owner.viewport.bridge.request('assetDocument.execute', {assetId: Prefab.assetId, operation: 'command', revision: Document.value.revision, value: {method: 'object.rename', params: {objectId: Document.value.document[0].id, name: 'PreviewEdited'}}});
    Assert.equal(Document.ok, true);
    await Delay(600);
    await FileSystem.writeFile(Path.join(Output, 'prefab-edited.png'), (await Window.webContents.capturePage()).toPNG());
    Report.checks.push('Isolated prefab scene renders and refreshes after native edits');
    Assert.equal(Report.errors.length, 0, Report.errors.join('\n'));
    Report.ok = true;
  } catch (Failure) {
    Report.error = Failure.stack;
  } finally {
    await FileSystem.writeFile(Path.join(Output, 'report.json'), JSON.stringify(Report, null, 2));
    await Promise.all([...Editors.values()].map(Editor => Editor.viewport.stop()));
    await GetViewport().stop();
    Application.exit(Report.ok ? 0 : 1);
  }
};
