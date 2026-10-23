// Project-owned CLI integration test. No browser driver, DOM injection or UI automation.
const fs = require('node:fs/promises');
const path = require('node:path');
const {setTimeout: delay} = require('node:timers/promises');
const assert = require('node:assert/strict');

exports.run = async ({app, window, getViewport, replaceProject}) => {
  console.log('Native smoke: starting');
  let viewport = getViewport();
  const report = {electron: process.versions.electron, napi: process.versions.napi, checks: []};
  const output = path.resolve(process.env.SACURA_NATIVE_TEST_OUTPUT || path.join(__dirname, '../..', 'build/electron-native-smoke'));
  const waitFor = async (label, check) => {
    const deadline = Date.now() + Number(process.env.SACURA_NATIVE_TEST_TIMEOUT || 60000);
    while (Date.now() < deadline) {
      if (viewport.state.error) throw new Error(viewport.state.error);
      if (viewport.state.presentation?.error) throw new Error(viewport.state.presentation.error);
      if (check()) {report.checks.push(label); return;}
      await delay(100);
    }
    throw new Error('Timeout: ' + label + ': ' + JSON.stringify(viewport.state));
  };
  try {
    await fs.mkdir(output, {recursive: true});
    console.log('Native smoke: output ready');
    await waitFor('native Engine ready', () => viewport.state.ready);
    assert.equal(String(viewport.state.ready.parent), viewport.parent);
    assert.equal(viewport.state.ready.isChild, true);
    assert.ok(viewport.state.ready.meshes >= 2);
    report.ready = viewport.state.ready;
    await waitFor('real meshes drawn and React IPC responsive', () => viewport.state.stats?.resolvedMeshes >= 2 && viewport.state.stats?.visibleObjects >= 2 && viewport.state.stats?.triangles >= 24 && viewport.heartbeat >= 3 && viewport.state.bounds?.visible && viewport.state.presentation?.frames > 30);
    let lastFrame = viewport.state.stats.frame;
    for (const size of [[1100, 760], [1450, 900], [960, 680]]) {
      const seq = viewport.sequence, heartbeat = viewport.heartbeat;
      window.setSize(...size);
      await waitFor('resize ' + size.join('x') + ': DOM → HWND → swapchain; React alive', () => {
        const {bounds, stats} = viewport.state;
        return viewport.sequence > seq && bounds?.seq === viewport.sequence && bounds.width > 0 &&
          bounds.width === stats?.width && bounds.height === stats?.height && stats.frame > lastFrame && viewport.heartbeat > heartbeat;
      });
      const {bounds, stats} = viewport.state;
      assert.equal(bounds.x, viewport.lastBounds.x); assert.equal(bounds.y, viewport.lastBounds.y);
      report.checks.push({bounds, stats});
      lastFrame = stats.frame;
    }
    window.setSize(1450,900); await delay(800);
    const test = command => window.webContents.send('native-test',command);
    const input = event => test({action:'input',event});
    const pointer = (phase,x,y,button=0,buttons=phase==='up'?0:1,extra={}) => input({kind:'pointer',phase,x,y,button,buttons,...extra});
    const key = (code,down,extra={}) => input({kind:'key',code,down,...extra});
    const state = () => viewport.state.viewport?.data;
    await waitFor('native input controller ready',()=>state()?.pickTargets?.length>=2);
    const target=state().pickTargets[1];
    pointer('down',target.x,target.y); await delay(40); pointer('up',target.x,target.y);
    await waitFor('React input → native ray picking selects actual scene object',()=>state()?.selection?.id===target.id);
    const before=state().selection.position;
    pointer('move',target.x,target.y,0,0); await delay(50);
    pointer('down',target.x,target.y); await delay(60);
    for(let i=1;i<=12;i++){pointer('move',target.x+i*5,target.y+10);await delay(20);}
    pointer('up',target.x+60,target.y+10);
    await waitFor('native ImGuizmo drag committed through EditorCore command stack',()=>state()?.edits>=1&&state()?.canUndo);
    assert.notDeepEqual(state().selection.position,before);
    key('KeyZ',true,{ctrl:true});key('KeyZ',false);
    await waitFor('native transform undo',()=>state()?.selection?.position.every((v,i)=>Math.abs(v-before[i])<.001));
    key('KeyY',true,{ctrl:true});key('KeyY',false);
    await waitFor('native transform redo',()=>state()?.canUndo);
    const camera=state().cameraPosition;
    pointer('down',300,150,2,2);key('KeyW',true);
    pointer('move',335,160,2,2);await delay(400);key('KeyW',false);pointer('up',335,160,2,0);
    await waitFor('native mouse look and keyboard fly camera',()=>state()?.cameraPosition.some((v,i)=>Math.abs(v-camera[i])>.05)&&state()?.heldKeys===0);
    input({kind:'wheel',delta:-120});
    key('KeyF',true);key('KeyF',false);
    await delay(600);
    // Continuous layout changes exercise the real React dock state and resize path.
    for(let i=0;i<25;i++){test({action:'layout',left:220+i*3,right:280-i*2,height:350+i*3});await delay(25);}
    await waitFor('continuous dock resize settles to latest DOM extent',()=>viewport.lastBounds&&!viewport.lastBounds.interactive&&viewport.state.stats.width===viewport.lastBounds.width&&viewport.state.stats.height===viewport.lastBounds.height);
    for (const position of ['left','right','bottom']) {
      test({action:'layout',left:240,right:280,height:400,position});await delay(500);
      await waitFor('dock relocation '+position,()=>viewport.state.stats.width===viewport.lastBounds.width&&viewport.state.stats.height===viewport.lastBounds.height);
    }
    for(const zoom of [1.25,0.8,1]) {
      const seq=viewport.sequence;window.webContents.setZoomFactor(zoom);
      await waitFor('live Chromium scale '+zoom,()=>viewport.sequence>seq&&!viewport.lastBounds.interactive&&viewport.state.stats.width===viewport.lastBounds.width&&viewport.state.stats.height===viewport.lastBounds.height);
    }
    key('KeyW',true);test({action:'visibility',value:false});
    await waitFor('hide suspends surface and resets input',()=>!viewport.state.bounds?.visible&&state()?.heldKeys===0);
    await delay(700);const hiddenFrames=viewport.state.presentation.frames;await delay(600);
    assert.ok(viewport.state.presentation.frames-hiddenFrames<5);
    test({action:'visibility',value:true});
    await waitFor('show resumes GPU frames',()=>viewport.state.bounds?.visible&&viewport.state.presentation.frames>hiddenFrames+10);
    window.minimize();
    await waitFor('minimize suspends native surface', () => viewport.state.bounds?.visible === false);
    await delay(500);
    window.restore(); window.focus();
    await waitFor('restore resumes presentation', () => viewport.state.bounds?.visible && viewport.state.stats.frame > lastFrame);
    window.setSize(1450, 900);
    await delay(1800);
    // One diagnostic GPU readback; never part of the presentation path.
    const capture=await window.webContents.capturePage();
    await fs.writeFile(path.join(output,'viewport.png'),capture.toPNG());
    const pixels=capture.toBitmap();
    let green=0, varied=new Set();
    for(let i=0;i<pixels.length;i+=4){const b=pixels[i],g=pixels[i+1],r=pixels[i+2];if(Math.abs(r-23)<3&&Math.abs(g-78)<3&&Math.abs(b-56)<3)green++;varied.add((r>>3)*1024+(g>>3)*32+(b>>3));}
    assert.ok(green>100,'React green overlay must be visible over GPU canvas');
    assert.ok(varied.size>100,'composited image must contain rendered detail');
    report.checks.push({compositedScreenshot:{greenOverlayPixels:green,colors:varied.size}});
    const soakSeconds=Number(process.env.SACURA_NATIVE_SOAK_SECONDS||15);
    const startFrames=viewport.state.presentation.frames, startTime=Date.now();
    const memory=[];
    for(let i=0;i<soakSeconds;i++){
      await delay(1000);if(viewport.state.error||viewport.state.presentation.error)throw new Error(JSON.stringify(viewport.state));
      assert.ok(viewport.imports<=9,'Unbounded GPU resource references');
      assert.ok(viewport.state.timing.sharedTextures<=9,'Unbounded native texture ring');
      if(i%10===0)memory.push({second:i,processes:app.getAppMetrics().map(p=>({type:p.type,pid:p.pid,memory:p.memory})),textures:viewport.state.timing.sharedTextures,bytes:viewport.state.timing.sharedTextureBytes});
    }
    report.measurements={seconds:(Date.now()-startTime)/1000,frames:viewport.state.presentation.frames-startFrames,
      fps:(viewport.state.presentation.frames-startFrames)*1000/(Date.now()-startTime),presentation:viewport.state.presentation,native:viewport.state.timing,memory};
    assert.ok(report.measurements.fps>15,'Native GPU presentation stalled');
    const old=viewport, oldPid=old.state.ready.pid;
    await replaceProject(old.project);viewport=getViewport();
    await waitFor('project replacement starts fresh Engine process and resumes frames',()=>viewport.state.ready?.pid!==oldPid&&viewport.state.stats?.resolvedMeshes>=2&&viewport.state.presentation?.frames>startFrames+30);
    assert.equal(old.state.exitCode,0);assert.equal(old.imports,0);
    report.checks.push('previous project GPU resources drained before new host');
    window.webContents.send('native-test',{action:'bridge-react'});
    await waitFor('React Native Bridge create/transform/undo/redo/delete/error contract',()=>viewport.bridgeTest);
    assert.equal(viewport.bridgeTest.ok,true,viewport.bridgeTest.error);
    report.panelChecks=viewport.bridgeTest.checks;
    await fs.writeFile(path.join(output,'panels.png'),(await window.webContents.capturePage()).toPNG());
    report.final = viewport.state;
    const code = await viewport.stop();
    assert.equal(code, 0); assert.equal(viewport.state.stopped?.type, 'stopped');
    report.final = viewport.state;
    report.checks.push('GPU shutdown completed before HWND parent destruction');
    report.passed = true;
    await fs.writeFile(path.join(output, 'report.json'), JSON.stringify(report, null, 2));
    console.log('PASS native viewport integration: ' + output);
    window.close();
  } catch (error) {
    console.error('FAIL native viewport integration:', error);
    await Promise.race([window.webContents.capturePage().then(capture=>fs.writeFile(path.join(output,'failure-ui.png'),capture.toPNG())).catch(()=>{}),delay(2000)]);
    viewport.send({type: 'capture', path: path.join(output, 'failure.png')});
    await delay(500);
    report.error = error.stack; report.state = viewport.state; report.heartbeat = viewport.heartbeat;
    await fs.mkdir(output, {recursive: true});
    await fs.writeFile(path.join(output, 'report.json'), JSON.stringify(report, null, 2));
    await viewport.stop(); app.exit(1);
  }
};
