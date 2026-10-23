export function CreateDocumentDesktopApi(Descriptor) {
  const Desktop = window.sacuraDesktop;
  const Identifier = Descriptor.id;
  const Call = (Channel, Value) => Desktop.nativeDocumentCall(Identifier, Channel, Value);
  return {
    ...Desktop,
    documentId:Identifier,
    assetEditorIdentity:Descriptor.identity,
    bridge:{
      connect:() => Call('bridge-connect'),
      request:Request => Call('bridge-command', Request),
      subscribe:Callback => Desktop.onDocumentEvent(Identifier, Callback)
    },
    nativeBounds:Value => Call('native-bounds', Value),
    nativeInput:Value => Call('native-input', Value),
    nativeTools:Value => Call('native-tools', Value),
    nativeStatus:() => Desktop.nativeDocumentStatus(Identifier),
    nativeHeartbeat:() => Call('native-heartbeat'),
    nativeDetach:() => Call('native-detach'),
    onAssetEditorAction:Callback => Desktop.onAssetEditorAction(Callback, Identifier)
  };
}
