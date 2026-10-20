import test from 'node:test';
import assert from 'node:assert/strict';
import {projectAudioAssets,validateImportedAudio} from '../src/audioAssets.js';
import {SoundDesk} from '../src/audio.js';
import {fileKind,storeAssetFile,collectAssetFiles} from '../src/assetFiles.js';
import {createEmptyProject} from '../src/projectLifecycle.js';
import {readProject} from '../src/projectFiles.js';

const imported={id:'custom-audio',name:'ambient.wav',path:'Звук/ambient.wav',kind:'audio',audioKind:'music',src:'data:audio/wav;base64,AAAA',bytes:3};
test('imported audio survives project export and becomes available to playback and explorer',()=>{
 const project=createEmptyProject('Sound import');storeAssetFile(project,imported);
 const restored=readProject(JSON.stringify(project)),asset=projectAudioAssets(restored).find(item=>item.id===imported.id);
 assert.equal(asset.kind,'music');assert.equal(asset.url,imported.src);assert.equal(asset.name,'ambient');
 const files=collectAssetFiles(restored,projectAudioAssets(restored));assert.equal(files.filter(file=>file.src===imported.src).length,1);assert.equal(files.find(file=>file.id===imported.id).audioId,imported.id);
 const builtin=projectAudioAssets(restored).find(asset=>!asset.imported);assert.equal(files.find(file=>file.src===builtin.url).audioId,builtin.id);
});
test('effect and voice assignments use different playback categories',()=>{
 for(const kind of ['sound','voice','music'])assert.equal(projectAudioAssets({assetFiles:[{...imported,audioKind:kind}]}).find(item=>item.id===imported.id).kind,kind);
 assert.equal(projectAudioAssets({assetFiles:[{...imported,audioKind:undefined}]}).find(item=>item.id===imported.id).kind,'sound');
 for(const name of ['a.WAV','b.mp3','c.ogg','d.m4a','e.aac','f.flac','g.webm'])assert.equal(fileKind(name),'audio');
});
test('sound desk resolves custom assets and forgets them on project change',async()=>{
 const urls=[];
 class Player extends EventTarget{constructor(){super();this.volume=1;this.duration=1;}play(){return Promise.resolve();}pause(){}}
 const desk=new SoundDesk(url=>{urls.push(url);return new Player();});desk.setAssets(projectAudioAssets({assetFiles:[imported]}));
 desk.play(imported.id);await desk.get(imported.id).started;assert.equal(urls[0],imported.src);assert.equal(desk.get(imported.id).kind,'music');desk.stopAll();
 desk.setAssets(projectAudioAssets({}));await assert.rejects(desk.play(imported.id),/Назначьте звуковой файл/);
});
test('audio validation rejects undecodable and empty recordings before saving',async()=>{
 const files=[{...imported}];await validateImportedAudio(files,{load:async()=>({buffer:{duration:2.5}})});assert.equal(files[0].duration,2.5);
 await assert.rejects(validateImportedAudio([{...imported}],{load:async()=>{throw new Error('Decode failed');}}),/ambient.wav/);
 await assert.rejects(validateImportedAudio([{...imported}],{load:async()=>({buffer:{duration:0}})}),/не удалось прочитать звук/);
});
