const fs = require('node:fs/promises');
const path = require('node:path');
const crypto = require('node:crypto');

const MAX_PROJECT_BYTES = 200 * 1024 * 1024;
const MAX_ASSET_BYTES = 100 * 1024 * 1024;
const MAX_ASSETS = 2000;
const DATA_URL = /^data:([\w.+-]+\/[\w.+-]+)?(?:;charset=[\w-]+)?;base64,([A-Za-z0-9+/=]+)$/;
const extension = {'model/gltf-binary': '.glb', 'image/png': '.png', 'image/jpeg': '.jpg', 'image/webp': '.webp', 'image/gif': '.gif', 'image/svg+xml': '.svg', 'image/avif': '.avif', 'audio/mpeg': '.mp3', 'audio/wav': '.wav', 'audio/x-wav': '.wav', 'audio/ogg': '.ogg', 'audio/mp4': '.m4a', 'audio/aac': '.aac', 'audio/webm': '.webm', 'audio/flac': '.flac'};

function validateProject(project) {
  if (!project || typeof project !== 'object' || Array.isArray(project) || ![1, 2].includes(project.version) || typeof project.title !== 'string' || !project.title.trim() || typeof project.id !== 'string' || !Array.isArray(project.chapters) || !Array.isArray(project.objects) || !Array.isArray(project.events)) throw new Error('Invalid Sacura project');
  const json = JSON.stringify(project);
  if (Buffer.byteLength(json) > MAX_PROJECT_BYTES) throw new Error('Project is too large');
  return JSON.parse(json);
}
function validName(value) {
  const name = String(value || '').trim();
  if (!name || name === '.' || name === '..' || name.length > 100 || /[<>:"/\\|?*\x00-\x1f]|[. ]$/.test(name)) throw new Error('Invalid project directory name');
  return name;
}
function relativeAsset(value) {
  if (typeof value !== 'string' || !/^Assets\/(Models|Textures|Audio|Files)\/[a-f0-9]{64}\.[a-z0-9]+$/.test(value)) return null;
  return value;
}
function inside(root, relative) {
  const resolved = path.resolve(root, ...relative.split('/'));
  const back = path.relative(path.resolve(root), resolved);
  if (!back || back.startsWith('..' + path.sep) || back === '..' || path.isAbsolute(back)) throw new Error('Asset path escapes project');
  return resolved;
}
function assertWithin(root, target) {
  const relative = path.relative(root, target);
  if (!relative || relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) throw new Error('Asset symlink escapes project');
}
async function atomicJson(filename, value) {
  const temp = filename + '.' + crypto.randomUUID() + '.tmp';
  try {
    await fs.writeFile(temp, JSON.stringify(value, null, 2), {flag: 'wx'});
    await fs.rename(temp, filename);
  } finally {await fs.rm(temp, {force: true});}
}
function kindFor(mime) {
  if (mime?.startsWith('model/')) return 'Models';
  if (mime?.startsWith('image/')) return 'Textures';
  if (mime?.startsWith('audio/')) return 'Audio';
  return 'Files';
}
async function writeProject(root, input) {
  const project = validateProject(input);
  const created = new Set();
  const replacements = new Map();
  let count = 0;
  async function visit(value) {
    if (!value || typeof value !== 'object') return;
    for (const [key, child] of Object.entries(value)) {
      if (typeof child === 'string') {
        const match = child.match(DATA_URL);
        if (!match) continue;
        if (replacements.has(child)) {value[key] = replacements.get(child); continue;}
        if (++count > MAX_ASSETS) throw new Error('Too many embedded assets');
        const bytes = Buffer.from(match[2], 'base64');
        if (bytes.length > MAX_ASSET_BYTES) throw new Error('Asset is too large');
        const mime = match[1] || 'application/octet-stream';
        const sha = crypto.createHash('sha256').update(bytes).digest('hex');
        const relative = `Assets/${kindFor(mime)}/${sha}${extension[mime] || '.bin'}`;
        const target = inside(root, relative);
        await fs.mkdir(path.dirname(target), {recursive: true});
        assertWithin(await fs.realpath(root), await fs.realpath(path.dirname(target)));
        try {await fs.writeFile(target, bytes, {flag: 'wx'}); created.add(target);} catch (error) {if (error.code !== 'EEXIST') throw error;}
        replacements.set(child, relative); value[key] = relative;
      } else if (child && typeof child === 'object') await visit(child);
    }
  }
  try {
    await visit(project);
    await atomicJson(path.join(root, 'project.json'), project);
  } catch (error) {
    for (const filename of created) await fs.rm(filename, {force: true});
    throw error;
  }
  return {id: project.id, title: project.title, path: root};
}
async function readProject(root) {
  const filename = path.join(root, 'project.json');
  const stat = await fs.stat(filename);
  if (!stat.isFile() || stat.size > MAX_PROJECT_BYTES) throw new Error('Invalid project.json');
  const project = validateProject(JSON.parse(await fs.readFile(filename, 'utf8')));
  async function visit(value) {
    if (!value || typeof value !== 'object') return;
    for (const [key, child] of Object.entries(value)) {
      const relative = relativeAsset(child);
      if (relative) {
        const target = inside(root, relative);
        const real = await fs.realpath(target);
        assertWithin(await fs.realpath(root), real);
        const stat = await fs.stat(real);
        if (!stat.isFile() || stat.size > MAX_ASSET_BYTES) throw new Error('Invalid asset');
        const mime = Object.entries(extension).find(([, ext]) => relative.endsWith(ext))?.[0] || 'application/octet-stream';
        value[key] = `data:${mime};base64,${(await fs.readFile(real)).toString('base64')}`;
      } else if (child && typeof child === 'object') await visit(child);
    }
  }
  await visit(project);
  return project;
}
async function createProject(parent, input) {
  const project = validateProject(input);
  const root = path.join(parent, validName(project.title));
  await fs.mkdir(root, {recursive: false});
  try {
    await Promise.all(['Assets/Models', 'Assets/Textures', 'Assets/Audio', 'Assets/Files', 'Stories'].map(p => fs.mkdir(inside(root, p), {recursive: true})));
    await writeProject(root, project);
    return {root, project};
  } catch (error) {await fs.rm(root, {recursive: true, force: true}); throw error;}
}
module.exports = {validateProject, validName, writeProject, readProject, createProject};
