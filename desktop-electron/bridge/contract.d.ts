/** Protocol v1. Identity strings are persistent IDs, scoped by session + epoch. */
export type JsonValue = null | boolean | number | string | JsonValue[] | {[key:string]:JsonValue};
export type Id = string;
export type Vec3 = [number,number,number];
export interface Transform {position:Vec3;rotation:[number,number,number,number];scale:Vec3}
export interface Context {session:string;epoch:number;revision:number}
export interface ComponentInfo {id:Id;typeId:string;enabled:boolean}
export interface ObjectInfo {id:Id;name:string;parentId:Id|null;active:boolean;transform:Transform;components:ComponentInfo[]}
export interface PropertyInfo {id:string;typeId:string;editable:boolean;assetTypes?:string[];subAssetId?:Id|null;label:string;category:string;value:JsonValue;error?:string;range?:[number,number]}
/** engine.int64 is represented by a decimal string, including inside reflected arrays/structs. */
export interface DialogueState {phase:'idle'|'line'|'choice'|'action'|'end'|'failed';speaker:string;text:string;choices:string[];canAdvance:boolean;canChoose:boolean;error:string}
export interface State {dirty:boolean;storyDirty:boolean;widgetsDirty:boolean;externalChanges:boolean;projectExternalChanges:boolean;path:string;project:string;play:'stopped'|'playing'|'paused';dialogue:DialogueState;startupAudio:boolean;startupAudioPath:string|null;selection:Id|null;canUndo:boolean;canRedo:boolean;assetError?:string}
export interface AssetInfo {id:Id;name:string;path:string;relativePath:string;typeId:string;builtin:boolean;bytes:number;version:string;subassets:JsonValue[]}
export interface StorySnapshot {open:boolean;revision:number;error?:string;dirty?:boolean;readOnly?:boolean;externalChanges?:boolean;canUndo?:boolean;canRedo?:boolean;path?:string;document?:JsonValue;positions?:Record<string,{x:number;y:number}>}
export interface WidgetsSnapshot {open:boolean;error?:string;library?:JsonValue;dirty?:boolean;canUndo?:boolean;canRedo?:boolean}
export interface Commands {
  'component.setAsset': {params:{objectId:Id;componentId:Id;property:string;assetId:Id|null;subAssetId?:Id|null};result:PropertyInfo[]};
  'project.save': {params:{useCurrentAsStartup?:boolean};result:State};
  'scene.create': {params:{path:string};result:State};
  'play.step': {params:Record<string,never>;result:State};
  'play.mechanicOperation': {params:{objectId:Id;operation:string;value:JsonValue};result:JsonValue};
  'play.advanceDialogue': {params:Record<string,never>;result:State};
  'play.selectChoice': {params:{index:number};result:State};
  'audio.resolveStartup': {params:Record<string,never>;result:{absolutePath:string}};
  'audio.setStartup': {params:{assetId?:Id};result:State};
  'assets.list': {params:Record<string,never>;result:AssetInfo[]};
  'assets.refresh': {params:Record<string,never>;result:JsonValue};
  'assets.resolve': {params:{assetId:Id};result:Pick<AssetInfo,'id'|'path'|'relativePath'|'typeId'|'builtin'> & {absolutePath:string}};
  'assets.import': {params:{sourcePath:string;destination:string};result:JsonValue};
  'story.get': {params:Record<string,never>;result:StorySnapshot};
  'widgets.get': {params:Record<string,never>;result:WidgetsSnapshot};
  'widgets.apply': {params:{library:JsonValue};result:WidgetsSnapshot};
  'widgets.save': {params:Record<string,never>;result:WidgetsSnapshot};
  'widgets.undo': {params:Record<string,never>;result:WidgetsSnapshot};
  'widgets.redo': {params:Record<string,never>;result:WidgetsSnapshot};
  'story.open': {params:{assetId:Id;discard?:boolean};result:StorySnapshot};
  'story.create': {params:{path:string};result:StorySnapshot};
  'story.addNode': {params:{kind:string;x:number;y:number};result:StorySnapshot};
  'story.updateNode': {params:{nodeId:Id;patch:JsonValue};result:StorySnapshot};
  'story.deleteNode': {params:{nodeId:Id};result:StorySnapshot};
  'story.connect': {params:{sourceId:Id;port:number;targetId:Id|null};result:StorySnapshot};
  'story.moveNodes': {params:{positions:JsonValue};result:StorySnapshot};
  'story.setStart': {params:{nodeId:Id};result:StorySnapshot};
  'story.addEvent': {params:{nodeId:Id;phase:string;eventId?:Id;actionKind?:string;batchId?:Id};result:StorySnapshot};
  'story.removeEvent': {params:{nodeId:Id;bindingId:Id};result:StorySnapshot};
  'story.updateEvent': {params:{eventId:Id;event:JsonValue};result:StorySnapshot};
  'story.setOverrides': {params:{nodeId:Id;bindingId:Id;actions:JsonValue};result:StorySnapshot};
  'story.placeEvent': {params:{nodeId:Id;bindingId:Id;phase:string;join:string;batchId?:string;beforeId?:string};result:StorySnapshot};
  'story.batchMode': {params:{nodeId:Id;batchId:Id;parallel:boolean};result:StorySnapshot};
  'story.save': {params:Record<string,never>;result:StorySnapshot};
  'story.undo': {params:Record<string,never>;result:StorySnapshot};
  'story.redo': {params:Record<string,never>;result:StorySnapshot};

  'bridge.describe': {params:Record<string,never>;result:State};
  'scene.open': {params:{path:string;discard?:boolean};result:State};
  'scene.save': {params:{path?:string};result:State};
  'scene.getHierarchy': {params:Record<string,never>;result:ObjectInfo[]};
  'object.create': {params:{name:string;parentId?:Id|null;transform?:Transform;componentType?:string;primitive?:string;cameraAspect?:number};result:{id:Id}};
  'character.create': {params:{name:string;transform?:Transform};result:{id:Id}};
  'object.duplicate': {params:{objectId:Id};result:{id:Id}};
  'object.destroy': {params:{objectId:Id};result:null};
  'object.rename': {params:{objectId:Id;name:string};result:null};
  'object.setParent': {params:{objectId:Id;parentId:Id|null};result:null};
  'object.setActive': {params:{objectId:Id;active:boolean};result:null};
  'object.getComponents': {params:{objectId:Id};result:ComponentInfo[]};
  'object.addComponent': {params:{objectId:Id;typeId:string};result:{id:Id}};
  'object.removeComponent': {params:{objectId:Id;componentId:Id};result:null};
  'component.setEnabled': {params:{objectId:Id;componentId:Id;enabled:boolean};result:null};
  'component.getProperties': {params:{objectId:Id;componentId:Id};result:PropertyInfo[]};
  'component.setProperty': {params:{objectId:Id;componentId:Id;property:string;value:JsonValue};result:PropertyInfo[]};
  'transform.get': {params:{objectId:Id};result:Transform};
  'transform.set': {params:{objectId:Id;transform:Transform};result:Transform};
  'selection.set': {params:{objectId:Id|null};result:State};
  'editor.undo': {params:Record<string,never>;result:State};
  'editor.redo': {params:Record<string,never>;result:State};
  'play.start': {params:Record<string,never>;result:State};
  'play.pause': {params:Record<string,never>;result:State};
  'play.stop': {params:Record<string,never>;result:State};
  'reflection.getComponentTypes': {params:Record<string,never>;result:{typeId:string;version:number}[]};
  'assets.getScenes': {params:Record<string,never>;result:{id:Id;path:string}[]};
}
export type Request<K extends keyof Commands>={method:K;params:Commands[K]['params'];context:Context};
export type Result<T>={version:1;id:string|null;context:Context|null}&(
  {ok:true;value:T}|{ok:false;error:{code:string;message:string}});
export type Event={version:1;type:'bridge.event'}&(
  {event:'state.changed';context:Context;state:State;changes?:{scene:boolean;assets:boolean;story:boolean;widgets:boolean}}|{event:'disconnected';context:Context|null;message:string});
export interface Bridge {
  connect():Promise<{connected:boolean;context:Context|null;state?:State}>;
  request<K extends keyof Commands>(request:Request<K>):Promise<Result<Commands[K]['result']>>;
  subscribe(callback:(event:Event)=>void):()=>void;
}
