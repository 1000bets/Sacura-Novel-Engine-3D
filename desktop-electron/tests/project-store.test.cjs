const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const {createProject, readProject, writeProject, validName} = require('../project-store.cjs');

const sample = () => ({version: 2, id: 'example', title: 'MyGame', chapters: [], objects: [{id: 'image', src: 'data:image/png;base64,aGVsbG8='}], events: []});
test('create, save, reopen and extract an imported asset', async () => {
  const parent = await fs.mkdtemp(path.join(os.tmpdir(), 'sacura-desktop-'));
  try {
    const {root} = await createProject(parent, sample());
    const disk = JSON.parse(await fs.readFile(path.join(root, 'project.json'), 'utf8'));
    assert.match(disk.objects[0].src, /^Assets\/Textures\/[a-f0-9]{64}\.png$/);
    assert.equal(await fs.readFile(path.join(root, disk.objects[0].src), 'utf8'), 'hello');
    const opened = await readProject(root);
    assert.deepEqual(opened, sample());
    opened.title = 'Edited';
    await writeProject(root, opened);
    assert.equal((await readProject(root)).title, 'Edited');
  } finally {await fs.rm(parent, {recursive: true, force: true});}
});
test('reject unsafe names and malformed project data', async () => {
  assert.throws(() => validName('../escape'));
  assert.throws(() => validName('nested/name'));
  const parent = await fs.mkdtemp(path.join(os.tmpdir(), 'sacura-desktop-'));
  try {await assert.rejects(createProject(parent, {...sample(), events: null}));}
  finally {await fs.rm(parent, {recursive: true, force: true});}
});
