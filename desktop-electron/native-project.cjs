const {spawn}=require('node:child_process');
const {createInterface}=require('node:readline');
const path=require('node:path');
function hostPath(app){return process.env.SACURA_NATIVE_HOST||(app.isPackaged?path.join(process.resourcesPath,'native','SakuraElectronHost.exe'):path.resolve(__dirname,'../build/windows-msvc-debug/ElectronNative/Debug/SakuraElectronHost.exe'));}
function projectCli(executable,args,Input){return new Promise((resolve,reject)=>{
  const child=spawn(executable,args,{windowsHide:true,stdio:['pipe','pipe','pipe']});
  child.stdin.on('error', () => {});
  child.stdin.end(Input);
  let result,errors='';const timer=setTimeout(()=>{child.kill();reject(Error('Project operation timed out'));},120000);
  child.on('error',e=>{clearTimeout(timer);reject(e);});
  child.stderr.on('data',b=>{errors=(errors+b).slice(-8000);});
  createInterface({input:child.stdout}).on('line',line=>{if(line.startsWith('@project ')){try{result=JSON.parse(line.slice(9));}catch{}}});
  child.on('close',code=>{clearTimeout(timer);if(code===0&&result?.ok)resolve(result);else reject(Error(result?.error||errors||`Native project operation failed (${code})`));});
});}
async function preserveDraft(bridge,choose){
  if(!bridge.connected)return true;
  const state=await bridge.request('bridge.describe',{});
  if(!state.ok)throw Error(state.error.message);
  if(!state.value.dirty&&!state.value.storyDirty&&!state.value.assetDocumentsDirty)return true;
  const choice=await choose();
  if(choice==='cancel')return false;
  if(choice==='discard')return true;
  if(choice!=='save')throw Error('Invalid close decision');
  if(state.value.play!=='stopped'){const stop=await bridge.request('play.stop',{});if(!stop.ok)throw Error(stop.error.message);}
  const saved=await bridge.request('project.save',{});
  if(!saved.ok)throw Error(saved.error.message);
  return true;
}
module.exports={hostPath,projectCli,preserveDraft};
