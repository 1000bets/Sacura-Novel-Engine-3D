/** Protocol v1. Identity strings are persistent IDs, scoped by session + epoch. */
export type JsonValue = null | boolean | number | string | JsonValue[] | {[key:string]:JsonValue};
export type Id = string;
export type Vec3 = [number,number,number];
export interface Transform {position:Vec3;rotation:[number,number,number,number];scale:Vec3}
export interface Context {session:string;epoch:number;revision:number}
export interface ComponentInfo {id:Id;typeId:string;displayName:string;enabled:boolean}
export interface ObjectInfo {id:Id;name:string;parentId:Id|null;active:boolean;transform:Transform;components:ComponentInfo[]}
export interface PropertyInfo {enumValues?:string[];id:string;typeId:string;editable:boolean;assetTypes?:string[];assetId?:Id|null;subAssetId?:Id|null;label:string;localizationKey?:string;category:string;value:JsonValue;error?:string;range?:[number,number]}
/** engine.int64 is represented by a decimal string, including inside reflected arrays/structs. */
export interface DialogueState {phase:'idle'|'line'|'choice'|'action'|'end'|'failed';speaker:string;text:string;choices:string[];canAdvance:boolean;canChoose:boolean;error:string}
export interface GameView {widgetLibrary?:Id;variables?:{[key:string]:JsonValue};widget?:JsonValue;widgets?:{[key:string]:JsonValue};objectId:Id;componentId:Id;title?:string;values?:{label:string;value:string|number}[];actions?:{id:string;label:string;enabled?:boolean}[];text?:string;objective?:string;controls?:string;reticle?:boolean;pointerLock?:boolean}
export interface State {gameViews:GameView[];gameRunning:boolean;gamePaused:boolean;dirty:boolean;storyDirty:boolean;assetDocumentsDirty?:boolean;externalChanges:boolean;projectExternalChanges:boolean;path:string;project:string;play:'stopped'|'playing'|'paused';dialogue:DialogueState;startupAudio:boolean;selection:Id|null;canUndo:boolean;canRedo:boolean;assetError?:string}
export type ReflectedPropertyChange =
  | {path:(string|number)[];operation:'set';value:JsonValue}
  | {path:(string|number)[];operation:'insert';value:JsonValue}
  | {path:(string|number)[];operation:'remove';index:number}
  | {path:(string|number)[];operation:'move';index:number;destination:number};
export interface ReflectedPropertySchema {
  typeId:string;kind:'value'|'struct'|'array';id?:string;label?:string;localizationKey?:string;
  category?:string;description?:string;editable?:boolean;fields?:ReflectedPropertySchema[];
  element?:ReflectedPropertySchema;elementDefault?:JsonValue;fixedSize?:number;
  enumValues?:string[];assetTypes?:string[];range?:[number,number];error?:string;
}
export type AssetDocumentRequest = {assetId:Id;subAssetId?:Id|null} & (
  {operation:'property';value:ReflectedPropertyChange;revision:number} |
  {operation:'open'|'get'|'preview'|'edit'|'source'|'save'|'undo'|'redo'|'reload'|'compile'|'command';value?:JsonValue;revision?:number}
);
export interface AssetInfo {id:Id;name:string;path:string;relativePath:string;typeId:string;builtin:boolean;browserVisible?:boolean;bytes:number;version:string;subassets:JsonValue[]}
export interface StorySnapshot {narrativeCatalog?:JsonValue[];open:boolean;revision:number;error?:string;dirty?:boolean;readOnly?:boolean;externalChanges?:boolean;canUndo?:boolean;canRedo?:boolean;path?:string;document?:JsonValue;positions?:Record<string,{x:number;y:number}>}
export interface AssetCreationType {id:string;label:string;typeId:string;category:string;extension:string;settings:(PropertyInfo & {required:boolean;enumValues?:string[]})[]}
export interface Commands {
  'story.addNarrative': {params:{type:string;x:number;y:number};result:StorySnapshot};
  'story.setNarrativeInput': {params:{nodeId:Id;input:string;value:JsonValue};result:StorySnapshot};
  'story.connectData': {params:{sourceId:string;output:string;targetId:Id;input:string};result:StorySnapshot};
  'story.validate': {params:{};result:StorySnapshot};
  'game.action': {params:{objectId:Id;componentId:Id;action:string};result:State};
  'game.state': {params:{};result:State};
  'game.advance': {params:{};result:State};
  'game.choose': {params:{index:number};result:State};
  'game.pause': {params:{paused:boolean};result:State};
  'prefab.inspect': {params:{objectId:Id};result:JsonValue};
  'prefab.create': {params:{objectId:Id;path:string;variant?:boolean};result:JsonValue};
  'prefab.instantiate': {params:{assetId:Id;parentId?:Id|null};result:JsonValue};
  'prefab.modify': {params:{objectId:Id;operation:string;selection?:JsonValue};result:JsonValue};
  'project.export': {params:{destination:string;runtimeDirectory:string;executableName:string};result:JsonValue};
  'project.settings.get': {params:Record<string,never>;result:{name:string;engineVersion:string;startupScene:string;startupStory:string}};
  'project.settings.set': {params:{settings:JsonValue};result:Commands['project.settings.get']['result']};
  'assets.folders': {params:Record<string,never>;result:string[]};
  'assets.rename': {params:{assetId:string;name:string};result:JsonValue};
  'assets.move': {params:{assetId:string;folder:string};result:JsonValue};
  'assets.delete': {params:{assetId:string};result:JsonValue};
  'assets.duplicate': {params:{assetId:string;folder:string};result:JsonValue};
  'assets.createFolder': {params:{folder:string;name:string};result:JsonValue};
  'assets.renameFolder': {params:{folder:string;name:string};result:JsonValue};
  'assets.deleteFolder': {params:{folder:string};result:JsonValue};
  'assets.creationTypes': {params:Record<string,never>;result:AssetCreationType[]};
  'assets.create': {params:{factoryId:string;path:string;settings?:JsonValue};result:{id:Id;path:string;typeId:string}};
  'editor.actions': {params:Record<string,never>;result:JsonValue};
  'editor.executeAction': {params:{actionId:Id;parentId?:Id|null};result:{id:Id|null}};
  'assets.thumbnail': {params:{assetId:Id;subAssetId?:Id|null};result:{ready:boolean;png?:string}};
  'assets.place': {params:{assetId:Id;subAssetId:Id;transform?:Transform};result:{id:Id}};
  'mesh.inspect': {params:{objectId:Id;componentId:Id};result:JsonValue};
  'mesh.material': {params:{objectId:Id;componentId:Id;slot:number;assetId:Id|null};result:JsonValue};
  'scene.pickMesh': {params:{horizontal:number;vertical:number};result:{objectId:Id;componentId:Id}|null};
  'mesh.physics': {params:{objectId:Id;componentId:Id;enabled:boolean;simulate:boolean};result:JsonValue};
  'assetDocument.execute': {params:{assetId:Id;subAssetId?:Id|null;operation:string;value?:JsonValue;revision?:number};result:JsonValue};
  'component.setAsset': {params:{objectId:Id;componentId:Id;property:string;assetId:Id|null;subAssetId?:Id|null};result:PropertyInfo[]};
  'project.save': {params:{useCurrentAsStartup?:boolean};result:State};
  'scene.create': {params:{path:string};result:State};
  'play.step': {params:Record<string,never>;result:State};
  'play.advanceDialogue': {params:Record<string,never>;result:State};
  'play.selectChoice': {params:{index:number};result:State};
  'audio.resolveStartup': {params:Record<string,never>;result:{absolutePath:string}};
  'assets.list': {params:Record<string,never>;result:AssetInfo[]};
  'assets.refresh': {params:Record<string,never>;result:JsonValue};
  'assets.resolve': {params:{assetId:Id};result:Pick<AssetInfo,'id'|'path'|'relativePath'|'typeId'|'builtin'> & {absolutePath:string}};
  'assets.import': {params:{sourcePath:string;destination:string};result:JsonValue};
  'story.get': {params:Record<string,never>;result:StorySnapshot};
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
  'object.copy': {params:{objectId:Id};result:{subtree:string}};
  'object.paste': {params:{subtree:string};result:{id:Id}};
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
  'transform.edit': {params:{objectId:Id;transaction:Id;operation:string;transform?:Transform};result:Transform|null};
  'transform.get': {params:{objectId:Id};result:Transform};
  'transform.set': {params:{objectId:Id;transform:Transform};result:Transform};
  'selection.set': {params:{objectId:Id|null};result:State};
  'editor.undo': {params:Record<string,never>;result:State};
  'editor.redo': {params:Record<string,never>;result:State};
  'play.start': {params:Record<string,never>;result:State};
  'play.pause': {params:Record<string,never>;result:State};
  'play.stop': {params:Record<string,never>;result:State};
  'reflection.getComponentTypes': {params:Record<string,never>;result:{typeId:string;displayName:string;version:number}[]};
  'assets.getScenes': {params:Record<string,never>;result:{id:Id;path:string}[]};
}
export type Request<K extends keyof Commands>={method:K;params:Commands[K]['params'];context:Context};
export type Result<T>={version:1;id:string|null;context:Context|null}&(
  {ok:true;value:T}|{ok:false;error:{code:string;message:string}});
export type Event={version:1;type:'bridge.event'}&(
  {event:'state.changed';context:Context;state:State;changes?:{scene:boolean;assets:boolean;story:boolean}}|{event:'disconnected';context:Context|null;message:string});
export interface Bridge {
  connect():Promise<{connected:boolean;context:Context|null;state?:State}>;
  request<K extends keyof Commands>(request:Request<K>):Promise<Result<Commands[K]['result']>>;
  subscribe(callback:(event:Event)=>void):()=>void;
}

export interface WidgetLibraryDocument {
  format:'sakura.widgets';
  version:1;
  widgets:WidgetLayout[];
  ui:Record<'dialogue'|'hud'|'mainMenu'|'pauseMenu',Id>;
}
export interface WidgetLayout {
  id:Id;
  name:string;
  kind:'dialogue'|'hud'|'mainMenu'|'pauseMenu';
  referenceSize:[number,number];
  elements:JsonValue[];
  css:string;
}
