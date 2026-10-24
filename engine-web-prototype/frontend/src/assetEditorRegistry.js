const AssetEditors = new Map([
  ['Widget', 'widgets'], ['Prefab', 'prefab'], ['PythonScript', 'source'], ['Texture', 'texture'],
  ['StaticMesh', 'mesh'], ['Material', 'material'], ['MaterialInstance', 'material']
]);

export function RegisterAssetEditor(Type, Component) {
  if (typeof Component !== 'function') {
    throw new TypeError('Asset editor registration requires a React component');
  }
  AssetEditors.set(Type, Component);
}

export function ResolveAssetEditor(Type) {
  return AssetEditors.get(Type) || 'properties';
}
