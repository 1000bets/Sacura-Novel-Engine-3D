// Project defaults. Times are in seconds; reduction is a positive dB amount.
export const SIDECHAIN = Object.freeze({reductionDb:4,attack:.6,release:.9});
export const SIDECHAIN_LIMITS = Object.freeze({
 attack:Object.freeze({min:.05,max:10,step:.05}),
 release:Object.freeze({min:.05,max:10,step:.05}),
 reductionDb:Object.freeze({min:0,max:24,step:.5}),
});
export function normalizeSidechain(source){
 return Object.fromEntries(Object.entries(SIDECHAIN).map(([key,fallback])=>{
  const raw=source?.[key],value=typeof raw==='number'||(typeof raw==='string'&&raw.trim())?Number(raw):NaN;
  const {min,max}=SIDECHAIN_LIMITS[key];
  return [key,Number.isFinite(value)?Math.min(max,Math.max(min,value)):fallback];
 }));
}
export function ensureAudioSettings(project){
 const settings=project.audioSettings&&typeof project.audioSettings==='object'&&!Array.isArray(project.audioSettings)?project.audioSettings:{};
 project.audioSettings={...settings,sidechain:normalizeSidechain(settings.sidechain)};
 return project;
}
